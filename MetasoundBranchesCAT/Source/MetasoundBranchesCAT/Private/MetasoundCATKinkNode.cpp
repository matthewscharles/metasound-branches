// Copyright 2026 Charles Matthews. All Rights Reserved.

#include "MetasoundBranchesCAT/Public/MetasoundCATKinkNode.h"

#include "MetasoundAudioBuffer.h"
#include "MetasoundBranches/Public/Kink.h"
#include "MetasoundBranches/Public/MetasoundCommonMacros.h"
#include "MetasoundChannelAgnosticType.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundParamHelper.h"
#include "MetasoundPrimitives.h"
#include "MetasoundVertex.h"
#include "TypeFamily/ChannelTypeFamily.h"

#define LOCTEXT_NAMESPACE "MetasoundBranches_CATKinkNode"

namespace Metasound
{
	namespace CATKinkVertexNames
	{
		METASOUND_PARAM(Input, "Input", "CAT signal to process.")
		METASOUND_PARAM(Slope, "Slope", "Kink slope control.")
		METASOUND_PARAM(Output, "Output", "Kinked CAT output.")
	}

	namespace CATKinkPrivate
	{
		class FCATKinkOperatorData final : public TOperatorData<FCATKinkOperatorData>
		{
		public:
			static const FLazyName OperatorDataTypeName;

			explicit FCATKinkOperatorData(
				const FName& InCatAudioTypeName,
				const EMetaSoundCATKinkControlMode InSlopeMode)
				: CatAudioTypeName(InCatAudioTypeName)
				, SlopeMode(InSlopeMode)
			{
			}

			FName CatAudioTypeName;
			EMetaSoundCATKinkControlMode SlopeMode;
		};

		const FLazyName FCATKinkOperatorData::OperatorDataTypeName = TEXT("FCATKinkOperatorData");

		void AddControlVertex(
			FInputVertexInterface& InOutInput,
			const FName& InVertexName,
			const FDataVertexMetadata& InMetadata,
			const FName& InCatFormat,
			const EMetaSoundCATKinkControlMode InMode,
			const float InDefaultFloat)
		{
			switch (InMode)
			{
			case EMetaSoundCATKinkControlMode::CAT:
				InOutInput.Add(FInputDataVertex(InVertexName, InCatFormat, InMetadata, EVertexAccessType::Reference));
				break;
			case EMetaSoundCATKinkControlMode::MonoAudio:
				InOutInput.Add(TInputDataVertex<FAudioBuffer>(InVertexName, InMetadata));
				break;
			case EMetaSoundCATKinkControlMode::Float:
				InOutInput.Add(TInputDataVertex<float>(InVertexName, InMetadata, InDefaultFloat));
				break;
			case EMetaSoundCATKinkControlMode::FloatArray:
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
			const EMetaSoundCATKinkControlMode InMode,
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
			case EMetaSoundCATKinkControlMode::CAT:
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
			case EMetaSoundCATKinkControlMode::MonoAudio:
			{
				const int32 NumSamples = InMono->Num();
				if (NumSamples <= 0)
				{
					return InDefault;
				}
				return InMono->GetData()[FMath::Min(InFrame, NumSamples - 1)];
			}
			case EMetaSoundCATKinkControlMode::Float:
				return *InFloat;
			case EMetaSoundCATKinkControlMode::FloatArray:
				return GetArrayValue(*InFloatArray, InChannel, InDefault);
			default:
				checkNoEntry();
				return InDefault;
			}
		}

		FVertexInterface GetVertexInterface(
			const FName& InCatFormat,
			const EMetaSoundCATKinkControlMode InSlopeMode)
		{
			using namespace CATKinkVertexNames;

			FInputVertexInterface InputInterface;
			InputInterface.Add(FInputDataVertex(METASOUND_GET_PARAM_NAME(Input), InCatFormat, METASOUND_GET_PARAM_METADATA(Input), EVertexAccessType::Reference));
			AddControlVertex(InputInterface, METASOUND_GET_PARAM_NAME(Slope), METASOUND_GET_PARAM_METADATA(Slope), InCatFormat, InSlopeMode, 1.0f);

			FOutputVertexInterface OutputInterface;
			OutputInterface.Add(FOutputDataVertex(METASOUND_GET_PARAM_NAME(Output), InCatFormat, METASOUND_GET_PARAM_METADATA(Output), EVertexAccessType::Reference));

			return { MoveTemp(InputInterface), MoveTemp(OutputInterface) };
		}
	}

	class FCATKinkOperator final : public TExecutableOperator<FCATKinkOperator>
	{
	public:
		using FCATKinkOperatorData = CATKinkPrivate::FCATKinkOperatorData;

		FCATKinkOperator(
			const TSharedPtr<const FCATKinkOperatorData>& InOperatorData,
			const FOperatorSettings& InSettings,
			FChannelAgnosticTypeReadRef&& InInput,
			FChannelAgnosticTypeReadRef&& InSlopeCAT,
			FAudioBufferReadRef&& InSlopeMono,
			FFloatReadRef&& InSlopeFloat,
			TDataReadReference<TArray<float>>&& InSlopeArray,
			FChannelAgnosticTypeWriteRef&& InOutput)
			: OperatorData(InOperatorData)
			, Settings(InSettings)
			, Input(MoveTemp(InInput))
			, SlopeCAT(MoveTemp(InSlopeCAT))
			, SlopeMono(MoveTemp(InSlopeMono))
			, SlopeFloat(MoveTemp(InSlopeFloat))
			, SlopeArray(MoveTemp(InSlopeArray))
			, Output(MoveTemp(InOutput))
		{
		}

		static const FVertexInterface& DeclareVertexInterface()
		{
			static const FVertexInterface Interface = CATKinkPrivate::GetVertexInterface(
				TEXT("Cat:Stereo2Dot0"),
				EMetaSoundCATKinkControlMode::Float);
			return Interface;
		}

		static FNodeClassMetadata GetNodeInfo()
		{
			FNodeClassMetadata Metadata;
			Metadata.ClassName = { TEXT("Experimental"), TEXT("CATKink"), TEXT("Audio") };
			Metadata.MajorVersion = 1;
			Metadata.MinorVersion = 0;
			Metadata.DisplayName = LOCTEXT("CATKinkDisplayName", "CAT Kink");
			Metadata.Description = LOCTEXT("CATKinkDescription", "Kink CAT audio with configurable slope control pin type.");
			Metadata.Author = TEXT("Charles Matthews");
			Metadata.PromptIfMissing = LOCTEXT("CATKinkMissingPrompt", "Enable MetaSoundExperimental for CAT channel format schemas.");
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
			using namespace CATKinkVertexNames;

			const FCATKinkOperatorData* ConfigData = CastOperatorData<const FCATKinkOperatorData>(InParams.Node.GetOperatorData().Get());
			if (!ConfigData)
			{
				return MakeUnique<FNoOpOperator>();
			}

			const Audio::FChannelTypeFamily* ConcreteType = Audio::GetChannelRegistry().FindConcreteChannel(ConfigData->CatAudioTypeName);
			if (!ConcreteType)
			{
				return MakeUnique<FNoOpOperator>();
			}

			const TSharedPtr<const FCATKinkOperatorData>& OperatorDataSharedPtr = StaticCastSharedPtr<const FCATKinkOperatorData>(InParams.Node.GetOperatorData());

			FChannelAgnosticTypeReadRef InSignal = InParams.InputData.GetOrCreateDefaultDataReadReference<FChannelAgnosticType>(METASOUND_GET_PARAM_NAME(Input), InParams.OperatorSettings);

			FChannelAgnosticTypeReadRef InSlopeCAT = FChannelAgnosticTypeWriteRef::CreateNew(InParams.OperatorSettings, ConcreteType->GetName());
			FAudioBufferReadRef InSlopeMono = FAudioBufferWriteRef::CreateNew(InParams.OperatorSettings);
			FFloatReadRef InSlopeFloat = TDataReadReference<float>::CreateNew(1.0f);
			TDataReadReference<TArray<float>> InSlopeArray = TDataReadReference<TArray<float>>::CreateNew(TArray<float>{});

			switch (ConfigData->SlopeMode)
			{
			case EMetaSoundCATKinkControlMode::CAT:
				InSlopeCAT = InParams.InputData.GetOrCreateDefaultDataReadReference<FChannelAgnosticType>(METASOUND_GET_PARAM_NAME(Slope), InParams.OperatorSettings);
				break;
			case EMetaSoundCATKinkControlMode::MonoAudio:
				InSlopeMono = InParams.InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(Slope), InParams.OperatorSettings);
				break;
			case EMetaSoundCATKinkControlMode::Float:
				InSlopeFloat = InParams.InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(Slope), InParams.OperatorSettings);
				break;
			case EMetaSoundCATKinkControlMode::FloatArray:
				InSlopeArray = InParams.InputData.GetOrCreateDefaultDataReadReference<TArray<float>>(METASOUND_GET_PARAM_NAME(Slope), InParams.OperatorSettings);
				break;
			default:
				checkNoEntry();
				break;
			}

			FChannelAgnosticTypeWriteRef Out = FChannelAgnosticTypeWriteRef::CreateNew(InParams.OperatorSettings, ConcreteType->GetName());

			return MakeUnique<FCATKinkOperator>(
				OperatorDataSharedPtr,
				InParams.OperatorSettings,
				MoveTemp(InSignal),
				MoveTemp(InSlopeCAT),
				MoveTemp(InSlopeMono),
				MoveTemp(InSlopeFloat),
				MoveTemp(InSlopeArray),
				MoveTemp(Out));
		}

		virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
		{
			using namespace CATKinkVertexNames;

			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Input), Input);

			switch (OperatorData->SlopeMode)
			{
			case EMetaSoundCATKinkControlMode::CAT:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Slope), SlopeCAT);
				break;
			case EMetaSoundCATKinkControlMode::MonoAudio:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Slope), SlopeMono);
				break;
			case EMetaSoundCATKinkControlMode::Float:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Slope), SlopeFloat);
				break;
			case EMetaSoundCATKinkControlMode::FloatArray:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Slope), SlopeArray);
				break;
			default:
				checkNoEntry();
				break;
			}
		}

		virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
		{
			using namespace CATKinkVertexNames;
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

			const int32 NumInChannels = FMath::Max(1, Input->NumChannels());
			for (int32 Channel = 0; Channel < NumOutChannels; ++Channel)
			{
				const int32 InChannel = (NumInChannels == 1) ? 0 : FMath::Min(Channel, NumInChannels - 1);
				TArrayView<const float> Src = Input->GetChannel(InChannel);
				TArrayView<float> Dst = Output->GetChannel(Channel);

				for (int32 Frame = 0; Frame < NumFrames; ++Frame)
				{
					float SlopeValue = CATKinkPrivate::GetControlValue(
						OperatorData->SlopeMode,
						Channel,
						Frame,
						SlopeCAT,
						SlopeMono,
						SlopeFloat,
						SlopeArray,
						1.0f);

					if (FMath::IsNearlyZero(SlopeValue))
					{
						SlopeValue = 0.000001f;
					}

					Dst[Frame] = KinkProcess(Src[Frame], SlopeValue);
				}
			}
		}

	private:
		TSharedPtr<const FCATKinkOperatorData> OperatorData;
		FOperatorSettings Settings;
		FChannelAgnosticTypeReadRef Input;

		FChannelAgnosticTypeReadRef SlopeCAT;
		FAudioBufferReadRef SlopeMono;
		FFloatReadRef SlopeFloat;
		TDataReadReference<TArray<float>> SlopeArray;

		FChannelAgnosticTypeWriteRef Output;
	};

	using FCATKinkNode = TNodeFacade<FCATKinkOperator>;
	METASOUND_REGISTER_NODE_AND_CONFIGURATION(FCATKinkNode, FMetaSoundCATKinkNodeConfiguration);
}

TArray<FPropertyTextFName> UMetaSoundCATKinkNodeOptionsHelper::GetSoundFileFormatChannelOptions()
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

FMetaSoundCATKinkNodeConfiguration::FMetaSoundCATKinkNodeConfiguration()
	: OperatorData(MakeShared<Metasound::CATKinkPrivate::FCATKinkOperatorData>(CatAudioTypeName, SlopeMode))
{
}

TInstancedStruct<FMetasoundFrontendClassInterface> FMetaSoundCATKinkNodeConfiguration::OverrideDefaultInterface(const FMetasoundFrontendClass& InClass) const
{
	return TInstancedStruct<FMetasoundFrontendClassInterface>::Make(
		FMetasoundFrontendClassInterface::GenerateClassInterface(
			Metasound::CATKinkPrivate::GetVertexInterface(CatAudioTypeName, SlopeMode)));
}

TSharedPtr<const Metasound::IOperatorData> FMetaSoundCATKinkNodeConfiguration::GetOperatorData() const
{
	OperatorData->CatAudioTypeName = CatAudioTypeName;
	OperatorData->SlopeMode = SlopeMode;
	return OperatorData;
}

#undef LOCTEXT_NAMESPACE
