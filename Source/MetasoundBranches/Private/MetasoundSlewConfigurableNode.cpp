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
/*──────────────────  PARAM & Helpers  ──────────────────*/
	METASOUND_PARAM(InSignal , "In"  , "Signal to smooth");
	METASOUND_PARAM(InRise   , "Rise Time" , "Rise time (s)");
	METASOUND_PARAM(InFall   , "Fall Time" , "Fall time (s)");
	METASOUND_PARAM(OutSignal, "Out" , "Slewed signal");

	static FVertexInterface BuildInterface(int32 NumPins, ESlewConfigurableMode Mode)
	{
		FInputVertexInterface  Ins;
		FOutputVertexInterface Outs;

		for (int32 i = 0; i < NumPins; ++i)
		{
			const FString Sfx = FString::Printf(TEXT(" %d"), i);
			const FName   InN  = * (METASOUND_GET_PARAM_NAME(InSignal ).ToString()  + Sfx);
			const FName   OutN = * (METASOUND_GET_PARAM_NAME(OutSignal).ToString() + Sfx);

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

		return FVertexInterface(MoveTemp(Ins), MoveTemp(Outs));
	}

/*──────────────────  Operator-data  ──────────────────*/
	struct FSlewConfigurableOperatorData : public TOperatorData<FSlewConfigurableOperatorData>
	{
		static const FLazyName OperatorDataTypeName;
		ESlewConfigurableMode Mode;
		int32                 NumPins;
		FSlewConfigurableOperatorData(ESlewConfigurableMode InMode, int32 InN)
			: Mode(InMode), NumPins(InN) {}
	};
	const FLazyName FSlewConfigurableOperatorData::OperatorDataTypeName =
		"SlewConfigurableOperatorData";

/*──────────────────  Float operator  ──────────────────*/
	class FFloatOp : public TExecutableOperator<FFloatOp>
	{
	public:
		FFloatOp(const FOperatorSettings& S,
		         TArray<TDataReadReference<float>>  InSig,
		         TDataReadReference<FTime>          Rise,
		         TDataReadReference<FTime>          Fall)
			: R(Rise), F(Fall), In(MoveTemp(InSig))
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

/*──────────────────  Audio operator  ─────────────────*/
	class FAudioOp : public TExecutableOperator<FAudioOp>
	{
	public:
		FAudioOp(const FOperatorSettings& S,
		         TArray<TDataReadReference<FAudioBuffer>> InSig,
		         TDataReadReference<FTime>                Rise,
		         TDataReadReference<FTime>                Fall)
			: R(Rise), F(Fall), In(MoveTemp(InSig)), SR(S.GetSampleRate())
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
				const int32 N = In[c]->Num();
				const float* src = In[c]->GetData();
				float*       dst = Out[c]->GetData();
				float&       p   = Prev[c];

				for (int32 i = 0; i < N; ++i)
				{
					const float v = src[i];
					if      (v > p) p = RA * p + (1 - RA) * v;
					else if (v < p) p = FA * p + (1 - FA) * v;
					dst[i] = p;
				}
			}
		}

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

/*──────────────────  Factory  ───────────────────────*/
	class FSelector
	{
	public:
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
					"Smooth signal; audio or control rate; variable pin count.");
				M.Author           = TEXT("Charles Matthews");
				M.PromptIfMissing  = LOCTEXT("CfgSlewMissing", "Enable MetaSound Branches.");
				M.DefaultInterface = BuildInterface(1, ESlewConfigurableMode::Audio);
				return M;
                
                			M.CategoryHierarchy =
			{
				LOCTEXT("Branches", "Branches"),
				LOCTEXT("Filters" , "Filters")
			};
			}();
			return Info;
		}

		static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& P, FBuildResults&)
		{
			const auto* C = CastOperatorData<const FSlewConfigurableOperatorData>(P.Node.GetOperatorData().Get());
			const int32 N = C->NumPins;

			auto R = P.InputData.GetOrCreateDefaultDataReadReference<FTime>(
				METASOUND_GET_PARAM_NAME(InRise), P.OperatorSettings);
			auto F = P.InputData.GetOrCreateDefaultDataReadReference<FTime>(
				METASOUND_GET_PARAM_NAME(InFall), P.OperatorSettings);

			if (C->Mode == ESlewConfigurableMode::Control)
			{
				TArray<TDataReadReference<float>> InSig;
				for (int32 i = 0; i < N; ++i)
					InSig.Add(P.InputData.GetOrCreateDefaultDataReadReference<float>(
						FName(*FString::Printf(TEXT("%s %d"),
							*METASOUND_GET_PARAM_NAME(InSignal).ToString(), i)), P.OperatorSettings));

				return MakeUnique<FFloatOp>(P.OperatorSettings, MoveTemp(InSig), R, F);
			}
			else
			{
				TArray<TDataReadReference<FAudioBuffer>> InSig;
				for (int32 i = 0; i < N; ++i)
					InSig.Add(P.InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(
						FName(*FString::Printf(TEXT("%s %d"),
							*METASOUND_GET_PARAM_NAME(InSignal).ToString(), i)), P.OperatorSettings));

				return MakeUnique<FAudioOp>(P.OperatorSettings, MoveTemp(InSig), R, F);
			}
		}
	};

/*──────────────────  Facade & alias ─────────────────*/
	class FSlewConfigurableNode : public FNodeFacade
	{
	public:
		FSlewConfigurableNode(const FNodeInitData& Init)
			: FNodeFacade(Init, MakeShared<FSelector>()) {}
	};

	using FNodeAlias = FSlewConfigurableNode;

} // namespace SlewConfigurablePrivate
} // namespace Metasound

/*────────── Global USTRUCT bridge ─────────*/
TInstancedStruct<FMetasoundFrontendClassInterface>
FMetaSoundSlewConfigurableNodeConfiguration::OverrideDefaultInterface(const FMetasoundFrontendClass&) const
{
	return TInstancedStruct<FMetasoundFrontendClassInterface>::Make(
		FMetasoundFrontendClassInterface::GenerateClassInterface(
			Metasound::SlewConfigurablePrivate::BuildInterface(NumPins, SlewMode)));
}

TSharedPtr<const Metasound::IOperatorData>
FMetaSoundSlewConfigurableNodeConfiguration::GetOperatorData() const
{
	return MakeShared<Metasound::SlewConfigurablePrivate::FSlewConfigurableOperatorData>(SlewMode, NumPins);
}

/*────────── Registration ─────────*/
METASOUND_REGISTER_NODE_AND_CONFIGURATION(
	Metasound::SlewConfigurablePrivate::FNodeAlias,
	FMetaSoundSlewConfigurableNodeConfiguration)

#undef LOCTEXT_NAMESPACE