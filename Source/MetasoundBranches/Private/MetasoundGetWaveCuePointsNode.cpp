// Copyright 2026 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundGetWaveCuePointsNode.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundPrimitives.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"
#include "Algo/Sort.h"
#include "MetasoundWave.h"
#include "MetasoundBranches/Public/MetasoundCommonMacros.h"

#define LOCTEXT_NAMESPACE "MetasoundGetWaveCuePoints"

namespace Metasound
{
	namespace WaveCuePointsNodeVertexNames
	{
		METASOUND_PARAM(ParamWaveAsset, "Wave", "Input Wave Asset");
		METASOUND_PARAM(ParamTriggerGetCuePoints, "Get Cue Points", "Trigger to extract cue points");

		METASOUND_PARAM(OutCuePointIDs, "Cue Point IDs", "Array of Cue Point IDs");
		METASOUND_PARAM(OutCuePointTimes, "Cue Point Times", "Array of Cue Point Times");
		METASOUND_PARAM(OutCuePointLabels, "Cue Point Labels", "Array of Cue Point Labels");
	}

	class FGetWaveCuePointsOperator : public TExecutableOperator<FGetWaveCuePointsOperator>
	{
	public:
		FGetWaveCuePointsOperator(
			const FOperatorSettings& InSettings,
			const TDataReadReference<FWaveAsset>& InWaveAsset,
			const FTriggerReadRef& InTriggerGetCuePoints)
			: WaveAsset(InWaveAsset)
			, TriggerGetCuePoints(InTriggerGetCuePoints)
			, CuePointIDs(TDataWriteReference<TArray<int32>>::CreateNew())
			, CuePointTimes(TDataWriteReference<TArray<FTime>>::CreateNew())
			, CuePointLabels(TDataWriteReference<TArray<FString>>::CreateNew())
		{
		}

		static const FVertexInterface& DeclareVertexInterface()
		{
			using namespace WaveCuePointsNodeVertexNames;
			static const FVertexInterface Interface(
				FInputVertexInterface(
					TInputDataVertex<FWaveAsset>(METASOUND_GET_PARAM_NAME_AND_METADATA(ParamWaveAsset)),
					TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(ParamTriggerGetCuePoints))
				),
				FOutputVertexInterface(
					TOutputDataVertex<TArray<int32>>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutCuePointIDs)),
					TOutputDataVertex<TArray<FTime>>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutCuePointTimes)),
					TOutputDataVertex<TArray<FString>>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutCuePointLabels))
				)
			);
			return Interface;
		}

		static const FNodeClassMetadata& GetNodeInfo()
		{
			auto CreateNodeClassMetadata = []() -> FNodeClassMetadata
			{
				FNodeClassMetadata Metadata;
				Metadata.ClassName = { TEXT("UE"), TEXT("GetWaveCuePoints"), TEXT("Audio") };
				Metadata.MajorVersion = 1;
				Metadata.MinorVersion = 0;
				Metadata.DisplayName = METASOUND_LOCTEXT("GetWaveCuePointsDisplayName", "Get Wave Cue Points");
				Metadata.Description = METASOUND_LOCTEXT("GetWaveCuePointsDesc", "Extracts cue points from a wave asset when triggered.");
				Metadata.Author = "Charles Matthews";
				Metadata.PromptIfMissing = PluginNodeMissingPrompt;
				Metadata.DefaultInterface = DeclareVertexInterface();
				Metadata.CategoryHierarchy = {
					METASOUND_LOCTEXT("Custom", "Branches"),
					METASOUND_LOCTEXT("CustomSub", "Wave Asset")
				};
				METASOUND_BRANCHES_APPLY_NODE_STYLE(Metadata);
                return Metadata;
			};

			static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
                return Metadata;
		}
    
		static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults)
		{
			using namespace WaveCuePointsNodeVertexNames;
			const FInputVertexInterfaceData& InputData = InParams.InputData;
			TDataReadReference<FWaveAsset> InWaveAsset = InputData.GetOrCreateDefaultDataReadReference<FWaveAsset>(METASOUND_GET_PARAM_NAME(ParamWaveAsset), InParams.OperatorSettings);
			FTriggerReadRef InTriggerGetCuePoints = InputData.GetOrCreateDefaultDataReadReference<FTrigger>(METASOUND_GET_PARAM_NAME(ParamTriggerGetCuePoints), InParams.OperatorSettings);
			return MakeUnique<FGetWaveCuePointsOperator>(InParams.OperatorSettings, InWaveAsset, InTriggerGetCuePoints);
		}
		
		METASOUND_DISABLE_LEGACY_IO()
		
		virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
		{
			using namespace WaveCuePointsNodeVertexNames;
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(ParamWaveAsset), WaveAsset);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(ParamTriggerGetCuePoints), TriggerGetCuePoints);
		}

		virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
		{
			using namespace WaveCuePointsNodeVertexNames;
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutCuePointIDs), CuePointIDs);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutCuePointTimes), CuePointTimes);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutCuePointLabels), CuePointLabels);
		}

		virtual void Execute()
		{
			TriggerGetCuePoints->ExecuteBlock(
				[](int32 StartFrame, int32 EndFrame) {},
				[this](int32 TriggerFrame, int32 TriggerFrameEnd)
				{
					CuePointIDs->Empty();
					CuePointTimes->Empty();
					CuePointLabels->Empty();

					if (WaveAsset->IsSoundWaveValid())
					{
						FSoundWaveProxyPtr SoundWaveProxy = (*WaveAsset).GetSoundWaveProxy();
						if (SoundWaveProxy.IsValid())
						{
							TArray<FSoundWaveCuePoint> SortedCuePoints = SoundWaveProxy->GetCuePoints();
							Algo::SortBy(SortedCuePoints, [](const FSoundWaveCuePoint& Cue) { return Cue.FramePosition; });
							const float SampleRate = SoundWaveProxy->GetSampleRate();
							for (const FSoundWaveCuePoint& Cue : SortedCuePoints)
							{
								CuePointIDs->Add(Cue.CuePointID);
								float TimeSeconds = (SampleRate > 0.f) ? static_cast<float>(Cue.FramePosition) / SampleRate : 0.f;
								CuePointTimes->Add(FTime::FromSeconds(TimeSeconds));
								CuePointLabels->Add(Cue.Label);
							}
						}
					}
				}
			);
		}

	private:
		TDataReadReference<FWaveAsset> WaveAsset;
		FTriggerReadRef TriggerGetCuePoints;

		TDataWriteReference<TArray<int32>> CuePointIDs;
		TDataWriteReference<TArray<FTime>> CuePointTimes;
		TDataWriteReference<TArray<FString>> CuePointLabels;
	};

	class FGetWaveCuePointsNode : public FNodeFacade
	{
	public:
				static FNodeClassMetadata CreateNodeClassMetadata()
		{
		    return FGetWaveCuePointsOperator::GetNodeInfo();
		}
		FGetWaveCuePointsNode(FNodeData InInitData)
			: FNodeFacade(InInitData, MakeShared<const FNodeClassMetadata>(FGetWaveCuePointsOperator::GetNodeInfo()), TFacadeOperatorClass<FGetWaveCuePointsOperator>())
		{
		}
	};

	METASOUND_REGISTER_NODE(FGetWaveCuePointsNode);
}

#undef LOCTEXT_NAMESPACE