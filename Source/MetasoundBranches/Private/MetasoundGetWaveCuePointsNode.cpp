// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundGetWaveCuePointsNode.h"  // Node class declaration

#include "MetasoundExecutableOperator.h"     // TExecutableOperator class
#include "MetasoundPrimitives.h"             // DataRead/WriteRef types for various primitives
#include "MetasoundNodeRegistrationMacro.h"  // METASOUND_LOCTEXT and METASOUND_REGISTER_NODE macros
#include "MetasoundFacade.h"                 // FNodeFacade, reduces boilerplate code
#include "MetasoundParamHelper.h"            // METASOUND_PARAM and METASOUND_GET_PARAM macros
#include "Algo/Sort.h"                       // For sorting cue points
#include "MetasoundWave.h"                   // For FWaveAsset and FSoundWaveCuePoint

#define LOCTEXT_NAMESPACE "MetasoundGetWaveCuePoints"

namespace Metasound
{
	// Vertex Names - define the node's inputs and outputs
	namespace WaveCuePointsNodeVertexNames
	{
		// Input
		METASOUND_PARAM(ParamWaveAsset, "Wave", "Input Wave Asset");

		// Outputs
		METASOUND_PARAM(OutCuePointIDs, "Cue Point IDs", "Array of Cue Point IDs");
		METASOUND_PARAM(OutCuePointTimes, "Cue Point Times", "Array of Cue Point Times");
		METASOUND_PARAM(OutCuePointLabels, "Cue Point Labels", "Array of Cue Point Labels");
	}

	// Operator Class - defines how the node is created and executed
	class FGetWaveCuePointsOperator : public TExecutableOperator<FGetWaveCuePointsOperator>
	{
	public:
		// Constructor: Initialize input and create output arrays.
		FGetWaveCuePointsOperator(
            const FOperatorSettings& InSettings,
            const TDataReadReference<FWaveAsset>& InWaveAsset)
            : WaveAsset(InWaveAsset)
            , CuePointIDs(TDataWriteReference<TArray<int32>>::CreateNew())
            , CuePointTimes(TDataWriteReference<TArray<FTime>>::CreateNew())
            , CuePointLabels(TDataWriteReference<TArray<FString>>::CreateNew())
        {
            Execute();
        }

		// Declare the vertex interface.
		static const FVertexInterface& DeclareVertexInterface()
		{
			using namespace WaveCuePointsNodeVertexNames;
			static const FVertexInterface Interface(
				FInputVertexInterface(
					TInputDataVertex<FWaveAsset>(METASOUND_GET_PARAM_NAME_AND_METADATA(ParamWaveAsset))
				),
				FOutputVertexInterface(
					TOutputDataVertex<TArray<int32>>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutCuePointIDs)),
					TOutputDataVertex<TArray<FTime>>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutCuePointTimes)),
					TOutputDataVertex<TArray<FString>>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutCuePointLabels))
				)
			);
			return Interface;
		}

		// Node metadata.
		static const FNodeClassMetadata& GetNodeInfo()
		{
			auto CreateNodeClassMetadata = []() -> FNodeClassMetadata
			{
				FNodeClassMetadata Metadata;
				Metadata.ClassName = { TEXT("UE"), TEXT("GetWaveCuePoints"), TEXT("Audio") };
				Metadata.MajorVersion = 1;
				Metadata.MinorVersion = 0;
				Metadata.DisplayName = METASOUND_LOCTEXT("GetWaveCuePointsDisplayName", "Get Wave Cue Points");
				Metadata.Description = METASOUND_LOCTEXT("GetWaveCuePointsDesc", "Extracts cue points from a wave asset.");
				Metadata.Author = "Charles Matthews";
				Metadata.PromptIfMissing = PluginNodeMissingPrompt;
				Metadata.DefaultInterface = DeclareVertexInterface();
				Metadata.CategoryHierarchy = { METASOUND_LOCTEXT("Custom", "Branches") };
				return Metadata;
			};

			static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
			return Metadata;
		}

		// Retrieve input data references.
		virtual FDataReferenceCollection GetInputs() const override
		{
			using namespace WaveCuePointsNodeVertexNames;
			FDataReferenceCollection Inputs;
			Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(ParamWaveAsset), WaveAsset);
			return Inputs;
		}

		// Retrieve output data references.
		virtual FDataReferenceCollection GetOutputs() const override
		{
			using namespace WaveCuePointsNodeVertexNames;
			FDataReferenceCollection Outputs;
			Outputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutCuePointIDs), CuePointIDs);
            Outputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutCuePointTimes), CuePointTimes);
            Outputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutCuePointLabels), CuePointLabels);
			return Outputs;
		}

		// Create a new operator instance.
		static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults)
		{
			using namespace WaveCuePointsNodeVertexNames;
			const FInputVertexInterfaceData& InputData = InParams.InputData;
			TDataReadReference<FWaveAsset> InWaveAsset = InputData.GetOrConstructDataReadReference<FWaveAsset>(METASOUND_GET_PARAM_NAME(ParamWaveAsset));
			return MakeUnique<FGetWaveCuePointsOperator>(InParams.OperatorSettings, InWaveAsset);
		}

		// Bind the input pin.
		virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
		{
			using namespace WaveCuePointsNodeVertexNames;
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(ParamWaveAsset), WaveAsset);
		}

		// Bind the output pins.
		virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
		{
			using namespace WaveCuePointsNodeVertexNames;
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutCuePointIDs), CuePointIDs);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutCuePointTimes), CuePointTimes);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutCuePointLabels), CuePointLabels);
		}

		// Primary execution function. Extracts cue points from the Wave asset.
		virtual void Execute()
		{
			// Clear any previous data.
			CuePointIDs->Empty();
			CuePointTimes->Empty();
			CuePointLabels->Empty();

			if (WaveAsset->IsSoundWaveValid())
			{
				FSoundWaveProxyPtr SoundWaveProxy = (*WaveAsset).GetSoundWaveProxy();
				if (SoundWaveProxy.IsValid())
				{
					// Extract and sort cue points.
					TArray<FSoundWaveCuePoint> SortedCuePoints = SoundWaveProxy->GetCuePoints();
					Algo::SortBy(SortedCuePoints, [](const FSoundWaveCuePoint& Cue) { return Cue.FramePosition; });
					const float SampleRate = SoundWaveProxy->GetSampleRate();
					for (const FSoundWaveCuePoint& Cue : SortedCuePoints)
					{
						// Populate the output arrays.
						CuePointIDs->Add(Cue.CuePointID);
						float TimeSeconds = (SampleRate > 0.f) ? static_cast<float>(Cue.FramePosition) / SampleRate : 0.f;
						CuePointTimes->Add(FTime::FromSeconds(TimeSeconds));
						CuePointLabels->Add(Cue.Label);
					}
				}
			}
		}

	private:
		// Input
		TDataReadReference<FWaveAsset> WaveAsset;

		// Outputs
		TDataWriteReference<TArray<int32>> CuePointIDs;
        TDataWriteReference<TArray<FTime>> CuePointTimes;
        TDataWriteReference<TArray<FString>> CuePointLabels;

	};

	// Node Facade - wraps the operator so that it can be used in MetaSound graphs.
	class FGetWaveCuePointsNode : public FNodeFacade
	{
	public:
		FGetWaveCuePointsNode(const FNodeInitData& InInitData)
			: FNodeFacade(InInitData.InstanceName, InInitData.InstanceID, TFacadeOperatorClass<FGetWaveCuePointsOperator>())
		{
		}
	};

	// Register the node with the MetaSound system.
	METASOUND_REGISTER_NODE(FGetWaveCuePointsNode);
}

#undef LOCTEXT_NAMESPACE