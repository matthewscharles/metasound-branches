// Copyright 2026 Charles Matthews. All Rights Reserved.

#include "MetasoundBranchesCAT/Public/MetasoundCATDivideNode.h"

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

#define LOCTEXT_NAMESPACE "MetasoundBranches_CATDivideNode"

namespace Metasound
{
	namespace CATDivideVertexNames
	{
		METASOUND_PARAM(InputA, "A", "First CAT input.")
		METASOUND_PARAM(InputB, "B", "Second input. Type is configurable.")
		METASOUND_PARAM(InputBSlewEnabled, "Slew B", "Enable one-pole slew smoothing on input B.")
		METASOUND_PARAM(InputBRiseTime, "B Rise Time", "Rise time in seconds for input B slew.")
		METASOUND_PARAM(InputBFallTime, "B Fall Time", "Fall time in seconds for input B slew.")
		METASOUND_PARAM(Output, "Output", "CAT divide result.")
	}

	namespace CATDividePrivate
	{
		class FCATDivideOperatorData final : public TOperatorData<FCATDivideOperatorData>
		{
		public:
			static const FLazyName OperatorDataTypeName;

			explicit FCATDivideOperatorData(const FName& InCatAudioTypeName, const EMetaSoundCATDivideInputBMode InInputBMode)
				: CatAudioTypeName(InCatAudioTypeName)
				, InputBMode(InInputBMode)
			{
			}

			FName CatAudioTypeName;
			EMetaSoundCATDivideInputBMode InputBMode;
		};

		const FLazyName FCATDivideOperatorData::OperatorDataTypeName = TEXT("FCATDivideOperatorData");

		FVertexInterface GetVertexInterface(const FName& InCatFormat, const EMetaSoundCATDivideInputBMode InInputBMode)
		{
			using namespace CATDivideVertexNames;

			FInputVertexInterface Input;
			Input.Add(FInputDataVertex(METASOUND_GET_PARAM_NAME(InputA), InCatFormat, METASOUND_GET_PARAM_METADATA(InputA), EVertexAccessType::Reference));

			switch (InInputBMode)
			{
			case EMetaSoundCATDivideInputBMode::CAT:
				Input.Add(FInputDataVertex(METASOUND_GET_PARAM_NAME(InputB), InCatFormat, METASOUND_GET_PARAM_METADATA(InputB), EVertexAccessType::Reference));
				break;
			case EMetaSoundCATDivideInputBMode::MonoAudio:
				Input.Add(TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputB)));
				break;
			case EMetaSoundCATDivideInputBMode::Float:
				Input.Add(TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputB), 1.0f));
				break;
			case EMetaSoundCATDivideInputBMode::FloatArray:
				Input.Add(TInputDataVertex<TArray<float>>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputB)));
				break;
			default:
				checkNoEntry();
				break;
			}

			Input.Add(TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputBSlewEnabled), false));
			Input.Add(TInputDataVertex<FTime>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputBRiseTime)));
			Input.Add(TInputDataVertex<FTime>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputBFallTime)));

			FOutputVertexInterface OutputInterface;
			OutputInterface.Add(FOutputDataVertex(METASOUND_GET_PARAM_NAME(Output), InCatFormat, METASOUND_GET_PARAM_METADATA(Output), EVertexAccessType::Reference));

			return { MoveTemp(Input), MoveTemp(OutputInterface) };
		}
	}

	class FCATDivideOperator final : public TExecutableOperator<FCATDivideOperator>
	{
	public:
		using FCATDivideOperatorData = CATDividePrivate::FCATDivideOperatorData;

		FCATDivideOperator(
			const TSharedPtr<const FCATDivideOperatorData>& InOperatorData,
			const FOperatorSettings& InSettings,
			FChannelAgnosticTypeReadRef&& InInputA,
			FChannelAgnosticTypeReadRef&& InInputBCAT,
			FAudioBufferReadRef&& InInputBMono,
			FFloatReadRef&& InInputBFloat,
			TDataReadReference<TArray<float>>&& InInputBFloatArray,
			FBoolReadRef&& InInputBSlewEnabled,
			FTimeReadRef&& InInputBRiseTime,
			FTimeReadRef&& InInputBFallTime,
			FChannelAgnosticTypeWriteRef&& InOutput)
			: OperatorData(InOperatorData)
			, Settings(InSettings)
			, InputA(MoveTemp(InInputA))
			, InputBCAT(MoveTemp(InInputBCAT))
			, InputBMono(MoveTemp(InInputBMono))
			, InputBFloat(MoveTemp(InInputBFloat))
			, InputBFloatArray(MoveTemp(InInputBFloatArray))
			, InputBSlewEnabled(MoveTemp(InInputBSlewEnabled))
			, InputBRiseTime(MoveTemp(InInputBRiseTime))
			, InputBFallTime(MoveTemp(InInputBFallTime))
			, Output(MoveTemp(InOutput))
		{
		}

		static const FVertexInterface& DeclareVertexInterface()
		{
			static const FVertexInterface Interface = CATDividePrivate::GetVertexInterface(TEXT("Cat:Stereo2Dot0"), EMetaSoundCATDivideInputBMode::MonoAudio);
			return Interface;
		}

		static FNodeClassMetadata GetNodeInfo()
		{
			FNodeClassMetadata Metadata;
			Metadata.ClassName = { TEXT("Experimental"), TEXT("CATDivide"), TEXT("Audio") };
			Metadata.MajorVersion = 1;
			Metadata.MinorVersion = 0;
			Metadata.DisplayName = LOCTEXT("CATDivideDisplayName", "CAT Divide");
			Metadata.Description = LOCTEXT("CATDivideDescription", "Divides a CAT signal by CAT, mono audio, float, or float array input.");
			Metadata.Author = TEXT("Charles Matthews");
			Metadata.PromptIfMissing = LOCTEXT("CATDivideMissingPrompt", "Enable MetaSoundExperimental for CAT channel format schemas.");
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
			using namespace CATDivideVertexNames;

			const FCATDivideOperatorData* ConfigData = CastOperatorData<const FCATDivideOperatorData>(InParams.Node.GetOperatorData().Get());
			if (!ConfigData)
			{
				return MakeUnique<FNoOpOperator>();
			}

			const Audio::FChannelTypeFamily* ConcreteType = Audio::GetChannelRegistry().FindConcreteChannel(ConfigData->CatAudioTypeName);
			if (!ConcreteType)
			{
				return MakeUnique<FNoOpOperator>();
			}

			const TSharedPtr<const FCATDivideOperatorData>& OperatorDataSharedPtr = StaticCastSharedPtr<const FCATDivideOperatorData>(InParams.Node.GetOperatorData());

			FChannelAgnosticTypeReadRef InA = InParams.InputData.GetOrCreateDefaultDataReadReference<FChannelAgnosticType>(METASOUND_GET_PARAM_NAME(InputA), InParams.OperatorSettings);

			FChannelAgnosticTypeReadRef InBCat = FChannelAgnosticTypeWriteRef::CreateNew(InParams.OperatorSettings, ConcreteType->GetName());
			FAudioBufferReadRef InBMono = FAudioBufferWriteRef::CreateNew(InParams.OperatorSettings);
			FFloatReadRef InBFloat = TDataReadReference<float>::CreateNew(1.0f);
			TDataReadReference<TArray<float>> InBFloatArray = TDataReadReference<TArray<float>>::CreateNew(TArray<float>{});
			FBoolReadRef InBSlewEnabled = InParams.InputData.GetOrCreateDefaultDataReadReference<bool>(METASOUND_GET_PARAM_NAME(InputBSlewEnabled), InParams.OperatorSettings);
			FTimeReadRef InBRiseTime = InParams.InputData.GetOrCreateDefaultDataReadReference<FTime>(METASOUND_GET_PARAM_NAME(InputBRiseTime), InParams.OperatorSettings);
			FTimeReadRef InBFallTime = InParams.InputData.GetOrCreateDefaultDataReadReference<FTime>(METASOUND_GET_PARAM_NAME(InputBFallTime), InParams.OperatorSettings);

			switch (ConfigData->InputBMode)
			{
			case EMetaSoundCATDivideInputBMode::CAT:
				InBCat = InParams.InputData.GetOrCreateDefaultDataReadReference<FChannelAgnosticType>(METASOUND_GET_PARAM_NAME(InputB), InParams.OperatorSettings);
				break;
			case EMetaSoundCATDivideInputBMode::MonoAudio:
				InBMono = InParams.InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(InputB), InParams.OperatorSettings);
				break;
			case EMetaSoundCATDivideInputBMode::Float:
				InBFloat = InParams.InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputB), InParams.OperatorSettings);
				break;
			case EMetaSoundCATDivideInputBMode::FloatArray:
				InBFloatArray = InParams.InputData.GetOrCreateDefaultDataReadReference<TArray<float>>(METASOUND_GET_PARAM_NAME(InputB), InParams.OperatorSettings);
				break;
			default:
				checkNoEntry();
				break;
			}

			FChannelAgnosticTypeWriteRef Out = FChannelAgnosticTypeWriteRef::CreateNew(InParams.OperatorSettings, ConcreteType->GetName());

			return MakeUnique<FCATDivideOperator>(
				OperatorDataSharedPtr,
				InParams.OperatorSettings,
				MoveTemp(InA),
				MoveTemp(InBCat),
				MoveTemp(InBMono),
				MoveTemp(InBFloat),
				MoveTemp(InBFloatArray),
				MoveTemp(InBSlewEnabled),
				MoveTemp(InBRiseTime),
				MoveTemp(InBFallTime),
				MoveTemp(Out));
		}

		virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
		{
			using namespace CATDivideVertexNames;

			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputA), InputA);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputBSlewEnabled), InputBSlewEnabled);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputBRiseTime), InputBRiseTime);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputBFallTime), InputBFallTime);

			switch (OperatorData->InputBMode)
			{
			case EMetaSoundCATDivideInputBMode::CAT:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputB), InputBCAT);
				break;
			case EMetaSoundCATDivideInputBMode::MonoAudio:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputB), InputBMono);
				break;
			case EMetaSoundCATDivideInputBMode::Float:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputB), InputBFloat);
				break;
			case EMetaSoundCATDivideInputBMode::FloatArray:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputB), InputBFloatArray);
				break;
			default:
				checkNoEntry();
				break;
			}
		}

		virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
		{
			using namespace CATDivideVertexNames;
			InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(Output), Output);
		}

		void Execute()
		{
			Output->Zero();
			constexpr float DenominatorEpsilon = 1.0e-6f;

			const int32 NumFrames = Settings.GetNumFramesPerBlock();
			const int32 NumOutChannels = Output->NumChannels();
			if (NumOutChannels <= 0 || NumFrames <= 0)
			{
				return;
			}

			const int32 NumAChannels = FMath::Max(1, InputA->NumChannels());

			const bool bUseSlew = *InputBSlewEnabled;
			const float RiseSeconds = FMath::Max(0.0f, (float)InputBRiseTime->GetSeconds());
			const float FallSeconds = FMath::Max(0.0f, (float)InputBFallTime->GetSeconds());
			const float SampleRate = FMath::Max(1.0f, Settings.GetSampleRate());
			const float RiseAlpha = (RiseSeconds > 0.0f) ? FMath::Exp(-1.0f / (RiseSeconds * SampleRate)) : 0.0f;
			const float FallAlpha = (FallSeconds > 0.0f) ? FMath::Exp(-1.0f / (FallSeconds * SampleRate)) : 0.0f;

			switch (OperatorData->InputBMode)
			{
			case EMetaSoundCATDivideInputBMode::Float:
			{
				BScratch.SetNumUninitialized(NumFrames);
				const float Target = *InputBFloat;
				float State = bScalarSlewInitialized ? PrevBScalar : Target;
				for (int32 Frame = 0; Frame < NumFrames; ++Frame)
				{
					if (bUseSlew)
					{
						if (Target > State)
						{
							State = RiseAlpha * State + (1.0f - RiseAlpha) * Target;
						}
						else if (Target < State)
						{
							State = FallAlpha * State + (1.0f - FallAlpha) * Target;
						}
					}
					else
					{
						State = Target;
					}
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
						const float Denom = BScratch[Frame];
						const float SafeDenom = (FMath::Abs(Denom) < DenominatorEpsilon) ? ((Denom < 0.0f) ? -DenominatorEpsilon : DenominatorEpsilon) : Denom;
						Dst[Frame] = SrcA[Frame] / SafeDenom;
					}
				}
				break;
			}
			case EMetaSoundCATDivideInputBMode::MonoAudio:
			{
				BScratch.SetNumUninitialized(NumFrames);
				const float* SrcB = InputBMono->GetData();
				float State = bScalarSlewInitialized ? PrevBScalar : SrcB[0];
				for (int32 Frame = 0; Frame < NumFrames; ++Frame)
				{
					const float Target = SrcB[Frame];
					if (bUseSlew)
					{
						if (Target > State)
						{
							State = RiseAlpha * State + (1.0f - RiseAlpha) * Target;
						}
						else if (Target < State)
						{
							State = FallAlpha * State + (1.0f - FallAlpha) * Target;
						}
					}
					else
					{
						State = Target;
					}
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
						const float Denom = BScratch[Frame];
						const float SafeDenom = (FMath::Abs(Denom) < DenominatorEpsilon) ? ((Denom < 0.0f) ? -DenominatorEpsilon : DenominatorEpsilon) : Denom;
						Dst[Frame] = SrcA[Frame] / SafeDenom;
					}
				}
				break;
			}
			case EMetaSoundCATDivideInputBMode::CAT:
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
						if (bUseSlew)
						{
							if (Target > State)
							{
								State = RiseAlpha * State + (1.0f - RiseAlpha) * Target;
							}
							else if (Target < State)
							{
								State = FallAlpha * State + (1.0f - FallAlpha) * Target;
							}
						}
						else
						{
							State = Target;
						}
						const float SafeDenom = (FMath::Abs(State) < DenominatorEpsilon) ? ((State < 0.0f) ? -DenominatorEpsilon : DenominatorEpsilon) : State;
						Dst[Frame] = SrcA[Frame] / SafeDenom;
					}
					PrevBPerChannel[Channel] = State;
				}
				bChannelSlewInitialized = true;
				break;
			}
			case EMetaSoundCATDivideInputBMode::FloatArray:
			{
				const TArray<float>& Values = *InputBFloatArray;
				if (PrevBPerChannel.Num() != NumOutChannels)
				{
					PrevBPerChannel.Init(1.0f, NumOutChannels);
					bChannelSlewInitialized = false;
				}

				for (int32 Channel = 0; Channel < NumOutChannels; ++Channel)
				{
					const int32 AChannel = (NumAChannels == 1) ? 0 : FMath::Min(Channel, NumAChannels - 1);
					TArrayView<const float> SrcA = InputA->GetChannel(AChannel);
					TArrayView<float> Dst = Output->GetChannel(Channel);

					float Target = 1.0f;
					if (Values.Num() == 1)
					{
						Target = Values[0];
					}
					else if (Values.Num() > 1)
					{
						Target = Values[FMath::Min(Channel, Values.Num() - 1)];
					}

					float State = bChannelSlewInitialized ? PrevBPerChannel[Channel] : Target;
					for (int32 Frame = 0; Frame < NumFrames; ++Frame)
					{
						if (bUseSlew)
						{
							if (Target > State)
							{
								State = RiseAlpha * State + (1.0f - RiseAlpha) * Target;
							}
							else if (Target < State)
							{
								State = FallAlpha * State + (1.0f - FallAlpha) * Target;
							}
						}
						else
						{
							State = Target;
						}
						const float SafeDenom = (FMath::Abs(State) < DenominatorEpsilon) ? ((State < 0.0f) ? -DenominatorEpsilon : DenominatorEpsilon) : State;
						Dst[Frame] = SrcA[Frame] / SafeDenom;
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
		TSharedPtr<const FCATDivideOperatorData> OperatorData;
		FOperatorSettings Settings;
		FChannelAgnosticTypeReadRef InputA;
		FChannelAgnosticTypeReadRef InputBCAT;
		FAudioBufferReadRef InputBMono;
		FFloatReadRef InputBFloat;
		TDataReadReference<TArray<float>> InputBFloatArray;
		FBoolReadRef InputBSlewEnabled;
		FTimeReadRef InputBRiseTime;
		FTimeReadRef InputBFallTime;
		FChannelAgnosticTypeWriteRef Output;
		TArray<float> BScratch;
		float PrevBScalar = 0.0f;
		bool bScalarSlewInitialized = false;
		TArray<float> PrevBPerChannel;
		bool bChannelSlewInitialized = false;
	};

	using FCATDivideNode = TNodeFacade<FCATDivideOperator>;
	METASOUND_REGISTER_NODE_AND_CONFIGURATION(FCATDivideNode, FMetaSoundCATDivideNodeConfiguration);
}

TArray<FPropertyTextFName> UMetaSoundCATDivideNodeOptionsHelper::GetSoundFileFormatChannelOptions()
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

FMetaSoundCATDivideNodeConfiguration::FMetaSoundCATDivideNodeConfiguration()
	: OperatorData(MakeShared<Metasound::CATDividePrivate::FCATDivideOperatorData>(CatAudioTypeName, InputBMode))
{
}

TInstancedStruct<FMetasoundFrontendClassInterface> FMetaSoundCATDivideNodeConfiguration::OverrideDefaultInterface(const FMetasoundFrontendClass& InClass) const
{
	return TInstancedStruct<FMetasoundFrontendClassInterface>::Make(
		FMetasoundFrontendClassInterface::GenerateClassInterface(
			Metasound::CATDividePrivate::GetVertexInterface(CatAudioTypeName, InputBMode)));
}

TSharedPtr<const Metasound::IOperatorData> FMetaSoundCATDivideNodeConfiguration::GetOperatorData() const
{
	OperatorData->CatAudioTypeName = CatAudioTypeName;
	OperatorData->InputBMode = InputBMode;
	return OperatorData;
}

#undef LOCTEXT_NAMESPACE
