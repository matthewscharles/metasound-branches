// Copyright 2026 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundCATBitcrusherNode.h"

#include "MetasoundAudioBuffer.h"
#include "MetasoundBranches/Public/MetasoundCommonMacros.h"
#include "MetasoundChannelAgnosticType.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundParamHelper.h"
#include "MetasoundPrimitives.h"
#include "MetasoundVertex.h"
#include "TypeFamily/ChannelTypeFamily.h"

#define LOCTEXT_NAMESPACE "MetasoundBranches_CATBitcrusherNode"

namespace Metasound
{
	namespace CATBitcrusherVertexNames
	{
		METASOUND_PARAM(Input, "Input", "CAT input to bitcrush.")
		METASOUND_PARAM(BitDepth, "Bit Depth", "Quantization bit depth.")
		METASOUND_PARAM(SampleRateControl, "Sample Rate", "Sample-hold rate in Hz for downsampling.")
		METASOUND_PARAM(Mix, "Mix", "Dry/wet balance from 0 (dry) to 1 (wet).")
		METASOUND_PARAM(Output, "Output", "Bitcrushed CAT output.")
	}

	namespace CATBitcrusherPrivate
	{
		class FCATBitcrusherOperatorData final : public TOperatorData<FCATBitcrusherOperatorData>
		{
		public:
			static const FLazyName OperatorDataTypeName;

			explicit FCATBitcrusherOperatorData(
				const FName& InCatAudioTypeName,
				const EMetaSoundCATBitcrusherControlMode InBitDepthMode,
				const EMetaSoundCATBitcrusherControlMode InSampleRateMode)
				: CatAudioTypeName(InCatAudioTypeName)
				, BitDepthMode(InBitDepthMode)
				, SampleRateMode(InSampleRateMode)
			{
			}

			FName CatAudioTypeName;
			EMetaSoundCATBitcrusherControlMode BitDepthMode;
			EMetaSoundCATBitcrusherControlMode SampleRateMode;
		};

		const FLazyName FCATBitcrusherOperatorData::OperatorDataTypeName = TEXT("FCATBitcrusherOperatorData");

		void AddControlVertex(
			FInputVertexInterface& InOutInput,
			const FName& InVertexName,
			const FDataVertexMetadata& InMetadata,
			const FName& InCatFormat,
			const EMetaSoundCATBitcrusherControlMode InMode,
			const float InDefaultFloat)
		{
			switch (InMode)
			{
			case EMetaSoundCATBitcrusherControlMode::CAT:
				InOutInput.Add(FInputDataVertex(InVertexName, InCatFormat, InMetadata, EVertexAccessType::Reference));
				break;
			case EMetaSoundCATBitcrusherControlMode::MonoAudio:
				InOutInput.Add(TInputDataVertex<FAudioBuffer>(InVertexName, InMetadata));
				break;
			case EMetaSoundCATBitcrusherControlMode::Float:
				InOutInput.Add(TInputDataVertex<float>(InVertexName, InMetadata, InDefaultFloat));
				break;
			case EMetaSoundCATBitcrusherControlMode::FloatArray:
				InOutInput.Add(TInputDataVertex<TArray<float>>(InVertexName, InMetadata));
				break;
			default:
				checkNoEntry();
				break;
			}
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
			const EMetaSoundCATBitcrusherControlMode InMode,
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
			case EMetaSoundCATBitcrusherControlMode::CAT:
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
				return Src[FMath::Min(InFrame, Src.Num() - 1)];
			}
			case EMetaSoundCATBitcrusherControlMode::MonoAudio:
			{
				const int32 NumSamples = InMono->Num();
				if (NumSamples <= 0)
				{
					return InDefault;
				}
				return InMono->GetData()[FMath::Min(InFrame, NumSamples - 1)];
			}
			case EMetaSoundCATBitcrusherControlMode::Float:
				return *InFloat;
			case EMetaSoundCATBitcrusherControlMode::FloatArray:
				return GetArrayValue(*InFloatArray, InChannel, InDefault);
			default:
				checkNoEntry();
				return InDefault;
			}
		}

		float QuantizeSigned(const float InSample, const float InBits)
		{
			const int32 Bits = FMath::Clamp(FMath::RoundToInt(InBits), 1, 24);
			const int32 Levels = (1 << Bits) - 1;
			const float Clamped = FMath::Clamp(InSample, -1.0f, 1.0f);
			const float Unipolar = (Clamped * 0.5f) + 0.5f;
			const float Quantized = FMath::RoundToFloat(Unipolar * Levels) / FMath::Max(1, Levels);
			return (Quantized * 2.0f) - 1.0f;
		}

		FVertexInterface GetVertexInterface(
			const FName& InCatFormat,
			const EMetaSoundCATBitcrusherControlMode InBitDepthMode,
			const EMetaSoundCATBitcrusherControlMode InSampleRateMode)
		{
			using namespace CATBitcrusherVertexNames;

			FInputVertexInterface InputInterface;
			InputInterface.Add(FInputDataVertex(METASOUND_GET_PARAM_NAME(Input), InCatFormat, METASOUND_GET_PARAM_METADATA(Input), EVertexAccessType::Reference));
			AddControlVertex(InputInterface, METASOUND_GET_PARAM_NAME(BitDepth), METASOUND_GET_PARAM_METADATA(BitDepth), InCatFormat, InBitDepthMode, 8.0f);
			AddControlVertex(InputInterface, METASOUND_GET_PARAM_NAME(SampleRateControl), METASOUND_GET_PARAM_METADATA(SampleRateControl), InCatFormat, InSampleRateMode, 8000.0f);
			InputInterface.Add(TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(Mix), 1.0f));

			FOutputVertexInterface OutputInterface;
			OutputInterface.Add(FOutputDataVertex(METASOUND_GET_PARAM_NAME(Output), InCatFormat, METASOUND_GET_PARAM_METADATA(Output), EVertexAccessType::Reference));
			return { MoveTemp(InputInterface), MoveTemp(OutputInterface) };
		}
	}

	class FCATBitcrusherOperator final : public TExecutableOperator<FCATBitcrusherOperator>
	{
	public:
		using FCATBitcrusherOperatorData = CATBitcrusherPrivate::FCATBitcrusherOperatorData;

		FCATBitcrusherOperator(
			const TSharedPtr<const FCATBitcrusherOperatorData>& InOperatorData,
			const FOperatorSettings& InSettings,
			FChannelAgnosticTypeReadRef&& InInput,
			FChannelAgnosticTypeReadRef&& InBitDepthCAT,
			FAudioBufferReadRef&& InBitDepthMono,
			FFloatReadRef&& InBitDepthFloat,
			TDataReadReference<TArray<float>>&& InBitDepthArray,
			FChannelAgnosticTypeReadRef&& InSampleRateCAT,
			FAudioBufferReadRef&& InSampleRateMono,
			FFloatReadRef&& InSampleRateFloat,
			TDataReadReference<TArray<float>>&& InSampleRateArray,
			FFloatReadRef&& InMix,
			FChannelAgnosticTypeWriteRef&& InOutput)
			: OperatorData(InOperatorData)
			, Settings(InSettings)
			, Input(MoveTemp(InInput))
			, BitDepthCAT(MoveTemp(InBitDepthCAT))
			, BitDepthMono(MoveTemp(InBitDepthMono))
			, BitDepthFloat(MoveTemp(InBitDepthFloat))
			, BitDepthArray(MoveTemp(InBitDepthArray))
			, SampleRateCAT(MoveTemp(InSampleRateCAT))
			, SampleRateMono(MoveTemp(InSampleRateMono))
			, SampleRateFloat(MoveTemp(InSampleRateFloat))
			, SampleRateArray(MoveTemp(InSampleRateArray))
			, Mix(MoveTemp(InMix))
			, Output(MoveTemp(InOutput))
			, EngineSampleRate(FMath::Max(1.0f, InSettings.GetSampleRate()))
		{
		}

		static const FVertexInterface& DeclareVertexInterface()
		{
			static const FVertexInterface Interface = CATBitcrusherPrivate::GetVertexInterface(
				TEXT("Cat:Stereo2Dot0"),
				EMetaSoundCATBitcrusherControlMode::Float,
				EMetaSoundCATBitcrusherControlMode::Float);
			return Interface;
		}

		static FNodeClassMetadata GetNodeInfo()
		{
			FNodeClassMetadata Metadata;
			Metadata.ClassName = { TEXT("Experimental"), TEXT("CATBtcrusher"), TEXT("Audio") };
			Metadata.MajorVersion = 1;
			Metadata.MinorVersion = 0;
			Metadata.DisplayName = LOCTEXT("CATBitcrusherDisplayName", "CAT Bitcrusher");
			Metadata.Description = LOCTEXT("CATBitcrusherDescription", "CAT bitcrusher with configurable bit depth and sample rate control pin types.");
			Metadata.Author = TEXT("Charles Matthews");
			Metadata.PromptIfMissing = LOCTEXT("CATBitcrusherMissingPrompt", "Enable MetaSoundExperimental for CAT channel format schemas.");
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
			using namespace CATBitcrusherVertexNames;

			const FCATBitcrusherOperatorData* ConfigData = CastOperatorData<const FCATBitcrusherOperatorData>(InParams.Node.GetOperatorData().Get());
			if (!ConfigData)
			{
				return MakeUnique<FNoOpOperator>();
			}

			const Audio::FChannelTypeFamily* ConcreteType = Audio::GetChannelRegistry().FindConcreteChannel(ConfigData->CatAudioTypeName);
			if (!ConcreteType)
			{
				return MakeUnique<FNoOpOperator>();
			}

			const TSharedPtr<const FCATBitcrusherOperatorData>& OperatorDataSharedPtr = StaticCastSharedPtr<const FCATBitcrusherOperatorData>(InParams.Node.GetOperatorData());

			FChannelAgnosticTypeReadRef InSignal = InParams.InputData.GetOrCreateDefaultDataReadReference<FChannelAgnosticType>(METASOUND_GET_PARAM_NAME(Input), InParams.OperatorSettings);

			FChannelAgnosticTypeReadRef InBitDepthCAT = FChannelAgnosticTypeWriteRef::CreateNew(InParams.OperatorSettings, ConcreteType->GetName());
			FAudioBufferReadRef InBitDepthMono = FAudioBufferWriteRef::CreateNew(InParams.OperatorSettings);
			FFloatReadRef InBitDepthFloat = TDataReadReference<float>::CreateNew(8.0f);
			TDataReadReference<TArray<float>> InBitDepthArray = TDataReadReference<TArray<float>>::CreateNew(TArray<float>{});

			FChannelAgnosticTypeReadRef InSampleRateCAT = FChannelAgnosticTypeWriteRef::CreateNew(InParams.OperatorSettings, ConcreteType->GetName());
			FAudioBufferReadRef InSampleRateMono = FAudioBufferWriteRef::CreateNew(InParams.OperatorSettings);
			FFloatReadRef InSampleRateFloat = TDataReadReference<float>::CreateNew(8000.0f);
			TDataReadReference<TArray<float>> InSampleRateArray = TDataReadReference<TArray<float>>::CreateNew(TArray<float>{});
			FFloatReadRef InMix = InParams.InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(Mix), InParams.OperatorSettings);

			switch (ConfigData->BitDepthMode)
			{
			case EMetaSoundCATBitcrusherControlMode::CAT:
				InBitDepthCAT = InParams.InputData.GetOrCreateDefaultDataReadReference<FChannelAgnosticType>(METASOUND_GET_PARAM_NAME(BitDepth), InParams.OperatorSettings);
				break;
			case EMetaSoundCATBitcrusherControlMode::MonoAudio:
				InBitDepthMono = InParams.InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(BitDepth), InParams.OperatorSettings);
				break;
			case EMetaSoundCATBitcrusherControlMode::Float:
				InBitDepthFloat = InParams.InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(BitDepth), InParams.OperatorSettings);
				break;
			case EMetaSoundCATBitcrusherControlMode::FloatArray:
				InBitDepthArray = InParams.InputData.GetOrCreateDefaultDataReadReference<TArray<float>>(METASOUND_GET_PARAM_NAME(BitDepth), InParams.OperatorSettings);
				break;
			default:
				checkNoEntry();
				break;
			}

			switch (ConfigData->SampleRateMode)
			{
			case EMetaSoundCATBitcrusherControlMode::CAT:
				InSampleRateCAT = InParams.InputData.GetOrCreateDefaultDataReadReference<FChannelAgnosticType>(METASOUND_GET_PARAM_NAME(SampleRateControl), InParams.OperatorSettings);
				break;
			case EMetaSoundCATBitcrusherControlMode::MonoAudio:
				InSampleRateMono = InParams.InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(SampleRateControl), InParams.OperatorSettings);
				break;
			case EMetaSoundCATBitcrusherControlMode::Float:
				InSampleRateFloat = InParams.InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(SampleRateControl), InParams.OperatorSettings);
				break;
			case EMetaSoundCATBitcrusherControlMode::FloatArray:
				InSampleRateArray = InParams.InputData.GetOrCreateDefaultDataReadReference<TArray<float>>(METASOUND_GET_PARAM_NAME(SampleRateControl), InParams.OperatorSettings);
				break;
			default:
				checkNoEntry();
				break;
			}

			FChannelAgnosticTypeWriteRef Out = FChannelAgnosticTypeWriteRef::CreateNew(InParams.OperatorSettings, ConcreteType->GetName());

			return MakeUnique<FCATBitcrusherOperator>(
				OperatorDataSharedPtr,
				InParams.OperatorSettings,
				MoveTemp(InSignal),
				MoveTemp(InBitDepthCAT),
				MoveTemp(InBitDepthMono),
				MoveTemp(InBitDepthFloat),
				MoveTemp(InBitDepthArray),
				MoveTemp(InSampleRateCAT),
				MoveTemp(InSampleRateMono),
				MoveTemp(InSampleRateFloat),
				MoveTemp(InSampleRateArray),
				MoveTemp(InMix),
				MoveTemp(Out));
		}

		virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
		{
			using namespace CATBitcrusherVertexNames;

			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Input), Input);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Mix), Mix);

			switch (OperatorData->BitDepthMode)
			{
			case EMetaSoundCATBitcrusherControlMode::CAT:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(BitDepth), BitDepthCAT);
				break;
			case EMetaSoundCATBitcrusherControlMode::MonoAudio:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(BitDepth), BitDepthMono);
				break;
			case EMetaSoundCATBitcrusherControlMode::Float:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(BitDepth), BitDepthFloat);
				break;
			case EMetaSoundCATBitcrusherControlMode::FloatArray:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(BitDepth), BitDepthArray);
				break;
			default:
				checkNoEntry();
				break;
			}

			switch (OperatorData->SampleRateMode)
			{
			case EMetaSoundCATBitcrusherControlMode::CAT:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(SampleRateControl), SampleRateCAT);
				break;
			case EMetaSoundCATBitcrusherControlMode::MonoAudio:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(SampleRateControl), SampleRateMono);
				break;
			case EMetaSoundCATBitcrusherControlMode::Float:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(SampleRateControl), SampleRateFloat);
				break;
			case EMetaSoundCATBitcrusherControlMode::FloatArray:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(SampleRateControl), SampleRateArray);
				break;
			default:
				checkNoEntry();
				break;
			}
		}

		virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
		{
			using namespace CATBitcrusherVertexNames;
			InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(Output), Output);
		}

		void Execute()
		{
			const int32 NumFrames = Settings.GetNumFramesPerBlock();
			const int32 NumOutChannels = Output->NumChannels();
			if (NumFrames <= 0 || NumOutChannels <= 0)
			{
				return;
			}

			if (PhasePerChannel.Num() != NumOutChannels)
			{
				PhasePerChannel.Init(1.0f, NumOutChannels);
				HeldSamplePerChannel.Init(0.0f, NumOutChannels);
			}

			const int32 NumInChannels = FMath::Max(1, Input->NumChannels());
			const float MixValue = FMath::Clamp(*Mix, 0.0f, 1.0f);
			for (int32 Channel = 0; Channel < NumOutChannels; ++Channel)
			{
				const int32 InChannel = (NumInChannels == 1) ? 0 : FMath::Min(Channel, NumInChannels - 1);
				TArrayView<const float> Src = Input->GetChannel(InChannel);
				TArrayView<float> Dst = Output->GetChannel(Channel);

				float Phase = PhasePerChannel[Channel];
				float Held = HeldSamplePerChannel[Channel];

				for (int32 Frame = 0; Frame < NumFrames; ++Frame)
				{
					const float BitDepthValue = CATBitcrusherPrivate::GetControlValue(
						OperatorData->BitDepthMode,
						Channel,
						Frame,
						BitDepthCAT,
						BitDepthMono,
						BitDepthFloat,
						BitDepthArray,
						8.0f);

					const float SampleRateValue = CATBitcrusherPrivate::GetControlValue(
						OperatorData->SampleRateMode,
						Channel,
						Frame,
						SampleRateCAT,
						SampleRateMono,
						SampleRateFloat,
						SampleRateArray,
						8000.0f);

					const float EffectiveRate = FMath::Clamp(SampleRateValue, 1.0f, EngineSampleRate);
					const float PhaseIncrement = EffectiveRate / EngineSampleRate;

					if (Phase >= 1.0f)
					{
						Held = CATBitcrusherPrivate::QuantizeSigned(Src[Frame], BitDepthValue);
						Phase -= 1.0f;
					}

					Dst[Frame] = FMath::Lerp(Src[Frame], Held, MixValue);
					Phase += PhaseIncrement;
				}

				PhasePerChannel[Channel] = Phase;
				HeldSamplePerChannel[Channel] = Held;
			}
		}

	private:
		TSharedPtr<const FCATBitcrusherOperatorData> OperatorData;
		FOperatorSettings Settings;
		FChannelAgnosticTypeReadRef Input;

		FChannelAgnosticTypeReadRef BitDepthCAT;
		FAudioBufferReadRef BitDepthMono;
		FFloatReadRef BitDepthFloat;
		TDataReadReference<TArray<float>> BitDepthArray;

		FChannelAgnosticTypeReadRef SampleRateCAT;
		FAudioBufferReadRef SampleRateMono;
		FFloatReadRef SampleRateFloat;
		TDataReadReference<TArray<float>> SampleRateArray;
		FFloatReadRef Mix;

		FChannelAgnosticTypeWriteRef Output;
		float EngineSampleRate = 48000.0f;
		TArray<float> PhasePerChannel;
		TArray<float> HeldSamplePerChannel;
	};

	using FCATBitcrusherNode = TNodeFacade<FCATBitcrusherOperator>;
	METASOUND_REGISTER_NODE_AND_CONFIGURATION(FCATBitcrusherNode, FMetaSoundCATBitcrusherNodeConfiguration);
}

TArray<FPropertyTextFName> UMetaSoundCATBitcrusherNodeOptionsHelper::GetSoundFileFormatChannelOptions()
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

FMetaSoundCATBitcrusherNodeConfiguration::FMetaSoundCATBitcrusherNodeConfiguration()
	: OperatorData(MakeShared<Metasound::CATBitcrusherPrivate::FCATBitcrusherOperatorData>(CatAudioTypeName, BitDepthMode, SampleRateMode))
{
}

TInstancedStruct<FMetasoundFrontendClassInterface> FMetaSoundCATBitcrusherNodeConfiguration::OverrideDefaultInterface(const FMetasoundFrontendClass& InClass) const
{
	return TInstancedStruct<FMetasoundFrontendClassInterface>::Make(
		FMetasoundFrontendClassInterface::GenerateClassInterface(
			Metasound::CATBitcrusherPrivate::GetVertexInterface(CatAudioTypeName, BitDepthMode, SampleRateMode)));
}

TSharedPtr<const Metasound::IOperatorData> FMetaSoundCATBitcrusherNodeConfiguration::GetOperatorData() const
{
	OperatorData->CatAudioTypeName = CatAudioTypeName;
	OperatorData->BitDepthMode = BitDepthMode;
	OperatorData->SampleRateMode = SampleRateMode;
	return OperatorData;
}

#undef LOCTEXT_NAMESPACE
