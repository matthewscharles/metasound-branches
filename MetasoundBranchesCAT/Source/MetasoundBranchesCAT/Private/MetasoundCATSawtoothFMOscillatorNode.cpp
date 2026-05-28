// Copyright 2026 Charles Matthews. All Rights Reserved.

#include "MetasoundBranchesCAT/Public/MetasoundCATSawtoothFMOscillatorNode.h"

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

#define LOCTEXT_NAMESPACE "MetasoundBranches_CATSawtoothFMOscillatorNode"

namespace Metasound
{
	namespace CATSawtoothFMOscillatorVertexNames
	{
		METASOUND_PARAM(InputEnabled, "Enabled", "Enable or disable function output.")
		METASOUND_PARAM(InputBipolar, "Bi Polar", "Output bipolar signal if true, unipolar if false.")
		METASOUND_PARAM(InputPhase, "Phase", "Audio-rate CAT phase modulation input.")
		METASOUND_PARAM(InputFeedbackFloat, "Feedback Amount", "Feedback intensity.")
		METASOUND_PARAM(InputFeedbackAudio, "Feedback Modulation", "Audio-rate CAT feedback modulation.")
		METASOUND_PARAM(InputReset, "Reset", "Reset feedback state when triggered.")
		METASOUND_PARAM(OutputAudio, "Output", "Generated CAT function output.")
	}

	namespace CATSawtoothFMOscillatorPrivate
	{
		class FCATSawtoothFMOscillatorOperatorData final : public TOperatorData<FCATSawtoothFMOscillatorOperatorData>
		{
		public:
			static const FLazyName OperatorDataTypeName;

			explicit FCATSawtoothFMOscillatorOperatorData(const FName& InCatAudioTypeName)
				: CatAudioTypeName(InCatAudioTypeName)
			{
			}

			FName CatAudioTypeName;
		};

		const FLazyName FCATSawtoothFMOscillatorOperatorData::OperatorDataTypeName = TEXT("FCATSawtoothFMOscillatorOperatorData");

		FVertexInterface GetVertexInterface(const FName& InCatFormat)
		{
			using namespace CATSawtoothFMOscillatorVertexNames;

			FInputVertexInterface InputInterface(
				TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputEnabled), true),
				TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputBipolar), true),
				FInputDataVertex(METASOUND_GET_PARAM_NAME(InputPhase), InCatFormat, METASOUND_GET_PARAM_METADATA(InputPhase), EVertexAccessType::Reference),
				TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputFeedbackFloat), 0.0f),
				FInputDataVertex(METASOUND_GET_PARAM_NAME(InputFeedbackAudio), InCatFormat, METASOUND_GET_PARAM_METADATA(InputFeedbackAudio), EVertexAccessType::Reference),
				TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputReset)));

			FOutputVertexInterface OutputInterface;
			OutputInterface.Add(FOutputDataVertex(METASOUND_GET_PARAM_NAME(OutputAudio), InCatFormat, METASOUND_GET_PARAM_METADATA(OutputAudio), EVertexAccessType::Reference));

			return { MoveTemp(InputInterface), MoveTemp(OutputInterface) };
		}
	}

	class FCATSawtoothFMOscillatorOperator final : public TExecutableOperator<FCATSawtoothFMOscillatorOperator>
	{
	public:
		using FCATSawtoothFMOscillatorOperatorData = CATSawtoothFMOscillatorPrivate::FCATSawtoothFMOscillatorOperatorData;

		FCATSawtoothFMOscillatorOperator(
			const FOperatorSettings& InSettings,
			const TSharedPtr<const FCATSawtoothFMOscillatorOperatorData>& InOperatorData,
			const FBoolReadRef& InEnabled,
			const FBoolReadRef& InBipolar,
			const FChannelAgnosticTypeReadRef& InPhase,
			const FFloatReadRef& InFeedbackFloat,
			const FChannelAgnosticTypeReadRef& InFeedbackAudio,
			const FTriggerReadRef& InReset,
			const FChannelAgnosticTypeWriteRef& InOutput)
			: Settings(InSettings)
			, OperatorData(InOperatorData)
			, InputEnabled(InEnabled)
			, InputBipolar(InBipolar)
			, InputPhase(InPhase)
			, InputFeedbackFloat(InFeedbackFloat)
			, InputFeedbackAudio(InFeedbackAudio)
			, InputReset(InReset)
			, OutputAudio(InOutput)
		{
		}

		static const FVertexInterface& DeclareVertexInterface()
		{
			static const FVertexInterface Interface = CATSawtoothFMOscillatorPrivate::GetVertexInterface(TEXT("Cat:Stereo2Dot0"));
			return Interface;
		}

		static FNodeClassMetadata GetNodeInfo()
		{
			FNodeClassMetadata Metadata;
			Metadata.ClassName = { TEXT("Experimental"), TEXT("CATSawtoothFMOscillator"), TEXT("Audio") };
			Metadata.MajorVersion = 1;
			Metadata.MinorVersion = 0;
			Metadata.DisplayName = LOCTEXT("CATSawtoothFMOscillatorDisplayName", "CAT Function (Sawtooth)");
			Metadata.Description = LOCTEXT("CATSawtoothFMOscillatorDescription", "Generate CAT sawtooth function output with audio-rate CAT phase control and feedback.");
			Metadata.Author = TEXT("Charles Matthews");
			Metadata.PromptIfMissing = LOCTEXT("CATSawtoothFMOscillatorMissingPrompt", "Enable MetaSoundExperimental for CAT channel format schemas.");
			Metadata.CategoryHierarchy = {
				METASOUND_LOCTEXT("Custom", "Branches"),
				METASOUND_LOCTEXT("CustomSub", "CAT")
			};
			Metadata.DefaultInterface = DeclareVertexInterface();
			METASOUND_BRANCHES_APPLY_CAT_NODE_STYLE(Metadata);
			return Metadata;
		}

		METASOUND_DISABLE_LEGACY_IO()

		virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
		{
			using namespace CATSawtoothFMOscillatorVertexNames;
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputEnabled), InputEnabled);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputBipolar), InputBipolar);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputPhase), InputPhase);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputFeedbackFloat), InputFeedbackFloat);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputFeedbackAudio), InputFeedbackAudio);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputReset), InputReset);
		}

		virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
		{
			using namespace CATSawtoothFMOscillatorVertexNames;
			InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputAudio), OutputAudio);
		}

		static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
		{
			using namespace CATSawtoothFMOscillatorVertexNames;

			const FCATSawtoothFMOscillatorOperatorData* ConfigData = CastOperatorData<const FCATSawtoothFMOscillatorOperatorData>(InParams.Node.GetOperatorData().Get());
			if (!ConfigData)
			{
				return MakeUnique<FNoOpOperator>();
			}

			const Audio::FChannelTypeFamily* ConcreteType = Audio::GetChannelRegistry().FindConcreteChannel(ConfigData->CatAudioTypeName);
			if (!ConcreteType)
			{
				return MakeUnique<FNoOpOperator>();
			}

			const TSharedPtr<const FCATSawtoothFMOscillatorOperatorData>& OperatorDataSharedPtr = StaticCastSharedPtr<const FCATSawtoothFMOscillatorOperatorData>(InParams.Node.GetOperatorData());

			TDataReadReference<bool> InEnabled = InParams.InputData.GetOrCreateDefaultDataReadReference<bool>(METASOUND_GET_PARAM_NAME(InputEnabled), InParams.OperatorSettings);
			TDataReadReference<bool> InBipolar = InParams.InputData.GetOrCreateDefaultDataReadReference<bool>(METASOUND_GET_PARAM_NAME(InputBipolar), InParams.OperatorSettings);
			FChannelAgnosticTypeReadRef InPhase = InParams.InputData.GetOrCreateDefaultDataReadReference<FChannelAgnosticType>(METASOUND_GET_PARAM_NAME(InputPhase), InParams.OperatorSettings);
			TDataReadReference<float> InFeedbackFloat = InParams.InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputFeedbackFloat), InParams.OperatorSettings);
			FChannelAgnosticTypeReadRef InFeedbackAudio = InParams.InputData.GetOrCreateDefaultDataReadReference<FChannelAgnosticType>(METASOUND_GET_PARAM_NAME(InputFeedbackAudio), InParams.OperatorSettings);
			FTriggerReadRef InReset = InParams.InputData.GetOrCreateDefaultDataReadReference<FTrigger>(METASOUND_GET_PARAM_NAME(InputReset), InParams.OperatorSettings);
			FChannelAgnosticTypeWriteRef Out = FChannelAgnosticTypeWriteRef::CreateNew(InParams.OperatorSettings, ConcreteType->GetName());

			return MakeUnique<FCATSawtoothFMOscillatorOperator>(
				InParams.OperatorSettings,
				OperatorDataSharedPtr,
				InEnabled,
				InBipolar,
				InPhase,
				InFeedbackFloat,
				InFeedbackAudio,
				InReset,
				Out);
		}

		void Execute()
		{
			OutputAudio->Zero();

			const int32 NumFrames = Settings.GetNumFramesPerBlock();
			const int32 NumOutChannels = OutputAudio->NumChannels();
			if (NumOutChannels <= 0 || NumFrames <= 0)
			{
				return;
			}

			const int32 NumPhaseChannels = InputPhase->NumChannels();
			const int32 NumFeedbackChannels = InputFeedbackAudio->NumChannels();
			if (NumPhaseChannels <= 0 || NumFeedbackChannels <= 0)
			{
				return;
			}

			if (FeedbackStatePerChannel.Num() != NumOutChannels)
			{
				FeedbackStatePerChannel.Init(0.0f, NumOutChannels);
			}

			TArray<bool> bResetFrames;
			bResetFrames.Init(false, NumFrames);
			InputReset->ExecuteBlock(
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

			const float FeedbackAmount = *InputFeedbackFloat;
			const bool bEnabled = *InputEnabled;
			const bool bBipolar = *InputBipolar;

			for (int32 Channel = 0; Channel < NumOutChannels; ++Channel)
			{
				const int32 PhaseChannel = (NumPhaseChannels == 1) ? 0 : FMath::Min(Channel, NumPhaseChannels - 1);
				const int32 FeedbackChannel = (NumFeedbackChannels == 1) ? 0 : FMath::Min(Channel, NumFeedbackChannels - 1);
				TArrayView<const float> PhaseData = InputPhase->GetChannel(PhaseChannel);
				TArrayView<const float> FeedbackAudioData = InputFeedbackAudio->GetChannel(FeedbackChannel);
				TArrayView<float> OutData = OutputAudio->GetChannel(Channel);
				float& FeedbackState = FeedbackStatePerChannel[Channel];
				if (PhaseData.Num() <= 0 || FeedbackAudioData.Num() <= 0)
				{
					continue;
				}

				for (int32 Frame = 0; Frame < NumFrames; ++Frame)
				{
					if (bResetFrames[Frame])
					{
						FeedbackState = 0.0f;
					}

					const int32 PhaseIndex = FMath::Min(Frame, PhaseData.Num() - 1);
					const int32 FeedbackIndex = FMath::Min(Frame, FeedbackAudioData.Num() - 1);
					const float FeedbackSignal = FeedbackAmount + FeedbackAudioData[FeedbackIndex];
					const float FeedbackPhase = PhaseData[PhaseIndex] + (FeedbackSignal * FeedbackState);
					const float WrappedPhase = FeedbackPhase - FMath::Floor(FeedbackPhase);
					const float SawValue = bBipolar ? (2.0f * WrappedPhase - 1.0f) : WrappedPhase;

					FeedbackState = SawValue;
					OutData[Frame] = bEnabled ? SawValue : 0.0f;
				}
			}
		}

	private:
		FOperatorSettings Settings;
		TSharedPtr<const FCATSawtoothFMOscillatorOperatorData> OperatorData;
		FBoolReadRef InputEnabled;
		FBoolReadRef InputBipolar;
		FChannelAgnosticTypeReadRef InputPhase;
		FFloatReadRef InputFeedbackFloat;
		FChannelAgnosticTypeReadRef InputFeedbackAudio;
		FTriggerReadRef InputReset;
		FChannelAgnosticTypeWriteRef OutputAudio;
		TArray<float> FeedbackStatePerChannel;
	};

	using FCATSawtoothFMOscillatorNode = TNodeFacade<FCATSawtoothFMOscillatorOperator>;
	METASOUND_REGISTER_NODE_AND_CONFIGURATION(FCATSawtoothFMOscillatorNode, FMetaSoundCATSawtoothFMOscillatorNodeConfiguration);
}

TArray<FPropertyTextFName> UMetaSoundCATSawtoothFMOscillatorNodeOptionsHelper::GetSoundFileFormatChannelOptions()
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

FMetaSoundCATSawtoothFMOscillatorNodeConfiguration::FMetaSoundCATSawtoothFMOscillatorNodeConfiguration()
	: OperatorData(MakeShared<Metasound::CATSawtoothFMOscillatorPrivate::FCATSawtoothFMOscillatorOperatorData>(CatAudioTypeName))
{
}

TInstancedStruct<FMetasoundFrontendClassInterface> FMetaSoundCATSawtoothFMOscillatorNodeConfiguration::OverrideDefaultInterface(const FMetasoundFrontendClass& InClass) const
{
	return TInstancedStruct<FMetasoundFrontendClassInterface>::Make(
		FMetasoundFrontendClassInterface::GenerateClassInterface(
			Metasound::CATSawtoothFMOscillatorPrivate::GetVertexInterface(CatAudioTypeName)));
}

TSharedPtr<const Metasound::IOperatorData> FMetaSoundCATSawtoothFMOscillatorNodeConfiguration::GetOperatorData() const
{
	OperatorData->CatAudioTypeName = CatAudioTypeName;
	return OperatorData;
}

#undef LOCTEXT_NAMESPACE
