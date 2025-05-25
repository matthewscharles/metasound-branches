// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundSlewConfigurableNode.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundPrimitives.h"
#include "MetasoundParamHelper.h"
#include "MetasoundAudioBuffer.h"

#define LOCTEXT_NAMESPACE "MetasoundSlewConfigurableNode"

using namespace Metasound;

/* ─────────────────────── Operator-data ────────────────────── */
const FLazyName FSlewConfigurableOperatorData::OperatorDataTypeName = "SlewConfigurableOperatorData";

/* ─────────────────────── Vertex helpers ───────────────────── */
namespace SlewConfigurableNames
{
	METASOUND_PARAM(InSignal , "In"       , "Signal to smooth");
	METASOUND_PARAM(InRise   , "Rise Time", "Rise time (s)");
	METASOUND_PARAM(InFall   , "Fall Time", "Fall time (s)");
	METASOUND_PARAM(OutSignal, "Out"      , "Slewed signal");
}

/* ─────────────────────── Interface builder ────────────────── */
static FVertexInterface BuildInterface(int32 NumPins, ESlewConfigurableMode Mode)
{
	FInputVertexInterface  Inputs;
	FOutputVertexInterface Outputs;

	for (int32 i = 0; i < NumPins; ++i)
	{
		const FString Suffix = FString::Printf(TEXT(" %d"), i);

		if (Mode == ESlewConfigurableMode::Control)
		{
			const FName InName  = FName(* (METASOUND_GET_PARAM_NAME(SlewConfigurableNames::InSignal).ToString()  + Suffix));
			const FName OutName = FName(* (METASOUND_GET_PARAM_NAME(SlewConfigurableNames::OutSignal).ToString() + Suffix));

			Inputs .Add(TInputDataVertex<float>      {InName , FDataVertexMetadata{}});
			Outputs.Add(TOutputDataVertex<float>     {OutName, FDataVertexMetadata{}});
		}
		else
		{
			const FName InName  = FName(* (METASOUND_GET_PARAM_NAME(SlewConfigurableNames::InSignal).ToString()  + Suffix));
			const FName OutName = FName(* (METASOUND_GET_PARAM_NAME(SlewConfigurableNames::OutSignal).ToString() + Suffix));

			Inputs .Add(TInputDataVertex<FAudioBuffer>{InName , FDataVertexMetadata{}});
			Outputs.Add(TOutputDataVertex<FAudioBuffer>{OutName, FDataVertexMetadata{}});
		}
	}

	Inputs.Add(TInputDataVertex<FTime>(METASOUND_GET_PARAM_NAME_AND_METADATA(SlewConfigurableNames::InRise)));
	Inputs.Add(TInputDataVertex<FTime>(METASOUND_GET_PARAM_NAME_AND_METADATA(SlewConfigurableNames::InFall)));

	return FVertexInterface(MoveTemp(Inputs), MoveTemp(Outputs));
}

/* ─────────────────────── Configuration ────────────────────── */
TInstancedStruct<FMetasoundFrontendClassInterface>
FMetaSoundSlewConfigurableNodeConfiguration::OverrideDefaultInterface(const FMetasoundFrontendClass&) const
{
	return TInstancedStruct<FMetasoundFrontendClassInterface>::Make(
		FMetasoundFrontendClassInterface::GenerateClassInterface(
			BuildInterface(NumPins, SlewMode)));
}

TSharedPtr<const IOperatorData>
FMetaSoundSlewConfigurableNodeConfiguration::GetOperatorData() const
{
	return MakeShared<FSlewConfigurableOperatorData>(SlewMode, NumPins);
}

/* ─────────────────── Float-rate operator ──────────────────── */
class FSlewConfigurableFloatOperator : public TExecutableOperator<FSlewConfigurableFloatOperator>
{
public:
	FSlewConfigurableFloatOperator(const FOperatorSettings& Settings,
	                               TArray<TDataReadReference<float>>  InSignals,
	                               TDataReadReference<FTime>          InRise,
	                               TDataReadReference<FTime>          InFall)
		: Rise(InRise), Fall(InFall), Inputs(MoveTemp(InSignals))
	{
		PrevOut.Init(0.f, Inputs.Num());
		for (int32 i = 0; i < Inputs.Num(); ++i)
			Outputs.Add(TDataWriteReferenceFactory<float>::CreateExplicitArgs(Settings));
	}

	void BindInputs (FInputVertexInterfaceData&) override {}
	void BindOutputs(FOutputVertexInterfaceData&) override {}

	void Execute()
	{
		const float RiseAlpha = CalcAlpha(Rise);
		const float FallAlpha = CalcAlpha(Fall);

		for (int32 c = 0; c < Inputs.Num(); ++c)
		{
			float  In  = *Inputs[c];
			float& Out = PrevOut[c];

			if      (In > Out) Out = RiseAlpha * Out + (1 - RiseAlpha) * In;
			else if (In < Out) Out = FallAlpha * Out + (1 - FallAlpha) * In;

			*Outputs[c] = Out;
		}
	}

private:
	static float CalcAlpha(const TDataReadReference<FTime>& T)
	{
		const float S = T->GetSeconds();
		return S > 0.f ? FMath::Exp(-1.f / S) : 0.f;
	}

	TDataReadReference<FTime>           Rise, Fall;
	TArray<TDataReadReference<float>>   Inputs;
	TArray<TDataWriteReference<float>>  Outputs;
	TArray<float>                       PrevOut;
};

/* ─────────────────── Audio-rate operator ──────────────────── */
class FSlewConfigurableAudioOperator : public TExecutableOperator<FSlewConfigurableAudioOperator>
{
public:
	FSlewConfigurableAudioOperator(const FOperatorSettings& Settings,
	                               TArray<TDataReadReference<FAudioBuffer>> InSignals,
	                               TDataReadReference<FTime>                InRise,
	                               TDataReadReference<FTime>                InFall)
		: Rise(InRise), Fall(InFall), Inputs(MoveTemp(InSignals)), SampleRate(Settings.GetSampleRate())
	{
		for (int32 i = 0; i < Inputs.Num(); ++i)
		{
			Outputs.Add(FAudioBufferWriteRef::CreateNew(Settings));
			PrevOut.Add(0.f);
		}
	}

	void BindInputs (FInputVertexInterfaceData&) override {}
	void BindOutputs(FOutputVertexInterfaceData&) override {}

	void Execute()
	{
		const float RiseAlpha = CalcAlpha(Rise);
		const float FallAlpha = CalcAlpha(Fall);

		for (int32 c = 0; c < Inputs.Num(); ++c)
		{
			const int32 Frames = Inputs[c]->Num();

			const float* In  = Inputs[c]->GetData();
			float*       Out = Outputs[c]->GetData();
			float&       Ref = PrevOut[c];

			for (int32 i = 0; i < Frames; ++i)
			{
				const float V = In[i];
				if      (V > Ref) Ref = RiseAlpha * Ref + (1 - RiseAlpha) * V;
				else if (V < Ref) Ref = FallAlpha * Ref + (1 - FallAlpha) * V;
				Out[i] = Ref;
			}
		}
	}

private:
	float CalcAlpha(const TDataReadReference<FTime>& T) const
	{
		const float S = T->GetSeconds();
		return S > 0.f ? FMath::Exp(-1.f / (S * SampleRate)) : 0.f;
	}

	TDataReadReference<FTime>                     Rise, Fall;
	TArray<TDataReadReference<FAudioBuffer>>      Inputs;
	TArray<FAudioBufferWriteRef>                  Outputs;
	TArray<float>                                 PrevOut;
	int32                                         SampleRate;
};

/* ─────────────────── Operator selector ───────────────────── */
class FSlewConfigurableOperator
{
public:
	static const FNodeClassMetadata& GetNodeInfo()
	{
		static const FNodeClassMetadata Meta = []()
		{
			FNodeClassMetadata M;
			M.ClassName       = { TEXT("Branches"), TEXT("SlewConfigurable"), TEXT("") };
			M.MajorVersion    = 1;
			M.MinorVersion    = 0;
			M.DisplayName     = LOCTEXT("SlewCfgDisplay", "Slew (Configurable)");
			M.Description     = LOCTEXT("SlewCfgDesc", "Smooth signal; audio- or control-rate, pin-count selectable.");
			M.Author          = TEXT("Charles Matthews");
			M.PromptIfMissing = LOCTEXT("SlewCfgMissing", "Enable MetaSound Branches.");
			M.DefaultInterface= BuildInterface(1, ESlewConfigurableMode::Audio);
			M.CategoryHierarchy =
			{
				LOCTEXT("Branches", "Branches"),
				LOCTEXT("Filters" , "Filters")
			};
			return M;
		}();
		return Meta;
	}

	static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& P, FBuildResults&)
	{
		const FSlewConfigurableOperatorData* Cfg =
			CastOperatorData<const FSlewConfigurableOperatorData>(P.Node.GetOperatorData().Get());

		const int32 Num = Cfg->NumPins;

		TDataReadReference<FTime> Rise =
			P.InputData.GetOrCreateDefaultDataReadReference<FTime>(
				METASOUND_GET_PARAM_NAME(SlewConfigurableNames::InRise), P.OperatorSettings);

		TDataReadReference<FTime> Fall =
			P.InputData.GetOrCreateDefaultDataReadReference<FTime>(
				METASOUND_GET_PARAM_NAME(SlewConfigurableNames::InFall), P.OperatorSettings);

		if (Cfg->Mode == ESlewConfigurableMode::Control)
		{
			TArray<TDataReadReference<float>> InRefs;
			for (int32 i = 0; i < Num; ++i)
			{
				const FString Pin = FString::Printf(TEXT("%s %d"),
					*METASOUND_GET_PARAM_NAME(SlewConfigurableNames::InSignal).ToString(), i);

				InRefs.Add(P.InputData.GetOrCreateDefaultDataReadReference<float>(
					FName(*Pin), P.OperatorSettings));
			}

			return MakeUnique<FSlewConfigurableFloatOperator>(
				P.OperatorSettings, MoveTemp(InRefs), Rise, Fall);
		}
		else
		{
			TArray<TDataReadReference<FAudioBuffer>> InRefs;
			for (int32 i = 0; i < Num; ++i)
			{
				const FString Pin = FString::Printf(TEXT("%s %d"),
					*METASOUND_GET_PARAM_NAME(SlewConfigurableNames::InSignal).ToString(), i);

				InRefs.Add(P.InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(
					FName(*Pin), P.OperatorSettings));
			}

			return MakeUnique<FSlewConfigurableAudioOperator>(
				P.OperatorSettings, MoveTemp(InRefs), Rise, Fall);
		}
	}
};

/* ─────────────────── Facade & registration ───────────────── */
// using FSlewConfigurableNode = TNodeFacade<FSlewConfigurableOperator>;

// METASOUND_REGISTER_NODE_AND_CONFIGURATION(
// 	FSlewConfigurableNode,
// 	FMetaSoundSlewConfigurableNodeConfiguration)

namespace Metasound
{
	class FSlewConfigurableNode : public FNodeFacade
	{
	public:
		FSlewConfigurableNode(const FNodeInitData& InitData)
			: FNodeFacade(InitData, MakeShared<FSlewConfigurableOperator>()) {}
	};
}

METASOUND_REGISTER_NODE_AND_CONFIGURATION(FSlewConfigurableNode, FMetaSoundSlewConfigurableNodeConfiguration)



#undef LOCTEXT_NAMESPACE