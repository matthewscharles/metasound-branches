// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundAudioGreaterThanOrEqualToFloatNode.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"
#include "MetasoundPrimitives.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundStandardNodesCategories.h"
#include "MetasoundBranches/Public/MetasoundCommonMacros.h"

#define LOCTEXT_NAMESPACE "MetasoundAudioGreaterThanOrEqualToFloatNode"

namespace Metasound
{
	namespace AudioGreaterThanOrEqualToFloatNodeVertexNames
	{
		METASOUND_PARAM(InputSignalA, "In A", "First audio input.");
		METASOUND_PARAM(InputSignalB, "In B", "Float to compare against.");
		METASOUND_PARAM(OutputSignal, "Out", "Output: 1.0 where In A >= In B, else 0.0.");
	}

	class FAudioGreaterThanOrEqualToFloatOperator : public TExecutableOperator<FAudioGreaterThanOrEqualToFloatOperator>
	{
	public:
		FAudioGreaterThanOrEqualToFloatOperator(
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
			using namespace AudioGreaterThanOrEqualToFloatNodeVertexNames;

			static const FVertexInterface Interface(
				FInputVertexInterface(
					TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputSignalA)),
					TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputSignalB))
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
				Metadata.ClassName = { TEXT("UE"), TEXT("Greater Equal"), TEXT("Float") };
				Metadata.MajorVersion = 1;
				Metadata.MinorVersion = 0;
				Metadata.DisplayName = LOCTEXT("AudioGreaterThanOrEqualToFloatNodeDisplayName", "Greater Equal (Audio >= Float)");
				Metadata.Description = LOCTEXT("AudioGreaterThanOrEqualToFloatNodeDesc", "Outputs 1.0 where In A is greater than or equal to In B, else 0.0.");
				Metadata.Author = TEXT("Charles Matthews");
				Metadata.PromptIfMissing = PluginNodeMissingPrompt;
				Metadata.DefaultInterface = DeclareVertexInterface();
				Metadata.CategoryHierarchy = {
					METASOUND_LOCTEXT("Custom", "Branches"),
					METASOUND_LOCTEXT("CustomSub", "Math")
				};
				Metadata.Keywords = {
					METASOUND_LOCTEXT("GreaterThanEqualKeyword", ">="),
					METASOUND_LOCTEXT("CompareKeyword", "Compare")
				};

				FNodeDisplayStyle DisplayStyle;
				DisplayStyle.ImageName = TEXT("MetasoundEditor.Graph.Node.Custom.GreaterThanEqualTo");
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
			using namespace AudioGreaterThanOrEqualToFloatNodeVertexNames;

			const FInputVertexInterfaceData& InputData = InParams.InputData;

			TDataReadReference<FAudioBuffer> InA =
				InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(InputSignalA), InParams.OperatorSettings);

			TDataReadReference<float> InB =
				InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputSignalB), InParams.OperatorSettings);

			return MakeUnique<FAudioGreaterThanOrEqualToFloatOperator>(InParams.OperatorSettings, InA, InB);
		}

		METASOUND_DISABLE_LEGACY_IO()

		virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
		{
			using namespace AudioGreaterThanOrEqualToFloatNodeVertexNames;
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputSignalA), A);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputSignalB), B);
		}

		virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
		{
			using namespace AudioGreaterThanOrEqualToFloatNodeVertexNames;
			InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputSignal), Out);
		}

		virtual void Execute()
		{
			const float* AData = A->GetData();
			float* OutData = Out->GetData();
			const float CompareB = *B;

			for (int32 i = 0; i < BlockSize; ++i)
			{
				OutData[i] = (AData[i] >= CompareB) ? 1.0f : 0.0f;
			}
		}

	private:
		FAudioBufferReadRef A;
		FFloatReadRef B;
		FAudioBufferWriteRef Out;
		int32 BlockSize;
	};

	class FAudioGreaterThanOrEqualToFloatNode : public FNodeFacade
	{
	public:
		FAudioGreaterThanOrEqualToFloatNode(const FNodeInitData& InitData)
			: FNodeFacade(InitData.InstanceName, InitData.InstanceID, TFacadeOperatorClass<FAudioGreaterThanOrEqualToFloatOperator>())
		{}
	};

	METASOUND_REGISTER_NODE(FAudioGreaterThanOrEqualToFloatNode);
}

#undef LOCTEXT_NAMESPACE