// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundShiftRegisterNode.h"
#include "Internationalization/Text.h"
#include "MetasoundDataFactory.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundParamHelper.h"
#include "MetasoundPrimitives.h"
#include "MetasoundStandardNodesCategories.h"
#include "MetasoundBranches/Public/MetasoundCommonMacros.h"
#include "MetasoundAudioBuffer.h" 

#define LOCTEXT_NAMESPACE "MetasoundBranches_ShiftRegister"

namespace Metasound
{
namespace ShiftRegisterParam
{
	METASOUND_PARAM(InputSignal,        "In",                 "Input float to the shift register.")
	METASOUND_PARAM(InputTrigger,       "Trigger",            "Clock trigger.")
	METASOUND_PARAM(InputReset,         "Reset",              "Reset trigger.")
	METASOUND_PARAM(InputDefault,       "Default",            "Default value loaded on reset.")
	METASOUND_PARAM(InputReverse,       "Reverse",            "Shift direction (true = reverse).")
	METASOUND_PARAM(InputOutputOnReset, "Output On Reset",    "If true, reset immediately outputs defaults.")
	METASOUND_PARAM(OutputTrigger,      "On Trigger",         "Fires every shift.")
	METASOUND_PARAM(OutputReset,        "On Reset",           "Fires when reset outputs.")
	METASOUND_PARAM(OutputDirChange,    "On Direction Change","Fires when Reverse toggles.")
}

namespace ShiftRegisterPrivate
{
#if WITH_EDITOR
	const FText StageTooltip = LOCTEXT("Stage_Tooltip", "Shift-register output.");
#endif

	FName MakeStageVertexName(int32 Index)
    {
        return FName(*FString::Printf(TEXT("Stage %d"), Index + 1));
    }

	FDataVertexMetadata MakeStageMetadata(int32 Index)
	{
#if WITH_EDITOR
    const int32 Num = Index + 1;
    const FText DisplayName = FText::Format(
        LOCTEXT("StageDisplayNameFmt", "Stage {0}"), Num);
    const FText Tooltip = FText::Format(
        LOCTEXT("StageTooltipFmt", "Shift-register output at stage {0}."), Num);
    return { Tooltip, DisplayName };
#else
    return {};
#endif
	}

	FVertexInterface GetVertexInterface(int32 NumStages)
	{
		using namespace ShiftRegisterParam;

		FInputVertexInterface In;
		In.Add(TInputDataVertex<float>   (METASOUND_GET_PARAM_NAME_AND_METADATA(InputSignal)));
		In.Add(TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTrigger)));
		In.Add(TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputReset)));
		In.Add(TInputDataVertex<float>   (METASOUND_GET_PARAM_NAME_AND_METADATA(InputDefault)));
		In.Add(TInputDataVertex<bool>    (METASOUND_GET_PARAM_NAME_AND_METADATA(InputReverse)));
		In.Add(TInputDataVertex<bool>    (METASOUND_GET_PARAM_NAME_AND_METADATA(InputOutputOnReset)));

		FOutputVertexInterface Out;
		Out.Add(TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputTrigger)));
		Out.Add(TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputReset)));
		Out.Add(TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputDirChange)));

		for (int32 i = 0; i < NumStages; ++i)
		{
			Out.Add(TOutputDataVertex<float>(MakeStageVertexName(i), MakeStageMetadata(i)));
		}

		return { MoveTemp(In), MoveTemp(Out) };
	}
}

class FShiftRegisterOperator : public TExecutableOperator<FShiftRegisterOperator>
{
public:
	FShiftRegisterOperator(const FOperatorSettings& InSettings,
	                       const FFloatReadRef& InSignal,
	                       const FTriggerReadRef& InTrig,
	                       const FTriggerReadRef& InReset,
	                       const FFloatReadRef& InDefault,
	                       const TDataReadReference<bool>& InReverse,
	                       const TDataReadReference<bool>& InOutOnReset,
	                       TArray<FFloatWriteRef> InStageOutputs,
	                       TArray<FName>           InStageNames)
		: Signal(InSignal)
		, Trig(InTrig)
		, Reset(InReset)
		, Default(InDefault)
		, Reverse(InReverse)
		, OutOnReset(InOutOnReset)
		, OutTrigger(FTriggerWriteRef::CreateNew(InSettings))
		, OutReset  (FTriggerWriteRef::CreateNew(InSettings))
		, OutDir    (FTriggerWriteRef::CreateNew(InSettings))
		, StageOutputs(MoveTemp(InStageOutputs))
		, StageNames  (MoveTemp(InStageNames))
		, bReverseState(false)
		, bResetPending(false)
	{
		Shifted.Init(0.f, StageOutputs.Num());
	}

	virtual void BindInputs(FInputVertexInterfaceData& Data) override
	{
		using namespace ShiftRegisterParam;
		Data.BindReadVertex(METASOUND_GET_PARAM_NAME(InputSignal),        Signal);
		Data.BindReadVertex(METASOUND_GET_PARAM_NAME(InputTrigger),       Trig);
		Data.BindReadVertex(METASOUND_GET_PARAM_NAME(InputReset),         Reset);
		Data.BindReadVertex(METASOUND_GET_PARAM_NAME(InputDefault),       Default);
		Data.BindReadVertex(METASOUND_GET_PARAM_NAME(InputReverse),       Reverse);
		Data.BindReadVertex(METASOUND_GET_PARAM_NAME(InputOutputOnReset), OutOnReset);
	}

	virtual void BindOutputs(FOutputVertexInterfaceData& Data) override
	{
		using namespace ShiftRegisterParam;
		Data.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputTrigger),   OutTrigger);
		Data.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputReset),     OutReset);
		Data.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputDirChange), OutDir);

		for (int32 i = 0; i < StageOutputs.Num(); ++i)
		{
			Data.BindReadVertex(StageNames[i], StageOutputs[i]);
		}
	}

	METASOUND_DISABLE_LEGACY_IO()

	static const FNodeClassMetadata& GetNodeInfo()
	{
		using namespace ShiftRegisterPrivate;

		static const FNodeClassMetadata Metadata =
		[]()
		{
			FNodeClassMetadata M;
			M.ClassName        = { TEXT("UE"), TEXT("Shift Register"), TEXT("Float") };
			M.MajorVersion     = 1;
			M.MinorVersion     = 1;
			M.DisplayName      = LOCTEXT("ShiftRegisterDisplay", "Shift Register");
			M.Description      = LOCTEXT("ShiftRegisterDesc",
				"A configurable N-stage shift register (float) with clock, reset, default and reverse options.");
			M.Author           = TEXT("Charles Matthews");
			M.PromptIfMissing  = PluginNodeMissingPrompt;
			M.DefaultInterface = ShiftRegisterPrivate::GetVertexInterface(8);
			M.CategoryHierarchy =
			{
				LOCTEXT("Branches",  "Branches"),
				LOCTEXT("Modulation","Modulation")
			};
			M.Keywords = { LOCTEXT("ShiftRegisterKW", "shift,register") };
			return M;
		}();
		return Metadata;
	}

	static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& Params,
	                                            FBuildResults&)
	{
		using namespace ShiftRegisterParam;
		using namespace ShiftRegisterPrivate;

		const FVertexInterface& Iface  = Params.Node.GetVertexInterface();
		const auto&             InData = Params.InputData;

		auto GetFloat = [&](const FName& N)
		{
			return InData.GetOrCreateDefaultDataReadReference<float>(N, Params.OperatorSettings);
		};

		TDataReadReference<float>    InSignal   = GetFloat(METASOUND_GET_PARAM_NAME(InputSignal));
		TDataReadReference<FTrigger> InTrig     = InData.GetOrCreateDefaultDataReadReference<FTrigger>(METASOUND_GET_PARAM_NAME(InputTrigger), Params.OperatorSettings);
		TDataReadReference<FTrigger> InReset    = InData.GetOrCreateDefaultDataReadReference<FTrigger>(METASOUND_GET_PARAM_NAME(InputReset),   Params.OperatorSettings);
		TDataReadReference<float>    InDefault  = GetFloat(METASOUND_GET_PARAM_NAME(InputDefault));
		TDataReadReference<bool>     InReverse  = InData.GetOrCreateDefaultDataReadReference<bool>(METASOUND_GET_PARAM_NAME(InputReverse),       Params.OperatorSettings);
		TDataReadReference<bool>     InOutReset = InData.GetOrCreateDefaultDataReadReference<bool>(METASOUND_GET_PARAM_NAME(InputOutputOnReset), Params.OperatorSettings);

		TArray<FFloatWriteRef> StageOutputs;
		TArray<FName>          StageNames;
		StageOutputs.Reserve(32);
		StageNames  .Reserve(32);

		for (int32 i = 0; ; ++i)
		{
			FName VName = MakeStageVertexName(i);
			if (!Iface.ContainsOutputVertex(VName))
			{
				break;
			}
			StageOutputs.Emplace(FFloatWriteRef::CreateNew(0.f));
			StageNames  .Add   (VName);
		}

		return MakeUnique<FShiftRegisterOperator>(Params.OperatorSettings,
		                                          InSignal, InTrig, InReset, InDefault,
		                                          InReverse, InOutReset,
		                                          MoveTemp(StageOutputs),
		                                          MoveTemp(StageNames));
	}

	void Execute()
	{
		OutTrigger->AdvanceBlock();
		OutReset  ->AdvanceBlock();
		OutDir    ->AdvanceBlock();

		bool bResetOccurred = false;
		bool bTrigOccurred  = false;

		Reset->ExecuteBlock([](int32, int32) {},
			[&](int32 Start, int32)
			{
				bResetOccurred = true;
				for (float& V : Shifted)
				{
					V = *Default;
				}
				if (*OutOnReset)
				{
					OutReset->TriggerFrame(Start);
				}
				else
				{
					bResetPending = true;
				}
			});

		Trig->ExecuteBlock([](int32, int32) {},
			[&](int32 Start, int32)
			{
				bTrigOccurred = true;
				if (bResetPending)
				{
					OutReset->TriggerFrame(Start);
					bResetPending = false;
				}

				const int32 N = Shifted.Num();

				if (*Reverse)
				{
					for (int32 i = 0; i < N - 1; ++i)
					{
						Shifted[i] = Shifted[i + 1];
					}
					Shifted[N - 1] = *Signal;
				}
				else
				{
					for (int32 i = N - 1; i > 0; --i)
					{
						Shifted[i] = Shifted[i - 1];
					}
					Shifted[0] = *Signal;
				}

				OutTrigger->TriggerFrame(Start);
			});

		if (*Reverse != bReverseState)
		{
			OutDir->TriggerFrame(0);
			bReverseState = *Reverse;
		}

		if (bTrigOccurred || (bResetOccurred && *OutOnReset))
		{
			for (int32 i = 0; i < StageOutputs.Num(); ++i)
			{
				*StageOutputs[i] = Shifted[i];
			}
		}
	}

private:
	FFloatReadRef            Signal;
	FTriggerReadRef          Trig;
	FTriggerReadRef          Reset;
	FFloatReadRef            Default;
	TDataReadReference<bool> Reverse;
	TDataReadReference<bool> OutOnReset;
	FTriggerWriteRef         OutTrigger;
	FTriggerWriteRef         OutReset;
	FTriggerWriteRef         OutDir;
	TArray<FFloatWriteRef>   StageOutputs;
	TArray<FName>            StageNames;
	TArray<float>            Shifted;
	bool                     bReverseState;
	bool                     bResetPending;
};

using FShiftRegisterNode = TNodeFacade<FShiftRegisterOperator>;
METASOUND_REGISTER_NODE_AND_CONFIGURATION(FShiftRegisterNode, FMetaSoundShiftRegisterNodeConfiguration);

} 

FMetaSoundShiftRegisterNodeConfiguration::FMetaSoundShiftRegisterNodeConfiguration()
	: NumStages(8)
{
}

TInstancedStruct<FMetasoundFrontendClassInterface>
FMetaSoundShiftRegisterNodeConfiguration::OverrideDefaultInterface(const FMetasoundFrontendClass&) const
{
	using namespace Metasound::ShiftRegisterPrivate;
	return TInstancedStruct<FMetasoundFrontendClassInterface>::Make(
		FMetasoundFrontendClassInterface::GenerateClassInterface(GetVertexInterface(NumStages)));
}

#undef LOCTEXT_NAMESPACE