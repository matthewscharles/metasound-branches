// Copyright 2026 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundCATShiftRegisterSampleAndHoldNode.h"

#include "MetasoundAudioBuffer.h"
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

#define LOCTEXT_NAMESPACE "MetasoundBranches_CATShiftRegisterSampleAndHoldNode"

namespace Metasound
{
	namespace CATShiftRegisterSampleAndHoldVertexNames
	{
		METASOUND_PARAM(Input, "In", "Mono audio input to sample into the CAT shift register.")
		METASOUND_PARAM(Trigger, "Trigger", "When triggered, sample In and shift the register across CAT channels.")
		METASOUND_PARAM(Output, "Out", "CAT shift-register output.")
	}

	namespace CATShiftRegisterSampleAndHoldPrivate
	{
		class FCATShiftRegisterSampleAndHoldOperatorData final : public TOperatorData<FCATShiftRegisterSampleAndHoldOperatorData>
		{
		public:
			static const FLazyName OperatorDataTypeName;

			explicit FCATShiftRegisterSampleAndHoldOperatorData(const FName& InCatAudioTypeName)
				: CatAudioTypeName(InCatAudioTypeName)
			{
			}

			FName CatAudioTypeName;
		};

		const FLazyName FCATShiftRegisterSampleAndHoldOperatorData::OperatorDataTypeName = TEXT("FCATShiftRegisterSampleAndHoldOperatorData");

		FVertexInterface GetVertexInterface(const FName& InCatFormat)
		{
			using namespace CATShiftRegisterSampleAndHoldVertexNames;

			FInputVertexInterface InputInterface(
				TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(Input)),
				TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(Trigger)));

			FOutputVertexInterface OutputInterface;
			OutputInterface.Add(FOutputDataVertex(METASOUND_GET_PARAM_NAME(Output), InCatFormat, METASOUND_GET_PARAM_METADATA(Output), EVertexAccessType::Reference));

			return { MoveTemp(InputInterface), MoveTemp(OutputInterface) };
		}
	}

	class FCATShiftRegisterSampleAndHoldOperator final : public TExecutableOperator<FCATShiftRegisterSampleAndHoldOperator>
	{
	public:
		using FCATShiftRegisterSampleAndHoldOperatorData = CATShiftRegisterSampleAndHoldPrivate::FCATShiftRegisterSampleAndHoldOperatorData;

		FCATShiftRegisterSampleAndHoldOperator(
			const FOperatorSettings& InSettings,
			const TSharedPtr<const FCATShiftRegisterSampleAndHoldOperatorData>& InOperatorData,
			FAudioBufferReadRef&& InInput,
			FTriggerReadRef&& InTrigger,
			FChannelAgnosticTypeWriteRef&& InOutput)
			: Settings(InSettings)
			, OperatorData(InOperatorData)
			, Input(MoveTemp(InInput))
			, Trigger(MoveTemp(InTrigger))
			, Output(MoveTemp(InOutput))
		{
		}

		static const FVertexInterface& DeclareVertexInterface()
		{
			static const FVertexInterface Interface = CATShiftRegisterSampleAndHoldPrivate::GetVertexInterface(TEXT("Cat:Stereo2Dot0"));
			return Interface;
		}

		static FNodeClassMetadata GetNodeInfo()
		{
			FNodeClassMetadata Metadata;
			Metadata.ClassName = { TEXT("Experimental"), TEXT("CATShiftRegisterSampleAndHold"), TEXT("Audio") };
			Metadata.MajorVersion = 1;
			Metadata.MinorVersion = 0;
			Metadata.DisplayName = LOCTEXT("CATShiftRegisterSampleAndHoldDisplayName", "CAT Shift Register Sample and Hold");
			Metadata.Description = LOCTEXT("CATShiftRegisterSampleAndHoldDescription", "Samples a mono input when triggered and shifts values across CAT output channels.");
			Metadata.Author = TEXT("Charles Matthews");
			Metadata.PromptIfMissing = LOCTEXT("CATShiftRegisterSampleAndHoldMissingPrompt", "Enable MetaSoundExperimental for CAT channel format schemas.");
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
			using namespace CATShiftRegisterSampleAndHoldVertexNames;

			const FCATShiftRegisterSampleAndHoldOperatorData* ConfigData = CastOperatorData<const FCATShiftRegisterSampleAndHoldOperatorData>(InParams.Node.GetOperatorData().Get());
			if (!ConfigData)
			{
				return MakeUnique<FNoOpOperator>();
			}

			const Audio::FChannelTypeFamily* ConcreteType = Audio::GetChannelRegistry().FindConcreteChannel(ConfigData->CatAudioTypeName);
			if (!ConcreteType)
			{
				return MakeUnique<FNoOpOperator>();
			}

			const TSharedPtr<const FCATShiftRegisterSampleAndHoldOperatorData>& OperatorDataSharedPtr = StaticCastSharedPtr<const FCATShiftRegisterSampleAndHoldOperatorData>(InParams.Node.GetOperatorData());

			FAudioBufferReadRef InSignal = InParams.InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(Input), InParams.OperatorSettings);
			FTriggerReadRef InTrigger = InParams.InputData.GetOrCreateDefaultDataReadReference<FTrigger>(METASOUND_GET_PARAM_NAME(Trigger), InParams.OperatorSettings);
			FChannelAgnosticTypeWriteRef Out = FChannelAgnosticTypeWriteRef::CreateNew(InParams.OperatorSettings, ConcreteType->GetName());

			return MakeUnique<FCATShiftRegisterSampleAndHoldOperator>(
				InParams.OperatorSettings,
				OperatorDataSharedPtr,
				MoveTemp(InSignal),
				MoveTemp(InTrigger),
				MoveTemp(Out));
		}

		virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
		{
			using namespace CATShiftRegisterSampleAndHoldVertexNames;
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Input), Input);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Trigger), Trigger);
		}

		virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
		{
			using namespace CATShiftRegisterSampleAndHoldVertexNames;
			InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(Output), Output);
		}

		void Execute()
		{
			const int32 NumFrames = Settings.GetNumFramesPerBlock();
			const int32 NumChannels = Output->NumChannels();
			if (NumFrames <= 0 || NumChannels <= 0)
			{
				return;
			}

			if (RegisterValues.Num() != NumChannels)
			{
				RegisterValues.Init(0.0f, NumChannels);
			}

			const float* InData = Input->GetData();

			auto WriteSegment = [this, NumChannels](const int32 StartFrame, const int32 EndFrame)
			{
				for (int32 Channel = 0; Channel < NumChannels; ++Channel)
				{
					TArrayView<float> Destination = Output->GetChannel(Channel);
					for (int32 Frame = StartFrame; Frame < EndFrame; ++Frame)
					{
						Destination[Frame] = RegisterValues[Channel];
					}
				}
			};

			auto ShiftAndSample = [this, InData, NumChannels, NumFrames](const int32 TriggerFrame)
			{
				if (TriggerFrame < 0 || TriggerFrame >= NumFrames)
				{
					return;
				}

				for (int32 Channel = NumChannels - 1; Channel > 0; --Channel)
				{
					RegisterValues[Channel] = RegisterValues[Channel - 1];
				}
				RegisterValues[0] = InData[TriggerFrame];
			};

			Trigger->ExecuteBlock(
				[&WriteSegment](const int32 StartFrame, const int32 EndFrame)
				{
					WriteSegment(StartFrame, EndFrame);
				},
				[&WriteSegment, &ShiftAndSample](const int32 StartFrame, const int32 EndFrame)
				{
					ShiftAndSample(StartFrame);
					WriteSegment(StartFrame, EndFrame);
				});
		}

	private:
		FOperatorSettings Settings;
		TSharedPtr<const FCATShiftRegisterSampleAndHoldOperatorData> OperatorData;
		FAudioBufferReadRef Input;
		FTriggerReadRef Trigger;
		FChannelAgnosticTypeWriteRef Output;
		TArray<float> RegisterValues;
	};

	using FCATShiftRegisterSampleAndHoldNode = TNodeFacade<FCATShiftRegisterSampleAndHoldOperator>;
	METASOUND_REGISTER_NODE_AND_CONFIGURATION(FCATShiftRegisterSampleAndHoldNode, FMetaSoundCATShiftRegisterSampleAndHoldNodeConfiguration);
}

TArray<FPropertyTextFName> UMetaSoundCATShiftRegisterSampleAndHoldNodeOptionsHelper::GetSoundFileFormatChannelOptions()
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

FMetaSoundCATShiftRegisterSampleAndHoldNodeConfiguration::FMetaSoundCATShiftRegisterSampleAndHoldNodeConfiguration()
	: OperatorData(MakeShared<Metasound::CATShiftRegisterSampleAndHoldPrivate::FCATShiftRegisterSampleAndHoldOperatorData>(CatAudioTypeName))
{
}

TInstancedStruct<FMetasoundFrontendClassInterface> FMetaSoundCATShiftRegisterSampleAndHoldNodeConfiguration::OverrideDefaultInterface(const FMetasoundFrontendClass& InClass) const
{
	return TInstancedStruct<FMetasoundFrontendClassInterface>::Make(
		FMetasoundFrontendClassInterface::GenerateClassInterface(
			Metasound::CATShiftRegisterSampleAndHoldPrivate::GetVertexInterface(CatAudioTypeName)));
}

TSharedPtr<const Metasound::IOperatorData> FMetaSoundCATShiftRegisterSampleAndHoldNodeConfiguration::GetOperatorData() const
{
	OperatorData->CatAudioTypeName = CatAudioTypeName;
	return OperatorData;
}

#undef LOCTEXT_NAMESPACE
