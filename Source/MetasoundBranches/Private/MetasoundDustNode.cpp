// Copyright 2026 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundDustNode.h"
#include "MetasoundAudioBuffer.h"
#include "MetasoundDataFactory.h"
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

	namespace DustPrivate
	{
		FName MakeImpulseOutputName(int32 ChannelIndex)
		{
			if (ChannelIndex == 0)
			{
				return METASOUND_GET_PARAM_NAME(DustNodeVertexNames::OutputImpulse);
			}
			return FName(*FString::Printf(TEXT("Impulse Out %d"), ChannelIndex));
		}

		FName MakeTriggerOutputName(int32 ChannelIndex)
		{
			if (ChannelIndex == 0)
			{
				return METASOUND_GET_PARAM_NAME(DustNodeVertexNames::OutputTrigger);
			}
			return FName(*FString::Printf(TEXT("Trigger Out %d"), ChannelIndex));
		}

		FDataVertexMetadata MakeImpulseOutputMeta(int32 ChannelIndex)
		{
#if WITH_EDITOR
			if (ChannelIndex == 0)
			{
				return { LOCTEXT("DustImpulseOutTooltip", "Generated impulse."), LOCTEXT("DustImpulseOutDisplay", "Impulse Out") };
			}
			const int32 Num = ChannelIndex;
			const FText DisplayName = FText::Format(LOCTEXT("DustImpulseChannelDisplayFmt", "Impulse Out {0}"), Num);
			const FText Tooltip = FText::Format(LOCTEXT("DustImpulseChannelTooltipFmt", "Generated impulse output for channel {0}."), Num);
			return { Tooltip, DisplayName };
#else
			return {};
#endif
		}

		FDataVertexMetadata MakeTriggerOutputMeta(int32 ChannelIndex)
		{
#if WITH_EDITOR
			if (ChannelIndex == 0)
			{
				return { LOCTEXT("DustTriggerOutTooltip", "Generated trigger."), LOCTEXT("DustTriggerOutDisplay", "Trigger Out") };
			}
			const int32 Num = ChannelIndex;
			const FText DisplayName = FText::Format(LOCTEXT("DustTriggerChannelDisplayFmt", "Trigger Out {0}"), Num);
			const FText Tooltip = FText::Format(LOCTEXT("DustTriggerChannelTooltipFmt", "Generated trigger output for channel {0}."), Num);
			return { Tooltip, DisplayName };
#else
			return {};
#endif
		}

		FVertexInterface GetVertexInterface(int32 NumChannels, bool bPerChannelTriggerPins, bool bPerChannelAudioPins)
		{
			using namespace DustNodeVertexNames;

			const int32 ClampedNumChannels = FMath::Clamp(NumChannels, 1, 32);

			FInputVertexInterface Input(
				TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputEnabled), true),
				TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputBiPolar), true),
				TInputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputSeed), -1),
				TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputDensityOffset), 0.f),
				TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputDensityAudio)),
				TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputAmpVarOffset), 1.f),
				TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputAmpVarAudio))
			);

			FOutputVertexInterface Output;

			if (bPerChannelAudioPins)
			{
				for (int32 ChannelIndex = 0; ChannelIndex < ClampedNumChannels; ++ChannelIndex)
				{
					Output.Add(TOutputDataVertex<FAudioBuffer>(MakeImpulseOutputName(ChannelIndex), MakeImpulseOutputMeta(ChannelIndex)));
				}
			}
			else
			{
				Output.Add(TOutputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputImpulse)));
			}

			if (bPerChannelTriggerPins)
			{
				for (int32 ChannelIndex = 0; ChannelIndex < ClampedNumChannels; ++ChannelIndex)
				{
					Output.Add(TOutputDataVertex<FTrigger>(MakeTriggerOutputName(ChannelIndex), MakeTriggerOutputMeta(ChannelIndex)));
				}
			}
			else
			{
				Output.Add(TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputTrigger)));
			}

			return { MoveTemp(Input), MoveTemp(Output) };
		}

		class FDustOperatorData : public TOperatorData<FDustOperatorData>
		{
		public:
			static const FLazyName OperatorDataTypeName;

			FDustOperatorData(int32 InNumChannels, bool bInCreateTriggerPinsPerChannel, bool bInCreateAudioPinsPerChannel)
				: NumChannels(InNumChannels)
				, bCreateTriggerPinsPerChannel(bInCreateTriggerPinsPerChannel)
				, bCreateAudioPinsPerChannel(bInCreateAudioPinsPerChannel)
			{
			}

			int32 NumChannels;
			bool bCreateTriggerPinsPerChannel;
			bool bCreateAudioPinsPerChannel;
		};

		const FLazyName FDustOperatorData::OperatorDataTypeName = "DustOperatorData";
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
			const FAudioBufferReadRef& InAmpVarAudio,
			TArray<FAudioBufferWriteRef> InOutputImpulses,
			TArray<FTriggerWriteRef> InOutputTriggers,
			TArray<FName> InOutputImpulseNames,
			TArray<FName> InOutputTriggerNames,
			int32 InNumChannels
		)
			: Enabled(InEnabled)
			, BiPolar(InBiPolar)
			, Seed(InSeed)
			, DensityOffset(InDensityOffset)
			, DensityAudio(InDensityAudio)
			, AmpVarOffset(InAmpVarOffset)
			, AmpVarAudio(InAmpVarAudio)
			, OutputImpulses(MoveTemp(InOutputImpulses))
			, OutputTriggers(MoveTemp(InOutputTriggers))
			, OutputImpulseNames(MoveTemp(InOutputImpulseNames))
			, OutputTriggerNames(MoveTemp(InOutputTriggerNames))
			, RNGStream((*InSeed == -1) ? FDateTime::UtcNow().GetTicks() : *InSeed)
			, SampleRate((float)InSettings.GetSampleRate())
			, SignalIsPositive(true)
			, LastSeed((*InSeed == -1) ? FDateTime::UtcNow().GetTicks() : *InSeed)
			, NumChannels(FMath::Max(1, InNumChannels))
			, NextChannelIndex(0)
		{
		}

		static const FVertexInterface& DeclareVertexInterface()
		{
			static const FVertexInterface Interface = DustPrivate::GetVertexInterface(1, false, false);
			return Interface;
		}

        static const FNodeClassMetadata& GetNodeInfo()
		{
			auto CreateNodeClassMetadata = []() -> FNodeClassMetadata
			{
				FNodeClassMetadata Metadata;
				Metadata.ClassName = { TEXT("UE"), TEXT("Dust (Audio)"), TEXT("Audio") };
				Metadata.MajorVersion = 1;
				Metadata.MinorVersion = 2;
				Metadata.DisplayName = LOCTEXT("DustNodeDisplayName", "Dust (Audio)");
				Metadata.Description = LOCTEXT("DustNodeDesc", "Generates randomly timed impulses (uni or bi-polar) alongside triggers, with optional audio rate modulation.");
				Metadata.Author = TEXT("Charles Matthews");
				Metadata.PromptIfMissing = PluginNodeMissingPrompt;
                Metadata.CategoryHierarchy = {
                    METASOUND_LOCTEXT("Custom", "Branches"),
                    METASOUND_LOCTEXT("CustomSub", "Generators")
                };
				Metadata.DefaultInterface = DeclareVertexInterface();
				METASOUND_BRANCHES_APPLY_NODE_STYLE(Metadata);
                return Metadata;
			};
			static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
                return Metadata;
		}

		METASOUND_DISABLE_LEGACY_IO()

		static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
		{
			using namespace DustNodeVertexNames;
			using namespace DustPrivate;
			const FInputVertexInterfaceData& InputData = InParams.InputData;
			const FVertexInterface& Interface = InParams.Node.GetVertexInterface();

			const FDustOperatorData* ConfigData = CastOperatorData<const FDustOperatorData>(InParams.Node.GetOperatorData().Get());
			const int32 ConfigNumChannels = ConfigData ? FMath::Clamp(ConfigData->NumChannels, 1, 32) : 1;
			const bool bCreateTriggerPinsPerChannel = ConfigData ? ConfigData->bCreateTriggerPinsPerChannel : false;
			const bool bCreateAudioPinsPerChannel = ConfigData ? ConfigData->bCreateAudioPinsPerChannel : false;

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

			TArray<FAudioBufferWriteRef> OutputImpulses;
			TArray<FTriggerWriteRef> OutputTriggers;
			TArray<FName> OutputImpulseNames;
			TArray<FName> OutputTriggerNames;

			if (bCreateAudioPinsPerChannel)
			{
				for (int32 ChannelIndex = 0; ChannelIndex < ConfigNumChannels; ++ChannelIndex)
				{
					const FName OutputName = MakeImpulseOutputName(ChannelIndex);
					if (Interface.ContainsOutputVertex(OutputName))
					{
						OutputImpulses.Add(FAudioBufferWriteRef::CreateNew(InParams.OperatorSettings));
						OutputImpulseNames.Add(OutputName);
					}
				}
			}
			if (OutputImpulses.Num() == 0)
			{
				OutputImpulses.Add(FAudioBufferWriteRef::CreateNew(InParams.OperatorSettings));
				OutputImpulseNames.Add(METASOUND_GET_PARAM_NAME(OutputImpulse));
			}

			if (bCreateTriggerPinsPerChannel)
			{
				for (int32 ChannelIndex = 0; ChannelIndex < ConfigNumChannels; ++ChannelIndex)
				{
					const FName OutputName = MakeTriggerOutputName(ChannelIndex);
					if (Interface.ContainsOutputVertex(OutputName))
					{
						OutputTriggers.Add(FTriggerWriteRef::CreateNew(InParams.OperatorSettings));
						OutputTriggerNames.Add(OutputName);
					}
				}
			}
			if (OutputTriggers.Num() == 0)
			{
				OutputTriggers.Add(FTriggerWriteRef::CreateNew(InParams.OperatorSettings));
				OutputTriggerNames.Add(METASOUND_GET_PARAM_NAME(OutputTrigger));
			}

			return MakeUnique<FDustOperator>(
				InParams.OperatorSettings,
				InEnabled,
				InBiPolar,
				InSeed,
				InDensityOffset,
				InDensityAudio,
				InAmpVarOffset,
				InAmpVarAudio,
				MoveTemp(OutputImpulses),
				MoveTemp(OutputTriggers),
				MoveTemp(OutputImpulseNames),
				MoveTemp(OutputTriggerNames),
				ConfigNumChannels
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
			for (int32 Index = 0; Index < OutputTriggers.Num(); ++Index)
			{
				InOutVertexData.BindWriteVertex(OutputTriggerNames[Index], OutputTriggers[Index]);
			}
			for (int32 Index = 0; Index < OutputImpulses.Num(); ++Index)
			{
				InOutVertexData.BindWriteVertex(OutputImpulseNames[Index], OutputImpulses[Index]);
			}
		}

        void Execute()
        {
			for (FTriggerWriteRef& OutputTrigger : OutputTriggers)
			{
				OutputTrigger->AdvanceBlock();
			}

			TArray<float*> AudioOutChannels;
			AudioOutChannels.Reserve(OutputImpulses.Num());
			for (FAudioBufferWriteRef& OutputImpulse : OutputImpulses)
			{
				float* AudioData = OutputImpulse->GetData();
				FMemory::Memset(AudioData, 0, sizeof(float) * OutputImpulse->Num());
				AudioOutChannels.Add(AudioData);
			}

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

					const int32 SelectedChannel = (NumChannels > 1) ? NextChannelIndex : 0;
					if (NumChannels > 1)
					{
						NextChannelIndex = (NextChannelIndex + 1) % NumChannels;
					}

					const int32 AudioOutputIndex = (AudioOutChannels.Num() > 1) ? FMath::Clamp(SelectedChannel, 0, AudioOutChannels.Num() - 1) : 0;
					const int32 TriggerOutputIndex = (OutputTriggers.Num() > 1) ? FMath::Clamp(SelectedChannel, 0, OutputTriggers.Num() - 1) : 0;

					AudioOutChannels[AudioOutputIndex][i] = a;
					OutputTriggers[TriggerOutputIndex]->TriggerFrame(i);
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
		TArray<FAudioBufferWriteRef> OutputImpulses;
		TArray<FTriggerWriteRef> OutputTriggers;
		TArray<FName> OutputImpulseNames;
		TArray<FName> OutputTriggerNames;
		FRandomStream RNGStream;
		float SampleRate;
		bool SignalIsPositive;
		int32 LastSeed;
		int32 NumChannels;
		int32 NextChannelIndex;
	};

	using FDustNode = TNodeFacade<FDustOperator>;
	METASOUND_REGISTER_NODE_AND_CONFIGURATION(FDustNode, FMetaSoundDustNodeConfiguration);
}

FMetaSoundDustNodeConfiguration::FMetaSoundDustNodeConfiguration()
	: NumChannels(1)
	, bCreateTriggerPinsPerChannel(false)
	, bCreateAudioPinsPerChannel(false)
{
}

TInstancedStruct<FMetasoundFrontendClassInterface>
FMetaSoundDustNodeConfiguration::OverrideDefaultInterface(const FMetasoundFrontendClass&) const
{
	using namespace Metasound::DustPrivate;
	return TInstancedStruct<FMetasoundFrontendClassInterface>::Make(
		FMetasoundFrontendClassInterface::GenerateClassInterface(
			GetVertexInterface(NumChannels, bCreateTriggerPinsPerChannel, bCreateAudioPinsPerChannel)));
}

TSharedPtr<const Metasound::IOperatorData>
FMetaSoundDustNodeConfiguration::GetOperatorData() const
{
	using namespace Metasound::DustPrivate;
	return MakeShared<FDustOperatorData>(NumChannels, bCreateTriggerPinsPerChannel, bCreateAudioPinsPerChannel);
}

#undef LOCTEXT_NAMESPACE