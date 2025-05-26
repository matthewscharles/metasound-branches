// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundShiftRegisterNode.h"
#include "Internationalization/Text.h"
#include "MetasoundDataFactory.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundParamHelper.h"
#include "MetasoundBranches/Public/MetasoundCommonMacros.h"

#define LOCTEXT_NAMESPACE "MetasoundBranches_ShiftRegister"

namespace Metasound
{
/* ------------------------------------------------------------------ */
/*  Parameter names                                                   */
/* ------------------------------------------------------------------ */
namespace ShiftRegisterParam
{
	METASOUND_PARAM(InputSignal,         "In",                "Input float to the shift register.")
	METASOUND_PARAM(InputTrigger,        "Trigger",           "Clock trigger.")
	METASOUND_PARAM(InputReset,          "Reset",             "Reset trigger.")
	METASOUND_PARAM(InputDefault,        "Default",           "Default value loaded on reset.")
	METASOUND_PARAM(InputReverse,        "Reverse",           "Shift direction (true = reverse).")
	METASOUND_PARAM(InputOutputOnReset,  "Output On Reset",   "If true, reset immediately outputs defaults.")
	METASOUND_PARAM(OutputTrigger,       "On Trigger",        "Fires every shift.")
	METASOUND_PARAM(OutputReset,         "On Reset",          "Fires when reset outputs.")
	METASOUND_PARAM(OutputDirChange,     "On Direction Change","Fires when Reverse toggles.")
}

/* ------------------------------------------------------------------ */
/*  Helper to build per-stage vertex names & metadata                 */
/* ------------------------------------------------------------------ */
namespace ShiftRegisterPrivate
{
	const FLazyName StageBaseName{ "Stage" };
#if WITH_EDITOR
	const FText     StageTooltip = LOCTEXT("Stage_Tooltip", "Shift-register output.");
#endif

	FName MakeStageVertexName(int32 Index)
	{
		FName N = StageBaseName;
		N.SetNumber(Index + 1); // 1-based for users
		return N;
	}

	FDataVertexMetadata MakeStageMetadata(int32 Index)
	{
#if WITH_EDITOR
		const FText DisplayName = FText::Format(LOCTEXT("StageDisplayName", "Stage {0}"), Index + 1);
		return { StageTooltip, DisplayName };
#else
		return {};
#endif
	}

	/* Full vertex interface for a given stage count. */
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

/* ------------------------------------------------------------------ */
/*  Operator                                                          */
/* ------------------------------------------------------------------ */
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
		, OutReset(FTriggerWriteRef::CreateNew(InSettings))
		, OutDir(FTriggerWriteRef::CreateNew(InSettings))
		, StageOutputs(MoveTemp(InStageOutputs))
		, StageNames(MoveTemp(InStageNames))
		, Shifted(StageOutputs.Num(), 0.f)
		, bReverseState(false)
		, bResetPending(false)
	{
	}

	/* ---------------- Bind ---------------- */
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
		Data.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputTrigger),    OutTrigger);
		Data.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputReset),      OutReset);
		Data.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputDirChange),  OutDir);

		for (int32 i = 0; i < StageOutputs.Num(); ++i)
		{
			Data.BindReadVertex(StageNames[i], StageOutputs[i]);
		}
	}

	METASOUND_DISABLE_LEGACY_IO()
    /* ---------------- Metadata ---------------- */

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
				"A configurable N-stage shift register (float) with clock, reset, "
				"default and reverse options.");
			M.Author           = TEXT("Charles Matthews");
			M.PromptIfMissing  = PluginNodeMissingPrompt;         
			M.DefaultInterface = ShiftRegisterPrivate::GetVertexInterface(8); // fallback preview
			M.CategoryHierarchy =
			{
				LOCTEXT("Branches",  "Branches"),
				LOCTEXT("Modulation","Modulation")
			};
			M.Keywords = { LOCTEXT("ShiftRegisterKW", "shift,register,buffer,latency") };
			return M;
		}();
		return Metadata;
	}
    
	/* ---------------- Factory ------------- */
	static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& Params,
	                                            FBuildResults& /*OutResults*/)
	{
		using namespace ShiftRegisterParam;
		using namespace ShiftRegisterPrivate;

		const FVertexInterface& Iface = Params.Node.GetVertexInterface();
		const FInputVertexInterfaceData& InData = Params.InputData;

		auto GetRead = [&](const FName& N)
		{
			return InData.GetOrCreateDefaultDataReadReference<float>(N, Params.OperatorSettings);
		};

		TDataReadReference<float>   InSignal  = GetRead(METASOUND_GET_PARAM_NAME(InputSignal));
		TDataReadReference<FTrigger>InTrig    = InData.GetOrCreateDefaultDataReadReference<FTrigger>(METASOUND_GET_PARAM_NAME(InputTrigger), Params.OperatorSettings);
		TDataReadReference<FTrigger>InReset   = InData.GetOrCreateDefaultDataReadReference<FTrigger>(METASOUND_GET_PARAM_NAME(InputReset), Params.OperatorSettings);
		TDataReadReference<float>   InDefault = GetRead(METASOUND_GET_PARAM_NAME(InputDefault));
		TDataReadReference<bool>    InReverse = InData.GetOrCreateDefaultDataReadReference<bool>(METASOUND_GET_PARAM_NAME(InputReverse), Params.OperatorSettings);
		TDataReadReference<bool>    InOutOnReset = InData.GetOrCreateDefaultDataReadReference<bool>(METASOUND_GET_PARAM_NAME(InputOutputOnReset), Params.OperatorSettings);

		/* Build dynamic stage outputs */
		TArray<FFloatWriteRef> StageOutputs;
		TArray<FName>          StageNames;

		for (int32 i = 0;; ++i)
		{
			const FName VName = MakeStageVertexName(i);
			if (!Iface.ContainsOutputVertex(VName))
			{
				break;
			}
			StageOutputs.Add(FFloatWriteRef::CreateNew(0.f));
			StageNames  .Add(VName);
		}

		return MakeUnique<FShiftRegisterOperator>(Params.OperatorSettings,
		                                          InSignal,
		                                          InTrig,
		                                          InReset,
		                                          InDefault,
		                                          InReverse,
		                                          InOutOnReset,
		                                          MoveTemp(StageOutputs),
		                                          MoveTemp(StageNames));
	}

	/* ---------------- Execute ------------- */
	void Execute()
	{
		OutTrigger->AdvanceBlock();
		OutReset  ->AdvanceBlock();
		OutDir    ->AdvanceBlock();

		bool bResetOccurred = false;
		bool bTrigOccurred  = false;
		int32 ResetFrame    = INDEX_NONE;

		/* --- Reset handling --- */
		Reset->ExecuteBlock([](int32, int32) {},
			[&](int32 Start, int32)
			{
				bResetOccurred = true;
				ResetFrame     = Start;
				for (float& Val : Shifted)
				{
					Val = *Default;
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

		/* --- Clock trigger --- */
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

		/* --- Direction change --- */
		if (*Reverse != bReverseState)
		{
			OutDir->TriggerFrame(0);
			bReverseState = *Reverse;
		}

		/* --- Output values if needed --- */
		if (bTrigOccurred || (bResetOccurred && *OutOnReset))
		{
			for (int32 i = 0; i < StageOutputs.Num(); ++i)
			{
				*StageOutputs[i] = Shifted[i];
			}
		}
	}

private:
	/* Inputs */
	FFloatReadRef          Signal;
	FTriggerReadRef        Trig;
	FTriggerReadRef        Reset;
	FFloatReadRef          Default;
	TDataReadReference<bool> Reverse;
	TDataReadReference<bool> OutOnReset;

	/* Fixed-name outputs */
	FTriggerWriteRef       OutTrigger;
	FTriggerWriteRef       OutReset;
	FTriggerWriteRef       OutDir;

	/* Dynamic stage outputs */
	TArray<FFloatWriteRef> StageOutputs;
	TArray<FName>          StageNames;

	/* State */
	TArray<float>          Shifted;
	bool                   bReverseState;
	bool                   bResetPending;
};

/* ------------------------------------------------------------------ */
/*  Facade & registration                                             */
/* ------------------------------------------------------------------ */
using FShiftRegisterNode = TNodeFacade<FShiftRegisterOperator>;

METASOUND_REGISTER_NODE_AND_CONFIGURATION(FShiftRegisterNode, FMetaSoundShiftRegisterNodeConfiguration);

} // namespace Metasound

/* ------------------------------------------------------------------ */
/*  Config implementation                                             */
/* ------------------------------------------------------------------ */
FMetaSoundShiftRegisterNodeConfiguration::FMetaSoundShiftRegisterNodeConfiguration()
	: NumStages(8) // default matches old behaviour
{
}

TInstancedStruct<FMetasoundFrontendClassInterface>
FMetaSoundShiftRegisterNodeConfiguration::OverrideDefaultInterface(const FMetasoundFrontendClass& /*InClass*/) const
{
	using namespace Metasound::ShiftRegisterPrivate;
	return TInstancedStruct<FMetasoundFrontendClassInterface>::Make(
		FMetasoundFrontendClassInterface::GenerateClassInterface(GetVertexInterface(NumStages)));
}

#undef LOCTEXT_NAMESPACE