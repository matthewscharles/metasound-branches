// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundClockDividerNode.h"
#include "Internationalization/Text.h"
#include "MetasoundDataFactory.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundParamHelper.h"
#include "MetasoundPrimitives.h"
#include "MetasoundBranches/Public/MetasoundCommonMacros.h"

#define LOCTEXT_NAMESPACE "MetasoundBranches_ClockDivider"

namespace Metasound::MetasoundBranches
{
/* ------------------------------------------------------------------ */
namespace ClockDivParam
{
	METASOUND_PARAM(InputTrigger, "Trigger", "Clock input.")
	METASOUND_PARAM(InputReset,   "Reset",   "Reset counter.")
}

/* ------------------------------------------------------------------ */
namespace ClockDivPrivate
{
	inline int32 DivisionForIndex(int32 Idx, int32 Offset, int32 Mult)
	{
		return (Idx + 1 + Offset) * Mult;
	}

	FName MakeOutputName(int32 Div)
	{
		return FName(*FString::FromInt(Div));
	}

	FDataVertexMetadata MakeOutputMeta(int32 Div)
	{
#if WITH_EDITOR
    const FText Tooltip = FText::Format(
        LOCTEXT("DivTooltipFmt", "Trigger every {0} clocks."), FText::AsNumber(Div));
    const FText Name    = FText::AsNumber(Div);
    return { Tooltip, Name };
#else
    return {};
#endif
	}

	FVertexInterface GetVertexInterface(int32 NumDiv, int32 Offset, int32 Mult)
	{
		using namespace ClockDivParam;
		FInputVertexInterface  In;
		FOutputVertexInterface Out;

		In.Add(TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTrigger)));
		In.Add(TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputReset)));

		for (int32 i = 0; i < NumDiv; ++i)
		{
			const int32 Div = DivisionForIndex(i, Offset, Mult);
			Out.Add(TOutputDataVertex<FTrigger>(MakeOutputName(Div), MakeOutputMeta(Div)));
		}
		return { MoveTemp(In), MoveTemp(Out) };
	}

	class FClockDivOperatorData : public TOperatorData<FClockDivOperatorData>
	{
	public:
		static const FLazyName OperatorDataTypeName;
		int32 NumDivisions;
		int32 Offset;
		int32 Multiplier;
		FClockDivOperatorData(int32 N, int32 O, int32 M)
			: NumDivisions(N), Offset(O), Multiplier(M) {}
	};
	const FLazyName FClockDivOperatorData::OperatorDataTypeName = "ClockDividerOpData";
}

/* ------------------------------------------------------------------ */
class FClockDividerOperator : public TExecutableOperator<FClockDividerOperator>
{
public:
	FClockDividerOperator(const FOperatorSettings& Settings,
	                      const FTriggerReadRef& InTrig,
	                      const FTriggerReadRef& InReset,
	                      TArray<FTriggerWriteRef> Outs,
	                      TArray<int32>            DivVals)
		: Trigger(InTrig)
		, Reset  (InReset)
		, Outputs(MoveTemp(Outs))
		, Divs   (MoveTemp(DivVals))
		, Counter(0)
	{
	}

	void BindInputs(FInputVertexInterfaceData& D) override
	{
		using namespace ClockDivParam;
		D.BindReadVertex(METASOUND_GET_PARAM_NAME(InputTrigger), Trigger);
		D.BindReadVertex(METASOUND_GET_PARAM_NAME(InputReset),   Reset);
	}

	void BindOutputs(FOutputVertexInterfaceData& D) override
	{
		for (int32 i = 0; i < Outputs.Num(); ++i)
		{
			D.BindWriteVertex(ClockDivPrivate::MakeOutputName(Divs[i]), Outputs[i]);
		}
	}

	METASOUND_DISABLE_LEGACY_IO()

	static const FNodeClassMetadata& GetNodeInfo()
	{
		static const FNodeClassMetadata Meta = []()
		{
			FNodeClassMetadata M;
			M.ClassName        = { TEXT("UE"), TEXT("Clock Divider"), TEXT("Trigger") };
			M.MajorVersion     = 1;
			M.MinorVersion     = 0;
			M.DisplayName      = LOCTEXT("ClockDivDisplay", "Clock Divider");
			M.Description      = LOCTEXT("ClockDivDesc", "Configurable N-way trigger divider.");
			M.Author           = TEXT("Charles Matthews");
			M.PromptIfMissing  = PluginNodeMissingPrompt;
			M.DefaultInterface = ClockDivPrivate::GetVertexInterface(4,0,1);
			M.CategoryHierarchy = { LOCTEXT("BranchesCat","Branches"), LOCTEXT("TriggerCat","Triggers") };
			return M;
		}();
		return Meta;
	}

	static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& P,
	                                            FBuildResults&)
	{
		using namespace ClockDivParam;
		using namespace ClockDivPrivate;

		const auto& InData = P.InputData;
		auto InTrig  = InData.GetOrCreateDefaultDataReadReference<FTrigger>(METASOUND_GET_PARAM_NAME(InputTrigger), P.OperatorSettings);
		auto InReset = InData.GetOrCreateDefaultDataReadReference<FTrigger>(METASOUND_GET_PARAM_NAME(InputReset),   P.OperatorSettings);

		const auto* Cfg = CastOperatorData<const FClockDivOperatorData>(P.Node.GetOperatorData().Get());
		const int32 Num = Cfg ? Cfg->NumDivisions : 4;
		const int32 Off = Cfg ? Cfg->Offset       : 0;
		const int32 Mul = Cfg ? Cfg->Multiplier   : 1;

		TArray<FTriggerWriteRef> Outs;
		TArray<int32> DivVals;
		Outs.Reserve(Num);
		DivVals.Reserve(Num);

		for (int32 i = 0; i < Num; ++i)
		{
			Outs.Add(FTriggerWriteRef::CreateNew(P.OperatorSettings));
			DivVals.Add(DivisionForIndex(i, Off, Mul));
		}

		return MakeUnique<FClockDividerOperator>(P.OperatorSettings,
		                                         InTrig, InReset,
		                                         MoveTemp(Outs),
		                                         MoveTemp(DivVals));
	}

	void Execute()
	{
		for (auto& O : Outputs) { O->AdvanceBlock(); }

		Reset->ExecuteBlock([](int32, int32) {}, [&](int32, int32) { Counter = 0; });

		Trigger->ExecuteBlock([](int32, int32) {},
			[&](int32 Start, int32)
			{
				++Counter;
				for (int32 i = 0; i < Divs.Num(); ++i)
				{
					if (Counter % Divs[i] == 0)
					{
						Outputs[i]->TriggerFrame(Start);
					}
				}
			});
	}

private:
	FTriggerReadRef           Trigger;
	FTriggerReadRef           Reset;
	TArray<FTriggerWriteRef>  Outputs;
	TArray<int32>             Divs;
	int32                     Counter;
};

/* ------------------------------------------------------------------ */
using FClockDividerNode = TNodeFacade<FClockDividerOperator>;
METASOUND_REGISTER_NODE_AND_CONFIGURATION(FClockDividerNode, FMetaSoundClockDividerNodeConfiguration);

} // namespace Metasound::MetasoundBranches

/* ------------------------------------------------------------------ */
FMetaSoundClockDividerNodeConfiguration::FMetaSoundClockDividerNodeConfiguration()
	: NumDivisions(8), Offset(0), Multiplier(1)
{
}

TInstancedStruct<FMetasoundFrontendClassInterface>
FMetaSoundClockDividerNodeConfiguration::OverrideDefaultInterface(const FMetasoundFrontendClass&) const
{
	using namespace Metasound::ClockDivPrivate;
	return TInstancedStruct<FMetasoundFrontendClassInterface>::Make(
		FMetasoundFrontendClassInterface::GenerateClassInterface(
			GetVertexInterface(NumDivisions, Offset, Multiplier)));
}

TSharedPtr<const Metasound::IOperatorData>
FMetaSoundClockDividerNodeConfiguration::GetOperatorData() const
{
	using namespace Metasound::ClockDivPrivate;
	return MakeShared<FClockDivOperatorData>(NumDivisions, Offset, Multiplier);
}

#undef LOCTEXT_NAMESPACE