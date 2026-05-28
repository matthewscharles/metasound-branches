// Copyright 2026 Charles Matthews. All Rights Reserved.

#include "MetasoundBranchesCAT/Public/MetasoundCATLadderARNode.h"

#include "DSP/Filter.h"
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

#define LOCTEXT_NAMESPACE "MetasoundBranches_CATLadderARNode"

namespace Metasound
{
	namespace CATLadderARVertexNames
	{
		METASOUND_PARAM(Input, "Input", "CAT signal to filter.")
		METASOUND_PARAM(Cutoff, "Cutoff", "Filter cutoff control.")
		METASOUND_PARAM(Resonance, "Resonance", "Filter resonance (Q) control.")
		METASOUND_PARAM(Output, "Output", "Filtered CAT output.")
	}

	namespace CATLadderARPrivate
	{
		class FCATLadderAROperatorData final : public TOperatorData<FCATLadderAROperatorData>
		{
		public:
			static const FLazyName OperatorDataTypeName;

			explicit FCATLadderAROperatorData(
				const FName& InCatAudioTypeName,
				const EMetaSoundCATLadderARControlMode InCutoffMode,
				const EMetaSoundCATLadderARControlMode InResonanceMode)
				: CatAudioTypeName(InCatAudioTypeName)
				, CutoffMode(InCutoffMode)
				, ResonanceMode(InResonanceMode)
			{
			}

			FName CatAudioTypeName;
			EMetaSoundCATLadderARControlMode CutoffMode;
			EMetaSoundCATLadderARControlMode ResonanceMode;
		};

		const FLazyName FCATLadderAROperatorData::OperatorDataTypeName = TEXT("FCATLadderAROperatorData");

		void AddControlVertex(
			FInputVertexInterface& InOutInput,
			const FName& InVertexName,
			const FDataVertexMetadata& InMetadata,
			const FName& InCatFormat,
			const EMetaSoundCATLadderARControlMode InMode,
			const float InDefaultFloat)
		{
			switch (InMode)
			{
			case EMetaSoundCATLadderARControlMode::CAT:
				InOutInput.Add(FInputDataVertex(InVertexName, InCatFormat, InMetadata, EVertexAccessType::Reference));
				break;
			case EMetaSoundCATLadderARControlMode::MonoAudio:
				InOutInput.Add(TInputDataVertex<FAudioBuffer>(InVertexName, InMetadata));
				break;
			case EMetaSoundCATLadderARControlMode::Float:
				InOutInput.Add(TInputDataVertex<float>(InVertexName, InMetadata, InDefaultFloat));
				break;
			case EMetaSoundCATLadderARControlMode::FloatArray:
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
			const EMetaSoundCATLadderARControlMode InMode,
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
			case EMetaSoundCATLadderARControlMode::CAT:
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
			case EMetaSoundCATLadderARControlMode::MonoAudio:
			{
				const int32 NumSamples = InMono->Num();
				if (NumSamples <= 0)
				{
					return InDefault;
				}
				return InMono->GetData()[FMath::Min(InFrame, NumSamples - 1)];
			}
			case EMetaSoundCATLadderARControlMode::Float:
				return *InFloat;
			case EMetaSoundCATLadderARControlMode::FloatArray:
				return GetArrayValue(*InFloatArray, InChannel, InDefault);
			default:
				checkNoEntry();
				return InDefault;
			}
		}

		FVertexInterface GetVertexInterface(
			const FName& InCatFormat,
			const EMetaSoundCATLadderARControlMode InCutoffMode,
			const EMetaSoundCATLadderARControlMode InResonanceMode)
		{
			using namespace CATLadderARVertexNames;

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
				METASOUND_GET_PARAM_NAME(Resonance),
				METASOUND_GET_PARAM_METADATA(Resonance),
				InCatFormat,
				InResonanceMode,
				1.0f);

			FOutputVertexInterface OutputInterface;
			OutputInterface.Add(FOutputDataVertex(METASOUND_GET_PARAM_NAME(Output), InCatFormat, METASOUND_GET_PARAM_METADATA(Output), EVertexAccessType::Reference));

			return { MoveTemp(InputInterface), MoveTemp(OutputInterface) };
		}
	}

	class FCATLadderAROperator final : public TExecutableOperator<FCATLadderAROperator>
	{
	public:
		using FCATLadderAROperatorData = CATLadderARPrivate::FCATLadderAROperatorData;

		FCATLadderAROperator(
			const TSharedPtr<const FCATLadderAROperatorData>& InOperatorData,
			const FOperatorSettings& InSettings,
			FChannelAgnosticTypeReadRef&& InInput,
			FChannelAgnosticTypeReadRef&& InCutoffCAT,
			FAudioBufferReadRef&& InCutoffMono,
			FFloatReadRef&& InCutoffFloat,
			TDataReadReference<TArray<float>>&& InCutoffArray,
			FChannelAgnosticTypeReadRef&& InResonanceCAT,
			FAudioBufferReadRef&& InResonanceMono,
			FFloatReadRef&& InResonanceFloat,
			TDataReadReference<TArray<float>>&& InResonanceArray,
			FChannelAgnosticTypeWriteRef&& InOutput)
			: OperatorData(InOperatorData)
			, Settings(InSettings)
			, Input(MoveTemp(InInput))
			, CutoffCAT(MoveTemp(InCutoffCAT))
			, CutoffMono(MoveTemp(InCutoffMono))
			, CutoffFloat(MoveTemp(InCutoffFloat))
			, CutoffArray(MoveTemp(InCutoffArray))
			, ResonanceCAT(MoveTemp(InResonanceCAT))
			, ResonanceMono(MoveTemp(InResonanceMono))
			, ResonanceFloat(MoveTemp(InResonanceFloat))
			, ResonanceArray(MoveTemp(InResonanceArray))
			, Output(MoveTemp(InOutput))
			, SampleRate(FMath::Max(1.0f, InSettings.GetSampleRate()))
			, MaxCutoffFrequency(0.5f * SampleRate)
		{
		}

		static const FVertexInterface& DeclareVertexInterface()
		{
			static const FVertexInterface Interface = CATLadderARPrivate::GetVertexInterface(
				TEXT("Cat:Stereo2Dot0"),
				EMetaSoundCATLadderARControlMode::Float,
				EMetaSoundCATLadderARControlMode::Float);
			return Interface;
		}

		static FNodeClassMetadata GetNodeInfo()
		{
			FNodeClassMetadata Metadata;
			Metadata.ClassName = { TEXT("Experimental"), TEXT("CATLadderAR"), TEXT("Audio") };
			Metadata.MajorVersion = 1;
			Metadata.MinorVersion = 1;
			Metadata.DisplayName = LOCTEXT("CATLadderARDisplayName", "CAT Ladder (AR)");
			Metadata.Description = LOCTEXT("CATLadderARDescription", "CAT ladder filter with configurable cutoff and resonance control pin types.");
			Metadata.Author = TEXT("Charles Matthews");
			Metadata.PromptIfMissing = LOCTEXT("CATLadderARMissingPrompt", "Enable MetaSoundExperimental for CAT channel format schemas.");
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
			using namespace CATLadderARVertexNames;

			const FCATLadderAROperatorData* ConfigData = CastOperatorData<const FCATLadderAROperatorData>(InParams.Node.GetOperatorData().Get());
			if (!ConfigData)
			{
				return MakeUnique<FNoOpOperator>();
			}

			const Audio::FChannelTypeFamily* ConcreteType = Audio::GetChannelRegistry().FindConcreteChannel(ConfigData->CatAudioTypeName);
			if (!ConcreteType)
			{
				return MakeUnique<FNoOpOperator>();
			}

			const TSharedPtr<const FCATLadderAROperatorData>& OperatorDataSharedPtr = StaticCastSharedPtr<const FCATLadderAROperatorData>(InParams.Node.GetOperatorData());

			FChannelAgnosticTypeReadRef InSignal = InParams.InputData.GetOrCreateDefaultDataReadReference<FChannelAgnosticType>(METASOUND_GET_PARAM_NAME(Input), InParams.OperatorSettings);

			FChannelAgnosticTypeReadRef InCutoffCAT = FChannelAgnosticTypeWriteRef::CreateNew(InParams.OperatorSettings, ConcreteType->GetName());
			FAudioBufferReadRef InCutoffMono = FAudioBufferWriteRef::CreateNew(InParams.OperatorSettings);
			FFloatReadRef InCutoffFloat = TDataReadReference<float>::CreateNew(20000.0f);
			TDataReadReference<TArray<float>> InCutoffArray = TDataReadReference<TArray<float>>::CreateNew(TArray<float>{});

			FChannelAgnosticTypeReadRef InResonanceCAT = FChannelAgnosticTypeWriteRef::CreateNew(InParams.OperatorSettings, ConcreteType->GetName());
			FAudioBufferReadRef InResonanceMono = FAudioBufferWriteRef::CreateNew(InParams.OperatorSettings);
			FFloatReadRef InResonanceFloat = TDataReadReference<float>::CreateNew(1.0f);
			TDataReadReference<TArray<float>> InResonanceArray = TDataReadReference<TArray<float>>::CreateNew(TArray<float>{});

			switch (ConfigData->CutoffMode)
			{
			case EMetaSoundCATLadderARControlMode::CAT:
				InCutoffCAT = InParams.InputData.GetOrCreateDefaultDataReadReference<FChannelAgnosticType>(METASOUND_GET_PARAM_NAME(Cutoff), InParams.OperatorSettings);
				break;
			case EMetaSoundCATLadderARControlMode::MonoAudio:
				InCutoffMono = InParams.InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(Cutoff), InParams.OperatorSettings);
				break;
			case EMetaSoundCATLadderARControlMode::Float:
				InCutoffFloat = InParams.InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(Cutoff), InParams.OperatorSettings);
				break;
			case EMetaSoundCATLadderARControlMode::FloatArray:
				InCutoffArray = InParams.InputData.GetOrCreateDefaultDataReadReference<TArray<float>>(METASOUND_GET_PARAM_NAME(Cutoff), InParams.OperatorSettings);
				break;
			default:
				checkNoEntry();
				break;
			}

			switch (ConfigData->ResonanceMode)
			{
			case EMetaSoundCATLadderARControlMode::CAT:
				InResonanceCAT = InParams.InputData.GetOrCreateDefaultDataReadReference<FChannelAgnosticType>(METASOUND_GET_PARAM_NAME(Resonance), InParams.OperatorSettings);
				break;
			case EMetaSoundCATLadderARControlMode::MonoAudio:
				InResonanceMono = InParams.InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(Resonance), InParams.OperatorSettings);
				break;
			case EMetaSoundCATLadderARControlMode::Float:
				InResonanceFloat = InParams.InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(Resonance), InParams.OperatorSettings);
				break;
			case EMetaSoundCATLadderARControlMode::FloatArray:
				InResonanceArray = InParams.InputData.GetOrCreateDefaultDataReadReference<TArray<float>>(METASOUND_GET_PARAM_NAME(Resonance), InParams.OperatorSettings);
				break;
			default:
				checkNoEntry();
				break;
			}

			FChannelAgnosticTypeWriteRef Out = FChannelAgnosticTypeWriteRef::CreateNew(InParams.OperatorSettings, ConcreteType->GetName());

			return MakeUnique<FCATLadderAROperator>(
				OperatorDataSharedPtr,
				InParams.OperatorSettings,
				MoveTemp(InSignal),
				MoveTemp(InCutoffCAT),
				MoveTemp(InCutoffMono),
				MoveTemp(InCutoffFloat),
				MoveTemp(InCutoffArray),
				MoveTemp(InResonanceCAT),
				MoveTemp(InResonanceMono),
				MoveTemp(InResonanceFloat),
				MoveTemp(InResonanceArray),
				MoveTemp(Out));
		}

		virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
		{
			using namespace CATLadderARVertexNames;

			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Input), Input);

			switch (OperatorData->CutoffMode)
			{
			case EMetaSoundCATLadderARControlMode::CAT:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Cutoff), CutoffCAT);
				break;
			case EMetaSoundCATLadderARControlMode::MonoAudio:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Cutoff), CutoffMono);
				break;
			case EMetaSoundCATLadderARControlMode::Float:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Cutoff), CutoffFloat);
				break;
			case EMetaSoundCATLadderARControlMode::FloatArray:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Cutoff), CutoffArray);
				break;
			default:
				checkNoEntry();
				break;
			}

			switch (OperatorData->ResonanceMode)
			{
			case EMetaSoundCATLadderARControlMode::CAT:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Resonance), ResonanceCAT);
				break;
			case EMetaSoundCATLadderARControlMode::MonoAudio:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Resonance), ResonanceMono);
				break;
			case EMetaSoundCATLadderARControlMode::Float:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Resonance), ResonanceFloat);
				break;
			case EMetaSoundCATLadderARControlMode::FloatArray:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Resonance), ResonanceArray);
				break;
			default:
				checkNoEntry();
				break;
			}
		}

		virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
		{
			using namespace CATLadderARVertexNames;
			InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(Output), Output);
		}

		virtual void Reset(const IOperator::FResetParams& InParams)
		{
			for (Audio::FLadderFilter& Filter : Filters)
			{
				Filter.Init(SampleRate, 1);
			}
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

			const int32 NumInChannels = FMath::Max(1, Input->NumChannels());

			for (int32 Channel = 0; Channel < NumOutChannels; ++Channel)
			{
				const int32 InChannel = (NumInChannels == 1) ? 0 : FMath::Min(Channel, NumInChannels - 1);
				TArrayView<const float> Src = Input->GetChannel(InChannel);
				TArrayView<float> Dst = Output->GetChannel(Channel);

				Audio::FLadderFilter& Filter = Filters[Channel];

				for (int32 Frame = 0; Frame < NumFrames; ++Frame)
				{
					const float CutoffValue = CATLadderARPrivate::GetControlValue(
						OperatorData->CutoffMode,
						Channel,
						Frame,
						CutoffCAT,
						CutoffMono,
						CutoffFloat,
						CutoffArray,
						20000.0f);

					const float ResonanceValue = CATLadderARPrivate::GetControlValue(
						OperatorData->ResonanceMode,
						Channel,
						Frame,
						ResonanceCAT,
						ResonanceMono,
						ResonanceFloat,
						ResonanceArray,
						1.0f);

					Filter.SetFrequency(FMath::Clamp(CutoffValue, 0.0f, MaxCutoffFrequency));
					Filter.SetQ(FMath::Clamp(ResonanceValue, 1.0f, 10.0f));
					Filter.Update();

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
			for (Audio::FLadderFilter& Filter : Filters)
			{
				Filter.Init(SampleRate, 1);
			}
		}

		TSharedPtr<const FCATLadderAROperatorData> OperatorData;
		FOperatorSettings Settings;
		FChannelAgnosticTypeReadRef Input;

		FChannelAgnosticTypeReadRef CutoffCAT;
		FAudioBufferReadRef CutoffMono;
		FFloatReadRef CutoffFloat;
		TDataReadReference<TArray<float>> CutoffArray;

		FChannelAgnosticTypeReadRef ResonanceCAT;
		FAudioBufferReadRef ResonanceMono;
		FFloatReadRef ResonanceFloat;
		TDataReadReference<TArray<float>> ResonanceArray;

		FChannelAgnosticTypeWriteRef Output;
		float SampleRate = 48000.0f;
		float MaxCutoffFrequency = 24000.0f;
		TArray<Audio::FLadderFilter> Filters;
	};

	using FCATLadderARNode = TNodeFacade<FCATLadderAROperator>;
	METASOUND_REGISTER_NODE_AND_CONFIGURATION(FCATLadderARNode, FMetaSoundCATLadderARNodeConfiguration);
}

TArray<FPropertyTextFName> UMetaSoundCATLadderARNodeOptionsHelper::GetSoundFileFormatChannelOptions()
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

FMetaSoundCATLadderARNodeConfiguration::FMetaSoundCATLadderARNodeConfiguration()
	: OperatorData(MakeShared<Metasound::CATLadderARPrivate::FCATLadderAROperatorData>(CatAudioTypeName, CutoffMode, ResonanceMode))
{
}

TInstancedStruct<FMetasoundFrontendClassInterface> FMetaSoundCATLadderARNodeConfiguration::OverrideDefaultInterface(const FMetasoundFrontendClass& InClass) const
{
	return TInstancedStruct<FMetasoundFrontendClassInterface>::Make(
		FMetasoundFrontendClassInterface::GenerateClassInterface(
			Metasound::CATLadderARPrivate::GetVertexInterface(CatAudioTypeName, CutoffMode, ResonanceMode)));
}

TSharedPtr<const Metasound::IOperatorData> FMetaSoundCATLadderARNodeConfiguration::GetOperatorData() const
{
	OperatorData->CatAudioTypeName = CatAudioTypeName;
	OperatorData->CutoffMode = CutoffMode;
	OperatorData->ResonanceMode = ResonanceMode;
	return OperatorData;
}

#undef LOCTEXT_NAMESPACE
