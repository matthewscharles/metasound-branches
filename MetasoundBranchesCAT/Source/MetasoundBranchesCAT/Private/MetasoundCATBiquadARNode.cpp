// Copyright 2026 Charles Matthews. All Rights Reserved.

#include "MetasoundBranchesCAT/Public/MetasoundCATBiquadARNode.h"

#include "DSP/Filter.h"
#include "MetasoundAudioBuffer.h"
#include "MetasoundBranches/Public/MetasoundCommonMacros.h"
#include "MetasoundChannelAgnosticType.h"
#include "MetasoundEnumRegistrationMacro.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundParamHelper.h"
#include "MetasoundPrimitives.h"
#include "MetasoundVertex.h"
#include "TypeFamily/ChannelTypeFamily.h"

#define LOCTEXT_NAMESPACE "MetasoundBranches_CATBiquadARNode"

namespace Metasound
{
	DECLARE_METASOUND_ENUM(Audio::EBiquadFilter::Type, Audio::EBiquadFilter::Lowpass,
		METASOUNDSTANDARDNODES_API, FEnumEBiquadFilterType, FEnumBiQuadFilterTypeInfo, FEnumBiQuadFilterReadRef, FEnumBiQuadFilterWriteRef);

	namespace CATBiquadARVertexNames
	{
		METASOUND_PARAM(Input, "Input", "CAT signal to filter.")
		METASOUND_PARAM(Cutoff, "Cutoff", "Filter cutoff control.")
		METASOUND_PARAM(Bandwidth, "Bandwidth", "Filter bandwidth control.")
		METASOUND_PARAM(GainDb, "Gain", "Gain in dB for gain-using filter types.")
		METASOUND_PARAM(FilterType, "Type", "Biquad filter type.")
		METASOUND_PARAM(Output, "Output", "Filtered CAT output.")
	}

	namespace CATBiquadARPrivate
	{
		class FCATBiquadAROperatorData final : public TOperatorData<FCATBiquadAROperatorData>
		{
		public:
			static const FLazyName OperatorDataTypeName;

			explicit FCATBiquadAROperatorData(
				const FName& InCatAudioTypeName,
				const EMetaSoundCATBiquadARControlMode InCutoffMode,
				const EMetaSoundCATBiquadARControlMode InBandwidthMode)
				: CatAudioTypeName(InCatAudioTypeName)
				, CutoffMode(InCutoffMode)
				, BandwidthMode(InBandwidthMode)
			{
			}

			FName CatAudioTypeName;
			EMetaSoundCATBiquadARControlMode CutoffMode;
			EMetaSoundCATBiquadARControlMode BandwidthMode;
		};

		const FLazyName FCATBiquadAROperatorData::OperatorDataTypeName = TEXT("FCATBiquadAROperatorData");

		void AddControlVertex(
			FInputVertexInterface& InOutInput,
			const FName& InVertexName,
			const FDataVertexMetadata& InMetadata,
			const FName& InCatFormat,
			const EMetaSoundCATBiquadARControlMode InMode,
			const float InDefaultFloat)
		{
			switch (InMode)
			{
			case EMetaSoundCATBiquadARControlMode::CAT:
				InOutInput.Add(FInputDataVertex(InVertexName, InCatFormat, InMetadata, EVertexAccessType::Reference));
				break;
			case EMetaSoundCATBiquadARControlMode::MonoAudio:
				InOutInput.Add(TInputDataVertex<FAudioBuffer>(InVertexName, InMetadata));
				break;
			case EMetaSoundCATBiquadARControlMode::Float:
				InOutInput.Add(TInputDataVertex<float>(InVertexName, InMetadata, InDefaultFloat));
				break;
			case EMetaSoundCATBiquadARControlMode::FloatArray:
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
			const EMetaSoundCATBiquadARControlMode InMode,
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
			case EMetaSoundCATBiquadARControlMode::CAT:
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
			case EMetaSoundCATBiquadARControlMode::MonoAudio:
			{
				const int32 NumSamples = InMono->Num();
				if (NumSamples <= 0)
				{
					return InDefault;
				}
				return InMono->GetData()[FMath::Min(InFrame, NumSamples - 1)];
			}
			case EMetaSoundCATBiquadARControlMode::Float:
				return *InFloat;
			case EMetaSoundCATBiquadARControlMode::FloatArray:
				return GetArrayValue(*InFloatArray, InChannel, InDefault);
			default:
				checkNoEntry();
				return InDefault;
			}
		}

		FVertexInterface GetVertexInterface(
			const FName& InCatFormat,
			const EMetaSoundCATBiquadARControlMode InCutoffMode,
			const EMetaSoundCATBiquadARControlMode InBandwidthMode)
		{
			using namespace CATBiquadARVertexNames;

			FInputVertexInterface InputInterface;
			InputInterface.Add(FInputDataVertex(METASOUND_GET_PARAM_NAME(Input), InCatFormat, METASOUND_GET_PARAM_METADATA(Input), EVertexAccessType::Reference));

			AddControlVertex(
				InputInterface,
				METASOUND_GET_PARAM_NAME(Cutoff),
				METASOUND_GET_PARAM_METADATA(Cutoff),
				InCatFormat,
				InCutoffMode,
				20000.0f);

			AddControlVertex(
				InputInterface,
				METASOUND_GET_PARAM_NAME(Bandwidth),
				METASOUND_GET_PARAM_METADATA(Bandwidth),
				InCatFormat,
				InBandwidthMode,
				1.89997f);

			InputInterface.Add(TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(GainDb), 0.0f));
			InputInterface.Add(TInputDataVertex<FEnumEBiquadFilterType>(METASOUND_GET_PARAM_NAME_AND_METADATA(FilterType)));

			FOutputVertexInterface OutputInterface;
			OutputInterface.Add(FOutputDataVertex(METASOUND_GET_PARAM_NAME(Output), InCatFormat, METASOUND_GET_PARAM_METADATA(Output), EVertexAccessType::Reference));

			return { MoveTemp(InputInterface), MoveTemp(OutputInterface) };
		}
	}

	class FCATBiquadAROperator final : public TExecutableOperator<FCATBiquadAROperator>
	{
	public:
		using FCATBiquadAROperatorData = CATBiquadARPrivate::FCATBiquadAROperatorData;

		FCATBiquadAROperator(
			const TSharedPtr<const FCATBiquadAROperatorData>& InOperatorData,
			const FOperatorSettings& InSettings,
			FChannelAgnosticTypeReadRef&& InInput,
			FChannelAgnosticTypeReadRef&& InCutoffCAT,
			FAudioBufferReadRef&& InCutoffMono,
			FFloatReadRef&& InCutoffFloat,
			TDataReadReference<TArray<float>>&& InCutoffArray,
			FChannelAgnosticTypeReadRef&& InBandwidthCAT,
			FAudioBufferReadRef&& InBandwidthMono,
			FFloatReadRef&& InBandwidthFloat,
			TDataReadReference<TArray<float>>&& InBandwidthArray,
			FFloatReadRef&& InGainDb,
			FEnumBiQuadFilterReadRef&& InFilterType,
			FChannelAgnosticTypeWriteRef&& InOutput)
			: OperatorData(InOperatorData)
			, Settings(InSettings)
			, Input(MoveTemp(InInput))
			, CutoffCAT(MoveTemp(InCutoffCAT))
			, CutoffMono(MoveTemp(InCutoffMono))
			, CutoffFloat(MoveTemp(InCutoffFloat))
			, CutoffArray(MoveTemp(InCutoffArray))
			, BandwidthCAT(MoveTemp(InBandwidthCAT))
			, BandwidthMono(MoveTemp(InBandwidthMono))
			, BandwidthFloat(MoveTemp(InBandwidthFloat))
			, BandwidthArray(MoveTemp(InBandwidthArray))
			, GainDb(MoveTemp(InGainDb))
			, FilterType(MoveTemp(InFilterType))
			, Output(MoveTemp(InOutput))
			, SampleRate(FMath::Max(1.0f, InSettings.GetSampleRate()))
			, MaxCutoffFrequency(0.5f * SampleRate)
			, PreviousFilterType(*FilterType)
		{
		}

		static const FVertexInterface& DeclareVertexInterface()
		{
			static const FVertexInterface Interface = CATBiquadARPrivate::GetVertexInterface(
				TEXT("Cat:Stereo2Dot0"),
				EMetaSoundCATBiquadARControlMode::Float,
				EMetaSoundCATBiquadARControlMode::Float);
			return Interface;
		}

		static FNodeClassMetadata GetNodeInfo()
		{
			FNodeClassMetadata Metadata;
			Metadata.ClassName = { TEXT("Experimental"), TEXT("CATBiquadAR"), TEXT("Audio") };
			Metadata.MajorVersion = 1;
			Metadata.MinorVersion = 1;
			Metadata.DisplayName = LOCTEXT("CATBiquadARDisplayName", "CAT Biquad (AR)");
			Metadata.Description = LOCTEXT("CATBiquadARDescription", "CAT biquad filter with configurable cutoff and bandwidth control pin types.");
			Metadata.Author = TEXT("Charles Matthews");
			Metadata.PromptIfMissing = LOCTEXT("CATBiquadARMissingPrompt", "Enable MetaSoundExperimental for CAT channel format schemas.");
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
			using namespace CATBiquadARVertexNames;

			const FCATBiquadAROperatorData* ConfigData = CastOperatorData<const FCATBiquadAROperatorData>(InParams.Node.GetOperatorData().Get());
			if (!ConfigData)
			{
				return MakeUnique<FNoOpOperator>();
			}

			const Audio::FChannelTypeFamily* ConcreteType = Audio::GetChannelRegistry().FindConcreteChannel(ConfigData->CatAudioTypeName);
			if (!ConcreteType)
			{
				return MakeUnique<FNoOpOperator>();
			}

			const TSharedPtr<const FCATBiquadAROperatorData>& OperatorDataSharedPtr = StaticCastSharedPtr<const FCATBiquadAROperatorData>(InParams.Node.GetOperatorData());

			FChannelAgnosticTypeReadRef InSignal = InParams.InputData.GetOrCreateDefaultDataReadReference<FChannelAgnosticType>(METASOUND_GET_PARAM_NAME(Input), InParams.OperatorSettings);

			FChannelAgnosticTypeReadRef InCutoffCAT = FChannelAgnosticTypeWriteRef::CreateNew(InParams.OperatorSettings, ConcreteType->GetName());
			FAudioBufferReadRef InCutoffMono = FAudioBufferWriteRef::CreateNew(InParams.OperatorSettings);
			FFloatReadRef InCutoffFloat = TDataReadReference<float>::CreateNew(20000.0f);
			TDataReadReference<TArray<float>> InCutoffArray = TDataReadReference<TArray<float>>::CreateNew(TArray<float>{});

			FChannelAgnosticTypeReadRef InBandwidthCAT = FChannelAgnosticTypeWriteRef::CreateNew(InParams.OperatorSettings, ConcreteType->GetName());
			FAudioBufferReadRef InBandwidthMono = FAudioBufferWriteRef::CreateNew(InParams.OperatorSettings);
			FFloatReadRef InBandwidthFloat = TDataReadReference<float>::CreateNew(1.89997f);
			TDataReadReference<TArray<float>> InBandwidthArray = TDataReadReference<TArray<float>>::CreateNew(TArray<float>{});

			FFloatReadRef InGain = InParams.InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(GainDb), InParams.OperatorSettings);
			FEnumBiQuadFilterReadRef InFilterType = InParams.InputData.GetOrCreateDefaultDataReadReference<FEnumEBiquadFilterType>(METASOUND_GET_PARAM_NAME(FilterType), InParams.OperatorSettings);

			switch (ConfigData->CutoffMode)
			{
			case EMetaSoundCATBiquadARControlMode::CAT:
				InCutoffCAT = InParams.InputData.GetOrCreateDefaultDataReadReference<FChannelAgnosticType>(METASOUND_GET_PARAM_NAME(Cutoff), InParams.OperatorSettings);
				break;
			case EMetaSoundCATBiquadARControlMode::MonoAudio:
				InCutoffMono = InParams.InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(Cutoff), InParams.OperatorSettings);
				break;
			case EMetaSoundCATBiquadARControlMode::Float:
				InCutoffFloat = InParams.InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(Cutoff), InParams.OperatorSettings);
				break;
			case EMetaSoundCATBiquadARControlMode::FloatArray:
				InCutoffArray = InParams.InputData.GetOrCreateDefaultDataReadReference<TArray<float>>(METASOUND_GET_PARAM_NAME(Cutoff), InParams.OperatorSettings);
				break;
			default:
				checkNoEntry();
				break;
			}

			switch (ConfigData->BandwidthMode)
			{
			case EMetaSoundCATBiquadARControlMode::CAT:
				InBandwidthCAT = InParams.InputData.GetOrCreateDefaultDataReadReference<FChannelAgnosticType>(METASOUND_GET_PARAM_NAME(Bandwidth), InParams.OperatorSettings);
				break;
			case EMetaSoundCATBiquadARControlMode::MonoAudio:
				InBandwidthMono = InParams.InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(Bandwidth), InParams.OperatorSettings);
				break;
			case EMetaSoundCATBiquadARControlMode::Float:
				InBandwidthFloat = InParams.InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(Bandwidth), InParams.OperatorSettings);
				break;
			case EMetaSoundCATBiquadARControlMode::FloatArray:
				InBandwidthArray = InParams.InputData.GetOrCreateDefaultDataReadReference<TArray<float>>(METASOUND_GET_PARAM_NAME(Bandwidth), InParams.OperatorSettings);
				break;
			default:
				checkNoEntry();
				break;
			}

			FChannelAgnosticTypeWriteRef Out = FChannelAgnosticTypeWriteRef::CreateNew(InParams.OperatorSettings, ConcreteType->GetName());

			return MakeUnique<FCATBiquadAROperator>(
				OperatorDataSharedPtr,
				InParams.OperatorSettings,
				MoveTemp(InSignal),
				MoveTemp(InCutoffCAT),
				MoveTemp(InCutoffMono),
				MoveTemp(InCutoffFloat),
				MoveTemp(InCutoffArray),
				MoveTemp(InBandwidthCAT),
				MoveTemp(InBandwidthMono),
				MoveTemp(InBandwidthFloat),
				MoveTemp(InBandwidthArray),
				MoveTemp(InGain),
				MoveTemp(InFilterType),
				MoveTemp(Out));
		}

		virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
		{
			using namespace CATBiquadARVertexNames;

			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Input), Input);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(GainDb), GainDb);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(FilterType), FilterType);

			switch (OperatorData->CutoffMode)
			{
			case EMetaSoundCATBiquadARControlMode::CAT:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Cutoff), CutoffCAT);
				break;
			case EMetaSoundCATBiquadARControlMode::MonoAudio:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Cutoff), CutoffMono);
				break;
			case EMetaSoundCATBiquadARControlMode::Float:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Cutoff), CutoffFloat);
				break;
			case EMetaSoundCATBiquadARControlMode::FloatArray:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Cutoff), CutoffArray);
				break;
			default:
				checkNoEntry();
				break;
			}

			switch (OperatorData->BandwidthMode)
			{
			case EMetaSoundCATBiquadARControlMode::CAT:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Bandwidth), BandwidthCAT);
				break;
			case EMetaSoundCATBiquadARControlMode::MonoAudio:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Bandwidth), BandwidthMono);
				break;
			case EMetaSoundCATBiquadARControlMode::Float:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Bandwidth), BandwidthFloat);
				break;
			case EMetaSoundCATBiquadARControlMode::FloatArray:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Bandwidth), BandwidthArray);
				break;
			default:
				checkNoEntry();
				break;
			}
		}

		virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
		{
			using namespace CATBiquadARVertexNames;
			InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(Output), Output);
		}

		virtual void Reset(const IOperator::FResetParams& InParams)
		{
			for (Audio::FBiquadFilter& Filter : Filters)
			{
				Filter.Init(SampleRate, 1, *FilterType);
			}
			PreviousFilterType = *FilterType;
		}

		void Execute()
		{
			const int32 NumFrames = Settings.GetNumFramesPerBlock();
			const int32 NumOutChannels = Output->NumChannels();
			if (NumFrames <= 0 || NumOutChannels <= 0)
			{
				return;
			}

			EnsureStateForChannels(NumOutChannels);

			if (*FilterType != PreviousFilterType)
			{
				for (Audio::FBiquadFilter& Filter : Filters)
				{
					Filter.SetType(*FilterType);
				}
				PreviousFilterType = *FilterType;
			}

			const int32 NumInChannels = FMath::Max(1, Input->NumChannels());
			const float CurrentGain = FMath::Clamp(*GainDb, -90.0f, 20.0f);

			for (int32 Channel = 0; Channel < NumOutChannels; ++Channel)
			{
				const int32 InChannel = (NumInChannels == 1) ? 0 : FMath::Min(Channel, NumInChannels - 1);
				TArrayView<const float> Src = Input->GetChannel(InChannel);
				TArrayView<float> Dst = Output->GetChannel(Channel);

				Audio::FBiquadFilter& Filter = Filters[Channel];
				Filter.SetGainDB(CurrentGain);

				for (int32 Frame = 0; Frame < NumFrames; ++Frame)
				{
					const float CutoffValue = CATBiquadARPrivate::GetControlValue(
						OperatorData->CutoffMode,
						Channel,
						Frame,
						CutoffCAT,
						CutoffMono,
						CutoffFloat,
						CutoffArray,
						20000.0f);

					const float BandwidthValue = CATBiquadARPrivate::GetControlValue(
						OperatorData->BandwidthMode,
						Channel,
						Frame,
						BandwidthCAT,
						BandwidthMono,
						BandwidthFloat,
						BandwidthArray,
						1.89997f);

					Filter.SetFrequency(FMath::Clamp(CutoffValue, 0.0f, MaxCutoffFrequency));
					Filter.SetBandwidth(FMath::Max(0.001f, BandwidthValue));

					float OutSample = 0.0f;
					Filter.ProcessAudio(&Src[Frame], 1, &OutSample);
					Dst[Frame] = OutSample;
				}
			}
		}

	private:
		void EnsureStateForChannels(const int32 InNumChannels)
		{
			if (Filters.Num() == InNumChannels)
			{
				return;
			}

			Filters.SetNum(InNumChannels);
			for (Audio::FBiquadFilter& Filter : Filters)
			{
				Filter.Init(SampleRate, 1, *FilterType);
			}
			PreviousFilterType = *FilterType;
		}

		TSharedPtr<const FCATBiquadAROperatorData> OperatorData;
		FOperatorSettings Settings;
		FChannelAgnosticTypeReadRef Input;

		FChannelAgnosticTypeReadRef CutoffCAT;
		FAudioBufferReadRef CutoffMono;
		FFloatReadRef CutoffFloat;
		TDataReadReference<TArray<float>> CutoffArray;

		FChannelAgnosticTypeReadRef BandwidthCAT;
		FAudioBufferReadRef BandwidthMono;
		FFloatReadRef BandwidthFloat;
		TDataReadReference<TArray<float>> BandwidthArray;

		FFloatReadRef GainDb;
		FEnumBiQuadFilterReadRef FilterType;
		FChannelAgnosticTypeWriteRef Output;
		float SampleRate = 48000.0f;
		float MaxCutoffFrequency = 24000.0f;
		Audio::EBiquadFilter::Type PreviousFilterType = Audio::EBiquadFilter::Lowpass;
		TArray<Audio::FBiquadFilter> Filters;
	};

	using FCATBiquadARNode = TNodeFacade<FCATBiquadAROperator>;
	METASOUND_REGISTER_NODE_AND_CONFIGURATION(FCATBiquadARNode, FMetaSoundCATBiquadARNodeConfiguration);
}

TArray<FPropertyTextFName> UMetaSoundCATBiquadARNodeOptionsHelper::GetSoundFileFormatChannelOptions()
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

FMetaSoundCATBiquadARNodeConfiguration::FMetaSoundCATBiquadARNodeConfiguration()
	: OperatorData(MakeShared<Metasound::CATBiquadARPrivate::FCATBiquadAROperatorData>(CatAudioTypeName, CutoffMode, BandwidthMode))
{
}

TInstancedStruct<FMetasoundFrontendClassInterface> FMetaSoundCATBiquadARNodeConfiguration::OverrideDefaultInterface(const FMetasoundFrontendClass& InClass) const
{
	return TInstancedStruct<FMetasoundFrontendClassInterface>::Make(
		FMetasoundFrontendClassInterface::GenerateClassInterface(
			Metasound::CATBiquadARPrivate::GetVertexInterface(CatAudioTypeName, CutoffMode, BandwidthMode)));
}

TSharedPtr<const Metasound::IOperatorData> FMetaSoundCATBiquadARNodeConfiguration::GetOperatorData() const
{
	OperatorData->CatAudioTypeName = CatAudioTypeName;
	OperatorData->CutoffMode = CutoffMode;
	OperatorData->BandwidthMode = BandwidthMode;
	return OperatorData;
}

#undef LOCTEXT_NAMESPACE
