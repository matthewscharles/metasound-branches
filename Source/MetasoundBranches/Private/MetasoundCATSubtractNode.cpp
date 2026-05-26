// Copyright 2026 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundCATSubtractNode.h"

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

#define LOCTEXT_NAMESPACE "MetasoundBranches_CATSubtractNode"

namespace Metasound
{
	namespace CATSubtractVertexNames
	{
		METASOUND_PARAM(InputA, "A", "First CAT input.")
		METASOUND_PARAM(InputB, "B", "Second input. Type is configurable.")
		METASOUND_PARAM(InputBSlewEnabled, "Slew B", "Enable one-pole slew smoothing on input B.")
		METASOUND_PARAM(InputBSlewTimeMs, "B Slew Time (ms)", "Slew time in milliseconds for input B.")
		METASOUND_PARAM(Output, "Output", "CAT subtract result.")
	}

	namespace CATSubtractPrivate
	{
		class FCATSubtractOperatorData final : public TOperatorData<FCATSubtractOperatorData>
		{
		public:
			static const FLazyName OperatorDataTypeName;

			explicit FCATSubtractOperatorData(const FName& InCatAudioTypeName, const EMetaSoundCATSubtractInputBMode InInputBMode)
				: CatAudioTypeName(InCatAudioTypeName)
				, InputBMode(InInputBMode)
			{
			}

			FName CatAudioTypeName;
			EMetaSoundCATSubtractInputBMode InputBMode;
		};

		const FLazyName FCATSubtractOperatorData::OperatorDataTypeName = TEXT("FCATSubtractOperatorData");

		FVertexInterface GetVertexInterface(const FName& InCatFormat, const EMetaSoundCATSubtractInputBMode InInputBMode)
		{
			using namespace CATSubtractVertexNames;

			FInputVertexInterface Input;
			Input.Add(FInputDataVertex(METASOUND_GET_PARAM_NAME(InputA), InCatFormat, METASOUND_GET_PARAM_METADATA(InputA), EVertexAccessType::Reference));

			switch (InInputBMode)
			{
			case EMetaSoundCATSubtractInputBMode::CAT:
				Input.Add(FInputDataVertex(METASOUND_GET_PARAM_NAME(InputB), InCatFormat, METASOUND_GET_PARAM_METADATA(InputB), EVertexAccessType::Reference));
				break;
			case EMetaSoundCATSubtractInputBMode::MonoAudio:
				Input.Add(TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputB)));
				break;
			case EMetaSoundCATSubtractInputBMode::Float:
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

	class FCATSubtractOperator final : public TExecutableOperator<FCATSubtractOperator>
	{
	public:
		using FCATSubtractOperatorData = CATSubtractPrivate::FCATSubtractOperatorData;

		FCATSubtractOperator(
			const TSharedPtr<const FCATSubtractOperatorData>& InOperatorData,
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
			static const FVertexInterface Interface = CATSubtractPrivate::GetVertexInterface(TEXT("Cat:Stereo2Dot0"), EMetaSoundCATSubtractInputBMode::MonoAudio);
			return Interface;
		}

		static FNodeClassMetadata GetNodeInfo()
		{
			FNodeClassMetadata Metadata;
			Metadata.ClassName = { TEXT("Experimental"), TEXT("CATSubtract"), TEXT("Audio") };
			Metadata.MajorVersion = 1;
			Metadata.MinorVersion = 0;
			Metadata.DisplayName = LOCTEXT("CATSubtractDisplayName", "CAT Subtract");
			Metadata.Description = LOCTEXT("CATSubtractDescription", "Subtracts CAT, mono audio, or float input from a CAT signal.");
			Metadata.Author = TEXT("Charles Matthews");
			Metadata.PromptIfMissing = LOCTEXT("CATSubtractMissingPrompt", "Enable MetaSoundExperimental for CAT channel format schemas.");
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
			using namespace CATSubtractVertexNames;

			const FCATSubtractOperatorData* ConfigData = CastOperatorData<const FCATSubtractOperatorData>(InParams.Node.GetOperatorData().Get());
			if (!ConfigData)
			{
				return MakeUnique<FNoOpOperator>();
			}

			const Audio::FChannelTypeFamily* ConcreteType = Audio::GetChannelRegistry().FindConcreteChannel(ConfigData->CatAudioTypeName);
			if (!ConcreteType)
			{
				return MakeUnique<FNoOpOperator>();
			}

			const TSharedPtr<const FCATSubtractOperatorData>& OperatorDataSharedPtr = StaticCastSharedPtr<const FCATSubtractOperatorData>(InParams.Node.GetOperatorData());

			FChannelAgnosticTypeReadRef InA = InParams.InputData.GetOrCreateDefaultDataReadReference<FChannelAgnosticType>(METASOUND_GET_PARAM_NAME(InputA), InParams.OperatorSettings);

			FChannelAgnosticTypeReadRef InBCat = FChannelAgnosticTypeWriteRef::CreateNew(InParams.OperatorSettings, ConcreteType->GetName());
			FAudioBufferReadRef InBMono = FAudioBufferWriteRef::CreateNew(InParams.OperatorSettings);
			FFloatReadRef InBFloat = TDataReadReference<float>::CreateNew(1.0f);
			FBoolReadRef InBSlewEnabled = InParams.InputData.GetOrCreateDefaultDataReadReference<bool>(METASOUND_GET_PARAM_NAME(InputBSlewEnabled), InParams.OperatorSettings);
			FFloatReadRef InBSlewTimeMs = InParams.InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputBSlewTimeMs), InParams.OperatorSettings);

			switch (ConfigData->InputBMode)
			{
			case EMetaSoundCATSubtractInputBMode::CAT:
				InBCat = InParams.InputData.GetOrCreateDefaultDataReadReference<FChannelAgnosticType>(METASOUND_GET_PARAM_NAME(InputB), InParams.OperatorSettings);
				break;
			case EMetaSoundCATSubtractInputBMode::MonoAudio:
				InBMono = InParams.InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(InputB), InParams.OperatorSettings);
				break;
			case EMetaSoundCATSubtractInputBMode::Float:
				InBFloat = InParams.InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputB), InParams.OperatorSettings);
				break;
			default:
				checkNoEntry();
				break;
			}

			FChannelAgnosticTypeWriteRef Out = FChannelAgnosticTypeWriteRef::CreateNew(InParams.OperatorSettings, ConcreteType->GetName());

			return MakeUnique<FCATSubtractOperator>(
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
			using namespace CATSubtractVertexNames;

			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputA), InputA);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputBSlewEnabled), InputBSlewEnabled);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputBSlewTimeMs), InputBSlewTimeMs);

			switch (OperatorData->InputBMode)
			{
			case EMetaSoundCATSubtractInputBMode::CAT:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputB), InputBCAT);
				break;
			case EMetaSoundCATSubtractInputBMode::MonoAudio:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputB), InputBMono);
				break;
			case EMetaSoundCATSubtractInputBMode::Float:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputB), InputBFloat);
				break;
			default:
				checkNoEntry();
				break;
			}
		}

		virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
		{
			using namespace CATSubtractVertexNames;
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
			case EMetaSoundCATSubtractInputBMode::Float:
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
						Dst[Frame] = SrcA[Frame] - BScratch[Frame];
					}
				}
				break;
			}
			case EMetaSoundCATSubtractInputBMode::MonoAudio:
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
						Dst[Frame] = SrcA[Frame] - BScratch[Frame];
					}
				}
				break;
			}
			case EMetaSoundCATSubtractInputBMode::CAT:
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
						Dst[Frame] = SrcA[Frame] - State;
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
		TSharedPtr<const FCATSubtractOperatorData> OperatorData;
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

	using FCATSubtractNode = TNodeFacade<FCATSubtractOperator>;
	METASOUND_REGISTER_NODE_AND_CONFIGURATION(FCATSubtractNode, FMetaSoundCATSubtractNodeConfiguration);
}

TArray<FPropertyTextFName> UMetaSoundCATSubtractNodeOptionsHelper::GetSoundFileFormatChannelOptions()
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

FMetaSoundCATSubtractNodeConfiguration::FMetaSoundCATSubtractNodeConfiguration()
	: OperatorData(MakeShared<Metasound::CATSubtractPrivate::FCATSubtractOperatorData>(CatAudioTypeName, InputBMode))
{
}

TInstancedStruct<FMetasoundFrontendClassInterface> FMetaSoundCATSubtractNodeConfiguration::OverrideDefaultInterface(const FMetasoundFrontendClass& InClass) const
{
	return TInstancedStruct<FMetasoundFrontendClassInterface>::Make(
		FMetasoundFrontendClassInterface::GenerateClassInterface(
			Metasound::CATSubtractPrivate::GetVertexInterface(CatAudioTypeName, InputBMode)));
}

TSharedPtr<const Metasound::IOperatorData> FMetaSoundCATSubtractNodeConfiguration::GetOperatorData() const
{
	OperatorData->CatAudioTypeName = CatAudioTypeName;
	OperatorData->InputBMode = InputBMode;
	return OperatorData;
}

#undef LOCTEXT_NAMESPACE
