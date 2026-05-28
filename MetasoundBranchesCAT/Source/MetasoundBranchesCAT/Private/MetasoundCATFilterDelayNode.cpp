// Copyright 2026 Charles Matthews. All Rights Reserved.

#include "MetasoundBranchesCAT/Public/MetasoundCATFilterDelayNode.h"

#include "DSP/Filter.h"
#include "MetasoundAudioBuffer.h"
#include "MetasoundBranches/Public/MetasoundCommonMacros.h"
#include "MetasoundChannelAgnosticType.h"
#include "MetasoundEnumRegistrationMacro.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundParamHelper.h"
#include "MetasoundPrimitives.h"
#include "MetasoundVertex.h"
#include "TypeFamily/ChannelTypeFamily.h"

#define LOCTEXT_NAMESPACE "MetasoundBranches_CATFilterDelayNode"

namespace Metasound
{
	DECLARE_METASOUND_ENUM(Audio::EBiquadFilter::Type, Audio::EBiquadFilter::Lowpass,
		METASOUNDSTANDARDNODES_API, FEnumEBiquadFilterType, FEnumBiQuadFilterTypeInfo, FEnumBiQuadFilterReadRef, FEnumBiQuadFilterWriteRef);

	namespace CATFilterDelayVertexNames
	{
		METASOUND_PARAM(Input, "Input", "CAT input signal.")
		METASOUND_PARAM(DelayTime, "Delay Time", "Delay time in seconds.")
		METASOUND_PARAM(Feedback, "Feedback", "Feedback amount.")
		METASOUND_PARAM(Mix, "Mix", "Dry/wet balance from 0 (dry) to 1 (wet).")
		METASOUND_PARAM(CrossfeedAmount, "Crossfeed Amount", "Crossfeed amount for multichannel behaviour, mainly Ping Pong.")
		METASOUND_PARAM(DelayRiseTime, "Delay Rise Time", "Rise time in seconds for delay time slew.")
		METASOUND_PARAM(DelayFallTime, "Delay Fall Time", "Fall time in seconds for delay time slew.")
		METASOUND_PARAM(FilterType, "Type", "Biquad filter type used in feedback loop.")
		METASOUND_PARAM(Cutoff, "Cutoff", "Filter cutoff in Hz for feedback loop.")
		METASOUND_PARAM(Bandwidth, "Bandwidth", "Filter bandwidth in octaves for feedback loop.")
		METASOUND_PARAM(GainDb, "Gain", "Filter gain in dB for gain-using types.")
		METASOUND_PARAM(Output, "Output", "Delayed CAT output.")
	}

	namespace CATFilterDelayPrivate
	{
		static constexpr float DefaultCutoffHz = 20000.0f;
		static constexpr float DefaultBandwidth = 1.89997f;
		static constexpr float DefaultGainDb = 0.0f;

		class FCATFilterDelayOperatorData final : public TOperatorData<FCATFilterDelayOperatorData>
		{
		public:
			static const FLazyName OperatorDataTypeName;

			explicit FCATFilterDelayOperatorData(
				const FName& InCatAudioTypeName,
				const EMetaSoundCATFilterDelayControlMode InDelayMode,
				const EMetaSoundCATFilterDelayControlMode InFeedbackMode,
				const EMetaSoundCATFilterDelayFilterControlMode InCutoffMode,
				const EMetaSoundCATFilterDelayFilterControlMode InBandwidthMode,
				const EMetaSoundCATFilterDelayFilterControlMode InGainMode,
				const EMetaSoundCATFilterDelayMultichannelBehaviour InMultichannelBehaviour,
				const bool bInAllowUnityFeedback,
				const bool bInEnableDelaySlew,
				const float InMaxDelaySeconds)
				: CatAudioTypeName(InCatAudioTypeName)
				, DelayMode(InDelayMode)
				, FeedbackMode(InFeedbackMode)
				, CutoffMode(InCutoffMode)
				, BandwidthMode(InBandwidthMode)
				, GainMode(InGainMode)
				, MultichannelBehaviour(InMultichannelBehaviour)
				, bAllowUnityFeedback(bInAllowUnityFeedback)
				, bEnableDelaySlew(bInEnableDelaySlew)
				, MaxDelaySeconds(InMaxDelaySeconds)
			{
			}

			FName CatAudioTypeName;
			EMetaSoundCATFilterDelayControlMode DelayMode;
			EMetaSoundCATFilterDelayControlMode FeedbackMode;
			EMetaSoundCATFilterDelayFilterControlMode CutoffMode;
			EMetaSoundCATFilterDelayFilterControlMode BandwidthMode;
			EMetaSoundCATFilterDelayFilterControlMode GainMode;
			EMetaSoundCATFilterDelayMultichannelBehaviour MultichannelBehaviour;
			bool bAllowUnityFeedback = false;
			bool bEnableDelaySlew = false;
			float MaxDelaySeconds = 2.0f;
		};

		const FLazyName FCATFilterDelayOperatorData::OperatorDataTypeName = TEXT("FCATFilterDelayOperatorData");

		FDataVertexMetadata MakeAdvancedMeta(const FDataVertexMetadata& InMeta)
		{
			FDataVertexMetadata Result = InMeta;
			Result.bIsAdvancedDisplay = true;
			return Result;
		}

		void AddControlVertex(
			FInputVertexInterface& InOutInput,
			const FName& InVertexName,
			const FDataVertexMetadata& InMetadata,
			const FName& InCatFormat,
			const EMetaSoundCATFilterDelayControlMode InMode,
			const float InDefaultFloat)
		{
			switch (InMode)
			{
			case EMetaSoundCATFilterDelayControlMode::CAT:
				InOutInput.Add(FInputDataVertex(InVertexName, InCatFormat, InMetadata, EVertexAccessType::Reference));
				break;
			case EMetaSoundCATFilterDelayControlMode::MonoAudio:
				InOutInput.Add(TInputDataVertex<FAudioBuffer>(InVertexName, InMetadata));
				break;
			case EMetaSoundCATFilterDelayControlMode::Float:
				InOutInput.Add(TInputDataVertex<float>(InVertexName, InMetadata, InDefaultFloat));
				break;
			case EMetaSoundCATFilterDelayControlMode::FloatArray:
				InOutInput.Add(TInputDataVertex<TArray<float>>(InVertexName, InMetadata));
				break;
			default:
				checkNoEntry();
				break;
			}
		}

		void AddFilterControlVertex(
			FInputVertexInterface& InOutInput,
			const FName& InVertexName,
			const FDataVertexMetadata& InMetadata,
			const EMetaSoundCATFilterDelayFilterControlMode InMode,
			const float InDefaultFloat)
		{
			switch (InMode)
			{
			case EMetaSoundCATFilterDelayFilterControlMode::Float:
				InOutInput.Add(TInputDataVertex<float>(InVertexName, InMetadata, InDefaultFloat));
				break;
			case EMetaSoundCATFilterDelayFilterControlMode::FloatArray:
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
			const EMetaSoundCATFilterDelayControlMode InMode,
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
			case EMetaSoundCATFilterDelayControlMode::CAT:
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
			case EMetaSoundCATFilterDelayControlMode::MonoAudio:
			{
				const int32 NumSamples = InMono->Num();
				if (NumSamples <= 0)
				{
					return InDefault;
				}
				return InMono->GetData()[FMath::Min(InFrame, NumSamples - 1)];
			}
			case EMetaSoundCATFilterDelayControlMode::Float:
				return *InFloat;
			case EMetaSoundCATFilterDelayControlMode::FloatArray:
				return GetArrayValue(*InFloatArray, InChannel, InDefault);
			default:
				checkNoEntry();
				return InDefault;
			}
		}

		float GetFilterControlValue(
			const EMetaSoundCATFilterDelayFilterControlMode InMode,
			const int32 InChannel,
			const FFloatReadRef& InFloat,
			const TDataReadReference<TArray<float>>& InFloatArray,
			const float InDefault)
		{
			switch (InMode)
			{
			case EMetaSoundCATFilterDelayFilterControlMode::Float:
				return *InFloat;
			case EMetaSoundCATFilterDelayFilterControlMode::FloatArray:
				return GetArrayValue(*InFloatArray, InChannel, InDefault);
			default:
				checkNoEntry();
				return InDefault;
			}
		}

		FVertexInterface GetVertexInterface(
			const FName& InCatFormat,
			const EMetaSoundCATFilterDelayControlMode InDelayMode,
			const EMetaSoundCATFilterDelayControlMode InFeedbackMode,
			const EMetaSoundCATFilterDelayFilterControlMode InCutoffMode,
			const EMetaSoundCATFilterDelayFilterControlMode InBandwidthMode,
			const EMetaSoundCATFilterDelayFilterControlMode InGainMode,
			const bool bEnableDelaySlew)
		{
			using namespace CATFilterDelayVertexNames;

			FInputVertexInterface InputInterface;
			InputInterface.Add(FInputDataVertex(METASOUND_GET_PARAM_NAME(Input), InCatFormat, METASOUND_GET_PARAM_METADATA(Input), EVertexAccessType::Reference));
			AddControlVertex(InputInterface, METASOUND_GET_PARAM_NAME(DelayTime), METASOUND_GET_PARAM_METADATA(DelayTime), InCatFormat, InDelayMode, 0.25f);
			AddControlVertex(InputInterface, METASOUND_GET_PARAM_NAME(Feedback), METASOUND_GET_PARAM_METADATA(Feedback), InCatFormat, InFeedbackMode, 0.35f);
			InputInterface.Add(TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(Mix), 1.0f));
			InputInterface.Add(TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(CrossfeedAmount), 1.0f));
			InputInterface.Add(TInputDataVertex<FEnumEBiquadFilterType>(METASOUND_GET_PARAM_NAME_AND_METADATA(FilterType)));
			AddFilterControlVertex(InputInterface, METASOUND_GET_PARAM_NAME(Cutoff), METASOUND_GET_PARAM_METADATA(Cutoff), InCutoffMode, DefaultCutoffHz);
			AddFilterControlVertex(InputInterface, METASOUND_GET_PARAM_NAME(Bandwidth), METASOUND_GET_PARAM_METADATA(Bandwidth), InBandwidthMode, DefaultBandwidth);
			AddFilterControlVertex(InputInterface, METASOUND_GET_PARAM_NAME(GainDb), METASOUND_GET_PARAM_METADATA(GainDb), InGainMode, DefaultGainDb);

			if (bEnableDelaySlew)
			{
				InputInterface.Add(TInputDataVertex<FTime>(METASOUND_GET_PARAM_NAME(DelayRiseTime), MakeAdvancedMeta(METASOUND_GET_PARAM_METADATA(DelayRiseTime))));
				InputInterface.Add(TInputDataVertex<FTime>(METASOUND_GET_PARAM_NAME(DelayFallTime), MakeAdvancedMeta(METASOUND_GET_PARAM_METADATA(DelayFallTime))));
			}

			FOutputVertexInterface OutputInterface;
			OutputInterface.Add(FOutputDataVertex(METASOUND_GET_PARAM_NAME(Output), InCatFormat, METASOUND_GET_PARAM_METADATA(Output), EVertexAccessType::Reference));

			return { MoveTemp(InputInterface), MoveTemp(OutputInterface) };
		}
	}

	class FCATFilterDelayOperator final : public TExecutableOperator<FCATFilterDelayOperator>
	{
	public:
		using FCATFilterDelayOperatorData = CATFilterDelayPrivate::FCATFilterDelayOperatorData;

		FCATFilterDelayOperator(
			const TSharedPtr<const FCATFilterDelayOperatorData>& InOperatorData,
			const FOperatorSettings& InSettings,
			FChannelAgnosticTypeReadRef&& InInput,
			FChannelAgnosticTypeReadRef&& InDelayCAT,
			FAudioBufferReadRef&& InDelayMono,
			FFloatReadRef&& InDelayFloat,
			TDataReadReference<TArray<float>>&& InDelayArray,
			FChannelAgnosticTypeReadRef&& InFeedbackCAT,
			FAudioBufferReadRef&& InFeedbackMono,
			FFloatReadRef&& InFeedbackFloat,
			TDataReadReference<TArray<float>>&& InFeedbackArray,
			FFloatReadRef&& InMix,
			FFloatReadRef&& InCrossfeedAmount,
			FEnumBiQuadFilterReadRef&& InFilterType,
			FFloatReadRef&& InCutoffFloat,
			TDataReadReference<TArray<float>>&& InCutoffArray,
			FFloatReadRef&& InBandwidthFloat,
			TDataReadReference<TArray<float>>&& InBandwidthArray,
			FFloatReadRef&& InGainFloat,
			TDataReadReference<TArray<float>>&& InGainArray,
			FTimeReadRef&& InDelayRiseTime,
			FTimeReadRef&& InDelayFallTime,
			FChannelAgnosticTypeWriteRef&& InOutput)
			: OperatorData(InOperatorData)
			, Settings(InSettings)
			, Input(MoveTemp(InInput))
			, DelayCAT(MoveTemp(InDelayCAT))
			, DelayMono(MoveTemp(InDelayMono))
			, DelayFloat(MoveTemp(InDelayFloat))
			, DelayArray(MoveTemp(InDelayArray))
			, FeedbackCAT(MoveTemp(InFeedbackCAT))
			, FeedbackMono(MoveTemp(InFeedbackMono))
			, FeedbackFloat(MoveTemp(InFeedbackFloat))
			, FeedbackArray(MoveTemp(InFeedbackArray))
			, Mix(MoveTemp(InMix))
			, CrossfeedAmount(MoveTemp(InCrossfeedAmount))
			, FilterType(MoveTemp(InFilterType))
			, CutoffFloat(MoveTemp(InCutoffFloat))
			, CutoffArray(MoveTemp(InCutoffArray))
			, BandwidthFloat(MoveTemp(InBandwidthFloat))
			, BandwidthArray(MoveTemp(InBandwidthArray))
			, GainFloat(MoveTemp(InGainFloat))
			, GainArray(MoveTemp(InGainArray))
			, DelayRiseTime(MoveTemp(InDelayRiseTime))
			, DelayFallTime(MoveTemp(InDelayFallTime))
			, Output(MoveTemp(InOutput))
			, SampleRate(FMath::Max(1.0f, InSettings.GetSampleRate()))
			, MaxDelaySamples(FMath::Max(1, FMath::RoundToInt(FMath::Max(0.01f, InOperatorData->MaxDelaySeconds) * SampleRate)))
			, BufferLength(MaxDelaySamples + 1)
			, MaxCutoffFrequency(0.5f * SampleRate)
			, PreviousFilterType(*FilterType)
		{
		}

		static const FVertexInterface& DeclareVertexInterface()
		{
			static const FVertexInterface Interface = CATFilterDelayPrivate::GetVertexInterface(
				TEXT("Cat:Stereo2Dot0"),
				EMetaSoundCATFilterDelayControlMode::Float,
				EMetaSoundCATFilterDelayControlMode::Float,
				EMetaSoundCATFilterDelayFilterControlMode::Float,
				EMetaSoundCATFilterDelayFilterControlMode::Float,
				EMetaSoundCATFilterDelayFilterControlMode::Float,
				false);
			return Interface;
		}

		static FNodeClassMetadata GetNodeInfo()
		{
			FNodeClassMetadata Metadata;
			Metadata.ClassName = { TEXT("Experimental"), TEXT("CATFilterDelay"), TEXT("Audio") };
			Metadata.MajorVersion = 1;
			Metadata.MinorVersion = 0;
			Metadata.DisplayName = LOCTEXT("CATFilterDelayDisplayName", "CAT Filter Delay");
			Metadata.Description = LOCTEXT("CATFilterDelayDescription", "CAT feedback delay with an integrated biquad filter in the feedback loop.");
			Metadata.Author = TEXT("Charles Matthews");
			Metadata.PromptIfMissing = LOCTEXT("CATFilterDelayMissingPrompt", "Enable MetaSoundExperimental for CAT channel format schemas.");
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
			using namespace CATFilterDelayVertexNames;

			const FCATFilterDelayOperatorData* ConfigData = CastOperatorData<const FCATFilterDelayOperatorData>(InParams.Node.GetOperatorData().Get());
			if (!ConfigData)
			{
				return MakeUnique<FNoOpOperator>();
			}

			const Audio::FChannelTypeFamily* ConcreteType = Audio::GetChannelRegistry().FindConcreteChannel(ConfigData->CatAudioTypeName);
			if (!ConcreteType)
			{
				return MakeUnique<FNoOpOperator>();
			}

			const TSharedPtr<const FCATFilterDelayOperatorData>& OperatorDataSharedPtr = StaticCastSharedPtr<const FCATFilterDelayOperatorData>(InParams.Node.GetOperatorData());

			FChannelAgnosticTypeReadRef InSignal = InParams.InputData.GetOrCreateDefaultDataReadReference<FChannelAgnosticType>(METASOUND_GET_PARAM_NAME(Input), InParams.OperatorSettings);

			FChannelAgnosticTypeReadRef InDelayCAT = FChannelAgnosticTypeWriteRef::CreateNew(InParams.OperatorSettings, ConcreteType->GetName());
			FAudioBufferReadRef InDelayMono = FAudioBufferWriteRef::CreateNew(InParams.OperatorSettings);
			FFloatReadRef InDelayFloat = TDataReadReference<float>::CreateNew(0.25f);
			TDataReadReference<TArray<float>> InDelayArray = TDataReadReference<TArray<float>>::CreateNew(TArray<float>{});

			FChannelAgnosticTypeReadRef InFeedbackCAT = FChannelAgnosticTypeWriteRef::CreateNew(InParams.OperatorSettings, ConcreteType->GetName());
			FAudioBufferReadRef InFeedbackMono = FAudioBufferWriteRef::CreateNew(InParams.OperatorSettings);
			FFloatReadRef InFeedbackFloat = TDataReadReference<float>::CreateNew(0.35f);
			TDataReadReference<TArray<float>> InFeedbackArray = TDataReadReference<TArray<float>>::CreateNew(TArray<float>{});

			FFloatReadRef InMix = InParams.InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(Mix), InParams.OperatorSettings);
			FFloatReadRef InCrossfeedAmount = InParams.InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(CrossfeedAmount), InParams.OperatorSettings);
			FEnumBiQuadFilterReadRef InFilterType = InParams.InputData.GetOrCreateDefaultDataReadReference<FEnumEBiquadFilterType>(METASOUND_GET_PARAM_NAME(FilterType), InParams.OperatorSettings);
			FFloatReadRef InCutoffFloat = TDataReadReference<float>::CreateNew(CATFilterDelayPrivate::DefaultCutoffHz);
			TDataReadReference<TArray<float>> InCutoffArray = TDataReadReference<TArray<float>>::CreateNew(TArray<float>{});
			FFloatReadRef InBandwidthFloat = TDataReadReference<float>::CreateNew(CATFilterDelayPrivate::DefaultBandwidth);
			TDataReadReference<TArray<float>> InBandwidthArray = TDataReadReference<TArray<float>>::CreateNew(TArray<float>{});
			FFloatReadRef InGainFloat = TDataReadReference<float>::CreateNew(CATFilterDelayPrivate::DefaultGainDb);
			TDataReadReference<TArray<float>> InGainArray = TDataReadReference<TArray<float>>::CreateNew(TArray<float>{});
			FTimeReadRef InDelayRiseTime = InParams.InputData.GetOrCreateDefaultDataReadReference<FTime>(METASOUND_GET_PARAM_NAME(DelayRiseTime), InParams.OperatorSettings);
			FTimeReadRef InDelayFallTime = InParams.InputData.GetOrCreateDefaultDataReadReference<FTime>(METASOUND_GET_PARAM_NAME(DelayFallTime), InParams.OperatorSettings);

			switch (ConfigData->DelayMode)
			{
			case EMetaSoundCATFilterDelayControlMode::CAT:
				InDelayCAT = InParams.InputData.GetOrCreateDefaultDataReadReference<FChannelAgnosticType>(METASOUND_GET_PARAM_NAME(DelayTime), InParams.OperatorSettings);
				break;
			case EMetaSoundCATFilterDelayControlMode::MonoAudio:
				InDelayMono = InParams.InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(DelayTime), InParams.OperatorSettings);
				break;
			case EMetaSoundCATFilterDelayControlMode::Float:
				InDelayFloat = InParams.InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(DelayTime), InParams.OperatorSettings);
				break;
			case EMetaSoundCATFilterDelayControlMode::FloatArray:
				InDelayArray = InParams.InputData.GetOrCreateDefaultDataReadReference<TArray<float>>(METASOUND_GET_PARAM_NAME(DelayTime), InParams.OperatorSettings);
				break;
			default:
				checkNoEntry();
				break;
			}

			switch (ConfigData->FeedbackMode)
			{
			case EMetaSoundCATFilterDelayControlMode::CAT:
				InFeedbackCAT = InParams.InputData.GetOrCreateDefaultDataReadReference<FChannelAgnosticType>(METASOUND_GET_PARAM_NAME(Feedback), InParams.OperatorSettings);
				break;
			case EMetaSoundCATFilterDelayControlMode::MonoAudio:
				InFeedbackMono = InParams.InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(Feedback), InParams.OperatorSettings);
				break;
			case EMetaSoundCATFilterDelayControlMode::Float:
				InFeedbackFloat = InParams.InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(Feedback), InParams.OperatorSettings);
				break;
			case EMetaSoundCATFilterDelayControlMode::FloatArray:
				InFeedbackArray = InParams.InputData.GetOrCreateDefaultDataReadReference<TArray<float>>(METASOUND_GET_PARAM_NAME(Feedback), InParams.OperatorSettings);
				break;
			default:
				checkNoEntry();
				break;
			}

			switch (ConfigData->CutoffMode)
			{
			case EMetaSoundCATFilterDelayFilterControlMode::Float:
				InCutoffFloat = InParams.InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(Cutoff), InParams.OperatorSettings);
				break;
			case EMetaSoundCATFilterDelayFilterControlMode::FloatArray:
				InCutoffArray = InParams.InputData.GetOrCreateDefaultDataReadReference<TArray<float>>(METASOUND_GET_PARAM_NAME(Cutoff), InParams.OperatorSettings);
				break;
			default:
				checkNoEntry();
				break;
			}

			switch (ConfigData->BandwidthMode)
			{
			case EMetaSoundCATFilterDelayFilterControlMode::Float:
				InBandwidthFloat = InParams.InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(Bandwidth), InParams.OperatorSettings);
				break;
			case EMetaSoundCATFilterDelayFilterControlMode::FloatArray:
				InBandwidthArray = InParams.InputData.GetOrCreateDefaultDataReadReference<TArray<float>>(METASOUND_GET_PARAM_NAME(Bandwidth), InParams.OperatorSettings);
				break;
			default:
				checkNoEntry();
				break;
			}

			switch (ConfigData->GainMode)
			{
			case EMetaSoundCATFilterDelayFilterControlMode::Float:
				InGainFloat = InParams.InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(GainDb), InParams.OperatorSettings);
				break;
			case EMetaSoundCATFilterDelayFilterControlMode::FloatArray:
				InGainArray = InParams.InputData.GetOrCreateDefaultDataReadReference<TArray<float>>(METASOUND_GET_PARAM_NAME(GainDb), InParams.OperatorSettings);
				break;
			default:
				checkNoEntry();
				break;
			}

			FChannelAgnosticTypeWriteRef Out = FChannelAgnosticTypeWriteRef::CreateNew(InParams.OperatorSettings, ConcreteType->GetName());

			return MakeUnique<FCATFilterDelayOperator>(
				OperatorDataSharedPtr,
				InParams.OperatorSettings,
				MoveTemp(InSignal),
				MoveTemp(InDelayCAT),
				MoveTemp(InDelayMono),
				MoveTemp(InDelayFloat),
				MoveTemp(InDelayArray),
				MoveTemp(InFeedbackCAT),
				MoveTemp(InFeedbackMono),
				MoveTemp(InFeedbackFloat),
				MoveTemp(InFeedbackArray),
				MoveTemp(InMix),
				MoveTemp(InCrossfeedAmount),
				MoveTemp(InFilterType),
				MoveTemp(InCutoffFloat),
				MoveTemp(InCutoffArray),
				MoveTemp(InBandwidthFloat),
				MoveTemp(InBandwidthArray),
				MoveTemp(InGainFloat),
				MoveTemp(InGainArray),
				MoveTemp(InDelayRiseTime),
				MoveTemp(InDelayFallTime),
				MoveTemp(Out));
		}

		virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
		{
			using namespace CATFilterDelayVertexNames;

			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Input), Input);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Mix), Mix);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(CrossfeedAmount), CrossfeedAmount);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(FilterType), FilterType);

			if (OperatorData->bEnableDelaySlew)
			{
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(DelayRiseTime), DelayRiseTime);
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(DelayFallTime), DelayFallTime);
			}

			switch (OperatorData->DelayMode)
			{
			case EMetaSoundCATFilterDelayControlMode::CAT:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(DelayTime), DelayCAT);
				break;
			case EMetaSoundCATFilterDelayControlMode::MonoAudio:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(DelayTime), DelayMono);
				break;
			case EMetaSoundCATFilterDelayControlMode::Float:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(DelayTime), DelayFloat);
				break;
			case EMetaSoundCATFilterDelayControlMode::FloatArray:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(DelayTime), DelayArray);
				break;
			default:
				checkNoEntry();
				break;
			}

			switch (OperatorData->FeedbackMode)
			{
			case EMetaSoundCATFilterDelayControlMode::CAT:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Feedback), FeedbackCAT);
				break;
			case EMetaSoundCATFilterDelayControlMode::MonoAudio:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Feedback), FeedbackMono);
				break;
			case EMetaSoundCATFilterDelayControlMode::Float:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Feedback), FeedbackFloat);
				break;
			case EMetaSoundCATFilterDelayControlMode::FloatArray:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Feedback), FeedbackArray);
				break;
			default:
				checkNoEntry();
				break;
			}

			switch (OperatorData->CutoffMode)
			{
			case EMetaSoundCATFilterDelayFilterControlMode::Float:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Cutoff), CutoffFloat);
				break;
			case EMetaSoundCATFilterDelayFilterControlMode::FloatArray:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Cutoff), CutoffArray);
				break;
			default:
				checkNoEntry();
				break;
			}

			switch (OperatorData->BandwidthMode)
			{
			case EMetaSoundCATFilterDelayFilterControlMode::Float:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Bandwidth), BandwidthFloat);
				break;
			case EMetaSoundCATFilterDelayFilterControlMode::FloatArray:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Bandwidth), BandwidthArray);
				break;
			default:
				checkNoEntry();
				break;
			}

			switch (OperatorData->GainMode)
			{
			case EMetaSoundCATFilterDelayFilterControlMode::Float:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(GainDb), GainFloat);
				break;
			case EMetaSoundCATFilterDelayFilterControlMode::FloatArray:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(GainDb), GainArray);
				break;
			default:
				checkNoEntry();
				break;
			}
		}

		virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
		{
			using namespace CATFilterDelayVertexNames;
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

			if (*FilterType != PreviousFilterType)
			{
				for (Audio::FBiquadFilter& Filter : FeedbackFilters)
				{
					Filter.SetType(*FilterType);
				}
				PreviousFilterType = *FilterType;
			}

			const float MixValue = FMath::Clamp(*Mix, 0.0f, 1.0f);
			const float Crossfeed = FMath::Clamp(*CrossfeedAmount, 0.0f, 1.0f);
			const float FeedbackMax = OperatorData->bAllowUnityFeedback ? 1.0f : 0.999f;
			const float RiseSeconds = FMath::Max(0.0f, (float)DelayRiseTime->GetSeconds());
			const float FallSeconds = FMath::Max(0.0f, (float)DelayFallTime->GetSeconds());
			const float RiseAlpha = (RiseSeconds > 0.0f) ? FMath::Exp(-1.0f / (RiseSeconds * SampleRate)) : 0.0f;
			const float FallAlpha = (FallSeconds > 0.0f) ? FMath::Exp(-1.0f / (FallSeconds * SampleRate)) : 0.0f;

			const int32 NumInChannels = FMath::Max(1, Input->NumChannels());
			DryFrameScratch.SetNumUninitialized(NumOutChannels);
			DelayedFrameScratch.SetNumUninitialized(NumOutChannels);
			FeedbackScratch.SetNumUninitialized(NumOutChannels);

			for (int32 Frame = 0; Frame < NumFrames; ++Frame)
			{
				for (int32 Channel = 0; Channel < NumOutChannels; ++Channel)
				{
					const int32 InChannel = (NumInChannels == 1) ? 0 : FMath::Min(Channel, NumInChannels - 1);
					const TArrayView<const float> Src = Input->GetChannel(InChannel);
					DryFrameScratch[Channel] = Src[Frame];

					const int32 ControlChannel = (OperatorData->MultichannelBehaviour == EMetaSoundCATFilterDelayMultichannelBehaviour::Linked) ? 0 : Channel;
					const float TargetDelaySeconds = FMath::Max(0.0f, CATFilterDelayPrivate::GetControlValue(
						OperatorData->DelayMode,
						ControlChannel,
						Frame,
						DelayCAT,
						DelayMono,
						DelayFloat,
						DelayArray,
						0.25f));

					float DelaySeconds = TargetDelaySeconds;
					if (OperatorData->bEnableDelaySlew)
					{
						if (!bDelaySlewInitialized)
						{
							SlewedDelayPerChannel[Channel] = TargetDelaySeconds;
						}

						float& DelayState = SlewedDelayPerChannel[Channel];
						if (TargetDelaySeconds > DelayState)
						{
							DelayState = RiseAlpha * DelayState + (1.0f - RiseAlpha) * TargetDelaySeconds;
						}
						else if (TargetDelaySeconds < DelayState)
						{
							DelayState = FallAlpha * DelayState + (1.0f - FallAlpha) * TargetDelaySeconds;
						}
						DelaySeconds = DelayState;
					}

					const float FeedbackAmount = FMath::Clamp(CATFilterDelayPrivate::GetControlValue(
						OperatorData->FeedbackMode,
						ControlChannel,
						Frame,
						FeedbackCAT,
						FeedbackMono,
						FeedbackFloat,
						FeedbackArray,
						0.35f), 0.0f, FeedbackMax);
					FeedbackScratch[Channel] = FeedbackAmount;

					const int32 DelaySamples = FMath::Clamp(FMath::RoundToInt(DelaySeconds * SampleRate), 1, MaxDelaySamples);
					int32 ReadIndex = WriteIndices[Channel] - DelaySamples;
					if (ReadIndex < 0)
					{
						ReadIndex += BufferLength;
					}

					DelayedFrameScratch[Channel] = DelayLines[Channel][ReadIndex];
				}

				if (OperatorData->bEnableDelaySlew)
				{
					bDelaySlewInitialized = true;
				}

				for (int32 Channel = 0; Channel < NumOutChannels; ++Channel)
				{
					const float FeedbackSource = GetFeedbackSample(Channel, NumOutChannels, Crossfeed);
					const float Cutoff = FMath::Clamp(CATFilterDelayPrivate::GetFilterControlValue(OperatorData->CutoffMode, Channel, CutoffFloat, CutoffArray, CATFilterDelayPrivate::DefaultCutoffHz), 0.0f, MaxCutoffFrequency);
					const float Bandwidth = FMath::Max(0.001f, CATFilterDelayPrivate::GetFilterControlValue(OperatorData->BandwidthMode, Channel, BandwidthFloat, BandwidthArray, CATFilterDelayPrivate::DefaultBandwidth));
					const float GainDbValue = FMath::Clamp(CATFilterDelayPrivate::GetFilterControlValue(OperatorData->GainMode, Channel, GainFloat, GainArray, CATFilterDelayPrivate::DefaultGainDb), -90.0f, 20.0f);

					Audio::FBiquadFilter& LoopFilter = FeedbackFilters[Channel];
					LoopFilter.SetFrequency(Cutoff);
					LoopFilter.SetBandwidth(Bandwidth);
					LoopFilter.SetGainDB(GainDbValue);

					float FilteredFeedback = 0.0f;
					LoopFilter.ProcessAudio(&FeedbackSource, 1, &FilteredFeedback);

					DelayLines[Channel][WriteIndices[Channel]] = DryFrameScratch[Channel] + FeedbackScratch[Channel] * FilteredFeedback;

					TArrayView<float> Dst = Output->GetChannel(Channel);
					Dst[Frame] = FMath::Lerp(DryFrameScratch[Channel], DelayedFrameScratch[Channel], MixValue);
				}

				for (int32 Channel = 0; Channel < NumOutChannels; ++Channel)
				{
					WriteIndices[Channel] = (WriteIndices[Channel] + 1) % BufferLength;
				}
			}
		}

	private:
		float GetFeedbackSample(const int32 InChannel, const int32 InNumChannels, const float InCrossfeed) const
		{
			switch (OperatorData->MultichannelBehaviour)
			{
			case EMetaSoundCATFilterDelayMultichannelBehaviour::PingPong:
			{
				float PairSample = DelayedFrameScratch[InChannel];
				if (InNumChannels >= 2)
				{
					if ((InChannel % 2) == 0)
					{
						const int32 Pair = InChannel + 1;
						if (Pair < InNumChannels)
						{
							PairSample = DelayedFrameScratch[Pair];
						}
					}
					else
					{
						PairSample = DelayedFrameScratch[InChannel - 1];
					}
				}

				return FMath::Lerp(DelayedFrameScratch[InChannel], PairSample, InCrossfeed);
			}
			case EMetaSoundCATFilterDelayMultichannelBehaviour::Linked:
			case EMetaSoundCATFilterDelayMultichannelBehaviour::Unlinked:
			default:
				return DelayedFrameScratch[InChannel];
			}
		}

		void EnsureStateForChannels(const int32 InNumChannels)
		{
			if (DelayLines.Num() == InNumChannels)
			{
				return;
			}

			DelayLines.SetNum(InNumChannels);
			WriteIndices.SetNumZeroed(InNumChannels);
			SlewedDelayPerChannel.Init(0.25f, InNumChannels);
			bDelaySlewInitialized = false;
			FeedbackFilters.SetNum(InNumChannels);

			for (TArray<float>& DelayLine : DelayLines)
			{
				DelayLine.SetNumZeroed(BufferLength);
			}
			for (Audio::FBiquadFilter& Filter : FeedbackFilters)
			{
				Filter.Init(SampleRate, 1, *FilterType);
			}
			PreviousFilterType = *FilterType;
		}

		TSharedPtr<const FCATFilterDelayOperatorData> OperatorData;
		FOperatorSettings Settings;
		FChannelAgnosticTypeReadRef Input;

		FChannelAgnosticTypeReadRef DelayCAT;
		FAudioBufferReadRef DelayMono;
		FFloatReadRef DelayFloat;
		TDataReadReference<TArray<float>> DelayArray;

		FChannelAgnosticTypeReadRef FeedbackCAT;
		FAudioBufferReadRef FeedbackMono;
		FFloatReadRef FeedbackFloat;
		TDataReadReference<TArray<float>> FeedbackArray;

		FFloatReadRef Mix;
		FFloatReadRef CrossfeedAmount;
		FEnumBiQuadFilterReadRef FilterType;
		FFloatReadRef CutoffFloat;
		TDataReadReference<TArray<float>> CutoffArray;
		FFloatReadRef BandwidthFloat;
		TDataReadReference<TArray<float>> BandwidthArray;
		FFloatReadRef GainFloat;
		TDataReadReference<TArray<float>> GainArray;
		FTimeReadRef DelayRiseTime;
		FTimeReadRef DelayFallTime;

		FChannelAgnosticTypeWriteRef Output;

		float SampleRate = 48000.0f;
		int32 MaxDelaySamples = 96000;
		int32 BufferLength = 96001;
		float MaxCutoffFrequency = 24000.0f;
		Audio::EBiquadFilter::Type PreviousFilterType = Audio::EBiquadFilter::Lowpass;
		TArray<TArray<float>> DelayLines;
		TArray<int32> WriteIndices;
		TArray<float> SlewedDelayPerChannel;
		bool bDelaySlewInitialized = false;
		TArray<Audio::FBiquadFilter> FeedbackFilters;
		TArray<float> DryFrameScratch;
		TArray<float> DelayedFrameScratch;
		TArray<float> FeedbackScratch;
	};

	using FCATFilterDelayNode = TNodeFacade<FCATFilterDelayOperator>;
	METASOUND_REGISTER_NODE_AND_CONFIGURATION(FCATFilterDelayNode, FMetaSoundCATFilterDelayNodeConfiguration);
}

TArray<FPropertyTextFName> UMetaSoundCATFilterDelayNodeOptionsHelper::GetSoundFileFormatChannelOptions()
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

FMetaSoundCATFilterDelayNodeConfiguration::FMetaSoundCATFilterDelayNodeConfiguration()
	: OperatorData(MakeShared<Metasound::CATFilterDelayPrivate::FCATFilterDelayOperatorData>(
		CatAudioTypeName,
		DelayMode,
		FeedbackMode,
		CutoffMode,
		BandwidthMode,
		GainMode,
		MultichannelBehaviour,
		bAllowUnityFeedback,
		bEnableDelaySlew,
		MaxDelaySeconds))
{
}

TInstancedStruct<FMetasoundFrontendClassInterface> FMetaSoundCATFilterDelayNodeConfiguration::OverrideDefaultInterface(const FMetasoundFrontendClass& InClass) const
{
	return TInstancedStruct<FMetasoundFrontendClassInterface>::Make(
		FMetasoundFrontendClassInterface::GenerateClassInterface(
			Metasound::CATFilterDelayPrivate::GetVertexInterface(
				CatAudioTypeName,
				DelayMode,
				FeedbackMode,
				CutoffMode,
				BandwidthMode,
				GainMode,
				bEnableDelaySlew)));
}

TSharedPtr<const Metasound::IOperatorData> FMetaSoundCATFilterDelayNodeConfiguration::GetOperatorData() const
{
	OperatorData->CatAudioTypeName = CatAudioTypeName;
	OperatorData->DelayMode = DelayMode;
	OperatorData->FeedbackMode = FeedbackMode;
	OperatorData->CutoffMode = CutoffMode;
	OperatorData->BandwidthMode = BandwidthMode;
	OperatorData->GainMode = GainMode;
	OperatorData->MultichannelBehaviour = MultichannelBehaviour;
	OperatorData->bAllowUnityFeedback = bAllowUnityFeedback;
	OperatorData->bEnableDelaySlew = bEnableDelaySlew;
	OperatorData->MaxDelaySeconds = MaxDelaySeconds;
	return OperatorData;
}

#undef LOCTEXT_NAMESPACE
