// Copyright 2026 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundCATDustNode.h"

#include "DSP/BufferVectorOperations.h"
#include "DSP/Vbap.h"
#include "MetasoundAudioBuffer.h"
#include "MetasoundBranches/Public/MetasoundCommonMacros.h"
#include "MetasoundChannelAgnosticType.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundParamHelper.h"
#include "MetasoundPrimitives.h"
#include "MetasoundTrigger.h"
#include "MetasoundVertex.h"
#include "Misc/DateTime.h"
#include "TypeFamily/ChannelTypeFamily.h"

#define LOCTEXT_NAMESPACE "MetasoundBranches_CATDustNode"

namespace Metasound
{
	namespace CATDustVertexNames
	{
		METASOUND_PARAM(InputEnabled, "Enabled", "Enable or disable output")
		METASOUND_PARAM(InputBiPolar, "Bi-Polar", "Toggle between bipolar and unipolar impulse output.")
		METASOUND_PARAM(InputSeed, "Seed", "Seed for seeding the Random Number Generator, -1 (default) will use current time.")
		METASOUND_PARAM(InputDensityOffset, "Density", "Density of impulses (roughly equivalent to Hz).")
		METASOUND_PARAM(InputDensityAudio, "Density Modulation", "Audio modulation for density.")
		METASOUND_PARAM(InputAmpVarOffset, "Amp Variation", "Base amplitude variation (0=fixed at maximum amplitude, 1=fully random).")
		METASOUND_PARAM(InputAmpVarAudio, "Amp Variation Modulation", "Audio modulation for amplitude variation.")
		METASOUND_PARAM(InputPan, "Pan", "Azimuthal pan of new impulses in range [0.0, 2.0). 0.0 is left, 0.5 is center, 1.0 is right, 1.5 is behind.")
		METASOUND_PARAM(OutputImpulse, "Output", "Generated CAT impulse.")
		METASOUND_PARAM(OutputTrigger, "Trigger Out", "Generated trigger.")
	}

	namespace CATDustPrivate
	{
		class FCATDustOperatorData final : public TOperatorData<FCATDustOperatorData>
		{
		public:
			static const FLazyName OperatorDataTypeName;

			explicit FCATDustOperatorData(const FName& InOutputAudioTypeName, const bool bInGenerateImpulsePerChannel)
				: OutputAudioTypeName(InOutputAudioTypeName)
				, bGenerateImpulsePerChannel(bInGenerateImpulsePerChannel)
			{
			}

			FName OutputAudioTypeName;
			bool bGenerateImpulsePerChannel;
		};

		const FLazyName FCATDustOperatorData::OperatorDataTypeName = TEXT("FCATDustOperatorData");

		// Map normalized CAT azimuth [0,2) to degrees [-180,180).
		float NormalizedAzimuthToDegrees(const float InNormalizedAzimuth)
		{
			float WrappedAzimuth = FMath::Fmod(InNormalizedAzimuth, 2.0f);
			if (WrappedAzimuth < 0.0f)
			{
				WrappedAzimuth += 2.0f;
			}

			float Degrees = (WrappedAzimuth - 0.5f) * 180.0f;
			Degrees = FMath::Fmod(Degrees + 180.0f, 360.0f);
			if (Degrees < 0.0f)
			{
				Degrees += 360.0f;
			}
			Degrees -= 180.0f;

			return Degrees;
		}

		static const Audio::FDiscreteChannelTypeFamily& GetDiscreteFamilyChecked(const Audio::FChannelTypeFamily& FamilyType)
		{
			check(FamilyType.GetFamilyName() == Audio::FDiscreteChannelTypeFamily::GetFamilyTypeName());
			return static_cast<const Audio::FDiscreteChannelTypeFamily&>(FamilyType);
		}

		FVertexInterface GetVertexInterface(const FName& InOutputFormat, const bool bGenerateImpulsePerChannel)
		{
			using namespace CATDustVertexNames;

			FInputVertexInterface Input(
				TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputEnabled), true),
				TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputBiPolar), true),
				TInputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputSeed), -1),
				TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputDensityOffset), 0.f),
				TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputDensityAudio)),
				TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputAmpVarOffset), 1.f),
				TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputAmpVarAudio))
			);

			if (!bGenerateImpulsePerChannel)
			{
				Input.Add(TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputPan), 0.5f));
			}

			FOutputVertexInterface Output;
			Output.Add(FOutputDataVertex(METASOUND_GET_PARAM_NAME(OutputImpulse), InOutputFormat, METASOUND_GET_PARAM_METADATA(OutputImpulse), EVertexAccessType::Reference));
			Output.Add(TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputTrigger)));

			return { MoveTemp(Input), MoveTemp(Output) };
		}
	}

	class FCATDustOperator : public TExecutableOperator<FCATDustOperator>
	{
	public:
		using FDiscreteChannelTypeFamily = Audio::FDiscreteChannelTypeFamily;
		using FCATDustOperatorData = CATDustPrivate::FCATDustOperatorData;

		FCATDustOperator(
			const FBuildOperatorParams& InParams,
			const TSharedPtr<const FCATDustOperatorData>& InOperatorData,
			const FBoolReadRef& InEnabled,
			const FBoolReadRef& InBiPolar,
			const FInt32ReadRef& InSeed,
			const FFloatReadRef& InDensityOffset,
			const FAudioBufferReadRef& InDensityAudio,
			const FFloatReadRef& InAmpVarOffset,
			const FAudioBufferReadRef& InAmpVarAudio,
			const FFloatReadRef& InPan,
			FDiscreteChannelAgnosticTypeWriteRef&& InOutputImpulse,
			FTriggerWriteRef&& InOutputTrigger)
			: OperatorData(InOperatorData)
			, Enabled(InEnabled)
			, BiPolar(InBiPolar)
			, Seed(InSeed)
			, DensityOffset(InDensityOffset)
			, DensityAudio(InDensityAudio)
			, AmpVarOffset(InAmpVarOffset)
			, AmpVarAudio(InAmpVarAudio)
			, Pan(InPan)
			, OutputImpulse(MoveTemp(InOutputImpulse))
			, OutputTrigger(MoveTemp(InOutputTrigger))
			, RNGStream((*InSeed == -1) ? FDateTime::UtcNow().GetTicks() : *InSeed)
			, SampleRate((float)InParams.OperatorSettings.GetSampleRate())
			, SignalIsPositive(true)
			, LastSeed((*InSeed == -1) ? FDateTime::UtcNow().GetTicks() : *InSeed)
			, DiscreteFamily(CATDustPrivate::GetDiscreteFamilyChecked(OutputImpulse->GetType()))
			, Panner(OutputImpulse->GetType().GetPanner())
		{
		}

		static const FVertexInterface& DeclareVertexInterface()
		{
			static const FVertexInterface Interface = CATDustPrivate::GetVertexInterface(TEXT("Cat:Stereo2Dot0"), false);
			return Interface;
		}

		static FNodeClassMetadata GetNodeInfo()
		{
			FNodeClassMetadata Metadata;
			Metadata.ClassName = { TEXT("Experimental"), TEXT("CATDust"), TEXT("Audio") };
			Metadata.MajorVersion = 1;
			Metadata.MinorVersion = 0;
			Metadata.DisplayName = LOCTEXT("CATDustNodeDisplayName", "CAT Dust");
			Metadata.Description = LOCTEXT("CATDustNodeDesc", "Generates randomly timed CAT impulses, either alternating across output channels or panned azimuthally.");
			Metadata.Author = TEXT("Charles Matthews");
			Metadata.PromptIfMissing = LOCTEXT("CATDustNodePromptIfMissing", "Enable MetaSoundExperimental for CAT channel format schemas.");
			Metadata.CategoryHierarchy = {
				METASOUND_LOCTEXT("Custom", "Branches"),
				METASOUND_LOCTEXT("CustomSub", "CAT")
			};
			Metadata.DefaultInterface = DeclareVertexInterface();
			METASOUND_BRANCHES_APPLY_NODE_STYLE(Metadata);
			return Metadata;
		}

		METASOUND_DISABLE_LEGACY_IO()

		static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
		{
			using namespace CATDustVertexNames;

			const FCATDustOperatorData* ConfigData = CastOperatorData<const FCATDustOperatorData>(InParams.Node.GetOperatorData().Get());
			if (!ConfigData)
			{
				return MakeUnique<FNoOpOperator>();
			}

			const Audio::FChannelTypeFamily* ConcreteOutputType = Audio::GetChannelRegistry().FindConcreteChannel(ConfigData->OutputAudioTypeName);
			if (!ConcreteOutputType)
			{
				return MakeUnique<FNoOpOperator>();
			}

			const TSharedPtr<const FCATDustOperatorData>& OperatorDataSharedPtr = StaticCastSharedPtr<const FCATDustOperatorData>(InParams.Node.GetOperatorData());

			TDataReadReference<bool> InEnabled = InParams.InputData.GetOrCreateDefaultDataReadReference<bool>(METASOUND_GET_PARAM_NAME(InputEnabled), InParams.OperatorSettings);
			TDataReadReference<bool> InBiPolar = InParams.InputData.GetOrCreateDefaultDataReadReference<bool>(METASOUND_GET_PARAM_NAME(InputBiPolar), InParams.OperatorSettings);
			TDataReadReference<int32> InSeed = InParams.InputData.GetOrCreateDefaultDataReadReference<int32>(METASOUND_GET_PARAM_NAME(InputSeed), InParams.OperatorSettings);
			TDataReadReference<float> InDensityOffset = InParams.InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputDensityOffset), InParams.OperatorSettings);
			TDataReadReference<FAudioBuffer> InDensityAudio = InParams.InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(InputDensityAudio), InParams.OperatorSettings);
			TDataReadReference<float> InAmpVarOffset = InParams.InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputAmpVarOffset), InParams.OperatorSettings);
			TDataReadReference<FAudioBuffer> InAmpVarAudio = InParams.InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(InputAmpVarAudio), InParams.OperatorSettings);

			TDataReadReference<float> InPan =
				ConfigData->bGenerateImpulsePerChannel
				? TDataReadReference<float>::CreateNew(0.5f)
				: InParams.InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputPan), InParams.OperatorSettings);

			TDataWriteReference<FDiscreteChannelAgnosticType> OutputCAT =
				FDiscreteChannelAgnosticTypeWriteRef::CreateNewDerivedAs<FDiscreteChannelAgnosticType>(
					ConcreteOutputType->GetName(),
					Metasound::GetMetasoundDataTypeId<FDiscreteChannelAgnosticType>(),
					InParams.OperatorSettings,
					ConcreteOutputType->GetName());

			FTriggerWriteRef OutputTrigger = FTriggerWriteRef::CreateNew(InParams.OperatorSettings);

			return MakeUnique<FCATDustOperator>(
				InParams,
				OperatorDataSharedPtr,
				InEnabled,
				InBiPolar,
				InSeed,
				InDensityOffset,
				InDensityAudio,
				InAmpVarOffset,
				InAmpVarAudio,
				InPan,
				MoveTemp(OutputCAT),
				MoveTemp(OutputTrigger));
		}

		void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
		{
			using namespace CATDustVertexNames;

			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputEnabled), Enabled);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputBiPolar), BiPolar);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputSeed), Seed);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputDensityOffset), DensityOffset);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputDensityAudio), DensityAudio);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputAmpVarOffset), AmpVarOffset);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputAmpVarAudio), AmpVarAudio);

			if (!OperatorData->bGenerateImpulsePerChannel)
			{
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputPan), Pan);
			}
		}

		void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
		{
			using namespace CATDustVertexNames;
			InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputImpulse), OutputImpulse);
			InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputTrigger), OutputTrigger);
		}

		void Execute()
		{
			OutputTrigger->AdvanceBlock();
			OutputImpulse->Zero();

			const float* DensityIn = DensityAudio->GetData();
			const float* AmpVarIn = AmpVarAudio->GetData();
			const int32 NumFrames = DensityAudio->Num();

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

			for (int32 FrameIndex = 0; FrameIndex < NumFrames; ++FrameIndex)
			{
				const float Density = (*DensityOffset) + DensityIn[FrameIndex];
				const float Probability = FMath::Clamp(FMath::Max(0.f, Density) / SampleRate, 0.f, 1.f);
				const float RandomValue = RNGStream.GetFraction();

				if (RandomValue >= Probability)
				{
					continue;
				}

				float AmplitudeMod = FMath::Clamp((*AmpVarOffset) + AmpVarIn[FrameIndex], 0.f, 1.f);
				const float RandomAmplitude = RNGStream.GetFraction();
				float Amplitude = FMath::Lerp(1.f, RandomAmplitude, AmplitudeMod);

				if (*BiPolar)
				{
					Amplitude = SignalIsPositive ? Amplitude : -Amplitude;
					SignalIsPositive = !SignalIsPositive;
				}

				WriteImpulseFrame(FrameIndex, Amplitude);
				OutputTrigger->TriggerFrame(FrameIndex);
			}
		}

	private:
		void WriteImpulseFrame(const int32 FrameIndex, const float Amplitude)
		{
			if (OperatorData->bGenerateImpulsePerChannel)
			{
				const int32 NumChannels = OutputImpulse->NumChannels();
				if (NumChannels <= 0)
				{
					return;
				}

				OutputImpulse->GetChannel(NextChannelIndex)[FrameIndex] = Amplitude;
				NextChannelIndex = (NextChannelIndex + 1) % NumChannels;
				return;
			}

			const float WrappedPan = FMath::Fmod(*Pan, 2.0f) < 0.0f ? FMath::Fmod(*Pan, 2.0f) + 2.0f : FMath::Fmod(*Pan, 2.0f);
			if (!Panner)
			{
				OutputImpulse->GetChannel(0)[FrameIndex] = Amplitude;
				return;
			}

			Audio::IDiscretePanner::FOutputParams OutputParams;
			Panner->ComputeGains(
				{
					.AzimuthDegrees = CATDustPrivate::NormalizedAzimuthToDegrees(WrappedPan),
					.ElevationDegrees = 0.f,
					.bAllowAzimuthMirroring = true
				},
				OutputParams);

			for (const Audio::IDiscretePanner::FPanResult& Result : OutputParams.Results)
			{
				if (Result.ChannelID.IsNone())
				{
					continue;
				}

				const int32 SpeakerIndex = DiscreteFamily.FindSpeakerIndex(Result.ChannelID);
				if (SpeakerIndex != INDEX_NONE)
				{
					OutputImpulse->GetChannel(SpeakerIndex)[FrameIndex] += Amplitude * Result.Gain;
				}
			}
		}

		TSharedPtr<const FCATDustOperatorData> OperatorData;
		FBoolReadRef Enabled;
		FBoolReadRef BiPolar;
		FInt32ReadRef Seed;
		FFloatReadRef DensityOffset;
		FAudioBufferReadRef DensityAudio;
		FFloatReadRef AmpVarOffset;
		FAudioBufferReadRef AmpVarAudio;
		FFloatReadRef Pan;
		FDiscreteChannelAgnosticTypeWriteRef OutputImpulse;
		FTriggerWriteRef OutputTrigger;
		FRandomStream RNGStream;
		float SampleRate;
		bool SignalIsPositive;
		int32 LastSeed;
		const FDiscreteChannelTypeFamily& DiscreteFamily;
		const Audio::IDiscretePanner* Panner = nullptr;
		int32 NextChannelIndex = 0;
	};

	using FCATDustNode = TNodeFacade<FCATDustOperator>;
	METASOUND_REGISTER_NODE_AND_CONFIGURATION(FCATDustNode, FMetaSoundCATDustNodeConfiguration);
}

TArray<FPropertyTextFName> UMetaSoundCATDustNodeOptionsHelper::GetSoundFileFormatChannelOptions()
{
	const TArray<TSharedRef<const Audio::FChannelTypeFamily>> AllFormats = Audio::GetChannelRegistry().GetAllChannelFormats();
	TArray<FPropertyTextFName> FormatsOptions;
	for (const TSharedRef<const Audio::FChannelTypeFamily>& Format : AllFormats)
	{
		if (const Audio::FDiscreteChannelTypeFamily* Discrete = Format->Cast<Audio::FDiscreteChannelTypeFamily>())
		{
			const FString TypeName = Discrete->GetName().ToString();
			if (TypeName.StartsWith(TEXT("Cat:")))
			{
				FormatsOptions.Emplace(Format->GetName(), FText::FromString(Format->GetFriendlyName()));
			}
		}
	}
	return FormatsOptions;
}

FMetaSoundCATDustNodeConfiguration::FMetaSoundCATDustNodeConfiguration()
	: OperatorData(MakeShared<Metasound::CATDustPrivate::FCATDustOperatorData>(OutputAudioTypeName, bGenerateImpulsePerChannel))
{
}

TInstancedStruct<FMetasoundFrontendClassInterface> FMetaSoundCATDustNodeConfiguration::OverrideDefaultInterface(const FMetasoundFrontendClass& InClass) const
{
	return TInstancedStruct<FMetasoundFrontendClassInterface>::Make(
		FMetasoundFrontendClassInterface::GenerateClassInterface(
			Metasound::CATDustPrivate::GetVertexInterface(OutputAudioTypeName, bGenerateImpulsePerChannel)));
}

TSharedPtr<const Metasound::IOperatorData> FMetaSoundCATDustNodeConfiguration::GetOperatorData() const
{
	OperatorData->OutputAudioTypeName = OutputAudioTypeName;
	OperatorData->bGenerateImpulsePerChannel = bGenerateImpulsePerChannel;
	return OperatorData;
}

#undef LOCTEXT_NAMESPACE