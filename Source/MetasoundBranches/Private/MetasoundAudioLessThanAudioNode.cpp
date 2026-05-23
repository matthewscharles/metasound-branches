// Copyright 2026 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundAudioLessThanAudioNode.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"
#include "MetasoundPrimitives.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundStandardNodesCategories.h"
#include "MetasoundBranches/Public/MetasoundCommonMacros.h"

#define LOCTEXT_NAMESPACE "MetasoundAudioLessThanAudioNode"

namespace Metasound
{
	namespace AudioLessThanAudioNodeVertexNames
	{
		METASOUND_PARAM(InputSignalA, "In A", "First audio input.");
		METASOUND_PARAM(InputSignalB, "In B", "Second audio input to compare against.");
		METASOUND_PARAM(OutputSignal, "Out", "Output: 1.0 where In A < In B, else 0.0.");
	}

	class FAudioLessThanAudioOperator : public TExecutableOperator<FAudioLessThanAudioOperator>
	{
	public:
		FAudioLessThanAudioOperator(
			const FOperatorSettings& InSettings,
			const FAudioBufferReadRef& InA,
			const FAudioBufferReadRef& InB
		)
			: A(InA)
			, B(InB)
			, Out(FAudioBufferWriteRef::CreateNew(InSettings))
			, BlockSize(InSettings.GetNumFramesPerBlock())
		{}

		static const FVertexInterface& DeclareVertexInterface()
		{
			using namespace AudioLessThanAudioNodeVertexNames;

			static const FVertexInterface Interface(
				FInputVertexInterface(
					TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputSignalA)),
					TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputSignalB))
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
				Metadata.ClassName = { TEXT("UE"), TEXT("Less"), TEXT("Audio") };
				Metadata.MajorVersion = 1;
				Metadata.MinorVersion = 0;
				Metadata.DisplayName = LOCTEXT("AudioLessThanAudioNodeDisplayName", "Less (Audio < Audio)");
				Metadata.Description = LOCTEXT("AudioLessThanAudioNodeDesc", "Outputs audio signal of 1.0 where In A is less than In B, otherwise 0.0.");
				Metadata.Author = TEXT("Charles Matthews");
				Metadata.PromptIfMissing = PluginNodeMissingPrompt;
				Metadata.DefaultInterface = DeclareVertexInterface();
				Metadata.CategoryHierarchy = {
					METASOUND_LOCTEXT("Custom", "Branches"),
					METASOUND_LOCTEXT("CustomSub", "Math")
				};
				Metadata.Keywords = {
					METASOUND_LOCTEXT("LessThanKeyword", "<"),
					METASOUND_LOCTEXT("CompareKeyword", "Compare")
				};

				FNodeDisplayStyle DisplayStyle;
				DisplayStyle.ImageName = TEXT("MetasoundEditor.Graph.Node.Custom.LessThan");
				DisplayStyle.bShowName = false;
				DisplayStyle.bShowInputNames = false;
				DisplayStyle.bShowOutputNames = false;
				Metadata.DisplayStyle = DisplayStyle;

				METASOUND_BRANCHES_APPLY_NODE_STYLE(Metadata);

				return Metadata;
			};

			static const FNodeClassMetadata Metadata = CreateMetadata();
			return Metadata;
		}

		static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
		{
			using namespace AudioLessThanAudioNodeVertexNames;

			const FInputVertexInterfaceData& InputData = InParams.InputData;

			TDataReadReference<FAudioBuffer> InA =
				InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(InputSignalA), InParams.OperatorSettings);

			TDataReadReference<FAudioBuffer> InB =
				InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(InputSignalB), InParams.OperatorSettings);

			return MakeUnique<FAudioLessThanAudioOperator>(InParams.OperatorSettings, InA, InB);
		}

		METASOUND_DISABLE_LEGACY_IO()

		virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
		{
			using namespace AudioLessThanAudioNodeVertexNames;
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputSignalA), A);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputSignalB), B);
		}

		virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
		{
			using namespace AudioLessThanAudioNodeVertexNames;
			InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputSignal), Out);
		}

		virtual void Execute()
		{
			const float* AData = A->GetData();
			const float* BData = B->GetData();
			float* OutData = Out->GetData();

			for (int32 i = 0; i < BlockSize; ++i)
			{
				OutData[i] = (AData[i] < BData[i]) ? 1.0f : 0.0f;
			}
		}

	private:
		FAudioBufferReadRef A;
		FAudioBufferReadRef B;
		FAudioBufferWriteRef Out;
		int32 BlockSize;
	};

	class FAudioLessThanAudioNode : public FNodeFacade
	{
	public:
				static FNodeClassMetadata CreateNodeClassMetadata()
		{
		    return FAudioLessThanAudioOperator::GetNodeInfo();
		}
		FAudioLessThanAudioNode(FNodeData InitData)
			: FNodeFacade(InitData, MakeShared<const FNodeClassMetadata>(FAudioLessThanAudioOperator::GetNodeInfo()), TFacadeOperatorClass<FAudioLessThanAudioOperator>())
		{}
	};

	METASOUND_REGISTER_NODE(FAudioLessThanAudioNode);
}

#undef LOCTEXT_NAMESPACE