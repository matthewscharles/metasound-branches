// Copyright 2026 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundCATBiquadNode.h"

#include "DSP/Filter.h"
#include "MetasoundBranches/Public/MetasoundCommonMacros.h"
#include "MetasoundChannelAgnosticType.h"
#include "MetasoundEnumRegistrationMacro.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundParamHelper.h"
#include "MetasoundPrimitives.h"
#include "MetasoundVertex.h"
#include "Math/UnrealMathUtility.h"
#include "TypeFamily/ChannelTypeFamily.h"

#define LOCTEXT_NAMESPACE "MetasoundBranches_CATBiquadNode"

namespace Metasound
{
	DECLARE_METASOUND_ENUM(Audio::EBiquadFilter::Type, Audio::EBiquadFilter::Lowpass,
		METASOUNDSTANDARDNODES_API, FEnumEBiquadFilterType, FEnumBiQuadFilterTypeInfo, FEnumBiQuadFilterReadRef, FEnumBiQuadFilterWriteRef);

	namespace CATBiquadVertexNames
	{
		METASOUND_PARAM(Input, "Input", "CAT signal to filter.")
		METASOUND_PARAM(Cutoff, "Cutoff", "Cutoff frequency in Hz. Type is configurable.")
		METASOUND_PARAM(Bandwidth, "Bandwidth", "Filter bandwidth in octaves. Type is configurable.")
		METASOUND_PARAM(GainDb, "Gain", "Gain in dB for gain-using filter types. Type is configurable.")
		METASOUND_PARAM(FilterType, "Type", "Biquad filter type.")

		METASOUND_PARAM(CutoffRiseTime, "Cutoff Rise Time", "Rise time in seconds for cutoff slew.")
		METASOUND_PARAM(CutoffFallTime, "Cutoff Fall Time", "Fall time in seconds for cutoff slew.")
		METASOUND_PARAM(BandwidthRiseTime, "Bandwidth Rise Time", "Rise time in seconds for bandwidth slew.")
		METASOUND_PARAM(BandwidthFallTime, "Bandwidth Fall Time", "Fall time in seconds for bandwidth slew.")
		METASOUND_PARAM(GainRiseTime, "Gain Rise Time", "Rise time in seconds for gain slew.")
		METASOUND_PARAM(GainFallTime, "Gain Fall Time", "Fall time in seconds for gain slew.")

		METASOUND_PARAM(Output, "Output", "Filtered CAT output.")
	}

	namespace CATBiquadPrivate
	{
		static constexpr float DefaultCutoffHz = 20000.0f;
		static constexpr float DefaultBandwidth = 1.89997f;
		static constexpr float DefaultGainDb = 0.0f;
		static constexpr float InvalidValue = -1.0f;

		class FCATBiquadOperatorData final : public TOperatorData<FCATBiquadOperatorData>
		{
		public:
			static const FLazyName OperatorDataTypeName;

			explicit FCATBiquadOperatorData(
				const FName& InCatAudioTypeName,
				const EMetaSoundCATBiquadControlMode InCutoffMode,
				const EMetaSoundCATBiquadControlMode InBandwidthMode,
				const EMetaSoundCATBiquadControlMode InGainMode,
				const bool bInEnableCutoffSlew,
				const bool bInEnableBandwidthSlew,
				const bool bInEnableGainSlew)
				: CatAudioTypeName(InCatAudioTypeName)
				, CutoffMode(InCutoffMode)
					, BandwidthMode(InBandwidthMode)
				, GainMode(InGainMode)
					, bEnableCutoffSlew(bInEnableCutoffSlew)
					, bEnableBandwidthSlew(bInEnableBandwidthSlew)
					, bEnableGainSlew(bInEnableGainSlew)
			{
			}

			FName CatAudioTypeName;
			EMetaSoundCATBiquadControlMode CutoffMode;
			EMetaSoundCATBiquadControlMode BandwidthMode;
			EMetaSoundCATBiquadControlMode GainMode;
			bool bEnableCutoffSlew;
			bool bEnableBandwidthSlew;
			bool bEnableGainSlew;
		};

		const FLazyName FCATBiquadOperatorData::OperatorDataTypeName = TEXT("FCATBiquadOperatorData");

		float GetArrayValueOrDefault(const TArray<float>& InValues, const int32 InChannel, const float InDefault)
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

		FDataVertexMetadata MakeAdvancedMeta(const FDataVertexMetadata& InMeta)
		{
			FDataVertexMetadata Result = InMeta;
			Result.bIsAdvancedDisplay = true;
			return Result;
		}

		FVertexInterface GetVertexInterface(
			const FName& InCatFormat,
			const EMetaSoundCATBiquadControlMode InCutoffMode,
			const EMetaSoundCATBiquadControlMode InBandwidthMode,
			const EMetaSoundCATBiquadControlMode InGainMode,
			const bool bEnableCutoffSlew,
			const bool bEnableBandwidthSlew,
			const bool bEnableGainSlew)
		{
			using namespace CATBiquadVertexNames;

			const FDataVertexMetadata CutoffRiseMeta = MakeAdvancedMeta(METASOUND_GET_PARAM_METADATA(CutoffRiseTime));
			const FDataVertexMetadata CutoffFallMeta = MakeAdvancedMeta(METASOUND_GET_PARAM_METADATA(CutoffFallTime));
			const FDataVertexMetadata BandwidthRiseMeta = MakeAdvancedMeta(METASOUND_GET_PARAM_METADATA(BandwidthRiseTime));
			const FDataVertexMetadata BandwidthFallMeta = MakeAdvancedMeta(METASOUND_GET_PARAM_METADATA(BandwidthFallTime));
			const FDataVertexMetadata GainRiseMeta = MakeAdvancedMeta(METASOUND_GET_PARAM_METADATA(GainRiseTime));
			const FDataVertexMetadata GainFallMeta = MakeAdvancedMeta(METASOUND_GET_PARAM_METADATA(GainFallTime));

			FInputVertexInterface Input;
			Input.Add(FInputDataVertex(METASOUND_GET_PARAM_NAME(Input), InCatFormat, METASOUND_GET_PARAM_METADATA(Input), EVertexAccessType::Reference));

			switch (InCutoffMode)
			{
			case EMetaSoundCATBiquadControlMode::Float:
				Input.Add(TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(Cutoff), DefaultCutoffHz));
				break;
			case EMetaSoundCATBiquadControlMode::FloatArray:
				Input.Add(TInputDataVertex<TArray<float>>(METASOUND_GET_PARAM_NAME_AND_METADATA(Cutoff)));
				break;
			default:
				checkNoEntry();
				break;
			}

			switch (InBandwidthMode)
			{
			case EMetaSoundCATBiquadControlMode::Float:
				Input.Add(TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(Bandwidth), DefaultBandwidth));
				break;
			case EMetaSoundCATBiquadControlMode::FloatArray:
				Input.Add(TInputDataVertex<TArray<float>>(METASOUND_GET_PARAM_NAME_AND_METADATA(Bandwidth)));
				break;
			default:
				checkNoEntry();
				break;
			}

			switch (InGainMode)
			{
			case EMetaSoundCATBiquadControlMode::Float:
				Input.Add(TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(GainDb), DefaultGainDb));
				break;
			case EMetaSoundCATBiquadControlMode::FloatArray:
				Input.Add(TInputDataVertex<TArray<float>>(METASOUND_GET_PARAM_NAME_AND_METADATA(GainDb)));
				break;
			default:
				checkNoEntry();
				break;
			}

			Input.Add(TInputDataVertex<FEnumEBiquadFilterType>(METASOUND_GET_PARAM_NAME_AND_METADATA(FilterType)));

			if (bEnableCutoffSlew)
			{
				Input.Add(TInputDataVertex<FTime>(METASOUND_GET_PARAM_NAME(CutoffRiseTime), CutoffRiseMeta));
				Input.Add(TInputDataVertex<FTime>(METASOUND_GET_PARAM_NAME(CutoffFallTime), CutoffFallMeta));
			}

			if (bEnableBandwidthSlew)
			{
				Input.Add(TInputDataVertex<FTime>(METASOUND_GET_PARAM_NAME(BandwidthRiseTime), BandwidthRiseMeta));
				Input.Add(TInputDataVertex<FTime>(METASOUND_GET_PARAM_NAME(BandwidthFallTime), BandwidthFallMeta));
			}

			if (bEnableGainSlew)
			{
				Input.Add(TInputDataVertex<FTime>(METASOUND_GET_PARAM_NAME(GainRiseTime), GainRiseMeta));
				Input.Add(TInputDataVertex<FTime>(METASOUND_GET_PARAM_NAME(GainFallTime), GainFallMeta));
			}

			FOutputVertexInterface OutputInterface;
			OutputInterface.Add(FOutputDataVertex(METASOUND_GET_PARAM_NAME(Output), InCatFormat, METASOUND_GET_PARAM_METADATA(Output), EVertexAccessType::Reference));

			return { MoveTemp(Input), MoveTemp(OutputInterface) };
		}
	}

	class FCATBiquadOperator final : public TExecutableOperator<FCATBiquadOperator>
	{
	public:
		using FCATBiquadOperatorData = CATBiquadPrivate::FCATBiquadOperatorData;

		FCATBiquadOperator(
			const TSharedPtr<const FCATBiquadOperatorData>& InOperatorData,
			const FOperatorSettings& InSettings,
			FChannelAgnosticTypeReadRef&& InInput,
			FFloatReadRef&& InCutoffFloat,
			TDataReadReference<TArray<float>>&& InCutoffArray,
			FFloatReadRef&& InBandwidthFloat,
			TDataReadReference<TArray<float>>&& InBandwidthArray,
			FFloatReadRef&& InGainFloat,
			TDataReadReference<TArray<float>>&& InGainArray,
			FEnumBiQuadFilterReadRef&& InFilterType,
			FTimeReadRef&& InCutoffRiseTime,
			FTimeReadRef&& InCutoffFallTime,
			FTimeReadRef&& InBandwidthRiseTime,
			FTimeReadRef&& InBandwidthFallTime,
			FTimeReadRef&& InGainRiseTime,
			FTimeReadRef&& InGainFallTime,
			FChannelAgnosticTypeWriteRef&& InOutput)
			: OperatorData(InOperatorData)
			, Settings(InSettings)
			, Input(MoveTemp(InInput))
			, CutoffFloat(MoveTemp(InCutoffFloat))
			, CutoffArray(MoveTemp(InCutoffArray))
			, BandwidthFloat(MoveTemp(InBandwidthFloat))
			, BandwidthArray(MoveTemp(InBandwidthArray))
			, GainFloat(MoveTemp(InGainFloat))
			, GainArray(MoveTemp(InGainArray))
			, FilterType(MoveTemp(InFilterType))
			, CutoffRiseTime(MoveTemp(InCutoffRiseTime))
			, CutoffFallTime(MoveTemp(InCutoffFallTime))
			, BandwidthRiseTime(MoveTemp(InBandwidthRiseTime))
			, BandwidthFallTime(MoveTemp(InBandwidthFallTime))
			, GainRiseTime(MoveTemp(InGainRiseTime))
			, GainFallTime(MoveTemp(InGainFallTime))
			, Output(MoveTemp(InOutput))
			, SampleRate(FMath::Max(1.0f, InSettings.GetSampleRate()))
			, MaxCutoffFrequency(0.5f * SampleRate)
			, PreviousFilterType(*FilterType)
		{
		}

		static const FVertexInterface& DeclareVertexInterface()
		{
			static const FVertexInterface Interface = CATBiquadPrivate::GetVertexInterface(
				TEXT("Cat:Stereo2Dot0"),
				EMetaSoundCATBiquadControlMode::Float,
				EMetaSoundCATBiquadControlMode::Float,
				EMetaSoundCATBiquadControlMode::Float,
				false,
				false,
				false);
			return Interface;
		}

		static FNodeClassMetadata GetNodeInfo()
		{
			FNodeClassMetadata Metadata;
			Metadata.ClassName = { TEXT("Experimental"), TEXT("CATBiquad"), TEXT("Audio") };
			Metadata.MajorVersion = 1;
			Metadata.MinorVersion = 3;
			Metadata.DisplayName = LOCTEXT("CATBiquadDisplayName", "CAT Biquad Filter");
			Metadata.Description = LOCTEXT("CATBiquadDescription", "Biquad filter for CAT with configurable float/array Cutoff, Bandwidth, and Gain controls.");
			Metadata.Author = TEXT("Charles Matthews");
			Metadata.PromptIfMissing = LOCTEXT("CATBiquadMissingPrompt", "Enable MetaSoundExperimental for CAT channel format schemas.");
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
			using namespace CATBiquadVertexNames;

			const FCATBiquadOperatorData* ConfigData = CastOperatorData<const FCATBiquadOperatorData>(InParams.Node.GetOperatorData().Get());
			if (!ConfigData)
			{
				return MakeUnique<FNoOpOperator>();
			}

			const Audio::FChannelTypeFamily* ConcreteType = Audio::GetChannelRegistry().FindConcreteChannel(ConfigData->CatAudioTypeName);
			if (!ConcreteType)
			{
				return MakeUnique<FNoOpOperator>();
			}

			const TSharedPtr<const FCATBiquadOperatorData>& OperatorDataSharedPtr = StaticCastSharedPtr<const FCATBiquadOperatorData>(InParams.Node.GetOperatorData());

			FChannelAgnosticTypeReadRef InSignal = InParams.InputData.GetOrCreateDefaultDataReadReference<FChannelAgnosticType>(METASOUND_GET_PARAM_NAME(Input), InParams.OperatorSettings);

			FFloatReadRef InCutoffFloat = TDataReadReference<float>::CreateNew(CATBiquadPrivate::DefaultCutoffHz);
			TDataReadReference<TArray<float>> InCutoffArray = TDataReadReference<TArray<float>>::CreateNew(TArray<float>{});
			switch (ConfigData->CutoffMode)
			{
			case EMetaSoundCATBiquadControlMode::Float:
				InCutoffFloat = InParams.InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(Cutoff), InParams.OperatorSettings);
				break;
			case EMetaSoundCATBiquadControlMode::FloatArray:
				InCutoffArray = InParams.InputData.GetOrCreateDefaultDataReadReference<TArray<float>>(METASOUND_GET_PARAM_NAME(Cutoff), InParams.OperatorSettings);
				break;
			default:
				checkNoEntry();
				break;
			}

			FFloatReadRef InBandwidthFloat = TDataReadReference<float>::CreateNew(CATBiquadPrivate::DefaultBandwidth);
			TDataReadReference<TArray<float>> InBandwidthArray = TDataReadReference<TArray<float>>::CreateNew(TArray<float>{});
			switch (ConfigData->BandwidthMode)
			{
			case EMetaSoundCATBiquadControlMode::Float:
				InBandwidthFloat = InParams.InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(Bandwidth), InParams.OperatorSettings);
				break;
			case EMetaSoundCATBiquadControlMode::FloatArray:
				InBandwidthArray = InParams.InputData.GetOrCreateDefaultDataReadReference<TArray<float>>(METASOUND_GET_PARAM_NAME(Bandwidth), InParams.OperatorSettings);
				break;
			default:
				checkNoEntry();
				break;
			}

			FFloatReadRef InGainFloat = TDataReadReference<float>::CreateNew(CATBiquadPrivate::DefaultGainDb);
			TDataReadReference<TArray<float>> InGainArray = TDataReadReference<TArray<float>>::CreateNew(TArray<float>{});
			switch (ConfigData->GainMode)
			{
			case EMetaSoundCATBiquadControlMode::Float:
				InGainFloat = InParams.InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(GainDb), InParams.OperatorSettings);
				break;
			case EMetaSoundCATBiquadControlMode::FloatArray:
				InGainArray = InParams.InputData.GetOrCreateDefaultDataReadReference<TArray<float>>(METASOUND_GET_PARAM_NAME(GainDb), InParams.OperatorSettings);
				break;
			default:
				checkNoEntry();
				break;
			}

			FEnumBiQuadFilterReadRef InFilterType = InParams.InputData.GetOrCreateDefaultDataReadReference<FEnumEBiquadFilterType>(METASOUND_GET_PARAM_NAME(FilterType), InParams.OperatorSettings);

			FTimeReadRef InCutoffRiseTime = TDataReadReference<FTime>::CreateNew(FTime(0.0));
			FTimeReadRef InCutoffFallTime = TDataReadReference<FTime>::CreateNew(FTime(0.0));
			FTimeReadRef InBandwidthRiseTime = TDataReadReference<FTime>::CreateNew(FTime(0.0));
			FTimeReadRef InBandwidthFallTime = TDataReadReference<FTime>::CreateNew(FTime(0.0));
			FTimeReadRef InGainRiseTime = TDataReadReference<FTime>::CreateNew(FTime(0.0));
			FTimeReadRef InGainFallTime = TDataReadReference<FTime>::CreateNew(FTime(0.0));

			if (ConfigData->bEnableCutoffSlew)
			{
				InCutoffRiseTime = InParams.InputData.GetOrCreateDefaultDataReadReference<FTime>(METASOUND_GET_PARAM_NAME(CutoffRiseTime), InParams.OperatorSettings);
				InCutoffFallTime = InParams.InputData.GetOrCreateDefaultDataReadReference<FTime>(METASOUND_GET_PARAM_NAME(CutoffFallTime), InParams.OperatorSettings);
			}
			if (ConfigData->bEnableBandwidthSlew)
			{
				InBandwidthRiseTime = InParams.InputData.GetOrCreateDefaultDataReadReference<FTime>(METASOUND_GET_PARAM_NAME(BandwidthRiseTime), InParams.OperatorSettings);
				InBandwidthFallTime = InParams.InputData.GetOrCreateDefaultDataReadReference<FTime>(METASOUND_GET_PARAM_NAME(BandwidthFallTime), InParams.OperatorSettings);
			}
			if (ConfigData->bEnableGainSlew)
			{
				InGainRiseTime = InParams.InputData.GetOrCreateDefaultDataReadReference<FTime>(METASOUND_GET_PARAM_NAME(GainRiseTime), InParams.OperatorSettings);
				InGainFallTime = InParams.InputData.GetOrCreateDefaultDataReadReference<FTime>(METASOUND_GET_PARAM_NAME(GainFallTime), InParams.OperatorSettings);
			}

			FChannelAgnosticTypeWriteRef Out = FChannelAgnosticTypeWriteRef::CreateNew(InParams.OperatorSettings, ConcreteType->GetName());

			return MakeUnique<FCATBiquadOperator>(
				OperatorDataSharedPtr,
				InParams.OperatorSettings,
				MoveTemp(InSignal),
				MoveTemp(InCutoffFloat),
				MoveTemp(InCutoffArray),
				MoveTemp(InBandwidthFloat),
				MoveTemp(InBandwidthArray),
				MoveTemp(InGainFloat),
				MoveTemp(InGainArray),
				MoveTemp(InFilterType),
				MoveTemp(InCutoffRiseTime),
				MoveTemp(InCutoffFallTime),
				MoveTemp(InBandwidthRiseTime),
				MoveTemp(InBandwidthFallTime),
				MoveTemp(InGainRiseTime),
				MoveTemp(InGainFallTime),
				MoveTemp(Out));
		}

		virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
		{
			using namespace CATBiquadVertexNames;

			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Input), Input);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(FilterType), FilterType);

			if (OperatorData->bEnableCutoffSlew)
			{
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(CutoffRiseTime), CutoffRiseTime);
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(CutoffFallTime), CutoffFallTime);
			}
			if (OperatorData->bEnableBandwidthSlew)
			{
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(BandwidthRiseTime), BandwidthRiseTime);
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(BandwidthFallTime), BandwidthFallTime);
			}
			if (OperatorData->bEnableGainSlew)
			{
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(GainRiseTime), GainRiseTime);
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(GainFallTime), GainFallTime);
			}

			switch (OperatorData->CutoffMode)
			{
			case EMetaSoundCATBiquadControlMode::Float:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Cutoff), CutoffFloat);
				break;
			case EMetaSoundCATBiquadControlMode::FloatArray:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Cutoff), CutoffArray);
				break;
			default:
				checkNoEntry();
				break;
			}

			switch (OperatorData->BandwidthMode)
			{
			case EMetaSoundCATBiquadControlMode::Float:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Bandwidth), BandwidthFloat);
				break;
			case EMetaSoundCATBiquadControlMode::FloatArray:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Bandwidth), BandwidthArray);
				break;
			default:
				checkNoEntry();
				break;
			}

			switch (OperatorData->GainMode)
			{
			case EMetaSoundCATBiquadControlMode::Float:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(GainDb), GainFloat);
				break;
			case EMetaSoundCATBiquadControlMode::FloatArray:
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(GainDb), GainArray);
				break;
			default:
				checkNoEntry();
				break;
			}
		}

		virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
		{
			using namespace CATBiquadVertexNames;
			InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(Output), Output);
		}

		virtual void Reset(const IOperator::FResetParams& InParams)
		{
			ResetFilterState();
		}

		void Execute()
		{
			Output->Zero();

			const int32 NumFrames = Settings.GetNumFramesPerBlock();
			const int32 NumOutChannels = Output->NumChannels();
			if (NumFrames <= 0 || NumOutChannels <= 0)
			{
				return;
			}

			EnsureStateForChannels(NumOutChannels);

			if (*FilterType != PreviousFilterType)
			{
				for (int32 Channel = 0; Channel < NumOutChannels; ++Channel)
				{
					BiquadFilters[Channel].SetType(*FilterType);
				}
				PreviousFilterType = *FilterType;
			}

			const int32 NumInChannels = FMath::Max(1, Input->NumChannels());

			const bool bUseCutoffSlew = OperatorData->bEnableCutoffSlew;
			const bool bUseBandwidthSlew = OperatorData->bEnableBandwidthSlew;
			const bool bUseGainSlew = OperatorData->bEnableGainSlew;
			const bool bAnySlew = bUseCutoffSlew || bUseBandwidthSlew || bUseGainSlew;

			const float CutoffRiseSeconds = FMath::Max(0.0f, (float)CutoffRiseTime->GetSeconds());
			const float CutoffFallSeconds = FMath::Max(0.0f, (float)CutoffFallTime->GetSeconds());
			const float BandwidthRiseSeconds = FMath::Max(0.0f, (float)BandwidthRiseTime->GetSeconds());
			const float BandwidthFallSeconds = FMath::Max(0.0f, (float)BandwidthFallTime->GetSeconds());
			const float GainRiseSeconds = FMath::Max(0.0f, (float)GainRiseTime->GetSeconds());
			const float GainFallSeconds = FMath::Max(0.0f, (float)GainFallTime->GetSeconds());

			const float CutoffRiseAlpha = (CutoffRiseSeconds > 0.0f) ? FMath::Exp(-1.0f / (CutoffRiseSeconds * SampleRate)) : 0.0f;
			const float CutoffFallAlpha = (CutoffFallSeconds > 0.0f) ? FMath::Exp(-1.0f / (CutoffFallSeconds * SampleRate)) : 0.0f;
			const float BandwidthRiseAlpha = (BandwidthRiseSeconds > 0.0f) ? FMath::Exp(-1.0f / (BandwidthRiseSeconds * SampleRate)) : 0.0f;
			const float BandwidthFallAlpha = (BandwidthFallSeconds > 0.0f) ? FMath::Exp(-1.0f / (BandwidthFallSeconds * SampleRate)) : 0.0f;
			const float GainRiseAlpha = (GainRiseSeconds > 0.0f) ? FMath::Exp(-1.0f / (GainRiseSeconds * SampleRate)) : 0.0f;
			const float GainFallAlpha = (GainFallSeconds > 0.0f) ? FMath::Exp(-1.0f / (GainFallSeconds * SampleRate)) : 0.0f;

			const TArray<float>* CutoffValues = (OperatorData->CutoffMode == EMetaSoundCATBiquadControlMode::FloatArray) ? &(*CutoffArray) : nullptr;
			const TArray<float>* BandwidthValues = (OperatorData->BandwidthMode == EMetaSoundCATBiquadControlMode::FloatArray) ? &(*BandwidthArray) : nullptr;
			const TArray<float>* GainValues = (OperatorData->GainMode == EMetaSoundCATBiquadControlMode::FloatArray) ? &(*GainArray) : nullptr;

			for (int32 Channel = 0; Channel < NumOutChannels; ++Channel)
			{
				const int32 InChannel = (NumInChannels == 1) ? 0 : FMath::Min(Channel, NumInChannels - 1);
				TArrayView<const float> Src = Input->GetChannel(InChannel);
				TArrayView<float> Dst = Output->GetChannel(Channel);

				const float CutoffTarget = (OperatorData->CutoffMode == EMetaSoundCATBiquadControlMode::Float)
					? *CutoffFloat
					: CATBiquadPrivate::GetArrayValueOrDefault(*CutoffValues, Channel, CATBiquadPrivate::DefaultCutoffHz);
				const float BandwidthTarget = (OperatorData->BandwidthMode == EMetaSoundCATBiquadControlMode::Float)
					? *BandwidthFloat
					: CATBiquadPrivate::GetArrayValueOrDefault(*BandwidthValues, Channel, CATBiquadPrivate::DefaultBandwidth);
				const float GainTarget = (OperatorData->GainMode == EMetaSoundCATBiquadControlMode::Float)
					? *GainFloat
					: CATBiquadPrivate::GetArrayValueOrDefault(*GainValues, Channel, CATBiquadPrivate::DefaultGainDb);

				if (!bAnySlew)
				{
					const float CurrentCutoff = FMath::Clamp(CutoffTarget, 0.0f, MaxCutoffFrequency);
					const float CurrentBandwidth = FMath::Max(0.0f, BandwidthTarget);
					const float CurrentGain = FMath::Clamp(GainTarget, -90.0f, 20.0f);

					if (!FMath::IsNearlyEqual(PreviousFrequency[Channel], CurrentCutoff))
					{
						BiquadFilters[Channel].SetFrequency(CurrentCutoff);
						PreviousFrequency[Channel] = CurrentCutoff;
					}
					if (!FMath::IsNearlyEqual(PreviousBandwidth[Channel], CurrentBandwidth))
					{
						BiquadFilters[Channel].SetBandwidth(CurrentBandwidth);
						PreviousBandwidth[Channel] = CurrentBandwidth;
					}
					if (!FMath::IsNearlyEqual(PreviousGainDb[Channel], CurrentGain))
					{
						BiquadFilters[Channel].SetGainDB(CurrentGain);
						PreviousGainDb[Channel] = CurrentGain;
					}

					BiquadFilters[Channel].ProcessAudio(Src.GetData(), NumFrames, Dst.GetData());
					CutoffSlewState[Channel] = CurrentCutoff;
					BandwidthSlewState[Channel] = CurrentBandwidth;
					GainSlewState[Channel] = CurrentGain;
					continue;
				}

				float CurrentCutoffState = bSlewStateInitialized ? CutoffSlewState[Channel] : FMath::Clamp(CutoffTarget, 0.0f, MaxCutoffFrequency);
				float CurrentBandwidthState = bSlewStateInitialized ? BandwidthSlewState[Channel] : FMath::Max(0.0f, BandwidthTarget);
				float CurrentGainState = bSlewStateInitialized ? GainSlewState[Channel] : FMath::Clamp(GainTarget, -90.0f, 20.0f);

				for (int32 Frame = 0; Frame < NumFrames; ++Frame)
				{
					if (bUseCutoffSlew)
					{
						if (CutoffTarget > CurrentCutoffState)
						{
							CurrentCutoffState = CutoffRiseAlpha * CurrentCutoffState + (1.0f - CutoffRiseAlpha) * CutoffTarget;
						}
						else if (CutoffTarget < CurrentCutoffState)
						{
							CurrentCutoffState = CutoffFallAlpha * CurrentCutoffState + (1.0f - CutoffFallAlpha) * CutoffTarget;
						}
					}
					else
					{
						CurrentCutoffState = CutoffTarget;
					}

					if (bUseBandwidthSlew)
					{
						if (BandwidthTarget > CurrentBandwidthState)
						{
							CurrentBandwidthState = BandwidthRiseAlpha * CurrentBandwidthState + (1.0f - BandwidthRiseAlpha) * BandwidthTarget;
						}
						else if (BandwidthTarget < CurrentBandwidthState)
						{
							CurrentBandwidthState = BandwidthFallAlpha * CurrentBandwidthState + (1.0f - BandwidthFallAlpha) * BandwidthTarget;
						}
					}
					else
					{
						CurrentBandwidthState = BandwidthTarget;
					}

					if (bUseGainSlew)
					{
						if (GainTarget > CurrentGainState)
						{
							CurrentGainState = GainRiseAlpha * CurrentGainState + (1.0f - GainRiseAlpha) * GainTarget;
						}
						else if (GainTarget < CurrentGainState)
						{
							CurrentGainState = GainFallAlpha * CurrentGainState + (1.0f - GainFallAlpha) * GainTarget;
						}
					}
					else
					{
						CurrentGainState = GainTarget;
					}

					const float CurrentCutoff = FMath::Clamp(CurrentCutoffState, 0.0f, MaxCutoffFrequency);
					const float CurrentBandwidth = FMath::Max(0.0f, CurrentBandwidthState);
					const float CurrentGain = FMath::Clamp(CurrentGainState, -90.0f, 20.0f);

					if (!FMath::IsNearlyEqual(PreviousFrequency[Channel], CurrentCutoff))
					{
						BiquadFilters[Channel].SetFrequency(CurrentCutoff);
						PreviousFrequency[Channel] = CurrentCutoff;
					}
					if (!FMath::IsNearlyEqual(PreviousBandwidth[Channel], CurrentBandwidth))
					{
						BiquadFilters[Channel].SetBandwidth(CurrentBandwidth);
						PreviousBandwidth[Channel] = CurrentBandwidth;
					}
					if (!FMath::IsNearlyEqual(PreviousGainDb[Channel], CurrentGain))
					{
						BiquadFilters[Channel].SetGainDB(CurrentGain);
						PreviousGainDb[Channel] = CurrentGain;
					}

					float OutSample = 0.0f;
					BiquadFilters[Channel].ProcessAudio(&Src[Frame], 1, &OutSample);
					Dst[Frame] = OutSample;
				}

				CutoffSlewState[Channel] = FMath::Clamp(CurrentCutoffState, 0.0f, MaxCutoffFrequency);
				BandwidthSlewState[Channel] = FMath::Max(0.0f, CurrentBandwidthState);
				GainSlewState[Channel] = FMath::Clamp(CurrentGainState, -90.0f, 20.0f);
			}

			bSlewStateInitialized = true;
		}

	private:
		void EnsureStateForChannels(const int32 InNumChannels)
		{
			const bool bChannelCountChanged = (BiquadFilters.Num() != InNumChannels);
			if (!bChannelCountChanged)
			{
				return;
			}

			BiquadFilters.SetNum(InNumChannels);
			for (int32 Channel = 0; Channel < InNumChannels; ++Channel)
			{
				BiquadFilters[Channel].Init(SampleRate, 1, *FilterType);
			}

			PreviousFrequency.Init(CATBiquadPrivate::InvalidValue, InNumChannels);
			PreviousBandwidth.Init(CATBiquadPrivate::InvalidValue, InNumChannels);
			PreviousGainDb.Init(CATBiquadPrivate::InvalidValue, InNumChannels);
			CutoffSlewState.Init(CATBiquadPrivate::DefaultCutoffHz, InNumChannels);
			BandwidthSlewState.Init(CATBiquadPrivate::DefaultBandwidth, InNumChannels);
			GainSlewState.Init(CATBiquadPrivate::DefaultGainDb, InNumChannels);
			bSlewStateInitialized = false;
			PreviousFilterType = *FilterType;
		}

		void ResetFilterState()
		{
			for (Audio::FBiquadFilter& Filter : BiquadFilters)
			{
				Filter.Init(SampleRate, 1, *FilterType);
			}

			for (float& Value : PreviousFrequency)
			{
				Value = CATBiquadPrivate::InvalidValue;
			}
			for (float& Value : PreviousBandwidth)
			{
				Value = CATBiquadPrivate::InvalidValue;
			}
			for (float& Value : PreviousGainDb)
			{
				Value = CATBiquadPrivate::InvalidValue;
			}
			bSlewStateInitialized = false;
		}

		TSharedPtr<const FCATBiquadOperatorData> OperatorData;
		FOperatorSettings Settings;
		FChannelAgnosticTypeReadRef Input;
		FFloatReadRef CutoffFloat;
		TDataReadReference<TArray<float>> CutoffArray;
		FFloatReadRef BandwidthFloat;
		TDataReadReference<TArray<float>> BandwidthArray;
		FFloatReadRef GainFloat;
		TDataReadReference<TArray<float>> GainArray;
		FEnumBiQuadFilterReadRef FilterType;
		FTimeReadRef CutoffRiseTime;
		FTimeReadRef CutoffFallTime;
		FTimeReadRef BandwidthRiseTime;
		FTimeReadRef BandwidthFallTime;
		FTimeReadRef GainRiseTime;
		FTimeReadRef GainFallTime;
		FChannelAgnosticTypeWriteRef Output;

		float SampleRate = 48000.0f;
		float MaxCutoffFrequency = 24000.0f;
		Audio::EBiquadFilter::Type PreviousFilterType = Audio::EBiquadFilter::Lowpass;

		TArray<Audio::FBiquadFilter> BiquadFilters;
		TArray<float> PreviousFrequency;
		TArray<float> PreviousBandwidth;
		TArray<float> PreviousGainDb;
		TArray<float> CutoffSlewState;
		TArray<float> BandwidthSlewState;
		TArray<float> GainSlewState;
		bool bSlewStateInitialized = false;
	};

	using FCATBiquadNode = TNodeFacade<FCATBiquadOperator>;
	METASOUND_REGISTER_NODE_AND_CONFIGURATION(FCATBiquadNode, FMetaSoundCATBiquadNodeConfiguration);
}

TArray<FPropertyTextFName> UMetaSoundCATBiquadNodeOptionsHelper::GetSoundFileFormatChannelOptions()
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

FMetaSoundCATBiquadNodeConfiguration::FMetaSoundCATBiquadNodeConfiguration()
	: OperatorData(MakeShared<Metasound::CATBiquadPrivate::FCATBiquadOperatorData>(CatAudioTypeName, CutoffMode, BandwidthMode, GainMode, bEnableCutoffSlew, bEnableBandwidthSlew, bEnableGainSlew))
{
}

TInstancedStruct<FMetasoundFrontendClassInterface> FMetaSoundCATBiquadNodeConfiguration::OverrideDefaultInterface(const FMetasoundFrontendClass& InClass) const
{
	return TInstancedStruct<FMetasoundFrontendClassInterface>::Make(
		FMetasoundFrontendClassInterface::GenerateClassInterface(
			Metasound::CATBiquadPrivate::GetVertexInterface(CatAudioTypeName, CutoffMode, BandwidthMode, GainMode, bEnableCutoffSlew, bEnableBandwidthSlew, bEnableGainSlew)));
}

TSharedPtr<const Metasound::IOperatorData> FMetaSoundCATBiquadNodeConfiguration::GetOperatorData() const
{
	OperatorData->CatAudioTypeName = CatAudioTypeName;
	OperatorData->CutoffMode = CutoffMode;
	OperatorData->BandwidthMode = BandwidthMode;
	OperatorData->GainMode = GainMode;
	OperatorData->bEnableCutoffSlew = bEnableCutoffSlew;
	OperatorData->bEnableBandwidthSlew = bEnableBandwidthSlew;
	OperatorData->bEnableGainSlew = bEnableGainSlew;
	return OperatorData;
}

#undef LOCTEXT_NAMESPACE
