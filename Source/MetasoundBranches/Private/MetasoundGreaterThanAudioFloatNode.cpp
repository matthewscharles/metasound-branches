// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundGreaterThanAudioFloatNode.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"
#include "MetasoundPrimitives.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundStandardNodesCategories.h"
#include "MetasoundBranches/Public/MetasoundCommonMacros.h"

#define LOCTEXT_NAMESPACE "MetasoundGreaterThanAudioFloatNode"

namespace Metasound
{
	namespace GreaterThanAudioFloatNodeVertexNames
	{
		METASOUND_PARAM(InputSignal,  "A",         "Audio input.");
		METASOUND_PARAM(Threshold,    "B",         "Float to compare against.");
		METASOUND_PARAM(OutputSignal, "Out",       "Output: 1.0 where A > B, else 0.0.");
	}

	class FGreaterThanAudioFloatOperator : public TExecutableOperator<FGreaterThanAudioFloatOperator>
	{
	public:
		FGreaterThanAudioFloatOperator(
			const FOperatorSettings& InSettings,
			const FAudioBufferReadRef& InA,
			const FFloatReadRef& InB
		)
			: A(InA)
			, B(InB)
			, Out(FAudioBufferWriteRef::CreateNew(InSettings))
			, BlockSize(InSettings.GetNumFramesPerBlock())
		{}

		static const FVertexInterface& DeclareVertexInterface()
		{
			using namespace GreaterThanAudioFloatNodeVertexNames;

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
				Metadata.ClassName = { TEXT("UE"), TEXT("GreaterThanFloat"), TEXT("Audio") };
				Metadata.MajorVersion = 1;
				Metadata.MinorVersion = 0;
				Metadata.DisplayName = LOCTEXT("GreaterThanAudioFloatNodeDisplayName", "Greater Than (Audio > Float)");
				Metadata.Description = LOCTEXT("GreaterThanAudioFloatNodeDesc", "Outputs 1.0 where A > B, else 0.0.");
				Metadata.Author = TEXT("Charles Matthews");
				Metadata.PromptIfMissing = PluginNodeMissingPrompt;
				Metadata.DefaultInterface = DeclareVertexInterface();
				Metadata.CategoryHierarchy = {
					METASOUND_LOCTEXT("Custom", "Branches"),
					METASOUND_LOCTEXT("CustomSub", "Math")
				};

				FNodeDisplayStyle DisplayStyle;
				DisplayStyle.ImageName = TEXT("MetasoundEditor.Graph.Node.Custom.GreaterThan");
				DisplayStyle.bShowName = false;
				DisplayStyle.bShowInputNames = false;
				DisplayStyle.bShowOutputNames = false;
				Metadata.DisplayStyle = DisplayStyle;

				return Metadata;
			};

			static const FNodeClassMetadata Metadata = CreateMetadata();
			return Metadata;
		}

		static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
		{
			using namespace GreaterThanAudioFloatNodeVertexNames;

			const FInputVertexInterfaceData& InputData = InParams.InputData;

			TDataReadReference<FAudioBuffer> InA =
				InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(InputSignal), InParams.OperatorSettings);

			TDataReadReference<float> InB =
				InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(Threshold), InParams.OperatorSettings);

			return MakeUnique<FGreaterThanAudioFloatOperator>(InParams.OperatorSettings, InA, InB);
		}

		METASOUND_DISABLE_LEGACY_IO()

		virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
		{
			using namespace GreaterThanAudioFloatNodeVertexNames;
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputSignal), A);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Threshold), B);
		}

		virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
		{
			using namespace GreaterThanAudioFloatNodeVertexNames;
			InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputSignal), Out);
		}

		virtual void Execute()
		{
			const float* AData = A->GetData();
			float* OutData = Out->GetData();
			const float CompareB = *B;

			for (int32 i = 0; i < BlockSize; ++i)
			{
				OutData[i] = (AData[i] > CompareB) ? 1.0f : 0.0f;
			}
		}

	private:
		FAudioBufferReadRef A;
		FFloatReadRef B;
		FAudioBufferWriteRef Out;
		int32 BlockSize;
	};

	class FGreaterThanAudioFloatNode : public FNodeFacade
	{
	public:
		FGreaterThanAudioFloatNode(const FNodeInitData& InitData)
			: FNodeFacade(InitData.InstanceName, InitData.InstanceID, TFacadeOperatorClass<FGreaterThanAudioFloatOperator>())
		{}
	};

	METASOUND_REGISTER_NODE(FGreaterThanAudioFloatNode);
}

#undef LOCTEXT_NAMESPACE