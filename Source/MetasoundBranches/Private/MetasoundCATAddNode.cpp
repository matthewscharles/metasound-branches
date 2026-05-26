// Copyright 2026 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundCATAddNode.h"

#include "MetasoundAudioBuffer.h"
#include "MetasoundBranches/Public/MetasoundCommonMacros.h"
#include "MetasoundChannelAgnosticType.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundParamHelper.h"
#include "MetasoundVertex.h"
#include "Math/UnrealMathUtility.h"
#include "TypeFamily/ChannelTypeFamily.h"

#define LOCTEXT_NAMESPACE "MetasoundBranches_CATAddNode"

namespace Metasound
{
	namespace CATAddVertexNames
	{
		METASOUND_PARAM(InputA, "A", "First CAT input.")
		METASOUND_PARAM(InputB, "B", "Second input. Type is configurable.")
		METASOUND_PARAM(InputBSlewEnabled, "Slew B", "Enable one-pole slew smoothing on input B.")
		METASOUND_PARAM(InputBSlewTimeMs, "B Slew Time (ms)", "Slew time in milliseconds for input B.")
		METASOUND_PARAM(Output, "Output", "CAT add result.")
	}

	namespace CATAddPrivate
	{
		class FCATAddOperatorData final : public TOperatorData<FCATAddOperatorData>
		{
		public:
			static const FLazyName OperatorDataTypeName;

			explicit FCATAddOperatorData(const FName& InCatAudioTypeName, const EMetaSoundCATAddInputBMode InInputBMode)
				: CatAudioTypeName(InCatAudioTypeName)
				, InputBMode(InInputBMode)
			{
			}

			FName CatAudioTypeName;
			EMetaSoundCATAddInputBMode InputBMode;
		};

		const FLazyName FCATAddOperatorData::OperatorDataTypeName = TEXT("FCATAddOperatorData");

		FVertexInterface GetVertexInterface(const FName& InCatFormat, const EMetaSoundCATAddInputBMode InInputBMode)
		{
			using namespace CATAddVertexNames;

			FInputVertexInterface Input;
			Input.Add(FInputDataVertex(METASOUND_GET_PARAM_NAME(InputA), InCatFormat, METASOUND_GET_PARAM_METADATA(InputA), EVertexAccessType::Reference));

			switch (InInputBMode)
			{
			case EMetaSoundCATAddInputBMode::CAT:
				Input.Add(FInputDataVertex(METASOUND_GET_PARAM_NAME(InputB), InCatFormat, METASOUND_GET_PARAM_METADATA(InputB), EVertexAccessType::Reference));
				break;
			case EMetaSoundCATAddInputBMode::MonoAudio:
				Input.Add(TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputB)));
				break;
			case EMetaSoundCATAddInputBMode::Float:
				Input.Add(TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputB), 1.0f));
				break;
			default:
				checkNoEntry();
				break;
			}

			Input.Add(TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputBSlewEnabled), false));
			Input.Add(TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputBSlewTimeMs), 0.0f));

			FOutputVertexInterface OutputInterface;
			OutputInterface.Add(FOutputDataVertex(METASOUND_GET_PARAM_NAME(Output), InCatFormat, METASOUND_GET_PARAM_METADATA(Output), EVertexAccessType::Reference));

			return { MoveTemp(Input), MoveTemp(OutputInterface) };
		}
	}

	class FCATAddOperator final : public TExecutableOperator<FCATAddOperator>
	{
	public:
		using FCATAddOperatorData = CATAddPrivate::FCATAddOperatorData;

		FCATAddOperator(
			const TSharedPtr<const FCATAddOperatorData>& InOperatorData,
			const FOperatorSettings& InSettings,
			FChannelAgnosticTypeReadRef&& InInputA,
			FChannelAgnosticTypeReadRef&& InInputBCAT,
			FAudioBufferReadRef&& InInputBMono,
			FFloatReadRef&& InInputBFloat,
			FBoolReadRef&& InInputBSlewEnabled,
			FFloatReadRef&& InInputBSlewTimeMs,
			FChannelAgnosticTypeWriteRef&& InOutput)
			: OperatorData(InOperatorData)
			, Settings(InSettings)
			, InputA(MoveTemp(InInputA))
			, InputBCAT(MoveTemp(InInputBCAT))
			, InputBMono(MoveTemp(InInputBMono))
			, InputBFloat(MoveTemp(InInputBFloat))
			, InputBSlewEnabled(MoveTemp(InInputBSlewEnabled))
			, InputBSlewTimeMs(MoveTemp(InInputBSlewTimeMs))
			, Output(MoveTemp(InOutput))
		{
		}

		static const FVertexInterface& DeclareVertexInterface()
		{
			static const FVertexInterface Interface = CATAddPrivate::GetVertexInterface(TEXT("Cat:Stereo2Dot0"), EMetaSoundCATAddInputBMode::MonoAudio);
			return Interface;
		}

		static FNodeClassMetadata GetNodeInfo()
		{
			FNodeClassMetadata Metadata;
			Metadata.ClassName = { TEXT("Experimental"), TEXT("CATAdd"), TEXT("Audio") };
			Metadata.MajorVersion = 1;
			Metadata.MinorVersion = 0;
			Metadata.DisplayName = LOCTEXT("CATAddDisplayName", "CAT Add");
			Metadata.Description = LOCTEXT("CATAddDescription", "Adds a CAT signal with CAT, mono audio, or float input.");
			Metadata.Author = TEXT("Charles Matthews");
			Metadata.PromptIfMissing = LOCTEXT("CATAddMissingPrompt", "Enable MetaSoundExperimental for CAT channel format schemas.");
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
			using namespace CATAddVertexNames;

			const FCATAddOperatorData* ConfigData = CastOperatorData<const FCATAddOperatorData>(InParams.Node.GetOperatorData().Get());
			if (!ConfigData)
			{
				return MakeUnique<FNoOpOperator>();
			}

			const Audio::FChannelTypeFamily* ConcreteType = Audio::GetChannelRegistry().FindConcreteChannel(ConfigData->CatAudioTypeName);
			if (!ConcreteType)
			{
				return MakeUnique<FNoOpOperator>();
			}

			const TSharedPtr<const FCATAddOperatorData>& OperatorDataSharedPtr = StaticCastSharedPtr<const FCATAddOperatorData>(InParams.Node.GetOperatorData());

			FChannelAgnosticTypeReadRef InA = InParams.InputData.GetOrCreateDefaultDataReadReference<FChannelAgnosticType>(METASOUND_GET_PARAM_NAME(InputA), InParams.OperatorSettings);

			FChannelAgnosticTypeReadRef InBCat = FChannelAgnosticTypeWriteRef::CreateNew(InParams.OperatorSettings, ConcreteType->GetName());
			FAudioBufferReadRef InBMono = FAudioBufferWriteRef::CreateNew(InParams.OperatorSettings);
			FFloatReadRef InBFloat = TDataReadReference<float>::CreateNew(1.0f);
			FBoolReadRef InBSlewEnabled = InParams.InputData.GetOrCreateDefaultDataReadReference<bool>(METASOUND_GET_PARAM_NAME(InputBSlewEnabled), InParams.OperatorSettings);
			FFloatReadRef InBSlewTimeMs = InParams.InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputBSlewTimeMs), InParams.OperatorSettings);

			switch (ConfigData->InputBMode)
			{
			case EMetaSoundCATAddInputBMode::CAT:
				InBCat = InParams.InputData.GetOrCreateDefaultDataReadReference<FChannelAgnosticType>(METASOUND_GET_PARAM_NAME(InputB), InParams.OperatorSettings);
				break;
			case EMetaSoundCATAddInputBMode::MonoAudio:
				InBMono = InParams.InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(InputB), InParams.OperatorSettings);
				break;
			case EMetaSoundCATAddInputBMode::Float:
				InBFloat = InParams.InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputB), InParams.OperatorSettings);
				break;
			default:
				checkNoEntry();
				break;
			}

			FChannelAgnosticTypeWriteRef Out = FChannelAgnosticTypeWriteRef::CreateNew(InParams.OperatorSettings, ConcreteType->GetName());

			return MakeUnique<FCATAddOperator>(
				OperatorDataSharedPtr,
				InParams.OperatorSettings,
				MoveTemp(InA),
				MoveTemp(InBCat),
				MoveTemp(InBMono),
				MoveTemp(InBFloat),
				MoveTemp(InBSlewEnabled),
				MoveTemp(InBSlewTimeMs),
				MoveTemp(Out));
		}

		virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
		{
			using namespace CATAddVertexNames;

			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputA), InputA);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputBSlewEnabled), InputBSlewEnabled);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputBSlewTimeMs), InputBSlewTimeMs);

			switch (OperatorData->InputBMode)
			{
			case EMetaSoundCATAddInputBMode::CAT:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputB), InputBCAT);
				break;
			case EMetaSoundCATAddInputBMode::MonoAudio:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputB), InputBMono);
				break;
			case EMetaSoundCATAddInputBMode::Float:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputB), InputBFloat);
				break;
			default:
				checkNoEntry();
				break;
			}
		}

		virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
		{
			using namespace CATAddVertexNames;
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

			const int32 NumAChannels = FMath::Max(1, InputA->NumChannels());

			const bool bUseSlew = *InputBSlewEnabled && (*InputBSlewTimeMs > 0.0f);
			const float TauSeconds = FMath::Max(0.000001f, *InputBSlewTimeMs * 0.001f);
			const float SlewAlpha = bUseSlew ? (1.0f - FMath::Exp(-1.0f / (TauSeconds * FMath::Max(1.0f, Settings.GetSampleRate())))) : 1.0f;

			switch (OperatorData->InputBMode)
			{
			case EMetaSoundCATAddInputBMode::Float:
			{
				BScratch.SetNumUninitialized(NumFrames);
				const float Target = *InputBFloat;
				float State = bScalarSlewInitialized ? PrevBScalar : Target;
				for (int32 Frame = 0; Frame < NumFrames; ++Frame)
				{
					State = bUseSlew ? (State + SlewAlpha * (Target - State)) : Target;
					BScratch[Frame] = State;
				}
				PrevBScalar = State;
				bScalarSlewInitialized = true;
				for (int32 Channel = 0; Channel < NumOutChannels; ++Channel)
				{
					const int32 AChannel = (NumAChannels == 1) ? 0 : FMath::Min(Channel, NumAChannels - 1);
					TArrayView<const float> SrcA = InputA->GetChannel(AChannel);
					TArrayView<float> Dst = Output->GetChannel(Channel);
					for (int32 Frame = 0; Frame < NumFrames; ++Frame)
					{
						Dst[Frame] = SrcA[Frame] + BScratch[Frame];
					}
				}
				break;
			}
			case EMetaSoundCATAddInputBMode::MonoAudio:
			{
				BScratch.SetNumUninitialized(NumFrames);
				const float* SrcB = InputBMono->GetData();
				float State = bScalarSlewInitialized ? PrevBScalar : SrcB[0];
				for (int32 Frame = 0; Frame < NumFrames; ++Frame)
				{
					const float Target = SrcB[Frame];
					State = bUseSlew ? (State + SlewAlpha * (Target - State)) : Target;
					BScratch[Frame] = State;
				}
				PrevBScalar = State;
				bScalarSlewInitialized = true;
				for (int32 Channel = 0; Channel < NumOutChannels; ++Channel)
				{
					const int32 AChannel = (NumAChannels == 1) ? 0 : FMath::Min(Channel, NumAChannels - 1);
					TArrayView<const float> SrcA = InputA->GetChannel(AChannel);
					TArrayView<float> Dst = Output->GetChannel(Channel);
					for (int32 Frame = 0; Frame < NumFrames; ++Frame)
					{
						Dst[Frame] = SrcA[Frame] + BScratch[Frame];
					}
				}
				break;
			}
			case EMetaSoundCATAddInputBMode::CAT:
			{
				const int32 NumBChannels = FMath::Max(1, InputBCAT->NumChannels());
				if (PrevBPerChannel.Num() != NumOutChannels)
				{
					PrevBPerChannel.Init(0.0f, NumOutChannels);
					bChannelSlewInitialized = false;
				}
				for (int32 Channel = 0; Channel < NumOutChannels; ++Channel)
				{
					const int32 AChannel = (NumAChannels == 1) ? 0 : FMath::Min(Channel, NumAChannels - 1);
					const int32 BChannel = (NumBChannels == 1) ? 0 : FMath::Min(Channel, NumBChannels - 1);
					TArrayView<const float> SrcA = InputA->GetChannel(AChannel);
					TArrayView<const float> SrcB = InputBCAT->GetChannel(BChannel);
					TArrayView<float> Dst = Output->GetChannel(Channel);
					float State = bChannelSlewInitialized ? PrevBPerChannel[Channel] : SrcB[0];
					for (int32 Frame = 0; Frame < NumFrames; ++Frame)
					{
						const float Target = SrcB[Frame];
						State = bUseSlew ? (State + SlewAlpha * (Target - State)) : Target;
						Dst[Frame] = SrcA[Frame] + State;
					}
					PrevBPerChannel[Channel] = State;
				}
				bChannelSlewInitialized = true;
				break;
			}
			default:
				checkNoEntry();
				break;
			}
		}

	private:
		TSharedPtr<const FCATAddOperatorData> OperatorData;
		FOperatorSettings Settings;
		FChannelAgnosticTypeReadRef InputA;
		FChannelAgnosticTypeReadRef InputBCAT;
		FAudioBufferReadRef InputBMono;
		FFloatReadRef InputBFloat;
		FBoolReadRef InputBSlewEnabled;
		FFloatReadRef InputBSlewTimeMs;
		FChannelAgnosticTypeWriteRef Output;
		TArray<float> BScratch;
		float PrevBScalar = 0.0f;
		bool bScalarSlewInitialized = false;
		TArray<float> PrevBPerChannel;
		bool bChannelSlewInitialized = false;
	};

	using FCATAddNode = TNodeFacade<FCATAddOperator>;
	METASOUND_REGISTER_NODE_AND_CONFIGURATION(FCATAddNode, FMetaSoundCATAddNodeConfiguration);
}

TArray<FPropertyTextFName> UMetaSoundCATAddNodeOptionsHelper::GetSoundFileFormatChannelOptions()
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

FMetaSoundCATAddNodeConfiguration::FMetaSoundCATAddNodeConfiguration()
	: OperatorData(MakeShared<Metasound::CATAddPrivate::FCATAddOperatorData>(CatAudioTypeName, InputBMode))
{
}

TInstancedStruct<FMetasoundFrontendClassInterface> FMetaSoundCATAddNodeConfiguration::OverrideDefaultInterface(const FMetasoundFrontendClass& InClass) const
{
	return TInstancedStruct<FMetasoundFrontendClassInterface>::Make(
		FMetasoundFrontendClassInterface::GenerateClassInterface(
			Metasound::CATAddPrivate::GetVertexInterface(CatAudioTypeName, InputBMode)));
}

TSharedPtr<const Metasound::IOperatorData> FMetaSoundCATAddNodeConfiguration::GetOperatorData() const
{
	OperatorData->CatAudioTypeName = CatAudioTypeName;
	OperatorData->InputBMode = InputBMode;
	return OperatorData;
}

#undef LOCTEXT_NAMESPACE
