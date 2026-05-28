// Copyright 2026 Charles Matthews. All Rights Reserved.

#include "MetasoundBranchesCAT/Public/MetasoundCATSawtoothOscillatorNode.h"

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
#include "Math/UnrealMathUtility.h"
#include "TypeFamily/ChannelTypeFamily.h"

#define LOCTEXT_NAMESPACE "MetasoundBranches_CATSawtoothOscillatorNode"

namespace Metasound
{
	namespace CATSawtoothOscillatorVertexNames
	{
		METASOUND_PARAM(Frequency, "Frequency", "Oscillator frequency in Hz.")
		METASOUND_PARAM(Phase, "Phase", "Phase offset in cycles (0-1 wraps).")
		METASOUND_PARAM(Reset, "Reset", "Reset phase accumulators when triggered.")
		METASOUND_PARAM(BiPolar, "Bi-Polar", "Output bipolar signal if true, unipolar if false.")
		METASOUND_PARAM(Output, "Output", "CAT sawtooth oscillator output.")
	}

	namespace CATSawtoothOscillatorPrivate
	{
		class FCATSawtoothOscillatorOperatorData final : public TOperatorData<FCATSawtoothOscillatorOperatorData>
		{
		public:
			static const FLazyName OperatorDataTypeName;

			explicit FCATSawtoothOscillatorOperatorData(
				const FName& InCatAudioTypeName,
				const EMetaSoundCATSawtoothControlMode InFrequencyMode,
				const EMetaSoundCATSawtoothControlMode InPhaseMode)
				: CatAudioTypeName(InCatAudioTypeName)
				, FrequencyMode(InFrequencyMode)
				, PhaseMode(InPhaseMode)
			{
			}

			FName CatAudioTypeName;
			EMetaSoundCATSawtoothControlMode FrequencyMode;
			EMetaSoundCATSawtoothControlMode PhaseMode;
		};

		const FLazyName FCATSawtoothOscillatorOperatorData::OperatorDataTypeName = TEXT("FCATSawtoothOscillatorOperatorData");

		void AddControlVertex(
			FInputVertexInterface& InOutInput,
			const FName& InVertexName,
			const FDataVertexMetadata& InMetadata,
			const FName& InCatFormat,
			const EMetaSoundCATSawtoothControlMode InMode,
			const float InDefaultFloat)
		{
			switch (InMode)
			{
			case EMetaSoundCATSawtoothControlMode::CAT:
				InOutInput.Add(FInputDataVertex(InVertexName, InCatFormat, InMetadata, EVertexAccessType::Reference));
				break;
			case EMetaSoundCATSawtoothControlMode::MonoAudio:
				InOutInput.Add(TInputDataVertex<FAudioBuffer>(InVertexName, InMetadata));
				break;
			case EMetaSoundCATSawtoothControlMode::Float:
				InOutInput.Add(TInputDataVertex<float>(InVertexName, InMetadata, InDefaultFloat));
				break;
			case EMetaSoundCATSawtoothControlMode::FloatArray:
				InOutInput.Add(TInputDataVertex<TArray<float>>(InVertexName, InMetadata));
				break;
			default:
				checkNoEntry();
				break;
			}
		}

		FVertexInterface GetVertexInterface(
			const FName& InCatFormat,
			const EMetaSoundCATSawtoothControlMode InFrequencyMode,
			const EMetaSoundCATSawtoothControlMode InPhaseMode)
		{
			using namespace CATSawtoothOscillatorVertexNames;

			FInputVertexInterface InputInterface;
			AddControlVertex(
				InputInterface,
				METASOUND_GET_PARAM_NAME(Frequency),
				METASOUND_GET_PARAM_METADATA(Frequency),
				InCatFormat,
				InFrequencyMode,
				440.0f);

			AddControlVertex(
				InputInterface,
				METASOUND_GET_PARAM_NAME(Phase),
				METASOUND_GET_PARAM_METADATA(Phase),
				InCatFormat,
				InPhaseMode,
				0.0f);

			InputInterface.Add(TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(BiPolar), true));
			InputInterface.Add(TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(Reset)));

			FOutputVertexInterface OutputInterface;
			OutputInterface.Add(FOutputDataVertex(METASOUND_GET_PARAM_NAME(Output), InCatFormat, METASOUND_GET_PARAM_METADATA(Output), EVertexAccessType::Reference));

			return { MoveTemp(InputInterface), MoveTemp(OutputInterface) };
		}

		float GetArrayValue(const TArray<float>& InValues, const int32 InChannel, const float InDefault)
		{
			if (InValues.Num() == 1)
			{
				return InValues[0];
			}
			if (InValues.Num() > 1)
			{
				return InValues[FMath::Min(InChannel, InValues.Num() - 1)];
			}
			return InDefault;
		}

		float GetControlValue(
			const EMetaSoundCATSawtoothControlMode InMode,
			const int32 InChannel,
			const int32 InFrame,
			const FChannelAgnosticTypeReadRef& InCAT,
			const FAudioBufferReadRef& InMono,
			const FFloatReadRef& InFloat,
			const TDataReadReference<TArray<float>>& InFloatArray,
			const float InDefault)
		{
			switch (InMode)
			{
			case EMetaSoundCATSawtoothControlMode::CAT:
			{
				const int32 NumChannels = InCAT->NumChannels();
				if (NumChannels <= 0)
				{
					return InDefault;
				}

				const int32 Channel = (NumChannels == 1) ? 0 : FMath::Min(InChannel, NumChannels - 1);
				TArrayView<const float> Src = InCAT->GetChannel(Channel);
				if (Src.Num() <= 0)
				{
					return InDefault;
				}

				const int32 SampleIndex = FMath::Min(InFrame, Src.Num() - 1);
				return Src[SampleIndex];
			}
			case EMetaSoundCATSawtoothControlMode::MonoAudio:
			{
				const int32 NumSamples = InMono->Num();
				if (NumSamples <= 0)
				{
					return InDefault;
				}

				const int32 SampleIndex = FMath::Min(InFrame, NumSamples - 1);
				return InMono->GetData()[SampleIndex];
			}
			case EMetaSoundCATSawtoothControlMode::Float:
				return *InFloat;
			case EMetaSoundCATSawtoothControlMode::FloatArray:
				return GetArrayValue(*InFloatArray, InChannel, InDefault);
			default:
				checkNoEntry();
				return InDefault;
			}
		}
	}

	class FCATSawtoothOscillatorOperator final : public TExecutableOperator<FCATSawtoothOscillatorOperator>
	{
	public:
		using FCATSawtoothOscillatorOperatorData = CATSawtoothOscillatorPrivate::FCATSawtoothOscillatorOperatorData;

		FCATSawtoothOscillatorOperator(
			const TSharedPtr<const FCATSawtoothOscillatorOperatorData>& InOperatorData,
			const FOperatorSettings& InSettings,
			FChannelAgnosticTypeReadRef&& InFrequencyCAT,
			FAudioBufferReadRef&& InFrequencyMono,
			FFloatReadRef&& InFrequencyFloat,
			TDataReadReference<TArray<float>>&& InFrequencyArray,
			FChannelAgnosticTypeReadRef&& InPhaseCAT,
			FAudioBufferReadRef&& InPhaseMono,
			FFloatReadRef&& InPhaseFloat,
			TDataReadReference<TArray<float>>&& InPhaseArray,
			FTriggerReadRef&& InReset,
			FBoolReadRef&& InBiPolar,
			FChannelAgnosticTypeWriteRef&& InOutput)
			: OperatorData(InOperatorData)
			, Settings(InSettings)
			, FrequencyCAT(MoveTemp(InFrequencyCAT))
			, FrequencyMono(MoveTemp(InFrequencyMono))
			, FrequencyFloat(MoveTemp(InFrequencyFloat))
			, FrequencyArray(MoveTemp(InFrequencyArray))
			, PhaseCAT(MoveTemp(InPhaseCAT))
			, PhaseMono(MoveTemp(InPhaseMono))
			, PhaseFloat(MoveTemp(InPhaseFloat))
			, PhaseArray(MoveTemp(InPhaseArray))
			, Reset(MoveTemp(InReset))
			, BiPolar(MoveTemp(InBiPolar))
			, Output(MoveTemp(InOutput))
		{
		}

		static const FVertexInterface& DeclareVertexInterface()
		{
			static const FVertexInterface Interface = CATSawtoothOscillatorPrivate::GetVertexInterface(
				TEXT("Cat:Stereo2Dot0"),
				EMetaSoundCATSawtoothControlMode::Float,
				EMetaSoundCATSawtoothControlMode::Float);
			return Interface;
		}

		static FNodeClassMetadata GetNodeInfo()
		{
			FNodeClassMetadata Metadata;
			Metadata.ClassName = { TEXT("Experimental"), TEXT("CATSawtoothOscillator"), TEXT("Audio") };
			Metadata.MajorVersion = 1;
			Metadata.MinorVersion = 0;
			Metadata.DisplayName = LOCTEXT("CATSawtoothOscillatorDisplayName", "CAT Sawtooth Oscillator");
			Metadata.Description = LOCTEXT("CATSawtoothOscillatorDescription", "Per-channel sawtooth oscillator with split channel phase offsets and configurable frequency/phase controls.");
			Metadata.Author = TEXT("Charles Matthews");
			Metadata.PromptIfMissing = LOCTEXT("CATSawtoothOscillatorMissingPrompt", "Enable MetaSoundExperimental for CAT channel format schemas.");
			Metadata.CategoryHierarchy = {
				METASOUND_LOCTEXT("Custom", "Branches"),
				METASOUND_LOCTEXT("CustomSub", "CAT")
			};
			Metadata.DefaultInterface = DeclareVertexInterface();
			METASOUND_BRANCHES_APPLY_CAT_NODE_STYLE(Metadata);
			return Metadata;
		}

		METASOUND_DISABLE_LEGACY_IO()

		static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
		{
			using namespace CATSawtoothOscillatorVertexNames;

			const FCATSawtoothOscillatorOperatorData* ConfigData = CastOperatorData<const FCATSawtoothOscillatorOperatorData>(InParams.Node.GetOperatorData().Get());
			if (!ConfigData)
			{
				return MakeUnique<FNoOpOperator>();
			}

			const Audio::FChannelTypeFamily* ConcreteType = Audio::GetChannelRegistry().FindConcreteChannel(ConfigData->CatAudioTypeName);
			if (!ConcreteType)
			{
				return MakeUnique<FNoOpOperator>();
			}

			const TSharedPtr<const FCATSawtoothOscillatorOperatorData>& OperatorDataSharedPtr = StaticCastSharedPtr<const FCATSawtoothOscillatorOperatorData>(InParams.Node.GetOperatorData());

			FChannelAgnosticTypeReadRef InFrequencyCAT = FChannelAgnosticTypeWriteRef::CreateNew(InParams.OperatorSettings, ConcreteType->GetName());
			FAudioBufferReadRef InFrequencyMono = FAudioBufferWriteRef::CreateNew(InParams.OperatorSettings);
			FFloatReadRef InFrequencyFloat = TDataReadReference<float>::CreateNew(440.0f);
			TDataReadReference<TArray<float>> InFrequencyArray = TDataReadReference<TArray<float>>::CreateNew(TArray<float>{});

			FChannelAgnosticTypeReadRef InPhaseCAT = FChannelAgnosticTypeWriteRef::CreateNew(InParams.OperatorSettings, ConcreteType->GetName());
			FAudioBufferReadRef InPhaseMono = FAudioBufferWriteRef::CreateNew(InParams.OperatorSettings);
			FFloatReadRef InPhaseFloat = TDataReadReference<float>::CreateNew(0.0f);
			TDataReadReference<TArray<float>> InPhaseArray = TDataReadReference<TArray<float>>::CreateNew(TArray<float>{});

			FTriggerReadRef InReset = InParams.InputData.GetOrCreateDefaultDataReadReference<FTrigger>(METASOUND_GET_PARAM_NAME(Reset), InParams.OperatorSettings);
			FBoolReadRef InBiPolar = InParams.InputData.GetOrCreateDefaultDataReadReference<bool>(METASOUND_GET_PARAM_NAME(BiPolar), InParams.OperatorSettings);

			switch (ConfigData->FrequencyMode)
			{
			case EMetaSoundCATSawtoothControlMode::CAT:
				InFrequencyCAT = InParams.InputData.GetOrCreateDefaultDataReadReference<FChannelAgnosticType>(METASOUND_GET_PARAM_NAME(Frequency), InParams.OperatorSettings);
				break;
			case EMetaSoundCATSawtoothControlMode::MonoAudio:
				InFrequencyMono = InParams.InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(Frequency), InParams.OperatorSettings);
				break;
			case EMetaSoundCATSawtoothControlMode::Float:
				InFrequencyFloat = InParams.InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(Frequency), InParams.OperatorSettings);
				break;
			case EMetaSoundCATSawtoothControlMode::FloatArray:
				InFrequencyArray = InParams.InputData.GetOrCreateDefaultDataReadReference<TArray<float>>(METASOUND_GET_PARAM_NAME(Frequency), InParams.OperatorSettings);
				break;
			default:
				checkNoEntry();
				break;
			}

			switch (ConfigData->PhaseMode)
			{
			case EMetaSoundCATSawtoothControlMode::CAT:
				InPhaseCAT = InParams.InputData.GetOrCreateDefaultDataReadReference<FChannelAgnosticType>(METASOUND_GET_PARAM_NAME(Phase), InParams.OperatorSettings);
				break;
			case EMetaSoundCATSawtoothControlMode::MonoAudio:
				InPhaseMono = InParams.InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(Phase), InParams.OperatorSettings);
				break;
			case EMetaSoundCATSawtoothControlMode::Float:
				InPhaseFloat = InParams.InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(Phase), InParams.OperatorSettings);
				break;
			case EMetaSoundCATSawtoothControlMode::FloatArray:
				InPhaseArray = InParams.InputData.GetOrCreateDefaultDataReadReference<TArray<float>>(METASOUND_GET_PARAM_NAME(Phase), InParams.OperatorSettings);
				break;
			default:
				checkNoEntry();
				break;
			}

			FChannelAgnosticTypeWriteRef Out = FChannelAgnosticTypeWriteRef::CreateNew(InParams.OperatorSettings, ConcreteType->GetName());

			return MakeUnique<FCATSawtoothOscillatorOperator>(
				OperatorDataSharedPtr,
				InParams.OperatorSettings,
				MoveTemp(InFrequencyCAT),
				MoveTemp(InFrequencyMono),
				MoveTemp(InFrequencyFloat),
				MoveTemp(InFrequencyArray),
				MoveTemp(InPhaseCAT),
				MoveTemp(InPhaseMono),
				MoveTemp(InPhaseFloat),
				MoveTemp(InPhaseArray),
				MoveTemp(InReset),
				MoveTemp(InBiPolar),
				MoveTemp(Out));
		}

		virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
		{
			using namespace CATSawtoothOscillatorVertexNames;

			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Reset), Reset);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(BiPolar), BiPolar);

			switch (OperatorData->FrequencyMode)
			{
			case EMetaSoundCATSawtoothControlMode::CAT:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Frequency), FrequencyCAT);
				break;
			case EMetaSoundCATSawtoothControlMode::MonoAudio:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Frequency), FrequencyMono);
				break;
			case EMetaSoundCATSawtoothControlMode::Float:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Frequency), FrequencyFloat);
				break;
			case EMetaSoundCATSawtoothControlMode::FloatArray:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Frequency), FrequencyArray);
				break;
			default:
				checkNoEntry();
				break;
			}

			switch (OperatorData->PhaseMode)
			{
			case EMetaSoundCATSawtoothControlMode::CAT:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Phase), PhaseCAT);
				break;
			case EMetaSoundCATSawtoothControlMode::MonoAudio:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Phase), PhaseMono);
				break;
			case EMetaSoundCATSawtoothControlMode::Float:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Phase), PhaseFloat);
				break;
			case EMetaSoundCATSawtoothControlMode::FloatArray:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Phase), PhaseArray);
				break;
			default:
				checkNoEntry();
				break;
			}
		}

		virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
		{
			using namespace CATSawtoothOscillatorVertexNames;
			InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(Output), Output);
		}

		void Execute()
		{
			Output->Zero();

			const int32 NumFrames = Settings.GetNumFramesPerBlock();
			const int32 NumOutChannels = Output->NumChannels();
			if (NumOutChannels <= 0 || NumFrames <= 0)
			{
				return;
			}

			if (PhaseAccumulatorPerChannel.Num() != NumOutChannels)
			{
				PhaseAccumulatorPerChannel.Init(0.0f, NumOutChannels);
			}

			const float SampleRate = FMath::Max(1.0f, Settings.GetSampleRate());
			const bool bBiPolar = *BiPolar;

			TArray<bool> bResetFrames;
			bResetFrames.Init(false, NumFrames);
			Reset->ExecuteBlock(
				[](const int32 StartFrame, const int32 EndFrame)
				{
				},
				[&bResetFrames, NumFrames](const int32 StartFrame, const int32 EndFrame)
				{
					if (StartFrame >= 0 && StartFrame < NumFrames)
					{
						bResetFrames[StartFrame] = true;
					}
				});

			for (int32 Channel = 0; Channel < NumOutChannels; ++Channel)
			{
				TArrayView<float> Dst = Output->GetChannel(Channel);
				float& Accum = PhaseAccumulatorPerChannel[Channel];
				const float ChannelOffset = static_cast<float>(Channel) / static_cast<float>(NumOutChannels);

				for (int32 Frame = 0; Frame < NumFrames; ++Frame)
				{
					if (bResetFrames[Frame])
					{
						Accum = 0.0f;
					}

					const float FrequencyValue = CATSawtoothOscillatorPrivate::GetControlValue(
						OperatorData->FrequencyMode,
						Channel,
						Frame,
						FrequencyCAT,
						FrequencyMono,
						FrequencyFloat,
						FrequencyArray,
						440.0f);

					const float PhaseValue = CATSawtoothOscillatorPrivate::GetControlValue(
						OperatorData->PhaseMode,
						Channel,
						Frame,
						PhaseCAT,
						PhaseMono,
						PhaseFloat,
						PhaseArray,
						0.0f);

					const float ClampedFrequency = FMath::Max(0.0f, FrequencyValue);
					Accum += ClampedFrequency / SampleRate;
					Accum -= FMath::Floor(Accum);

					const float WrappedPhase = (Accum + ChannelOffset + PhaseValue) - FMath::Floor(Accum + ChannelOffset + PhaseValue);
					Dst[Frame] = bBiPolar ? (2.0f * WrappedPhase - 1.0f) : WrappedPhase;
				}
			}
		}

	private:
		TSharedPtr<const FCATSawtoothOscillatorOperatorData> OperatorData;
		FOperatorSettings Settings;

		FChannelAgnosticTypeReadRef FrequencyCAT;
		FAudioBufferReadRef FrequencyMono;
		FFloatReadRef FrequencyFloat;
		TDataReadReference<TArray<float>> FrequencyArray;

		FChannelAgnosticTypeReadRef PhaseCAT;
		FAudioBufferReadRef PhaseMono;
		FFloatReadRef PhaseFloat;
		TDataReadReference<TArray<float>> PhaseArray;

		FTriggerReadRef Reset;
		FBoolReadRef BiPolar;
		FChannelAgnosticTypeWriteRef Output;
		TArray<float> PhaseAccumulatorPerChannel;
	};

	using FCATSawtoothOscillatorNode = TNodeFacade<FCATSawtoothOscillatorOperator>;
	METASOUND_REGISTER_NODE_AND_CONFIGURATION(FCATSawtoothOscillatorNode, FMetaSoundCATSawtoothOscillatorNodeConfiguration);
}

TArray<FPropertyTextFName> UMetaSoundCATSawtoothOscillatorNodeOptionsHelper::GetSoundFileFormatChannelOptions()
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

FMetaSoundCATSawtoothOscillatorNodeConfiguration::FMetaSoundCATSawtoothOscillatorNodeConfiguration()
	: OperatorData(MakeShared<Metasound::CATSawtoothOscillatorPrivate::FCATSawtoothOscillatorOperatorData>(CatAudioTypeName, FrequencyMode, PhaseMode))
{
}

TInstancedStruct<FMetasoundFrontendClassInterface> FMetaSoundCATSawtoothOscillatorNodeConfiguration::OverrideDefaultInterface(const FMetasoundFrontendClass& InClass) const
{
	return TInstancedStruct<FMetasoundFrontendClassInterface>::Make(
		FMetasoundFrontendClassInterface::GenerateClassInterface(
			Metasound::CATSawtoothOscillatorPrivate::GetVertexInterface(CatAudioTypeName, FrequencyMode, PhaseMode)));
}

TSharedPtr<const Metasound::IOperatorData> FMetaSoundCATSawtoothOscillatorNodeConfiguration::GetOperatorData() const
{
	OperatorData->CatAudioTypeName = CatAudioTypeName;
	OperatorData->FrequencyMode = FrequencyMode;
	OperatorData->PhaseMode = PhaseMode;
	return OperatorData;
}

#undef LOCTEXT_NAMESPACE
