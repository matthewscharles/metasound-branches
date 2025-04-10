// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundAudioLessThanOrEqualToFloatNode.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"
#include "MetasoundPrimitives.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundStandardNodesCategories.h"
#include "MetasoundBranches/Public/MetasoundCommonMacros.h"

#define LOCTEXT_NAMESPACE "MetasoundAudioLessThanOrEqualToFloatNode"

namespace Metasound
{
	namespace AudioLessThanOrEqualToFloatNodeVertexNames
	{
		METASOUND_PARAM(InputSignal,  "In",        "Audio input.");
		METASOUND_PARAM(Threshold,    "Threshold", "Float threshold to compare input against.");
		METASOUND_PARAM(OutputSignal, "Out",       "Output signal if input <= threshold.");
	}

	class FAudioLessThanOrEqualToFloatOperator : public TExecutableOperator<FAudioLessThanOrEqualToFloatOperator>
	{
	public:
		FAudioLessThanOrEqualToFloatOperator(
			const FOperatorSettings& InSettings,
			const FAudioBufferReadRef& InAudio,
			const FFloatReadRef& InThreshold
		)
			: AudioIn(InAudio)
			, Threshold(InThreshold)
			, AudioOut(FAudioBufferWriteRef::CreateNew(InSettings))
			, BlockSize(InSettings.GetNumFramesPerBlock())
		{}

		static const FVertexInterface& DeclareVertexInterface()
		{
			using namespace AudioLessThanOrEqualToFloatNodeVertexNames;

			static const FVertexInterface Interface(
				FInputVertexInterface(
					TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputSignal)),
					TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(Threshold))
				),
				FOutputVertexInterface(
					TOutputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputSignal))
				)
			);

			return Interface;
		}

		static const FNodeClassMetadata& GetNodeInfo()
		{
			auto CreateMetadata = []() -> FNodeClassMetadata
			{
				FNodeClassMetadata Metadata;
				Metadata.ClassName = { TEXT("UE"), TEXT("AudioLessThanOrEqualToFloat"), TEXT("Audio") };
				Metadata.MajorVersion = 1;
				Metadata.MinorVersion = 0;
				Metadata.DisplayName = LOCTEXT("AudioLessThanOrEqualToFloatNodeDisplayName", "<=");
				Metadata.Description = LOCTEXT("AudioLessThanOrEqualToFloatNodeDesc", "Outputs audio signal of 1 if input <= threshold.");
				Metadata.Author = TEXT("Charles Matthews");
				Metadata.PromptIfMissing = PluginNodeMissingPrompt;
				Metadata.DefaultInterface = DeclareVertexInterface();
				Metadata.CategoryHierarchy = {
					METASOUND_LOCTEXT("Custom", "Branches"),
					METASOUND_LOCTEXT("CustomSub", "Math")
				};
				return Metadata;
			};

			static const FNodeClassMetadata Metadata = CreateMetadata();
			return Metadata;
		}

		static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
		{
			using namespace AudioLessThanOrEqualToFloatNodeVertexNames;

			const FInputVertexInterfaceData& InputData = InParams.InputData;

			TDataReadReference<FAudioBuffer> InAudio =
				InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(InputSignal), InParams.OperatorSettings);

			TDataReadReference<float> InThreshold =
				InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(Threshold), InParams.OperatorSettings);

			return MakeUnique<FAudioLessThanOrEqualToFloatOperator>(InParams.OperatorSettings, InAudio, InThreshold);
		}

		METASOUND_DISABLE_LEGACY_IO()

		virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
		{
			using namespace AudioLessThanOrEqualToFloatNodeVertexNames;
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputSignal), AudioIn);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Threshold), Threshold);
		}

		virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
		{
			using namespace AudioLessThanOrEqualToFloatNodeVertexNames;
			InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputSignal), AudioOut);
		}

		virtual void Execute()
		{
			const float* InData = AudioIn->GetData();
			float* OutData = AudioOut->GetData();
			const float CompareThreshold = *Threshold;

			for (int32 i = 0; i < BlockSize; ++i)
			{
				OutData[i] = (InData[i] <= CompareThreshold) ? 1.0f : 0.0f;
			}
		}

	private:
		FAudioBufferReadRef AudioIn;
		FFloatReadRef Threshold;
		FAudioBufferWriteRef AudioOut;
		int32 BlockSize;
	};

	class FAudioLessThanOrEqualToFloatNode : public FNodeFacade
	{
	public:
		FAudioLessThanOrEqualToFloatNode(const FNodeInitData& InitData)
			: FNodeFacade(InitData.InstanceName, InitData.InstanceID, TFacadeOperatorClass<FAudioLessThanOrEqualToFloatOperator>())
		{}
	};

	METASOUND_REGISTER_NODE(FAudioLessThanOrEqualToFloatNode);
}

#undef LOCTEXT_NAMESPACE