// Copyright 2026 Charles Matthews. All Rights Reserved.

#include "MetasoundBranchesCAT/Public/MetasoundCATPhaseDisperserNode.h"

#include "MetasoundBranches/Public/MetasoundCommonMacros.h"
#include "MetasoundChannelAgnosticType.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundParamHelper.h"
#include "MetasoundPrimitives.h"
#include "MetasoundVertex.h"
#include "TypeFamily/ChannelTypeFamily.h"

#define LOCTEXT_NAMESPACE "MetasoundBranches_CATPhaseDisperserNode"

namespace Metasound
{
	namespace CATPhaseDisperserVertexNames
	{
		METASOUND_PARAM(Input, "Input", "Incoming CAT audio.")
		METASOUND_PARAM(Output, "Output", "Phase-dispersed CAT output.")
		METASOUND_PARAM(NumFilters, "Stages", "Number of allpass filter stages to apply.")
		METASOUND_PARAM(MaximumStages, "Maximum Stages", "Upper bound on stage count. Evaluated once on instantiation.")
	}

	namespace CATPhaseDisperserPrivate
	{
		class FCATPhaseDisperserOperatorData final : public TOperatorData<FCATPhaseDisperserOperatorData>
		{
		public:
			static const FLazyName OperatorDataTypeName;

			explicit FCATPhaseDisperserOperatorData(const FName& InCatAudioTypeName)
				: CatAudioTypeName(InCatAudioTypeName)
			{
			}

			FName CatAudioTypeName;
		};

		const FLazyName FCATPhaseDisperserOperatorData::OperatorDataTypeName = TEXT("FCATPhaseDisperserOperatorData");

		FVertexInterface GetVertexInterface(const FName& InCatFormat)
		{
			using namespace CATPhaseDisperserVertexNames;

			FInputVertexInterface InputInterface;
			InputInterface.Add(FInputDataVertex(METASOUND_GET_PARAM_NAME(Input), InCatFormat, METASOUND_GET_PARAM_METADATA(Input), EVertexAccessType::Reference));
			InputInterface.Add(TInputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(NumFilters), 16));
			InputInterface.Add(TInputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(MaximumStages), 128));

			FOutputVertexInterface OutputInterface;
			OutputInterface.Add(FOutputDataVertex(METASOUND_GET_PARAM_NAME(Output), InCatFormat, METASOUND_GET_PARAM_METADATA(Output), EVertexAccessType::Reference));

			return { MoveTemp(InputInterface), MoveTemp(OutputInterface) };
		}
	}

	class FCATPhaseDisperserOperator final : public TExecutableOperator<FCATPhaseDisperserOperator>
	{
	public:
		using FCATPhaseDisperserOperatorData = CATPhaseDisperserPrivate::FCATPhaseDisperserOperatorData;

		FCATPhaseDisperserOperator(
			const TSharedPtr<const FCATPhaseDisperserOperatorData>& InOperatorData,
			const FOperatorSettings& InSettings,
			FChannelAgnosticTypeReadRef&& InInput,
			FInt32ReadRef&& InNumFilters,
			const int32 InMaximumStages,
			FChannelAgnosticTypeWriteRef&& InOutput)
			: OperatorData(InOperatorData)
			, Settings(InSettings)
			, Input(MoveTemp(InInput))
			, NumFilters(MoveTemp(InNumFilters))
			, MaxStages(FMath::Clamp(InMaximumStages, 1, 1024))
			, Output(MoveTemp(InOutput))
		{
			TempBuffer.SetNumUninitialized(InSettings.GetNumFramesPerBlock());
		}

		static const FVertexInterface& DeclareVertexInterface()
		{
			static const FVertexInterface Interface = CATPhaseDisperserPrivate::GetVertexInterface(TEXT("Cat:Stereo2Dot0"));
			return Interface;
		}

		static FNodeClassMetadata GetNodeInfo()
		{
			FNodeClassMetadata Metadata;
			Metadata.ClassName = { TEXT("Experimental"), TEXT("CATPhaseDisperser"), TEXT("Audio") };
			Metadata.MajorVersion = 1;
			Metadata.MinorVersion = 0;
			Metadata.DisplayName = LOCTEXT("CATPhaseDisperserDisplayName", "CAT Phase Disperser");
			Metadata.Description = LOCTEXT("CATPhaseDisperserDescription", "Apply a chain of allpass filters per CAT channel to disperse phase.");
			Metadata.Author = TEXT("Charles Matthews");
			Metadata.PromptIfMissing = LOCTEXT("CATPhaseDisperserMissingPrompt", "Enable MetaSoundExperimental for CAT channel format schemas.");
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
			using namespace CATPhaseDisperserVertexNames;

			const FCATPhaseDisperserOperatorData* ConfigData = CastOperatorData<const FCATPhaseDisperserOperatorData>(InParams.Node.GetOperatorData().Get());
			if (!ConfigData)
			{
				return MakeUnique<FNoOpOperator>();
			}

			const Audio::FChannelTypeFamily* ConcreteType = Audio::GetChannelRegistry().FindConcreteChannel(ConfigData->CatAudioTypeName);
			if (!ConcreteType)
			{
				return MakeUnique<FNoOpOperator>();
			}

			const TSharedPtr<const FCATPhaseDisperserOperatorData>& OperatorDataSharedPtr = StaticCastSharedPtr<const FCATPhaseDisperserOperatorData>(InParams.Node.GetOperatorData());

			FChannelAgnosticTypeReadRef InSignal = InParams.InputData.GetOrCreateDefaultDataReadReference<FChannelAgnosticType>(METASOUND_GET_PARAM_NAME(Input), InParams.OperatorSettings);
			FInt32ReadRef InNumFilters = InParams.InputData.GetOrCreateDefaultDataReadReference<int32>(METASOUND_GET_PARAM_NAME(NumFilters), InParams.OperatorSettings);
			FInt32ReadRef InMaximumStages = InParams.InputData.GetOrCreateDefaultDataReadReference<int32>(METASOUND_GET_PARAM_NAME(MaximumStages), InParams.OperatorSettings);

			FChannelAgnosticTypeWriteRef Out = FChannelAgnosticTypeWriteRef::CreateNew(InParams.OperatorSettings, ConcreteType->GetName());

			return MakeUnique<FCATPhaseDisperserOperator>(
				OperatorDataSharedPtr,
				InParams.OperatorSettings,
				MoveTemp(InSignal),
				MoveTemp(InNumFilters),
				*InMaximumStages,
				MoveTemp(Out));
		}

		virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
		{
			using namespace CATPhaseDisperserVertexNames;
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Input), Input);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(NumFilters), NumFilters);
		}

		virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
		{
			using namespace CATPhaseDisperserVertexNames;
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

			EnsureStateForChannels(NumOutChannels);

			const int32 NumInChannels = FMath::Max(1, Input->NumChannels());
			const int32 CurrentNumFilters = FMath::Clamp(*NumFilters, 1, MaxStages);

			for (int32 Channel = 0; Channel < NumOutChannels; ++Channel)
			{
				const int32 InChannel = (NumInChannels == 1) ? 0 : FMath::Min(Channel, NumInChannels - 1);
				TArrayView<const float> Src = Input->GetChannel(InChannel);
				TArrayView<float> Dst = Output->GetChannel(Channel);

				FMemory::Memcpy(TempBuffer.GetData(), Src.GetData(), NumFrames * sizeof(float));

				TArray<FAllPassFilter>& ChannelFilters = AllPassFiltersPerChannel[Channel];
				for (int32 FilterIndex = 0; FilterIndex < CurrentNumFilters; ++FilterIndex)
				{
					ChannelFilters[FilterIndex].ProcessBuffer(TempBuffer.GetData(), NumFrames);
				}

				FMemory::Memcpy(Dst.GetData(), TempBuffer.GetData(), NumFrames * sizeof(float));
			}
		}

	private:
		class FAllPassFilter
		{
		public:
			void Init(const float InFeedback = 0.5f)
			{
				DelayBuffer.SetNumZeroed(2);
				WriteIndex = 0;
				Feedback = InFeedback;
			}

			void ProcessBuffer(float* InOutBuffer, const int32 InNumSamples)
			{
				for (int32 i = 0; i < InNumSamples; ++i)
				{
					const float InSample = InOutBuffer[i];
					const float DelayedSample = DelayBuffer[WriteIndex];

					const float OutSample = -Feedback * InSample + DelayedSample;
					DelayBuffer[WriteIndex] = InSample + Feedback * OutSample;

					InOutBuffer[i] = OutSample;
					WriteIndex = (WriteIndex + 1) % 2;
				}
			}

		private:
			TArray<float> DelayBuffer;
			int32 WriteIndex = 0;
			float Feedback = 0.5f;
		};

		void EnsureStateForChannels(const int32 InNumChannels)
		{
			if (AllPassFiltersPerChannel.Num() == InNumChannels)
			{
				return;
			}

			AllPassFiltersPerChannel.SetNum(InNumChannels);
			for (TArray<FAllPassFilter>& ChannelFilters : AllPassFiltersPerChannel)
			{
				ChannelFilters.SetNum(MaxStages);
				for (FAllPassFilter& Filter : ChannelFilters)
				{
					Filter.Init();
				}
			}
		}

		TSharedPtr<const FCATPhaseDisperserOperatorData> OperatorData;
		FOperatorSettings Settings;
		FChannelAgnosticTypeReadRef Input;
		FInt32ReadRef NumFilters;
		int32 MaxStages = 128;
		FChannelAgnosticTypeWriteRef Output;

		TArray<TArray<FAllPassFilter>> AllPassFiltersPerChannel;
		TArray<float> TempBuffer;
	};

	using FCATPhaseDisperserNode = TNodeFacade<FCATPhaseDisperserOperator>;
	METASOUND_REGISTER_NODE_AND_CONFIGURATION(FCATPhaseDisperserNode, FMetaSoundCATPhaseDisperserNodeConfiguration);
}

TArray<FPropertyTextFName> UMetaSoundCATPhaseDisperserNodeOptionsHelper::GetSoundFileFormatChannelOptions()
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

FMetaSoundCATPhaseDisperserNodeConfiguration::FMetaSoundCATPhaseDisperserNodeConfiguration()
	: OperatorData(MakeShared<Metasound::CATPhaseDisperserPrivate::FCATPhaseDisperserOperatorData>(CatAudioTypeName))
{
}

TInstancedStruct<FMetasoundFrontendClassInterface> FMetaSoundCATPhaseDisperserNodeConfiguration::OverrideDefaultInterface(const FMetasoundFrontendClass& InClass) const
{
	return TInstancedStruct<FMetasoundFrontendClassInterface>::Make(
		FMetasoundFrontendClassInterface::GenerateClassInterface(
			Metasound::CATPhaseDisperserPrivate::GetVertexInterface(CatAudioTypeName)));
}

TSharedPtr<const Metasound::IOperatorData> FMetaSoundCATPhaseDisperserNodeConfiguration::GetOperatorData() const
{
	OperatorData->CatAudioTypeName = CatAudioTypeName;
	return OperatorData;
}

#undef LOCTEXT_NAMESPACE
