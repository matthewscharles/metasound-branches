// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundShiftRegisterNode.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundParamHelper.h"
#include "MetasoundPrimitives.h"
#include "MetasoundStandardNodesCategories.h"
#include "MetasoundBranches/Public/MetasoundCommonMacros.h"

#define LOCTEXT_NAMESPACE "MetasoundShiftRegisterNode"

namespace Metasound
{
namespace ShiftRegisterPrivate
{
/* ─────────────────── Params ─────────────────── */
METASOUND_PARAM(InSignal          , "In"               , "Input float.");
METASOUND_PARAM(InTrigger         , "Trigger"          , "Advance trigger.");
METASOUND_PARAM(InReset           , "Reset"            , "Reset trigger.");
METASOUND_PARAM(InDefault         , "Default"          , "Default value.");
METASOUND_PARAM(InReverse         , "Reverse"          , "Reverse flag.");
METASOUND_PARAM(InOutputOnReset   , "Output On Reset"  , "Output defaults immediately.");

METASOUND_PARAM(OutTrigger        , "On Trigger"       , "Fires after shift.");
METASOUND_PARAM(OutReset          , "On Reset"         , "Fires when reset outputs.");
METASOUND_PARAM(OutDirChange      , "On Direction Change", "Fires when Reverse toggles.");

/* ─────────────────── Config-data ─────────────── */
struct FShiftRegisterOpData : public TOperatorData<FShiftRegisterOpData>
{
	static const FLazyName OperatorDataTypeName;
	int32 NumStages;
	explicit FShiftRegisterOpData(int32 N) : NumStages(N) {}
};
const FLazyName FShiftRegisterOpData::OperatorDataTypeName = "ShiftRegisterOpData";

/* ─────────── Build vertex-interface dynamically ─────────── */
static FVertexInterface MakeInterface(int32 NumStages)
{
	FInputVertexInterface  Ins ({
		TInputDataVertex<float>   (METASOUND_GET_PARAM_NAME_AND_METADATA(InSignal)),
		TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InTrigger)),
		TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InReset)),
		TInputDataVertex<float>   (METASOUND_GET_PARAM_NAME_AND_METADATA(InDefault)),
		TInputDataVertex<bool>    (METASOUND_GET_PARAM_NAME_AND_METADATA(InReverse)),
		TInputDataVertex<bool>    (METASOUND_GET_PARAM_NAME_AND_METADATA(InOutputOnReset))
	});

	FOutputVertexInterface Outs ({
		TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutTrigger)),
		TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutReset)),
		TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutDirChange))
	});

	for (int32 i = 0; i < NumStages; ++i)
	{
		FString Label = FString::Printf(TEXT("Stage %d"), i + 1);
		FName   Pin   = *Label;
		Outs.Add(TOutputDataVertex<float>({ Pin, { FText::GetEmpty(), FText::FromString(Label) } }));
	}
	return { MoveTemp(Ins), MoveTemp(Outs) };
}

/* ─────────────────── Operator ────────────────── */
class FShiftRegisterOp : public TExecutableOperator<FShiftRegisterOp>
{
public:
	FShiftRegisterOp(const FOperatorSettings& S,
	                 const FBuildOperatorParams& P,
	                 const FShiftRegisterOpData* Cfg)
	{
		using namespace ShiftRegisterPrivate;

		NumStages = Cfg->NumStages;

		Signal  = P.InputData.GetOrCreateDefaultDataReadReference<float>   (METASOUND_GET_PARAM_NAME(InSignal) , P.OperatorSettings);
		TrigIn  = P.InputData.GetOrCreateDefaultDataReadReference<FTrigger>(METASOUND_GET_PARAM_NAME(InTrigger), P.OperatorSettings);
		ResetIn = P.InputData.GetOrCreateDefaultDataReadReference<FTrigger>(METASOUND_GET_PARAM_NAME(InReset)  , P.OperatorSettings);
		Default = P.InputData.GetOrCreateDefaultDataReadReference<float>   (METASOUND_GET_PARAM_NAME(InDefault), P.OperatorSettings);
		Reverse = P.InputData.GetOrCreateDefaultDataReadReference<bool>    (METASOUND_GET_PARAM_NAME(InReverse), P.OperatorSettings);
		OutputOnReset = P.InputData.GetOrCreateDefaultDataReadReference<bool>(METASOUND_GET_PARAM_NAME(InOutputOnReset), P.OperatorSettings);

		TrigOut      = FTriggerWriteRef::CreateNew(S);
		ResetOut     = FTriggerWriteRef::CreateNew(S);
		DirChangeOut = FTriggerWriteRef::CreateNew(S);

		for (int32 i = 0; i < NumStages; ++i)
		{
			StageData.Add(0.f);
			StageOut.Add(FFloatWriteRef::CreateNew(0.f));
		}
		bReverseState = false;
		bPendingReset = false;
	}

    static const FNodeClassMetadata& GetNodeInfo()
    {
        auto CreateNodeClassMetadata = []() -> FNodeClassMetadata
        {
            FVertexInterface NodeInterface = DeclareVertexInterface();
            FNodeClassMetadata Metadata;
            Metadata.ClassName = { TEXT("UE"), TEXT("Shift Register"), TEXT("Float") };
            Metadata.MajorVersion = 1;
            Metadata.MinorVersion = 1;
            Metadata.DisplayName = METASOUND_LOCTEXT("ShiftRegisterNodeDisplayName", "Shift Register");
            Metadata.Description = METASOUND_LOCTEXT("ShiftRegisterNodeDesc", "Shift register node with eight stages and additional reset/output options.");
            Metadata.Author = "Charles Matthews";
            Metadata.PromptIfMissing = PluginNodeMissingPrompt;
            Metadata.DefaultInterface = NodeInterface;
            Metadata.CategoryHierarchy = {
                METASOUND_LOCTEXT("Custom", "Branches"),
                METASOUND_LOCTEXT("CustomSub", "Modulation")
            };
            Metadata.Keywords = TArray<FText>();
            return Metadata;
        };
        static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
        return Metadata;
    }

    METASOUND_DISABLE_LEGACY_IO()
    
	/* bind dynamically-named outputs */
	void BindOutputs(FOutputVertexInterfaceData& D) override
	{
		using namespace ShiftRegisterPrivate;
		D.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutTrigger)   , TrigOut);
		D.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutReset)     , ResetOut);
		D.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutDirChange) , DirChangeOut);

		for (int32 i = 0; i < NumStages; ++i)
		{
			FName Pin = *FString::Printf(TEXT("Stage %d"), i + 1);
			D.BindReadVertex(Pin, StageOut[i]);
		}
	}

	void BindInputs (FInputVertexInterfaceData&) override {}
	void Reset(const IOperator::FResetParams&)   override {}

	void Execute()
	{
		TrigOut     ->AdvanceBlock();
		ResetOut    ->AdvanceBlock();
		DirChangeOut->AdvanceBlock();

		/* ---------- handle reset trigger ---------- */
		ResetIn->ExecuteBlock([](int32, int32){},
		[&](int32 F, int32)
		{
			for (float& v : StageData) v = *Default;
			if (*OutputOnReset) ResetOut->TriggerFrame(F);
			else                bPendingReset = true;
		});

		/* ---------- handle direction change -------- */
		bool NowRev = *Reverse;
		if (NowRev != bReverseState)
		{
			DirChangeOut->TriggerFrame(0);
			bReverseState = NowRev;
		}

		/* ---------- main shift trigger ------------ */
		TrigIn->ExecuteBlock([](int32, int32){},
		[&](int32 F, int32)
		{
			if (bPendingReset)
			{
				ResetOut->TriggerFrame(F);
				bPendingReset = false;
			}

			if (bReverseState)
			{
				for (int32 i = 0; i < NumStages - 1; ++i) StageData[i] = StageData[i + 1];
				StageData.Last() = *Signal;
			}
			else
			{
				for (int32 i = NumStages - 1; i > 0; --i) StageData[i] = StageData[i - 1];
				StageData[0] = *Signal;
			}

			for (int32 i = 0; i < NumStages; ++i) *StageOut[i] = StageData[i];
			TrigOut->TriggerFrame(F);
		});
	}

private:
	/* inputs */
	FFloatReadRef   Signal;
	FTriggerReadRef TrigIn;
	FTriggerReadRef ResetIn;
	FFloatReadRef   Default;
	TDataReadReference<bool> Reverse;
	TDataReadReference<bool> OutputOnReset;

	/* outputs */
	FTriggerWriteRef TrigOut;
	FTriggerWriteRef ResetOut;
	FTriggerWriteRef DirChangeOut;
	TArray<FFloatWriteRef> StageOut;

	/* state */
	TArray<float> StageData;
	int32         NumStages;
	bool          bReverseState;
	bool          bPendingReset;
};

/* ───────── Node façade  (generated by macro) ───────── */
using FShiftRegisterNode = TNodeFacade<FShiftRegisterOp>;

} // ShiftRegisterPrivate
} // Metasound

/*────────── USTRUCT configuration ─────────*/
TInstancedStruct<FMetasoundFrontendClassInterface>
FMetaSoundShiftRegisterNodeConfiguration::OverrideDefaultInterface(const FMetasoundFrontendClass&) const
{
	return TInstancedStruct<FMetasoundFrontendClassInterface>::Make(
		FMetasoundFrontendClassInterface::GenerateClassInterface(
			Metasound::ShiftRegisterPrivate::MakeInterface(NumStages)));
}

TSharedPtr<const Metasound::IOperatorData>
FMetaSoundShiftRegisterNodeConfiguration::GetOperatorData() const
{
	return MakeShared<Metasound::ShiftRegisterPrivate::FShiftRegisterOpData>(NumStages);
}

/*────────── Registration ─────────*/
METASOUND_REGISTER_NODE_AND_CONFIGURATION(
	Metasound::ShiftRegisterPrivate::FShiftRegisterNode,
	FMetaSoundShiftRegisterNodeConfiguration)

#undef LOCTEXT_NAMESPACE