// Copyright 2026 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundAudioAndAudioNode.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"
#include "MetasoundPrimitives.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundStandardNodesCategories.h"
#include "MetasoundBranches/Public/MetasoundCommonMacros.h"

#define LOCTEXT_NAMESPACE "MetasoundAudioAndAudioNode"

namespace Metasound
{
	namespace AudioAndAudioNodeVertexNames
	{
		METASOUND_PARAM(InputSignalA, "A", "First audio input.");
		METASOUND_PARAM(InputSignalB, "B", "Second audio input.");
		METASOUND_PARAM(OutputSignal, "Out", "Output: 1.0 where A && B, else 0.0.");
	}

	class FAudioAndAudioOperator : public TExecutableOperator<FAudioAndAudioOperator>
	{
	public:
		FAudioAndAudioOperator(
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
			using namespace AudioAndAudioNodeVertexNames;

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
				Metadata.ClassName = { TEXT("UE"), TEXT("And"), TEXT("Audio") };
				Metadata.MajorVersion = 1;
				Metadata.MinorVersion = 0;
				Metadata.DisplayName = LOCTEXT("AudioAndAudioNodeDisplayName", "And (Audio && Audio)");
				Metadata.Description = LOCTEXT("AudioAndAudioNodeDesc", "Outputs 1.0 where A and B are both greater than 0.0.");
				
				Metadata.Keywords = { 
					METASOUND_LOCTEXT("AndKeyword", "&&"),
					METASOUND_LOCTEXT("AndKeyword2", "Compare") 
				};

				Metadata.Author = TEXT("Charles Matthews");
				Metadata.PromptIfMissing = PluginNodeMissingPrompt;
				Metadata.DefaultInterface = DeclareVertexInterface();
				Metadata.CategoryHierarchy = {
					METASOUND_LOCTEXT("Custom", "Branches"),
					METASOUND_LOCTEXT("CustomSub", "Math")
				};

				FNodeDisplayStyle DisplayStyle;
				DisplayStyle.ImageName = TEXT("MetasoundEditor.Graph.Node.Custom.And");
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
			using namespace AudioAndAudioNodeVertexNames;

			const FInputVertexInterfaceData& InputData = InParams.InputData;

			TDataReadReference<FAudioBuffer> InA =
				InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(InputSignalA), InParams.OperatorSettings);

			TDataReadReference<FAudioBuffer> InB =
				InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(InputSignalB), InParams.OperatorSettings);

			return MakeUnique<FAudioAndAudioOperator>(InParams.OperatorSettings, InA, InB);
		}

		METASOUND_DISABLE_LEGACY_IO()

		virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
		{
			using namespace AudioAndAudioNodeVertexNames;
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputSignalA), A);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputSignalB), B);
		}

		virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
		{
			using namespace AudioAndAudioNodeVertexNames;
			InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputSignal), Out);
		}

		virtual void Execute()
		{
			const float* AData = A->GetData();
			const float* BData = B->GetData();
			float* OutData = Out->GetData();

			for (int32 i = 0; i < BlockSize; ++i)
			{
				OutData[i] = (AData[i] > 0.0f && BData[i] > 0.0f) ? 1.0f : 0.0f;
			}
		}

	private:
		FAudioBufferReadRef A;
		FAudioBufferReadRef B;
		FAudioBufferWriteRef Out;
		int32 BlockSize;
	};

	class FAudioAndAudioNode : public FNodeFacade
	{
	public:
				static FNodeClassMetadata CreateNodeClassMetadata()
		{
		    return FAudioAndAudioOperator::GetNodeInfo();
		}
		FAudioAndAudioNode(FNodeData InitData)
			: FNodeFacade(InitData, MakeShared<const FNodeClassMetadata>(FAudioAndAudioOperator::GetNodeInfo()), TFacadeOperatorClass<FAudioAndAudioOperator>())
		{}
	};

	METASOUND_REGISTER_NODE(FAudioAndAudioNode);
}

#undef LOCTEXT_NAMESPACE