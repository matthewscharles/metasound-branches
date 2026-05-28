// Copyright 2026 Charles Matthews. All Rights Reserved.

#include "MetasoundBranchesCAT/Public/MetasoundCATSlewNode.h"

#include "MetasoundAudioBuffer.h"
#include "MetasoundBranches/Public/MetasoundCommonMacros.h"
#include "MetasoundChannelAgnosticType.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundParamHelper.h"
#include "MetasoundPrimitives.h"
#include "MetasoundVertex.h"
#include "Math/UnrealMathUtility.h"
#include "TypeFamily/ChannelTypeFamily.h"

#define LOCTEXT_NAMESPACE "MetasoundBranches_CATSlewNode"

namespace Metasound
{
	namespace CATSlewVertexNames
	{
		METASOUND_PARAM(Input, "Input", "Input signal to slew.")
		METASOUND_PARAM(RiseTime, "Rise Time", "Rise time in seconds.")
		METASOUND_PARAM(FallTime, "Fall Time", "Fall time in seconds.")
		METASOUND_PARAM(Output, "Output", "Slewed CAT output.")
	}

	namespace CATSlewPrivate
	{
		class FCATSlewOperatorData final : public TOperatorData<FCATSlewOperatorData>
		{
		public:
			static const FLazyName OperatorDataTypeName;

			explicit FCATSlewOperatorData(const FName& InCatAudioTypeName, const EMetaSoundCATSlewInputMode InInputMode)
				: CatAudioTypeName(InCatAudioTypeName)
				, InputMode(InInputMode)
			{
			}

			FName CatAudioTypeName;
			EMetaSoundCATSlewInputMode InputMode;
		};

		const FLazyName FCATSlewOperatorData::OperatorDataTypeName = TEXT("FCATSlewOperatorData");

		FVertexInterface GetVertexInterface(const FName& InCatFormat, const EMetaSoundCATSlewInputMode InInputMode)
		{
			using namespace CATSlewVertexNames;

			FInputVertexInterface InputInterface;
			switch (InInputMode)
			{
			case EMetaSoundCATSlewInputMode::CAT:
				InputInterface.Add(FInputDataVertex(METASOUND_GET_PARAM_NAME(Input), InCatFormat, METASOUND_GET_PARAM_METADATA(Input), EVertexAccessType::Reference));
				break;
			case EMetaSoundCATSlewInputMode::MonoAudio:
				InputInterface.Add(TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(Input)));
				break;
			default:
				checkNoEntry();
				break;
			}

			InputInterface.Add(TInputDataVertex<FTime>(METASOUND_GET_PARAM_NAME_AND_METADATA(RiseTime)));
			InputInterface.Add(TInputDataVertex<FTime>(METASOUND_GET_PARAM_NAME_AND_METADATA(FallTime)));

			FOutputVertexInterface OutputInterface;
			OutputInterface.Add(FOutputDataVertex(METASOUND_GET_PARAM_NAME(Output), InCatFormat, METASOUND_GET_PARAM_METADATA(Output), EVertexAccessType::Reference));

			return { MoveTemp(InputInterface), MoveTemp(OutputInterface) };
		}
	}

	class FCATSlewOperator final : public TExecutableOperator<FCATSlewOperator>
	{
	public:
		using FCATSlewOperatorData = CATSlewPrivate::FCATSlewOperatorData;

		FCATSlewOperator(
			const TSharedPtr<const FCATSlewOperatorData>& InOperatorData,
			const FOperatorSettings& InSettings,
			FChannelAgnosticTypeReadRef&& InInputCAT,
			FAudioBufferReadRef&& InInputMono,
			FTimeReadRef&& InRiseTime,
			FTimeReadRef&& InFallTime,
			FChannelAgnosticTypeWriteRef&& InOutput)
			: OperatorData(InOperatorData)
			, Settings(InSettings)
			, InputCAT(MoveTemp(InInputCAT))
			, InputMono(MoveTemp(InInputMono))
			, RiseTime(MoveTemp(InRiseTime))
			, FallTime(MoveTemp(InFallTime))
			, Output(MoveTemp(InOutput))
		{
		}

		static const FVertexInterface& DeclareVertexInterface()
		{
			static const FVertexInterface Interface = CATSlewPrivate::GetVertexInterface(TEXT("Cat:Stereo2Dot0"), EMetaSoundCATSlewInputMode::CAT);
			return Interface;
		}

		static FNodeClassMetadata GetNodeInfo()
		{
			FNodeClassMetadata Metadata;
			Metadata.ClassName = { TEXT("Experimental"), TEXT("CATSlew"), TEXT("Audio") };
			Metadata.MajorVersion = 1;
			Metadata.MinorVersion = 0;
			Metadata.DisplayName = LOCTEXT("CATSlewDisplayName", "CAT Slew");
			Metadata.Description = LOCTEXT("CATSlewDescription", "Slew CAT output from CAT or mono audio input.");
			Metadata.Author = TEXT("Charles Matthews");
			Metadata.PromptIfMissing = LOCTEXT("CATSlewMissingPrompt", "Enable MetaSoundExperimental for CAT channel format schemas.");
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
			using namespace CATSlewVertexNames;

			const FCATSlewOperatorData* ConfigData = CastOperatorData<const FCATSlewOperatorData>(InParams.Node.GetOperatorData().Get());
			if (!ConfigData)
			{
				return MakeUnique<FNoOpOperator>();
			}

			const Audio::FChannelTypeFamily* ConcreteType = Audio::GetChannelRegistry().FindConcreteChannel(ConfigData->CatAudioTypeName);
			if (!ConcreteType)
			{
				return MakeUnique<FNoOpOperator>();
			}

			const TSharedPtr<const FCATSlewOperatorData>& OperatorDataSharedPtr = StaticCastSharedPtr<const FCATSlewOperatorData>(InParams.Node.GetOperatorData());

			FChannelAgnosticTypeReadRef InCAT = FChannelAgnosticTypeWriteRef::CreateNew(InParams.OperatorSettings, ConcreteType->GetName());
			FAudioBufferReadRef InMono = FAudioBufferWriteRef::CreateNew(InParams.OperatorSettings);
			FTimeReadRef InRise = InParams.InputData.GetOrCreateDefaultDataReadReference<FTime>(METASOUND_GET_PARAM_NAME(RiseTime), InParams.OperatorSettings);
			FTimeReadRef InFall = InParams.InputData.GetOrCreateDefaultDataReadReference<FTime>(METASOUND_GET_PARAM_NAME(FallTime), InParams.OperatorSettings);

			switch (ConfigData->InputMode)
			{
			case EMetaSoundCATSlewInputMode::CAT:
				InCAT = InParams.InputData.GetOrCreateDefaultDataReadReference<FChannelAgnosticType>(METASOUND_GET_PARAM_NAME(Input), InParams.OperatorSettings);
				break;
			case EMetaSoundCATSlewInputMode::MonoAudio:
				InMono = InParams.InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(Input), InParams.OperatorSettings);
				break;
			default:
				checkNoEntry();
				break;
			}

			FChannelAgnosticTypeWriteRef Out = FChannelAgnosticTypeWriteRef::CreateNew(InParams.OperatorSettings, ConcreteType->GetName());

			return MakeUnique<FCATSlewOperator>(
				OperatorDataSharedPtr,
				InParams.OperatorSettings,
				MoveTemp(InCAT),
				MoveTemp(InMono),
				MoveTemp(InRise),
				MoveTemp(InFall),
				MoveTemp(Out));
		}

		virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
		{
			using namespace CATSlewVertexNames;

			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(RiseTime), RiseTime);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(FallTime), FallTime);

			switch (OperatorData->InputMode)
			{
			case EMetaSoundCATSlewInputMode::CAT:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Input), InputCAT);
				break;
			case EMetaSoundCATSlewInputMode::MonoAudio:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Input), InputMono);
				break;
			default:
				checkNoEntry();
				break;
			}
		}

		virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
		{
			using namespace CATSlewVertexNames;
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

			const float RiseSeconds = FMath::Max(0.0f, (float)RiseTime->GetSeconds());
			const float FallSeconds = FMath::Max(0.0f, (float)FallTime->GetSeconds());
			const float SampleRate = FMath::Max(1.0f, Settings.GetSampleRate());
			const float RiseAlpha = (RiseSeconds > 0.0f) ? FMath::Exp(-1.0f / (RiseSeconds * SampleRate)) : 0.0f;
			const float FallAlpha = (FallSeconds > 0.0f) ? FMath::Exp(-1.0f / (FallSeconds * SampleRate)) : 0.0f;

			switch (OperatorData->InputMode)
			{
			case EMetaSoundCATSlewInputMode::MonoAudio:
			{
				Scratch.SetNumUninitialized(NumFrames);
				const float* Src = InputMono->GetData();
				float State = bScalarInitialized ? PrevScalar : Src[0];
				for (int32 Frame = 0; Frame < NumFrames; ++Frame)
				{
					const float Target = Src[Frame];
					if (Target > State)
					{
						State = RiseAlpha * State + (1.0f - RiseAlpha) * Target;
					}
					else if (Target < State)
					{
						State = FallAlpha * State + (1.0f - FallAlpha) * Target;
					}
					Scratch[Frame] = State;
				}
				PrevScalar = State;
				bScalarInitialized = true;

				for (int32 Channel = 0; Channel < NumOutChannels; ++Channel)
				{
					TArrayView<float> Dst = Output->GetChannel(Channel);
					for (int32 Frame = 0; Frame < NumFrames; ++Frame)
					{
						Dst[Frame] = Scratch[Frame];
					}
				}
				break;
			}
			case EMetaSoundCATSlewInputMode::CAT:
			{
				const int32 NumInChannels = FMath::Max(1, InputCAT->NumChannels());
				if (PrevPerChannel.Num() != NumOutChannels)
				{
					PrevPerChannel.Init(0.0f, NumOutChannels);
					bPerChannelInitialized = false;
				}

				for (int32 Channel = 0; Channel < NumOutChannels; ++Channel)
				{
					const int32 InChannel = (NumInChannels == 1) ? 0 : FMath::Min(Channel, NumInChannels - 1);
					TArrayView<const float> Src = InputCAT->GetChannel(InChannel);
					TArrayView<float> Dst = Output->GetChannel(Channel);
					float State = bPerChannelInitialized ? PrevPerChannel[Channel] : Src[0];

					for (int32 Frame = 0; Frame < NumFrames; ++Frame)
					{
						const float Target = Src[Frame];
						if (Target > State)
						{
							State = RiseAlpha * State + (1.0f - RiseAlpha) * Target;
						}
						else if (Target < State)
						{
							State = FallAlpha * State + (1.0f - FallAlpha) * Target;
						}
						Dst[Frame] = State;
					}
					PrevPerChannel[Channel] = State;
				}

				bPerChannelInitialized = true;
				break;
			}
			default:
				checkNoEntry();
				break;
			}
		}

	private:
		TSharedPtr<const FCATSlewOperatorData> OperatorData;
		FOperatorSettings Settings;
		FChannelAgnosticTypeReadRef InputCAT;
		FAudioBufferReadRef InputMono;
		FTimeReadRef RiseTime;
		FTimeReadRef FallTime;
		FChannelAgnosticTypeWriteRef Output;
		TArray<float> Scratch;
		float PrevScalar = 0.0f;
		bool bScalarInitialized = false;
		TArray<float> PrevPerChannel;
		bool bPerChannelInitialized = false;
	};

	using FCATSlewNode = TNodeFacade<FCATSlewOperator>;
	METASOUND_REGISTER_NODE_AND_CONFIGURATION(FCATSlewNode, FMetaSoundCATSlewNodeConfiguration);
}

TArray<FPropertyTextFName> UMetaSoundCATSlewNodeOptionsHelper::GetSoundFileFormatChannelOptions()
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

FMetaSoundCATSlewNodeConfiguration::FMetaSoundCATSlewNodeConfiguration()
	: OperatorData(MakeShared<Metasound::CATSlewPrivate::FCATSlewOperatorData>(CatAudioTypeName, InputMode))
{
}

TInstancedStruct<FMetasoundFrontendClassInterface> FMetaSoundCATSlewNodeConfiguration::OverrideDefaultInterface(const FMetasoundFrontendClass& InClass) const
{
	return TInstancedStruct<FMetasoundFrontendClassInterface>::Make(
		FMetasoundFrontendClassInterface::GenerateClassInterface(
			Metasound::CATSlewPrivate::GetVertexInterface(CatAudioTypeName, InputMode)));
}

TSharedPtr<const Metasound::IOperatorData> FMetaSoundCATSlewNodeConfiguration::GetOperatorData() const
{
	OperatorData->CatAudioTypeName = CatAudioTypeName;
	OperatorData->InputMode = InputMode;
	return OperatorData;
}

#undef LOCTEXT_NAMESPACE
