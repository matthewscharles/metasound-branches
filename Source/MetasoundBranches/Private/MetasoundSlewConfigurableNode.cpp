// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundSlewConfigurableNode.h"
#include "MetasoundAudioBuffer.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundParamHelper.h"
#include "MetasoundPrimitives.h"

#define LOCTEXT_NAMESPACE "MetasoundSlewConfigurableNode"

namespace Metasound
{
namespace SlewConfigurablePrivate
{
/*──────────────────  PARAMS & BUILD INTERFACE  ──────────────────*/
METASOUND_PARAM(InSignal , "In"       , "Signal to smooth");
METASOUND_PARAM(InRise   , "Rise Time", "Rise time (s)");
METASOUND_PARAM(InFall   , "Fall Time", "Fall time (s)");
METASOUND_PARAM(OutSignal, "Out"      , "Slewed signal");

static FVertexInterface BuildInterface(int32 NumPins, ESlewConfigurableMode Mode)
{
	FInputVertexInterface  Ins;
	FOutputVertexInterface Outs;

	for (int32 i = 0; i < NumPins; ++i)
	{
		FString Index = FString::Printf(TEXT(" %d"), i);
		FName   InN   = * (METASOUND_GET_PARAM_NAME(InSignal ).ToString()  + Index);
		FName   OutN  = * (METASOUND_GET_PARAM_NAME(OutSignal).ToString()  + Index);

		if (Mode == ESlewConfigurableMode::Control)
		{
			Ins .Add(TInputDataVertex <float>      {InN , FDataVertexMetadata{}});
			Outs.Add(TOutputDataVertex<float>      {OutN, FDataVertexMetadata{}});
		}
		else
		{
			Ins .Add(TInputDataVertex <FAudioBuffer>{InN , FDataVertexMetadata{}});
			Outs.Add(TOutputDataVertex<FAudioBuffer>{OutN, FDataVertexMetadata{}});
		}
	}

	Ins.Add(TInputDataVertex<FTime>(METASOUND_GET_PARAM_NAME_AND_METADATA(InRise)));
	Ins.Add(TInputDataVertex<FTime>(METASOUND_GET_PARAM_NAME_AND_METADATA(InFall)));

	return { MoveTemp(Ins), MoveTemp(Outs) };
}

/*──────────────────  CONFIG DATA  ─────────────────────────────*/
struct FSlewConfigurableOperatorData : public TOperatorData<FSlewConfigurableOperatorData>
{
	static const FLazyName OperatorDataTypeName;
	ESlewConfigurableMode Mode;
	int32                 NumPins;
	FSlewConfigurableOperatorData(ESlewConfigurableMode M, int32 N)
		: Mode(M), NumPins(N) {}
};
const FLazyName FSlewConfigurableOperatorData::OperatorDataTypeName =
	"SlewConfigurableOperatorData";

/*──────────────────  FLOAT-RATE OPERATOR  ─────────────────────*/
class FFloatOp : public TExecutableOperator<FFloatOp>
{
public:
	FFloatOp(const FOperatorSettings& S,
	         TArray<TDataReadReference<float>>  InSignals,
	         TDataReadReference<FTime>          Rise,
	         TDataReadReference<FTime>          Fall)
		: R(Rise), F(Fall), In(MoveTemp(InSignals))
	{
		Prev.Init(0.f, In.Num());
		for (int32 i = 0; i < In.Num(); ++i)
			Out.Add(TDataWriteReferenceFactory<float>::CreateExplicitArgs(S));
	}

	void BindInputs (FInputVertexInterfaceData&) override {}
	void BindOutputs(FOutputVertexInterfaceData&) override {}

	void Execute() 
	{
		const float RA = Alpha(R);
		const float FA = Alpha(F);

		for (int32 c = 0; c < In.Num(); ++c)
		{
			float v = *In[c];
			float& p = Prev[c];
			if      (v > p) p = RA * p + (1 - RA) * v;
			else if (v < p) p = FA * p + (1 - FA) * v;
			*Out[c] = p;
		}
	}

	void Reset(const IOperator::FResetParams&) { }

private:
	static float Alpha(const FTimeReadRef& T)
	{
		const float s = T->GetSeconds();
		return s > 0.f ? FMath::Exp(-1.f / s) : 0.f;
	}

	TDataReadReference<FTime>          R, F;
	TArray<TDataReadReference<float>>  In;
	TArray<TDataWriteReference<float>> Out;
	TArray<float>                      Prev;
};

/*──────────────────  AUDIO-RATE OPERATOR  ────────────────────*/
class FAudioOp : public TExecutableOperator<FAudioOp>
{
public:
	FAudioOp(const FOperatorSettings& S,
	         TArray<TDataReadReference<FAudioBuffer>> InSignals,
	         TDataReadReference<FTime>                Rise,
	         TDataReadReference<FTime>                Fall)
		: R(Rise), F(Fall), In(MoveTemp(InSignals)), SR(S.GetSampleRate())
	{
		for (int32 i = 0; i < In.Num(); ++i)
		{
			Out.Add(FAudioBufferWriteRef::CreateNew(S));
			Prev.Add(0.f);
		}
	}

	void BindInputs (FInputVertexInterfaceData&) override {}
	void BindOutputs(FOutputVertexInterfaceData&) override {}

	void Execute()
	{
		const float RA = Alpha(R);
		const float FA = Alpha(F);

		for (int32 c = 0; c < In.Num(); ++c)
		{
			const int32 N   = In[c]->Num();
			const float* Src = In[c]->GetData();
			float*       Dst = Out[c]->GetData();
			float&       p   = Prev[c];

			for (int32 i = 0; i < N; ++i)
			{
				const float v = Src[i];
				if      (v > p) p = RA * p + (1 - RA) * v;
				else if (v < p) p = FA * p + (1 - FA) * v;
				Dst[i] = p;
			}
		}
	}

	void Reset(const IOperator::FResetParams&) { }

private:
	float Alpha(const FTimeReadRef& T) const
	{
		const float s = T->GetSeconds();
		return s > 0.f ? FMath::Exp(-1.f / (s * SR)) : 0.f;
	}

	TDataReadReference<FTime>                     R, F;
	TArray<TDataReadReference<FAudioBuffer>>      In;
	TArray<FAudioBufferWriteRef>                  Out;
	TArray<float>                                 Prev;
	int32                                         SR;
};

/*──────────────────  WRAPPER OPERATOR  ───────────────────────*/
class FSlewConfigurableOperator :
	public TExecutableOperator<FSlewConfigurableOperator>
{
public:
	FSlewConfigurableOperator(TUniquePtr<IOperator> InImpl)
		: Impl(MoveTemp(InImpl)) {}

	/* static CreateOperator picks the concrete worker */
	static TUniquePtr<IOperator> CreateOperator(
		const FBuildOperatorParams& P, FBuildResults& BR)
	{
		const auto* C = CastOperatorData<const FSlewConfigurableOperatorData>(P.Node.GetOperatorData().Get());
		const int32 Num = C->NumPins;

		auto Rise = P.InputData.GetOrCreateDefaultDataReadReference<FTime>(
			METASOUND_GET_PARAM_NAME(InRise), P.OperatorSettings);
		auto Fall = P.InputData.GetOrCreateDefaultDataReadReference<FTime>(
			METASOUND_GET_PARAM_NAME(InFall), P.OperatorSettings);

		if (C->Mode == ESlewConfigurableMode::Control)
		{
			TArray<TDataReadReference<float>> InRefs;
			for (int32 i = 0; i < Num; ++i)
			{
				FName Pin(*FString::Printf(TEXT("%s %d"),
					*METASOUND_GET_PARAM_NAME(InSignal).ToString(), i));
				InRefs.Add(P.InputData.GetOrCreateDefaultDataReadReference<float>(Pin, P.OperatorSettings));
			}

			return MakeUnique<FSlewConfigurableOperator>(
				MakeUnique<FFloatOp>(P.OperatorSettings, MoveTemp(InRefs), Rise, Fall));
		}
		else
		{
			TArray<TDataReadReference<FAudioBuffer>> InRefs;
			for (int32 i = 0; i < Num; ++i)
			{
				FName Pin(*FString::Printf(TEXT("%s %d"),
					*METASOUND_GET_PARAM_NAME(InSignal).ToString(), i));
				InRefs.Add(P.InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(Pin, P.OperatorSettings));
			}

			return MakeUnique<FSlewConfigurableOperator>(
				MakeUnique<FAudioOp>(P.OperatorSettings, MoveTemp(InRefs), Rise, Fall));
		}
	}

	static const FNodeClassMetadata& GetNodeInfo()
	{
		static const FNodeClassMetadata Info = []()
		{
			FNodeClassMetadata M;
			M.ClassName        = { TEXT("Branches"), TEXT("SlewConfigurable"), TEXT("") };
			M.MajorVersion     = 1;
			M.MinorVersion     = 0;
			M.DisplayName      = LOCTEXT("CfgSlew", "Slew (Configurable)");
			M.Description      = LOCTEXT("CfgSlewDesc",
				"Smooth signal; audio- or control-rate; variable pin count.");
			M.Author           = TEXT("Charles Matthews");
			M.PromptIfMissing  = LOCTEXT("CfgSlewMissing", "Enable MetaSound Branches.");
			M.DefaultInterface = BuildInterface(1, ESlewConfigurableMode::Audio);
			M.CategoryHierarchy = {
				LOCTEXT("Branches", "Branches"),
				LOCTEXT("Filters" , "Filters")
			};
			return M;
		}();
		return Info;
	}

	/* IOperator forwards */
	void BindInputs (FInputVertexInterfaceData& Data)
	{
		Impl->BindInputs(Data);
	}
	void BindOutputs(FOutputVertexInterfaceData& Data) 
	{
		Impl->BindOutputs(Data);
	}
	void Execute() { Impl->Execute(); }
	void Reset  (const IOperator::FResetParams& Params)
	{
		Impl->Reset(Params);
	}

private:
	TUniquePtr<IOperator> Impl;
};

using FNodeAlias = TNodeFacade<FSlewConfigurableOperator>;

} // namespace SlewConfigurablePrivate
} // namespace Metasound

/*─────────  CONFIGURATION BRIDGE  ─────────*/
TInstancedStruct<FMetasoundFrontendClassInterface>
FMetaSoundSlewConfigurableNodeConfiguration::OverrideDefaultInterface(
	const FMetasoundFrontendClass&) const
{
	return TInstancedStruct<FMetasoundFrontendClassInterface>::Make(
		FMetasoundFrontendClassInterface::GenerateClassInterface(
			Metasound::SlewConfigurablePrivate::BuildInterface(NumPins, SlewMode)));
}

TSharedPtr<const Metasound::IOperatorData>
FMetaSoundSlewConfigurableNodeConfiguration::GetOperatorData() const
{
	return MakeShared<
		Metasound::SlewConfigurablePrivate::FSlewConfigurableOperatorData>(SlewMode, NumPins);
}

/*─────────  REGISTRATION  ─────────*/
METASOUND_REGISTER_NODE_AND_CONFIGURATION(
	Metasound::SlewConfigurablePrivate::FNodeAlias,
	FMetaSoundSlewConfigurableNodeConfiguration)

#undef LOCTEXT_NAMESPACE