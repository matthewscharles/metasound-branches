// Copyright 2026 Charles Matthews. All Rights Reserved.

#include "MetasoundBranchesCAT/Public/MetasoundCATFoldNode.h"

#include "MetasoundAudioBuffer.h"
#include "MetasoundBranches/Public/Fold.h"
#include "MetasoundBranches/Public/MetasoundCommonMacros.h"
#include "MetasoundChannelAgnosticType.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundParamHelper.h"
#include "MetasoundPrimitives.h"
#include "MetasoundVertex.h"
#include "TypeFamily/ChannelTypeFamily.h"

#define LOCTEXT_NAMESPACE "MetasoundBranches_CATFoldNode"

namespace Metasound
{
	namespace CATFoldVertexNames
	{
		METASOUND_PARAM(Input, "Input", "CAT signal to fold.")
		METASOUND_PARAM(High, "High", "Upper fold threshold.")
		METASOUND_PARAM(Low, "Low", "Lower fold threshold.")
		METASOUND_PARAM(Output, "Output", "Folded CAT output.")
	}

	namespace CATFoldPrivate
	{
		class FCATFoldOperatorData final : public TOperatorData<FCATFoldOperatorData>
		{
		public:
			static const FLazyName OperatorDataTypeName;

			explicit FCATFoldOperatorData(
				const FName& InCatAudioTypeName,
				const EMetaSoundCATFoldControlMode InHighMode,
				const EMetaSoundCATFoldControlMode InLowMode)
				: CatAudioTypeName(InCatAudioTypeName)
				, HighMode(InHighMode)
				, LowMode(InLowMode)
			{
			}

			FName CatAudioTypeName;
			EMetaSoundCATFoldControlMode HighMode;
			EMetaSoundCATFoldControlMode LowMode;
		};

		const FLazyName FCATFoldOperatorData::OperatorDataTypeName = TEXT("FCATFoldOperatorData");

		void AddControlVertex(
			FInputVertexInterface& InOutInput,
			const FName& InVertexName,
			const FDataVertexMetadata& InMetadata,
			const FName& InCatFormat,
			const EMetaSoundCATFoldControlMode InMode,
			const float InDefaultFloat)
		{
			switch (InMode)
			{
			case EMetaSoundCATFoldControlMode::CAT:
				InOutInput.Add(FInputDataVertex(InVertexName, InCatFormat, InMetadata, EVertexAccessType::Reference));
				break;
			case EMetaSoundCATFoldControlMode::MonoAudio:
				InOutInput.Add(TInputDataVertex<FAudioBuffer>(InVertexName, InMetadata));
				break;
			case EMetaSoundCATFoldControlMode::Float:
				InOutInput.Add(TInputDataVertex<float>(InVertexName, InMetadata, InDefaultFloat));
				break;
			case EMetaSoundCATFoldControlMode::FloatArray:
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
			const EMetaSoundCATFoldControlMode InMode,
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
			case EMetaSoundCATFoldControlMode::CAT:
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
			case EMetaSoundCATFoldControlMode::MonoAudio:
			{
				const int32 NumSamples = InMono->Num();
				if (NumSamples <= 0)
				{
					return InDefault;
				}
				return InMono->GetData()[FMath::Min(InFrame, NumSamples - 1)];
			}
			case EMetaSoundCATFoldControlMode::Float:
				return *InFloat;
			case EMetaSoundCATFoldControlMode::FloatArray:
				return GetArrayValue(*InFloatArray, InChannel, InDefault);
			default:
				checkNoEntry();
				return InDefault;
			}
		}

		FVertexInterface GetVertexInterface(
			const FName& InCatFormat,
			const EMetaSoundCATFoldControlMode InHighMode,
			const EMetaSoundCATFoldControlMode InLowMode)
		{
			using namespace CATFoldVertexNames;

			FInputVertexInterface InputInterface;
			InputInterface.Add(FInputDataVertex(METASOUND_GET_PARAM_NAME(Input), InCatFormat, METASOUND_GET_PARAM_METADATA(Input), EVertexAccessType::Reference));

			AddControlVertex(InputInterface, METASOUND_GET_PARAM_NAME(High), METASOUND_GET_PARAM_METADATA(High), InCatFormat, InHighMode, 1.0f);
			AddControlVertex(InputInterface, METASOUND_GET_PARAM_NAME(Low), METASOUND_GET_PARAM_METADATA(Low), InCatFormat, InLowMode, -1.0f);

			FOutputVertexInterface OutputInterface;
			OutputInterface.Add(FOutputDataVertex(METASOUND_GET_PARAM_NAME(Output), InCatFormat, METASOUND_GET_PARAM_METADATA(Output), EVertexAccessType::Reference));

			return { MoveTemp(InputInterface), MoveTemp(OutputInterface) };
		}
	}

	class FCATFoldOperator final : public TExecutableOperator<FCATFoldOperator>
	{
	public:
		using FCATFoldOperatorData = CATFoldPrivate::FCATFoldOperatorData;

		FCATFoldOperator(
			const TSharedPtr<const FCATFoldOperatorData>& InOperatorData,
			const FOperatorSettings& InSettings,
			FChannelAgnosticTypeReadRef&& InInput,
			FChannelAgnosticTypeReadRef&& InHighCAT,
			FAudioBufferReadRef&& InHighMono,
			FFloatReadRef&& InHighFloat,
			TDataReadReference<TArray<float>>&& InHighArray,
			FChannelAgnosticTypeReadRef&& InLowCAT,
			FAudioBufferReadRef&& InLowMono,
			FFloatReadRef&& InLowFloat,
			TDataReadReference<TArray<float>>&& InLowArray,
			FChannelAgnosticTypeWriteRef&& InOutput)
			: OperatorData(InOperatorData)
			, Settings(InSettings)
			, Input(MoveTemp(InInput))
			, HighCAT(MoveTemp(InHighCAT))
			, HighMono(MoveTemp(InHighMono))
			, HighFloat(MoveTemp(InHighFloat))
			, HighArray(MoveTemp(InHighArray))
			, LowCAT(MoveTemp(InLowCAT))
			, LowMono(MoveTemp(InLowMono))
			, LowFloat(MoveTemp(InLowFloat))
			, LowArray(MoveTemp(InLowArray))
			, Output(MoveTemp(InOutput))
		{
		}

		static const FVertexInterface& DeclareVertexInterface()
		{
			static const FVertexInterface Interface = CATFoldPrivate::GetVertexInterface(
				TEXT("Cat:Stereo2Dot0"),
				EMetaSoundCATFoldControlMode::Float,
				EMetaSoundCATFoldControlMode::Float);
			return Interface;
		}

		static FNodeClassMetadata GetNodeInfo()
		{
			FNodeClassMetadata Metadata;
			Metadata.ClassName = { TEXT("Experimental"), TEXT("CATFold"), TEXT("Audio") };
			Metadata.MajorVersion = 1;
			Metadata.MinorVersion = 0;
			Metadata.DisplayName = LOCTEXT("CATFoldDisplayName", "CAT Fold");
			Metadata.Description = LOCTEXT("CATFoldDescription", "Fold CAT audio with configurable high and low control pin types.");
			Metadata.Author = TEXT("Charles Matthews");
			Metadata.PromptIfMissing = LOCTEXT("CATFoldMissingPrompt", "Enable MetaSoundExperimental for CAT channel format schemas.");
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
			using namespace CATFoldVertexNames;

			const FCATFoldOperatorData* ConfigData = CastOperatorData<const FCATFoldOperatorData>(InParams.Node.GetOperatorData().Get());
			if (!ConfigData)
			{
				return MakeUnique<FNoOpOperator>();
			}

			const Audio::FChannelTypeFamily* ConcreteType = Audio::GetChannelRegistry().FindConcreteChannel(ConfigData->CatAudioTypeName);
			if (!ConcreteType)
			{
				return MakeUnique<FNoOpOperator>();
			}

			const TSharedPtr<const FCATFoldOperatorData>& OperatorDataSharedPtr = StaticCastSharedPtr<const FCATFoldOperatorData>(InParams.Node.GetOperatorData());

			FChannelAgnosticTypeReadRef InSignal = InParams.InputData.GetOrCreateDefaultDataReadReference<FChannelAgnosticType>(METASOUND_GET_PARAM_NAME(Input), InParams.OperatorSettings);

			FChannelAgnosticTypeReadRef InHighCAT = FChannelAgnosticTypeWriteRef::CreateNew(InParams.OperatorSettings, ConcreteType->GetName());
			FAudioBufferReadRef InHighMono = FAudioBufferWriteRef::CreateNew(InParams.OperatorSettings);
			FFloatReadRef InHighFloat = TDataReadReference<float>::CreateNew(1.0f);
			TDataReadReference<TArray<float>> InHighArray = TDataReadReference<TArray<float>>::CreateNew(TArray<float>{});

			FChannelAgnosticTypeReadRef InLowCAT = FChannelAgnosticTypeWriteRef::CreateNew(InParams.OperatorSettings, ConcreteType->GetName());
			FAudioBufferReadRef InLowMono = FAudioBufferWriteRef::CreateNew(InParams.OperatorSettings);
			FFloatReadRef InLowFloat = TDataReadReference<float>::CreateNew(-1.0f);
			TDataReadReference<TArray<float>> InLowArray = TDataReadReference<TArray<float>>::CreateNew(TArray<float>{});

			switch (ConfigData->HighMode)
			{
			case EMetaSoundCATFoldControlMode::CAT:
				InHighCAT = InParams.InputData.GetOrCreateDefaultDataReadReference<FChannelAgnosticType>(METASOUND_GET_PARAM_NAME(High), InParams.OperatorSettings);
				break;
			case EMetaSoundCATFoldControlMode::MonoAudio:
				InHighMono = InParams.InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(High), InParams.OperatorSettings);
				break;
			case EMetaSoundCATFoldControlMode::Float:
				InHighFloat = InParams.InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(High), InParams.OperatorSettings);
				break;
			case EMetaSoundCATFoldControlMode::FloatArray:
				InHighArray = InParams.InputData.GetOrCreateDefaultDataReadReference<TArray<float>>(METASOUND_GET_PARAM_NAME(High), InParams.OperatorSettings);
				break;
			default:
				checkNoEntry();
				break;
			}

			switch (ConfigData->LowMode)
			{
			case EMetaSoundCATFoldControlMode::CAT:
				InLowCAT = InParams.InputData.GetOrCreateDefaultDataReadReference<FChannelAgnosticType>(METASOUND_GET_PARAM_NAME(Low), InParams.OperatorSettings);
				break;
			case EMetaSoundCATFoldControlMode::MonoAudio:
				InLowMono = InParams.InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(Low), InParams.OperatorSettings);
				break;
			case EMetaSoundCATFoldControlMode::Float:
				InLowFloat = InParams.InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(Low), InParams.OperatorSettings);
				break;
			case EMetaSoundCATFoldControlMode::FloatArray:
				InLowArray = InParams.InputData.GetOrCreateDefaultDataReadReference<TArray<float>>(METASOUND_GET_PARAM_NAME(Low), InParams.OperatorSettings);
				break;
			default:
				checkNoEntry();
				break;
			}

			FChannelAgnosticTypeWriteRef Out = FChannelAgnosticTypeWriteRef::CreateNew(InParams.OperatorSettings, ConcreteType->GetName());

			return MakeUnique<FCATFoldOperator>(
				OperatorDataSharedPtr,
				InParams.OperatorSettings,
				MoveTemp(InSignal),
				MoveTemp(InHighCAT),
				MoveTemp(InHighMono),
				MoveTemp(InHighFloat),
				MoveTemp(InHighArray),
				MoveTemp(InLowCAT),
				MoveTemp(InLowMono),
				MoveTemp(InLowFloat),
				MoveTemp(InLowArray),
				MoveTemp(Out));
		}

		virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
		{
			using namespace CATFoldVertexNames;

			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Input), Input);

			switch (OperatorData->HighMode)
			{
			case EMetaSoundCATFoldControlMode::CAT:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(High), HighCAT);
				break;
			case EMetaSoundCATFoldControlMode::MonoAudio:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(High), HighMono);
				break;
			case EMetaSoundCATFoldControlMode::Float:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(High), HighFloat);
				break;
			case EMetaSoundCATFoldControlMode::FloatArray:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(High), HighArray);
				break;
			default:
				checkNoEntry();
				break;
			}

			switch (OperatorData->LowMode)
			{
			case EMetaSoundCATFoldControlMode::CAT:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Low), LowCAT);
				break;
			case EMetaSoundCATFoldControlMode::MonoAudio:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Low), LowMono);
				break;
			case EMetaSoundCATFoldControlMode::Float:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Low), LowFloat);
				break;
			case EMetaSoundCATFoldControlMode::FloatArray:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Low), LowArray);
				break;
			default:
				checkNoEntry();
				break;
			}
		}

		virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
		{
			using namespace CATFoldVertexNames;
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
					const float HighValue = CATFoldPrivate::GetControlValue(
						OperatorData->HighMode,
						Channel,
						Frame,
						HighCAT,
						HighMono,
						HighFloat,
						HighArray,
						1.0f);

					const float LowValue = CATFoldPrivate::GetControlValue(
						OperatorData->LowMode,
						Channel,
						Frame,
						LowCAT,
						LowMono,
						LowFloat,
						LowArray,
						-1.0f);

					Dst[Frame] = PerformFold(Src[Frame], LowValue, HighValue);
				}
			}
		}

	private:
		TSharedPtr<const FCATFoldOperatorData> OperatorData;
		FOperatorSettings Settings;
		FChannelAgnosticTypeReadRef Input;

		FChannelAgnosticTypeReadRef HighCAT;
		FAudioBufferReadRef HighMono;
		FFloatReadRef HighFloat;
		TDataReadReference<TArray<float>> HighArray;

		FChannelAgnosticTypeReadRef LowCAT;
		FAudioBufferReadRef LowMono;
		FFloatReadRef LowFloat;
		TDataReadReference<TArray<float>> LowArray;

		FChannelAgnosticTypeWriteRef Output;
	};

	using FCATFoldNode = TNodeFacade<FCATFoldOperator>;
	METASOUND_REGISTER_NODE_AND_CONFIGURATION(FCATFoldNode, FMetaSoundCATFoldNodeConfiguration);
}

TArray<FPropertyTextFName> UMetaSoundCATFoldNodeOptionsHelper::GetSoundFileFormatChannelOptions()
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

FMetaSoundCATFoldNodeConfiguration::FMetaSoundCATFoldNodeConfiguration()
	: OperatorData(MakeShared<Metasound::CATFoldPrivate::FCATFoldOperatorData>(CatAudioTypeName, HighMode, LowMode))
{
}

TInstancedStruct<FMetasoundFrontendClassInterface> FMetaSoundCATFoldNodeConfiguration::OverrideDefaultInterface(const FMetasoundFrontendClass& InClass) const
{
	return TInstancedStruct<FMetasoundFrontendClassInterface>::Make(
		FMetasoundFrontendClassInterface::GenerateClassInterface(
			Metasound::CATFoldPrivate::GetVertexInterface(CatAudioTypeName, HighMode, LowMode)));
}

TSharedPtr<const Metasound::IOperatorData> FMetaSoundCATFoldNodeConfiguration::GetOperatorData() const
{
	OperatorData->CatAudioTypeName = CatAudioTypeName;
	OperatorData->HighMode = HighMode;
	OperatorData->LowMode = LowMode;
	return OperatorData;
}

#undef LOCTEXT_NAMESPACE
