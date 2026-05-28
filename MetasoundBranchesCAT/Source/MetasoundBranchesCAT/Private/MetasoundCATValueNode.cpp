// Copyright 2026 Charles Matthews. All Rights Reserved.

#include "MetasoundBranchesCAT/Public/MetasoundCATValueNode.h"

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

#define LOCTEXT_NAMESPACE "MetasoundBranches_CATValueNode"

namespace Metasound
{
	namespace CATValueVertexNames
	{
		METASOUND_PARAM(Input, "Value", "Value input. Type is configurable.")
		METASOUND_PARAM(SlewEnabled, "Slew", "Enable slew smoothing on value input.")
		METASOUND_PARAM(RiseTime, "Rise Time", "Rise time in seconds for value slew.")
		METASOUND_PARAM(FallTime, "Fall Time", "Fall time in seconds for value slew.")
		METASOUND_PARAM(Output, "Output", "CAT value result.")
	}

	namespace CATValuePrivate
	{
		class FCATValueOperatorData final : public TOperatorData<FCATValueOperatorData>
		{
		public:
			static const FLazyName OperatorDataTypeName;

			explicit FCATValueOperatorData(const FName& InCatAudioTypeName, const EMetaSoundCATValueInputMode InInputMode)
				: CatAudioTypeName(InCatAudioTypeName)
				, InputMode(InInputMode)
			{
			}

			FName CatAudioTypeName;
			EMetaSoundCATValueInputMode InputMode;
		};

		const FLazyName FCATValueOperatorData::OperatorDataTypeName = TEXT("FCATValueOperatorData");

		FVertexInterface GetVertexInterface(const FName& InCatFormat, const EMetaSoundCATValueInputMode InInputMode)
		{
			using namespace CATValueVertexNames;

			FInputVertexInterface InputInterface;
			switch (InInputMode)
			{
			case EMetaSoundCATValueInputMode::Float:
				InputInterface.Add(TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(Input), 1.0f));
				break;
			case EMetaSoundCATValueInputMode::FloatArray:
				InputInterface.Add(TInputDataVertex<TArray<float>>(METASOUND_GET_PARAM_NAME_AND_METADATA(Input)));
				break;
			default:
				checkNoEntry();
				break;
			}

			InputInterface.Add(TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(SlewEnabled), false));
			InputInterface.Add(TInputDataVertex<FTime>(METASOUND_GET_PARAM_NAME_AND_METADATA(RiseTime)));
			InputInterface.Add(TInputDataVertex<FTime>(METASOUND_GET_PARAM_NAME_AND_METADATA(FallTime)));

			FOutputVertexInterface OutputInterface;
			OutputInterface.Add(FOutputDataVertex(METASOUND_GET_PARAM_NAME(Output), InCatFormat, METASOUND_GET_PARAM_METADATA(Output), EVertexAccessType::Reference));

			return { MoveTemp(InputInterface), MoveTemp(OutputInterface) };
		}
	}

	class FCATValueOperator final : public TExecutableOperator<FCATValueOperator>
	{
	public:
		using FCATValueOperatorData = CATValuePrivate::FCATValueOperatorData;

		FCATValueOperator(
			const TSharedPtr<const FCATValueOperatorData>& InOperatorData,
			const FOperatorSettings& InSettings,
			FFloatReadRef&& InInputFloat,
			TDataReadReference<TArray<float>>&& InInputFloatArray,
			FBoolReadRef&& InSlewEnabled,
			FTimeReadRef&& InRiseTime,
			FTimeReadRef&& InFallTime,
			FChannelAgnosticTypeWriteRef&& InOutput)
			: OperatorData(InOperatorData)
			, Settings(InSettings)
			, InputFloat(MoveTemp(InInputFloat))
			, InputFloatArray(MoveTemp(InInputFloatArray))
			, SlewEnabled(MoveTemp(InSlewEnabled))
			, RiseTime(MoveTemp(InRiseTime))
			, FallTime(MoveTemp(InFallTime))
			, Output(MoveTemp(InOutput))
		{
		}

		static const FVertexInterface& DeclareVertexInterface()
		{
			static const FVertexInterface Interface = CATValuePrivate::GetVertexInterface(TEXT("Cat:Stereo2Dot0"), EMetaSoundCATValueInputMode::Float);
			return Interface;
		}

		static FNodeClassMetadata GetNodeInfo()
		{
			FNodeClassMetadata Metadata;
			Metadata.ClassName = { TEXT("Experimental"), TEXT("CATValue"), TEXT("Audio") };
			Metadata.MajorVersion = 1;
			Metadata.MinorVersion = 1;
			Metadata.DisplayName = LOCTEXT("CATValueDisplayName", "CAT Value");
			Metadata.Description = LOCTEXT("CATValueDescription", "Outputs float or float array values as CAT.");
			Metadata.Author = TEXT("Charles Matthews");
			Metadata.PromptIfMissing = LOCTEXT("CATValueMissingPrompt", "Enable MetaSoundExperimental for CAT channel format schemas.");
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
			using namespace CATValueVertexNames;

			const FCATValueOperatorData* ConfigData = CastOperatorData<const FCATValueOperatorData>(InParams.Node.GetOperatorData().Get());
			if (!ConfigData)
			{
				return MakeUnique<FNoOpOperator>();
			}

			const Audio::FChannelTypeFamily* ConcreteType = Audio::GetChannelRegistry().FindConcreteChannel(ConfigData->CatAudioTypeName);
			if (!ConcreteType)
			{
				return MakeUnique<FNoOpOperator>();
			}

			const TSharedPtr<const FCATValueOperatorData>& OperatorDataSharedPtr = StaticCastSharedPtr<const FCATValueOperatorData>(InParams.Node.GetOperatorData());

			FFloatReadRef InFloat = TDataReadReference<float>::CreateNew(1.0f);
			TDataReadReference<TArray<float>> InFloatArray = TDataReadReference<TArray<float>>::CreateNew(TArray<float>{});
			FBoolReadRef InSlewEnabled = InParams.InputData.GetOrCreateDefaultDataReadReference<bool>(METASOUND_GET_PARAM_NAME(SlewEnabled), InParams.OperatorSettings);
			FTimeReadRef InRiseTime = InParams.InputData.GetOrCreateDefaultDataReadReference<FTime>(METASOUND_GET_PARAM_NAME(RiseTime), InParams.OperatorSettings);
			FTimeReadRef InFallTime = InParams.InputData.GetOrCreateDefaultDataReadReference<FTime>(METASOUND_GET_PARAM_NAME(FallTime), InParams.OperatorSettings);

			switch (ConfigData->InputMode)
			{
			case EMetaSoundCATValueInputMode::Float:
				InFloat = InParams.InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(Input), InParams.OperatorSettings);
				break;
			case EMetaSoundCATValueInputMode::FloatArray:
				InFloatArray = InParams.InputData.GetOrCreateDefaultDataReadReference<TArray<float>>(METASOUND_GET_PARAM_NAME(Input), InParams.OperatorSettings);
				break;
			default:
				checkNoEntry();
				break;
			}

			FChannelAgnosticTypeWriteRef Out = FChannelAgnosticTypeWriteRef::CreateNew(InParams.OperatorSettings, ConcreteType->GetName());

			return MakeUnique<FCATValueOperator>(
				OperatorDataSharedPtr,
				InParams.OperatorSettings,
				MoveTemp(InFloat),
				MoveTemp(InFloatArray),
				MoveTemp(InSlewEnabled),
				MoveTemp(InRiseTime),
				MoveTemp(InFallTime),
				MoveTemp(Out));
		}

		virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
		{
			using namespace CATValueVertexNames;

			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(SlewEnabled), SlewEnabled);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(RiseTime), RiseTime);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(FallTime), FallTime);

			switch (OperatorData->InputMode)
			{
			case EMetaSoundCATValueInputMode::Float:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Input), InputFloat);
				break;
			case EMetaSoundCATValueInputMode::FloatArray:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Input), InputFloatArray);
				break;
			default:
				checkNoEntry();
				break;
			}
		}

		virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
		{
			using namespace CATValueVertexNames;
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

			const bool bUseSlew = *SlewEnabled;
			const float RiseSeconds = FMath::Max(0.0f, (float)RiseTime->GetSeconds());
			const float FallSeconds = FMath::Max(0.0f, (float)FallTime->GetSeconds());
			const float SampleRate = FMath::Max(1.0f, Settings.GetSampleRate());
			const float RiseAlpha = (RiseSeconds > 0.0f) ? FMath::Exp(-1.0f / (RiseSeconds * SampleRate)) : 0.0f;
			const float FallAlpha = (FallSeconds > 0.0f) ? FMath::Exp(-1.0f / (FallSeconds * SampleRate)) : 0.0f;

			switch (OperatorData->InputMode)
			{
			case EMetaSoundCATValueInputMode::Float:
			{
				ValueScratch.SetNumUninitialized(NumFrames);
				const float Target = *InputFloat;
				float State = bScalarSlewInitialized ? PrevScalar : Target;
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
					ValueScratch[Frame] = State;
				}
				PrevScalar = State;
				bScalarSlewInitialized = true;
				for (int32 Channel = 0; Channel < NumOutChannels; ++Channel)
				{
					TArrayView<float> Dst = Output->GetChannel(Channel);
					for (int32 Frame = 0; Frame < NumFrames; ++Frame)
					{
						Dst[Frame] = ValueScratch[Frame];
					}
				}
				break;
			}
			case EMetaSoundCATValueInputMode::FloatArray:
			{
				const TArray<float>& Values = *InputFloatArray;
				if (PrevPerChannel.Num() != NumOutChannels)
				{
					PrevPerChannel.Init(1.0f, NumOutChannels);
					bPerChannelSlewInitialized = false;
				}

				for (int32 Channel = 0; Channel < NumOutChannels; ++Channel)
				{
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

					float State = bPerChannelSlewInitialized ? PrevPerChannel[Channel] : Target;
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
						Dst[Frame] = State;
					}
					PrevPerChannel[Channel] = State;
				}

				bPerChannelSlewInitialized = true;
				break;
			}
			default:
				checkNoEntry();
				break;
			}
		}

	private:
		TSharedPtr<const FCATValueOperatorData> OperatorData;
		FOperatorSettings Settings;
		FFloatReadRef InputFloat;
		TDataReadReference<TArray<float>> InputFloatArray;
		FBoolReadRef SlewEnabled;
		FTimeReadRef RiseTime;
		FTimeReadRef FallTime;
		FChannelAgnosticTypeWriteRef Output;
		TArray<float> ValueScratch;
		float PrevScalar = 0.0f;
		bool bScalarSlewInitialized = false;
		TArray<float> PrevPerChannel;
		bool bPerChannelSlewInitialized = false;
	};

	using FCATValueNode = TNodeFacade<FCATValueOperator>;
	METASOUND_REGISTER_NODE_AND_CONFIGURATION(FCATValueNode, FMetaSoundCATValueNodeConfiguration);
}

TArray<FPropertyTextFName> UMetaSoundCATValueNodeOptionsHelper::GetSoundFileFormatChannelOptions()
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

FMetaSoundCATValueNodeConfiguration::FMetaSoundCATValueNodeConfiguration()
	: OperatorData(MakeShared<Metasound::CATValuePrivate::FCATValueOperatorData>(CatAudioTypeName, InputMode))
{
}

TInstancedStruct<FMetasoundFrontendClassInterface> FMetaSoundCATValueNodeConfiguration::OverrideDefaultInterface(const FMetasoundFrontendClass& InClass) const
{
	return TInstancedStruct<FMetasoundFrontendClassInterface>::Make(
		FMetasoundFrontendClassInterface::GenerateClassInterface(
			Metasound::CATValuePrivate::GetVertexInterface(CatAudioTypeName, InputMode)));
}

TSharedPtr<const Metasound::IOperatorData> FMetaSoundCATValueNodeConfiguration::GetOperatorData() const
{
	OperatorData->CatAudioTypeName = CatAudioTypeName;
	OperatorData->InputMode = InputMode;
	return OperatorData;
}

#undef LOCTEXT_NAMESPACE
