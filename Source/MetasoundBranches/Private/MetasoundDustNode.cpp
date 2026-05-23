// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundDustNode.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundPrimitives.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/DateTime.h"
#include "MetasoundBranches/Public/MetasoundCommonMacros.h"
#include "MetasoundTrigger.h"

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_DustNode"

namespace Metasound
{
	namespace DustNodeVertexNames
	{
		METASOUND_PARAM(InputEnabled,             "Enabled",             "Enable or disable output")
		METASOUND_PARAM(InputBiPolar,             "Bi-Polar",            "Toggle between bipolar and unipolar impulse output.")
		METASOUND_PARAM(InputSeed,                "Seed",                "Seed for seeding the Random Number Generator, -1 (default) will use current time.")
		METASOUND_PARAM(InputDensityOffset,       "Density",             "Density of impulses (roughly equivalent to Hz).")
		METASOUND_PARAM(InputDensityAudio,        "Density Modulation",  "Audio modulation for density.")
		METASOUND_PARAM(InputAmpVarOffset,        "Amp Variation",       "Base amplitude variation (0=fixed at maximum amplitude, 1=fully random).")
		METASOUND_PARAM(InputAmpVarAudio,         "Amp Variation Modulation", "Audio modulation for amplitude variation.")
		METASOUND_PARAM(OutputImpulse,            "Impulse Out",         "Generated impulse.")
		METASOUND_PARAM(OutputTrigger,            "Trigger Out",         "Generated trigger.")
	}

	class FDustOperator : public TExecutableOperator<FDustOperator>
	{
	public:
		FDustOperator(
			const FOperatorSettings& InSettings,
			const FBoolReadRef& InEnabled,
			const FBoolReadRef& InBiPolar,
			const FInt32ReadRef& InSeed,
			const FFloatReadRef& InDensityOffset,
			const FAudioBufferReadRef& InDensityAudio,
			const FFloatReadRef& InAmpVarOffset,
			const FAudioBufferReadRef& InAmpVarAudio
		)
			: Enabled(InEnabled)
			, BiPolar(InBiPolar)
			, Seed(InSeed)
			, DensityOffset(InDensityOffset)
			, DensityAudio(InDensityAudio)
			, AmpVarOffset(InAmpVarOffset)
			, AmpVarAudio(InAmpVarAudio)
			, OutputImpulse(FAudioBufferWriteRef::CreateNew(InSettings))
			, OutputTrigger(FTriggerWriteRef::CreateNew(InSettings))
			, RNGStream((*InSeed == -1) ? FDateTime::UtcNow().GetTicks() : *InSeed)
			, SampleRate((float)InSettings.GetSampleRate())
			, SignalIsPositive(true)
			, LastSeed((*InSeed == -1) ? FDateTime::UtcNow().GetTicks() : *InSeed)
		{
		}

		static const FVertexInterface& DeclareVertexInterface()
		{
			using namespace DustNodeVertexNames;
			static const FVertexInterface Interface(
				FInputVertexInterface(
					TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputEnabled), true),
					TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputBiPolar), true),
					TInputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputSeed), -1),
					TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputDensityOffset), 0.f),
					TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputDensityAudio)),
					TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputAmpVarOffset), 1.f),
					TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputAmpVarAudio))
				),
				FOutputVertexInterface(
					TOutputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputImpulse)),
					TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputTrigger))
				)
			);
			return Interface;
		}

        static const FNodeClassMetadata& GetNodeInfo()
		{
			auto CreateNodeClassMetadata = []() -> FNodeClassMetadata
			{
				FNodeClassMetadata Metadata;
				Metadata.ClassName = { TEXT("UE"), TEXT("Dust (Audio)"), TEXT("Audio") };
				Metadata.MajorVersion = 1;
				Metadata.MinorVersion = 1;
				Metadata.DisplayName = LOCTEXT("DustNodeDisplayName", "Dust (Audio)");
				Metadata.Description = LOCTEXT("DustNodeDesc", "Generates randomly timed impulses (uni or bi-polar) alongside triggers, with optional audio rate modulation.");
				Metadata.Author = TEXT("Charles Matthews");
				Metadata.PromptIfMissing = PluginNodeMissingPrompt;
                Metadata.CategoryHierarchy = {
                    METASOUND_LOCTEXT("Custom", "Branches"),
                    METASOUND_LOCTEXT("CustomSub", "Generators")
                };
				Metadata.DefaultInterface = DeclareVertexInterface();
				return Metadata;
			};
			static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
			return Metadata;
		}

		METASOUND_DISABLE_LEGACY_IO()

		static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
		{
			using namespace DustNodeVertexNames;
			const FInputVertexInterfaceData& InputData = InParams.InputData;
			TDataReadReference<bool> InEnabled =
				InputData.GetOrCreateDefaultDataReadReference<bool>(METASOUND_GET_PARAM_NAME(InputEnabled), InParams.OperatorSettings);
			TDataReadReference<bool> InBiPolar =
				InputData.GetOrCreateDefaultDataReadReference<bool>(METASOUND_GET_PARAM_NAME(InputBiPolar), InParams.OperatorSettings);
			TDataReadReference<int32> InSeed =
				InputData.GetOrCreateDefaultDataReadReference<int32>(METASOUND_GET_PARAM_NAME(InputSeed), InParams.OperatorSettings);
			TDataReadReference<float> InDensityOffset =
				InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputDensityOffset), InParams.OperatorSettings);
			TDataReadReference<FAudioBuffer> InDensityAudio =
				InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(InputDensityAudio), InParams.OperatorSettings);
			TDataReadReference<float> InAmpVarOffset =
				InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputAmpVarOffset), InParams.OperatorSettings);
			TDataReadReference<FAudioBuffer> InAmpVarAudio =
				InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(InputAmpVarAudio), InParams.OperatorSettings);
			return MakeUnique<FDustOperator>(
				InParams.OperatorSettings,
				InEnabled,
				InBiPolar,
				InSeed,
				InDensityOffset,
				InDensityAudio,
				InAmpVarOffset,
				InAmpVarAudio
			);
		}

		void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
		{
			using namespace DustNodeVertexNames;
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputEnabled), Enabled);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputBiPolar), BiPolar);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputSeed), Seed);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputDensityOffset), DensityOffset);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputDensityAudio), DensityAudio);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputAmpVarOffset), AmpVarOffset);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputAmpVarAudio), AmpVarAudio);
		}

		void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
		{
			using namespace DustNodeVertexNames;
			InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputTrigger), OutputTrigger);
			InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputImpulse), OutputImpulse);
		}

        void Execute()
        {
            OutputTrigger->AdvanceBlock();
            float* AudioOut = OutputImpulse->GetData();
            const float* DensityIn = DensityAudio->GetData();
            const float* AmpVarIn = AmpVarAudio->GetData();
            int32 NumFrames = DensityAudio->Num();
        
            int32 CurrentSeed = *Seed;
            int32 NewSeed = (CurrentSeed == -1) ? FDateTime::UtcNow().GetTicks() : CurrentSeed;
            if (NewSeed != LastSeed)
            {
                RNGStream.Initialize(NewSeed);
                LastSeed = NewSeed;
            }
        
            if (!*Enabled)
            {
                FMemory::Memset(AudioOut, 0, sizeof(float) * NumFrames);
                return;
            }
        
            for (int32 i = 0; i < NumFrames; ++i)
            {
                float d = (*DensityOffset) + DensityIn[i];
                float p = FMath::Clamp(FMath::Max(0.f, d) / SampleRate, 0.f, 1.f);
                float r = RNGStream.GetFraction();
        
                if (r < p)
                {
                    float mod = FMath::Clamp((*AmpVarOffset) + AmpVarIn[i], 0.f, 1.f);
                    float randAmp = RNGStream.GetFraction();
                    float a = FMath::Lerp(1.f, randAmp, mod);
        
                    if (*BiPolar)
                    {
                        a = SignalIsPositive ? a : -a;
                        SignalIsPositive = !SignalIsPositive;
                    }
        
                    AudioOut[i] = a;
                    OutputTrigger->TriggerFrame(i);
                }
                else
                {
                    AudioOut[i] = 0.f;
                }
            }
        }

	private:
		FBoolReadRef Enabled;
		FBoolReadRef BiPolar;
		FInt32ReadRef Seed;
		FFloatReadRef DensityOffset;
		FAudioBufferReadRef DensityAudio;
		FFloatReadRef AmpVarOffset;
		FAudioBufferReadRef AmpVarAudio;
		FAudioBufferWriteRef OutputImpulse;
		FTriggerWriteRef OutputTrigger;
		FRandomStream RNGStream;
		float SampleRate;
		bool SignalIsPositive;
		int32 LastSeed;
	};

	class FDustNode : public FNodeFacade
	{
	public:
				static FNodeClassMetadata CreateNodeClassMetadata()
		{
		    return FDustOperator::GetNodeInfo();
		}
		FDustNode(FNodeData InitData)
			: FNodeFacade(InitData, MakeShared<const FNodeClassMetadata>(FDustOperator::GetNodeInfo()), TFacadeOperatorClass<FDustOperator>())
		{
		}
	};

	METASOUND_REGISTER_NODE(FDustNode);
}

#undef LOCTEXT_NAMESPACE