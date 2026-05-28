// Copyright 2026 Charles Matthews. All Rights Reserved.

#include "MetasoundBranchesCAT/Public/MetasoundCATCrossfadeNode.h"

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

#define LOCTEXT_NAMESPACE "MetasoundBranches_CATCrossfadeNode"

namespace Metasound
{
	namespace CATCrossfadeVertexNames
	{
		METASOUND_PARAM(InputA, "A", "First CAT input.")
		METASOUND_PARAM(InputB, "B", "Second CAT input.")
		METASOUND_PARAM(Crossfade, "Crossfade", "Crossfade control. Type is configurable.")
		METASOUND_PARAM(Output, "Output", "Crossfaded CAT output.")
	}

	namespace CATCrossfadePrivate
	{
		class FCATCrossfadeOperatorData final : public TOperatorData<FCATCrossfadeOperatorData>
		{
		public:
			static const FLazyName OperatorDataTypeName;

			explicit FCATCrossfadeOperatorData(
				const FName& InCatAudioTypeName,
				const EMetaSoundCATCrossfadeInputMode InCrossfadeMode,
				const float InCrossfadeMin,
				const float InCrossfadeMax,
				const EMetaSoundCATCrossfadePanningLaw InPanningLaw)
				: CatAudioTypeName(InCatAudioTypeName)
				, CrossfadeMode(InCrossfadeMode)
				, CrossfadeMin(InCrossfadeMin)
				, CrossfadeMax(InCrossfadeMax)
				, PanningLaw(InPanningLaw)
			{
			}

			FName CatAudioTypeName;
			EMetaSoundCATCrossfadeInputMode CrossfadeMode;
			float CrossfadeMin = -1.0f;
			float CrossfadeMax = 1.0f;
			EMetaSoundCATCrossfadePanningLaw PanningLaw = EMetaSoundCATCrossfadePanningLaw::EqualPower;
		};

		const FLazyName FCATCrossfadeOperatorData::OperatorDataTypeName = TEXT("FCATCrossfadeOperatorData");

		FVertexInterface GetVertexInterface(const FName& InCatFormat, const EMetaSoundCATCrossfadeInputMode InCrossfadeMode)
		{
			using namespace CATCrossfadeVertexNames;

			FInputVertexInterface Input;
			Input.Add(FInputDataVertex(METASOUND_GET_PARAM_NAME(InputA), InCatFormat, METASOUND_GET_PARAM_METADATA(InputA), EVertexAccessType::Reference));
			Input.Add(FInputDataVertex(METASOUND_GET_PARAM_NAME(InputB), InCatFormat, METASOUND_GET_PARAM_METADATA(InputB), EVertexAccessType::Reference));

			switch (InCrossfadeMode)
			{
			case EMetaSoundCATCrossfadeInputMode::CAT:
				Input.Add(FInputDataVertex(METASOUND_GET_PARAM_NAME(Crossfade), InCatFormat, METASOUND_GET_PARAM_METADATA(Crossfade), EVertexAccessType::Reference));
				break;
			case EMetaSoundCATCrossfadeInputMode::MonoAudio:
				Input.Add(TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(Crossfade)));
				break;
			case EMetaSoundCATCrossfadeInputMode::Float:
				Input.Add(TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(Crossfade), 0.0f));
				break;
			case EMetaSoundCATCrossfadeInputMode::FloatArray:
				Input.Add(TInputDataVertex<TArray<float>>(METASOUND_GET_PARAM_NAME_AND_METADATA(Crossfade)));
				break;
			default:
				checkNoEntry();
				break;
			}

			FOutputVertexInterface OutputInterface;
			OutputInterface.Add(FOutputDataVertex(METASOUND_GET_PARAM_NAME(Output), InCatFormat, METASOUND_GET_PARAM_METADATA(Output), EVertexAccessType::Reference));

			return { MoveTemp(Input), MoveTemp(OutputInterface) };
		}

		float NormalizeCrossfade(const float InValue, const float InMin, const float InMax)
		{
			const float MinValue = FMath::Min(InMin, InMax);
			const float MaxValue = FMath::Max(InMin, InMax);
			const float Range = FMath::Max(KINDA_SMALL_NUMBER, MaxValue - MinValue);
			const float Clamped = FMath::Clamp(InValue, MinValue, MaxValue);
			return (Clamped - MinValue) / Range;
		}

		void ComputeGains(const float InT, const EMetaSoundCATCrossfadePanningLaw InLaw, float& OutGainA, float& OutGainB)
		{
			const float T = FMath::Clamp(InT, 0.0f, 1.0f);
			switch (InLaw)
			{
			case EMetaSoundCATCrossfadePanningLaw::Linear:
				OutGainA = 1.0f - T;
				OutGainB = T;
				break;
			case EMetaSoundCATCrossfadePanningLaw::EqualPower:
			default:
				OutGainA = FMath::Cos(T * HALF_PI);
				OutGainB = FMath::Sin(T * HALF_PI);
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
	}

	class FCATCrossfadeOperator final : public TExecutableOperator<FCATCrossfadeOperator>
	{
	public:
		using FCATCrossfadeOperatorData = CATCrossfadePrivate::FCATCrossfadeOperatorData;

		FCATCrossfadeOperator(
			const TSharedPtr<const FCATCrossfadeOperatorData>& InOperatorData,
			const FOperatorSettings& InSettings,
			FChannelAgnosticTypeReadRef&& InInputA,
			FChannelAgnosticTypeReadRef&& InInputB,
			FChannelAgnosticTypeReadRef&& InCrossfadeCAT,
			FAudioBufferReadRef&& InCrossfadeMono,
			FFloatReadRef&& InCrossfadeFloat,
			TDataReadReference<TArray<float>>&& InCrossfadeFloatArray,
			FChannelAgnosticTypeWriteRef&& InOutput)
			: OperatorData(InOperatorData)
			, Settings(InSettings)
			, InputA(MoveTemp(InInputA))
			, InputB(MoveTemp(InInputB))
			, CrossfadeCAT(MoveTemp(InCrossfadeCAT))
			, CrossfadeMono(MoveTemp(InCrossfadeMono))
			, CrossfadeFloat(MoveTemp(InCrossfadeFloat))
			, CrossfadeFloatArray(MoveTemp(InCrossfadeFloatArray))
			, Output(MoveTemp(InOutput))
		{
		}

		static const FVertexInterface& DeclareVertexInterface()
		{
			static const FVertexInterface Interface = CATCrossfadePrivate::GetVertexInterface(TEXT("Cat:Stereo2Dot0"), EMetaSoundCATCrossfadeInputMode::Float);
			return Interface;
		}

		static FNodeClassMetadata GetNodeInfo()
		{
			FNodeClassMetadata Metadata;
			Metadata.ClassName = { TEXT("Experimental"), TEXT("CATCrossfade"), TEXT("Audio") };
			Metadata.MajorVersion = 1;
			Metadata.MinorVersion = 0;
			Metadata.DisplayName = LOCTEXT("CATCrossfadeDisplayName", "CAT Crossfade");
			Metadata.Description = LOCTEXT("CATCrossfadeDescription", "Crossfades between two CAT inputs with configurable control input type, clamp range, and panning law.");
			Metadata.Author = TEXT("Charles Matthews");
			Metadata.PromptIfMissing = LOCTEXT("CATCrossfadeMissingPrompt", "Enable MetaSoundExperimental for CAT channel format schemas.");
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
			using namespace CATCrossfadeVertexNames;

			const FCATCrossfadeOperatorData* ConfigData = CastOperatorData<const FCATCrossfadeOperatorData>(InParams.Node.GetOperatorData().Get());
			if (!ConfigData)
			{
				return MakeUnique<FNoOpOperator>();
			}

			const Audio::FChannelTypeFamily* ConcreteType = Audio::GetChannelRegistry().FindConcreteChannel(ConfigData->CatAudioTypeName);
			if (!ConcreteType)
			{
				return MakeUnique<FNoOpOperator>();
			}

			const TSharedPtr<const FCATCrossfadeOperatorData>& OperatorDataSharedPtr = StaticCastSharedPtr<const FCATCrossfadeOperatorData>(InParams.Node.GetOperatorData());

			FChannelAgnosticTypeReadRef InA = InParams.InputData.GetOrCreateDefaultDataReadReference<FChannelAgnosticType>(METASOUND_GET_PARAM_NAME(InputA), InParams.OperatorSettings);
			FChannelAgnosticTypeReadRef InB = InParams.InputData.GetOrCreateDefaultDataReadReference<FChannelAgnosticType>(METASOUND_GET_PARAM_NAME(InputB), InParams.OperatorSettings);

			FChannelAgnosticTypeReadRef InCrossfadeCat = FChannelAgnosticTypeWriteRef::CreateNew(InParams.OperatorSettings, ConcreteType->GetName());
			FAudioBufferReadRef InCrossfadeMono = FAudioBufferWriteRef::CreateNew(InParams.OperatorSettings);
			FFloatReadRef InCrossfadeFloat = TDataReadReference<float>::CreateNew(0.0f);
			TDataReadReference<TArray<float>> InCrossfadeFloatArray = TDataReadReference<TArray<float>>::CreateNew(TArray<float>{});

			switch (ConfigData->CrossfadeMode)
			{
			case EMetaSoundCATCrossfadeInputMode::CAT:
				InCrossfadeCat = InParams.InputData.GetOrCreateDefaultDataReadReference<FChannelAgnosticType>(METASOUND_GET_PARAM_NAME(Crossfade), InParams.OperatorSettings);
				break;
			case EMetaSoundCATCrossfadeInputMode::MonoAudio:
				InCrossfadeMono = InParams.InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(Crossfade), InParams.OperatorSettings);
				break;
			case EMetaSoundCATCrossfadeInputMode::Float:
				InCrossfadeFloat = InParams.InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(Crossfade), InParams.OperatorSettings);
				break;
			case EMetaSoundCATCrossfadeInputMode::FloatArray:
				InCrossfadeFloatArray = InParams.InputData.GetOrCreateDefaultDataReadReference<TArray<float>>(METASOUND_GET_PARAM_NAME(Crossfade), InParams.OperatorSettings);
				break;
			default:
				checkNoEntry();
				break;
			}

			FChannelAgnosticTypeWriteRef Out = FChannelAgnosticTypeWriteRef::CreateNew(InParams.OperatorSettings, ConcreteType->GetName());

			return MakeUnique<FCATCrossfadeOperator>(
				OperatorDataSharedPtr,
				InParams.OperatorSettings,
				MoveTemp(InA),
				MoveTemp(InB),
				MoveTemp(InCrossfadeCat),
				MoveTemp(InCrossfadeMono),
				MoveTemp(InCrossfadeFloat),
				MoveTemp(InCrossfadeFloatArray),
				MoveTemp(Out));
		}

		virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
		{
			using namespace CATCrossfadeVertexNames;

			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputA), InputA);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputB), InputB);

			switch (OperatorData->CrossfadeMode)
			{
			case EMetaSoundCATCrossfadeInputMode::CAT:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Crossfade), CrossfadeCAT);
				break;
			case EMetaSoundCATCrossfadeInputMode::MonoAudio:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Crossfade), CrossfadeMono);
				break;
			case EMetaSoundCATCrossfadeInputMode::Float:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Crossfade), CrossfadeFloat);
				break;
			case EMetaSoundCATCrossfadeInputMode::FloatArray:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Crossfade), CrossfadeFloatArray);
				break;
			default:
				checkNoEntry();
				break;
			}
		}

		virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
		{
			using namespace CATCrossfadeVertexNames;
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
			const int32 NumBChannels = FMath::Max(1, InputB->NumChannels());

			const float MinCrossfade = OperatorData->CrossfadeMin;
			const float MaxCrossfade = OperatorData->CrossfadeMax;
			const EMetaSoundCATCrossfadePanningLaw Law = OperatorData->PanningLaw;

			switch (OperatorData->CrossfadeMode)
			{
			case EMetaSoundCATCrossfadeInputMode::Float:
			{
				const float T = CATCrossfadePrivate::NormalizeCrossfade(*CrossfadeFloat, MinCrossfade, MaxCrossfade);
				float GainA = 0.0f;
				float GainB = 0.0f;
				CATCrossfadePrivate::ComputeGains(T, Law, GainA, GainB);

				for (int32 Channel = 0; Channel < NumOutChannels; ++Channel)
				{
					const int32 AChannel = (NumAChannels == 1) ? 0 : FMath::Min(Channel, NumAChannels - 1);
					const int32 BChannel = (NumBChannels == 1) ? 0 : FMath::Min(Channel, NumBChannels - 1);
					TArrayView<const float> SrcA = InputA->GetChannel(AChannel);
					TArrayView<const float> SrcB = InputB->GetChannel(BChannel);
					TArrayView<float> Dst = Output->GetChannel(Channel);
					for (int32 Frame = 0; Frame < NumFrames; ++Frame)
					{
						Dst[Frame] = GainA * SrcA[Frame] + GainB * SrcB[Frame];
					}
				}
				break;
			}
			case EMetaSoundCATCrossfadeInputMode::MonoAudio:
			{
				const float* CrossfadeData = CrossfadeMono->GetData();
				for (int32 Channel = 0; Channel < NumOutChannels; ++Channel)
				{
					const int32 AChannel = (NumAChannels == 1) ? 0 : FMath::Min(Channel, NumAChannels - 1);
					const int32 BChannel = (NumBChannels == 1) ? 0 : FMath::Min(Channel, NumBChannels - 1);
					TArrayView<const float> SrcA = InputA->GetChannel(AChannel);
					TArrayView<const float> SrcB = InputB->GetChannel(BChannel);
					TArrayView<float> Dst = Output->GetChannel(Channel);
					for (int32 Frame = 0; Frame < NumFrames; ++Frame)
					{
						const float T = CATCrossfadePrivate::NormalizeCrossfade(CrossfadeData[Frame], MinCrossfade, MaxCrossfade);
						float GainA = 0.0f;
						float GainB = 0.0f;
						CATCrossfadePrivate::ComputeGains(T, Law, GainA, GainB);
						Dst[Frame] = GainA * SrcA[Frame] + GainB * SrcB[Frame];
					}
				}
				break;
			}
			case EMetaSoundCATCrossfadeInputMode::CAT:
			{
				const int32 NumCrossfadeChannels = FMath::Max(1, CrossfadeCAT->NumChannels());
				for (int32 Channel = 0; Channel < NumOutChannels; ++Channel)
				{
					const int32 AChannel = (NumAChannels == 1) ? 0 : FMath::Min(Channel, NumAChannels - 1);
					const int32 BChannel = (NumBChannels == 1) ? 0 : FMath::Min(Channel, NumBChannels - 1);
					const int32 CChannel = (NumCrossfadeChannels == 1) ? 0 : FMath::Min(Channel, NumCrossfadeChannels - 1);
					TArrayView<const float> SrcA = InputA->GetChannel(AChannel);
					TArrayView<const float> SrcB = InputB->GetChannel(BChannel);
					TArrayView<const float> CrossfadeSrc = CrossfadeCAT->GetChannel(CChannel);
					TArrayView<float> Dst = Output->GetChannel(Channel);
					for (int32 Frame = 0; Frame < NumFrames; ++Frame)
					{
						const float T = CATCrossfadePrivate::NormalizeCrossfade(CrossfadeSrc[Frame], MinCrossfade, MaxCrossfade);
						float GainA = 0.0f;
						float GainB = 0.0f;
						CATCrossfadePrivate::ComputeGains(T, Law, GainA, GainB);
						Dst[Frame] = GainA * SrcA[Frame] + GainB * SrcB[Frame];
					}
				}
				break;
			}
			case EMetaSoundCATCrossfadeInputMode::FloatArray:
			{
				const TArray<float>& Values = *CrossfadeFloatArray;
				for (int32 Channel = 0; Channel < NumOutChannels; ++Channel)
				{
					const int32 AChannel = (NumAChannels == 1) ? 0 : FMath::Min(Channel, NumAChannels - 1);
					const int32 BChannel = (NumBChannels == 1) ? 0 : FMath::Min(Channel, NumBChannels - 1);
					const float ChannelCrossfade = CATCrossfadePrivate::GetArrayValue(Values, Channel, 0.0f);
					const float T = CATCrossfadePrivate::NormalizeCrossfade(ChannelCrossfade, MinCrossfade, MaxCrossfade);
					float GainA = 0.0f;
					float GainB = 0.0f;
					CATCrossfadePrivate::ComputeGains(T, Law, GainA, GainB);
					TArrayView<const float> SrcA = InputA->GetChannel(AChannel);
					TArrayView<const float> SrcB = InputB->GetChannel(BChannel);
					TArrayView<float> Dst = Output->GetChannel(Channel);
					for (int32 Frame = 0; Frame < NumFrames; ++Frame)
					{
						Dst[Frame] = GainA * SrcA[Frame] + GainB * SrcB[Frame];
					}
				}
				break;
			}
			default:
				checkNoEntry();
				break;
			}
		}

	private:
		TSharedPtr<const FCATCrossfadeOperatorData> OperatorData;
		FOperatorSettings Settings;
		FChannelAgnosticTypeReadRef InputA;
		FChannelAgnosticTypeReadRef InputB;
		FChannelAgnosticTypeReadRef CrossfadeCAT;
		FAudioBufferReadRef CrossfadeMono;
		FFloatReadRef CrossfadeFloat;
		TDataReadReference<TArray<float>> CrossfadeFloatArray;
		FChannelAgnosticTypeWriteRef Output;
	};

	using FCATCrossfadeNode = TNodeFacade<FCATCrossfadeOperator>;
	METASOUND_REGISTER_NODE_AND_CONFIGURATION(FCATCrossfadeNode, FMetaSoundCATCrossfadeNodeConfiguration);
}

TArray<FPropertyTextFName> UMetaSoundCATCrossfadeNodeOptionsHelper::GetSoundFileFormatChannelOptions()
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

FMetaSoundCATCrossfadeNodeConfiguration::FMetaSoundCATCrossfadeNodeConfiguration()
	: OperatorData(MakeShared<Metasound::CATCrossfadePrivate::FCATCrossfadeOperatorData>(CatAudioTypeName, CrossfadeMode, CrossfadeMin, CrossfadeMax, PanningLaw))
{
}

TInstancedStruct<FMetasoundFrontendClassInterface> FMetaSoundCATCrossfadeNodeConfiguration::OverrideDefaultInterface(const FMetasoundFrontendClass& InClass) const
{
	return TInstancedStruct<FMetasoundFrontendClassInterface>::Make(
		FMetasoundFrontendClassInterface::GenerateClassInterface(
			Metasound::CATCrossfadePrivate::GetVertexInterface(CatAudioTypeName, CrossfadeMode)));
}

TSharedPtr<const Metasound::IOperatorData> FMetaSoundCATCrossfadeNodeConfiguration::GetOperatorData() const
{
	OperatorData->CatAudioTypeName = CatAudioTypeName;
	OperatorData->CrossfadeMode = CrossfadeMode;
	OperatorData->CrossfadeMin = CrossfadeMin;
	OperatorData->CrossfadeMax = CrossfadeMax;
	OperatorData->PanningLaw = PanningLaw;
	return OperatorData;
}

#undef LOCTEXT_NAMESPACE
