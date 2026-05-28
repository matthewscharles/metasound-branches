// Copyright 2026 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundCATSampleAndHoldNode.h"

#include "MetasoundBranches/Public/MetasoundCommonMacros.h"
#include "MetasoundChannelAgnosticType.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundParamHelper.h"
#include "MetasoundPrimitives.h"
#include "MetasoundTrigger.h"
#include "MetasoundVertex.h"
#include "TypeFamily/ChannelTypeFamily.h"

#define LOCTEXT_NAMESPACE "MetasoundBranches_CATSampleAndHoldNode"

namespace Metasound
{
	namespace CATSampleAndHoldVertexNames
	{
		METASOUND_PARAM(Input, "In", "CAT signal to sample.")
		METASOUND_PARAM(TriggerAudio, "Trigger", "CAT audio trigger input. Rising threshold crossings capture samples.")
		METASOUND_PARAM(Threshold, "Threshold", "Trigger threshold for CAT audio trigger mode.")
		METASOUND_PARAM(Output, "Out", "CAT sampled-and-held output.")
	}

	namespace CATSampleAndHoldPrivate
	{
		class FCATSampleAndHoldOperatorData final : public TOperatorData<FCATSampleAndHoldOperatorData>
		{
		public:
			static const FLazyName OperatorDataTypeName;

			explicit FCATSampleAndHoldOperatorData(const FName& InCatAudioTypeName, const EMetaSoundCATSampleAndHoldTriggerMode InTriggerMode)
				: CatAudioTypeName(InCatAudioTypeName)
				, TriggerMode(InTriggerMode)
			{
			}

			FName CatAudioTypeName;
			EMetaSoundCATSampleAndHoldTriggerMode TriggerMode;
		};

		const FLazyName FCATSampleAndHoldOperatorData::OperatorDataTypeName = TEXT("FCATSampleAndHoldOperatorData");

		FName MakeFallbackTriggerVertexName(const int32 InChannelIndex)
		{
			return FName(*FString::Printf(TEXT("Trigger %d"), InChannelIndex + 1));
		}

		TArray<FName> GetTriggerVertexNames(const Audio::FChannelTypeFamily& InType)
		{
			TArray<FName> Names;
			Names.Reserve(InType.NumChannels());
			TSet<FName> UsedNames;

			for (int32 Channel = 0; Channel < InType.NumChannels(); ++Channel)
			{
				FName ChannelName = MakeFallbackTriggerVertexName(Channel);
				if (const TOptional<Audio::FChannelTypeFamily::FChannelName> MaybeChannelName = InType.GetChannelName(Channel))
				{
					if (!MaybeChannelName->Name.IsNone())
					{
						ChannelName = MaybeChannelName->Name;
					}
				}

				if (UsedNames.Contains(ChannelName))
				{
					ChannelName = FName(*FString::Printf(TEXT("%s %d"), *ChannelName.ToString(), Channel + 1));
				}

				UsedNames.Add(ChannelName);
				Names.Add(ChannelName);
			}

			if (Names.IsEmpty())
			{
				Names.Add(TEXT("Trigger"));
			}

			return Names;
		}

		FVertexInterface GetVertexInterface(const FName& InCatFormat, const EMetaSoundCATSampleAndHoldTriggerMode InTriggerMode)
		{
			using namespace CATSampleAndHoldVertexNames;

			FInputVertexInterface Input;
			Input.Add(FInputDataVertex(METASOUND_GET_PARAM_NAME(Input), InCatFormat, METASOUND_GET_PARAM_METADATA(Input), EVertexAccessType::Reference));

			const Audio::FChannelTypeFamily* ConcreteType = Audio::GetChannelRegistry().FindConcreteChannel(InCatFormat);
			if (InTriggerMode == EMetaSoundCATSampleAndHoldTriggerMode::Trigger)
			{
				if (ConcreteType)
				{
					const TArray<FName> TriggerNames = GetTriggerVertexNames(*ConcreteType);
					for (const FName& TriggerName : TriggerNames)
					{
						FDataVertexMetadata Metadata;
#if WITH_EDITOR
						Metadata.DisplayName = FText::FromName(TriggerName);
						Metadata.Description = FText::Format(
							LOCTEXT("CATSampleAndHoldTriggerPinTooltip", "Trigger input for channel {0}."),
							FText::FromName(TriggerName));
#endif
						Input.Add(TInputDataVertex<FTrigger>(TriggerName, Metadata));
					}
				}
				else
				{
					const FLazyName TriggerVertexName = TEXT("Trigger");
					const FDataVertexMetadata TriggerMetadata
					{
						METASOUND_LOCTEXT("CATSampleAndHoldTriggerInputDesc", "Trigger input."),
						FText::GetEmpty()
					};
					Input.Add(TInputDataVertex<FTrigger>(TriggerVertexName, TriggerMetadata));
				}
			}
			else
			{
				Input.Add(FInputDataVertex(METASOUND_GET_PARAM_NAME(TriggerAudio), InCatFormat, METASOUND_GET_PARAM_METADATA(TriggerAudio), EVertexAccessType::Reference));
				Input.Add(TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(Threshold), 0.0f));
			}

			FOutputVertexInterface OutputInterface;
			OutputInterface.Add(FOutputDataVertex(METASOUND_GET_PARAM_NAME(Output), InCatFormat, METASOUND_GET_PARAM_METADATA(Output), EVertexAccessType::Reference));

			return { MoveTemp(Input), MoveTemp(OutputInterface) };
		}
	}

	class FCATSampleAndHoldOperator final : public TExecutableOperator<FCATSampleAndHoldOperator>
	{
	public:
		using FCATSampleAndHoldOperatorData = CATSampleAndHoldPrivate::FCATSampleAndHoldOperatorData;

		FCATSampleAndHoldOperator(
			const TSharedPtr<const FCATSampleAndHoldOperatorData>& InOperatorData,
			const FOperatorSettings& InSettings,
			FChannelAgnosticTypeReadRef&& InInput,
			FChannelAgnosticTypeReadRef&& InTriggerAudio,
			FFloatReadRef&& InThreshold,
			TArray<FTriggerReadRef>&& InTriggerInputs,
			FChannelAgnosticTypeWriteRef&& InOutput)
			: OperatorData(InOperatorData)
			, Settings(InSettings)
			, Input(MoveTemp(InInput))
			, TriggerAudio(MoveTemp(InTriggerAudio))
			, Threshold(MoveTemp(InThreshold))
			, TriggerInputs(MoveTemp(InTriggerInputs))
			, Output(MoveTemp(InOutput))
		{
		}

		static const FVertexInterface& DeclareVertexInterface()
		{
			static const FVertexInterface Interface = CATSampleAndHoldPrivate::GetVertexInterface(TEXT("Cat:Stereo2Dot0"), EMetaSoundCATSampleAndHoldTriggerMode::Trigger);
			return Interface;
		}

		static FNodeClassMetadata GetNodeInfo()
		{
			FNodeClassMetadata Metadata;
			Metadata.ClassName = { TEXT("Experimental"), TEXT("CATSampleAndHold"), TEXT("Audio") };
			Metadata.MajorVersion = 1;
			Metadata.MinorVersion = 0;
			Metadata.DisplayName = LOCTEXT("CATSampleAndHoldDisplayName", "CAT Sample and Hold");
			Metadata.Description = LOCTEXT("CATSampleAndHoldDescription", "Samples a CAT signal on trigger events and holds the value between triggers. Trigger source is configurable.");
			Metadata.Author = TEXT("Charles Matthews");
			Metadata.PromptIfMissing = LOCTEXT("CATSampleAndHoldMissingPrompt", "Enable MetaSoundExperimental for CAT channel format schemas.");
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
			using namespace CATSampleAndHoldVertexNames;

			const FCATSampleAndHoldOperatorData* ConfigData = CastOperatorData<const FCATSampleAndHoldOperatorData>(InParams.Node.GetOperatorData().Get());
			if (!ConfigData)
			{
				return MakeUnique<FNoOpOperator>();
			}

			const Audio::FChannelTypeFamily* ConcreteType = Audio::GetChannelRegistry().FindConcreteChannel(ConfigData->CatAudioTypeName);
			if (!ConcreteType)
			{
				return MakeUnique<FNoOpOperator>();
			}

			const TSharedPtr<const FCATSampleAndHoldOperatorData>& OperatorDataSharedPtr = StaticCastSharedPtr<const FCATSampleAndHoldOperatorData>(InParams.Node.GetOperatorData());

			FChannelAgnosticTypeReadRef InSignal = InParams.InputData.GetOrCreateDefaultDataReadReference<FChannelAgnosticType>(METASOUND_GET_PARAM_NAME(Input), InParams.OperatorSettings);
			FChannelAgnosticTypeReadRef InTriggerAudio = FChannelAgnosticTypeWriteRef::CreateNew(InParams.OperatorSettings, ConcreteType->GetName());
			FFloatReadRef InThreshold = TDataReadReference<float>::CreateNew(0.0f);
			TArray<FTriggerReadRef> InTriggers;

			if (ConfigData->TriggerMode == EMetaSoundCATSampleAndHoldTriggerMode::Trigger)
			{
				const TArray<FName> TriggerNames = CATSampleAndHoldPrivate::GetTriggerVertexNames(*ConcreteType);
				InTriggers.Reserve(TriggerNames.Num());
				for (const FName& TriggerName : TriggerNames)
				{
					InTriggers.Add(InParams.InputData.GetOrCreateDefaultDataReadReference<FTrigger>(TriggerName, InParams.OperatorSettings));
				}
			}
			else
			{
				InTriggerAudio = InParams.InputData.GetOrCreateDefaultDataReadReference<FChannelAgnosticType>(METASOUND_GET_PARAM_NAME(TriggerAudio), InParams.OperatorSettings);
				InThreshold = InParams.InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(Threshold), InParams.OperatorSettings);
			}

			FChannelAgnosticTypeWriteRef Out = FChannelAgnosticTypeWriteRef::CreateNew(InParams.OperatorSettings, ConcreteType->GetName());

			return MakeUnique<FCATSampleAndHoldOperator>(
				OperatorDataSharedPtr,
				InParams.OperatorSettings,
				MoveTemp(InSignal),
				MoveTemp(InTriggerAudio),
				MoveTemp(InThreshold),
				MoveTemp(InTriggers),
				MoveTemp(Out));
		}

		virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
		{
			using namespace CATSampleAndHoldVertexNames;

			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Input), Input);

			if (OperatorData->TriggerMode == EMetaSoundCATSampleAndHoldTriggerMode::Trigger)
			{
				const Audio::FChannelTypeFamily* ConcreteType = Audio::GetChannelRegistry().FindConcreteChannel(OperatorData->CatAudioTypeName);
				if (ConcreteType)
				{
					const TArray<FName> TriggerNames = CATSampleAndHoldPrivate::GetTriggerVertexNames(*ConcreteType);
					const int32 NumBindings = FMath::Min(TriggerNames.Num(), TriggerInputs.Num());
					for (int32 Index = 0; Index < NumBindings; ++Index)
					{
						InOutVertexData.BindReadVertex(TriggerNames[Index], TriggerInputs[Index]);
					}
				}
			}
			else
			{
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(TriggerAudio), TriggerAudio);
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Threshold), Threshold);
			}
		}

		virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
		{
			using namespace CATSampleAndHoldVertexNames;
			InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(Output), Output);
		}

		void Execute()
		{
			const int32 NumFrames = Settings.GetNumFramesPerBlock();
			const int32 NumOutChannels = Output->NumChannels();
			if (NumOutChannels <= 0 || NumFrames <= 0)
			{
				return;
			}

			if (HeldValues.Num() != NumOutChannels)
			{
				HeldValues.Init(0.0f, NumOutChannels);
				PreviousTriggerValues.Init(0.0f, NumOutChannels);
				bIsChannelInitialized.Init(false, NumOutChannels);
			}

			const int32 NumInputChannels = FMath::Max(1, Input->NumChannels());

			if (OperatorData->TriggerMode == EMetaSoundCATSampleAndHoldTriggerMode::Trigger)
			{
				for (int32 Channel = 0; Channel < NumOutChannels; ++Channel)
				{
					const int32 InChannel = (NumInputChannels == 1) ? 0 : FMath::Min(Channel, NumInputChannels - 1);
					TArrayView<const float> Source = Input->GetChannel(InChannel);
					TArrayView<float> Destination = Output->GetChannel(Channel);

					if (!bIsChannelInitialized[Channel])
					{
						HeldValues[Channel] = Source[0];
						bIsChannelInitialized[Channel] = true;
					}

					if (!TriggerInputs.IsValidIndex(Channel))
					{
						for (int32 Frame = 0; Frame < NumFrames; ++Frame)
						{
							Destination[Frame] = HeldValues[Channel];
						}
						continue;
					}

					TriggerInputs[Channel]->ExecuteBlock(
						[&Destination, &Held = HeldValues[Channel]](int32 StartFrame, int32 EndFrame)
						{
							for (int32 Frame = StartFrame; Frame < EndFrame; ++Frame)
							{
								Destination[Frame] = Held;
							}
						},
						[&Source, &Destination, &Held = HeldValues[Channel], NumFrames](int32 StartFrame, int32 EndFrame)
						{
							if (StartFrame < NumFrames)
							{
								Held = Source[StartFrame];
							}
							for (int32 Frame = StartFrame; Frame < EndFrame; ++Frame)
							{
								Destination[Frame] = Held;
							}
						});
				}
				return;
			}

			const int32 NumTriggerChannels = FMath::Max(1, TriggerAudio->NumChannels());
			const float TriggerThreshold = *Threshold;

			for (int32 Channel = 0; Channel < NumOutChannels; ++Channel)
			{
				const int32 InChannel = (NumInputChannels == 1) ? 0 : FMath::Min(Channel, NumInputChannels - 1);
				const int32 TriggerChannel = (NumTriggerChannels == 1) ? 0 : FMath::Min(Channel, NumTriggerChannels - 1);

				TArrayView<const float> Source = Input->GetChannel(InChannel);
				TArrayView<const float> TriggerSource = TriggerAudio->GetChannel(TriggerChannel);
				TArrayView<float> Destination = Output->GetChannel(Channel);

				if (!bIsChannelInitialized[Channel])
				{
					HeldValues[Channel] = Source[0];
					PreviousTriggerValues[Channel] = TriggerSource[0];
					bIsChannelInitialized[Channel] = true;
				}

				float PreviousValue = PreviousTriggerValues[Channel];
				for (int32 Frame = 0; Frame < NumFrames; ++Frame)
				{
					const float TriggerValue = TriggerSource[Frame];
					if (PreviousValue <= TriggerThreshold && TriggerValue > TriggerThreshold)
					{
						HeldValues[Channel] = Source[Frame];
					}
					Destination[Frame] = HeldValues[Channel];
					PreviousValue = TriggerValue;
				}

				PreviousTriggerValues[Channel] = PreviousValue;
			}
		}

	private:
		TSharedPtr<const FCATSampleAndHoldOperatorData> OperatorData;
		FOperatorSettings Settings;
		FChannelAgnosticTypeReadRef Input;
		FChannelAgnosticTypeReadRef TriggerAudio;
		FFloatReadRef Threshold;
		TArray<FTriggerReadRef> TriggerInputs;
		FChannelAgnosticTypeWriteRef Output;
		TArray<float> HeldValues;
		TArray<float> PreviousTriggerValues;
		TArray<bool> bIsChannelInitialized;
	};

	using FCATSampleAndHoldNode = TNodeFacade<FCATSampleAndHoldOperator>;
	METASOUND_REGISTER_NODE_AND_CONFIGURATION(FCATSampleAndHoldNode, FMetaSoundCATSampleAndHoldNodeConfiguration);
}

TArray<FPropertyTextFName> UMetaSoundCATSampleAndHoldNodeOptionsHelper::GetSoundFileFormatChannelOptions()
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

FMetaSoundCATSampleAndHoldNodeConfiguration::FMetaSoundCATSampleAndHoldNodeConfiguration()
	: OperatorData(MakeShared<Metasound::CATSampleAndHoldPrivate::FCATSampleAndHoldOperatorData>(CatAudioTypeName, TriggerMode))
{
}

TInstancedStruct<FMetasoundFrontendClassInterface> FMetaSoundCATSampleAndHoldNodeConfiguration::OverrideDefaultInterface(const FMetasoundFrontendClass& InClass) const
{
	return TInstancedStruct<FMetasoundFrontendClassInterface>::Make(
		FMetasoundFrontendClassInterface::GenerateClassInterface(
			Metasound::CATSampleAndHoldPrivate::GetVertexInterface(CatAudioTypeName, TriggerMode)));
}

TSharedPtr<const Metasound::IOperatorData> FMetaSoundCATSampleAndHoldNodeConfiguration::GetOperatorData() const
{
	OperatorData->CatAudioTypeName = CatAudioTypeName;
	OperatorData->TriggerMode = TriggerMode;
	return OperatorData;
}

#undef LOCTEXT_NAMESPACE
