// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundAudioGreaterThanOrEqualToAudioNode.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"
#include "MetasoundPrimitives.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundStandardNodesCategories.h"
#include "MetasoundBranches/Public/MetasoundCommonMacros.h"

#define LOCTEXT_NAMESPACE "MetasoundAudioGreaterThanOrEqualToAudioNode"

namespace Metasound
{
	namespace AudioGreaterThanOrEqualToAudioNodeVertexNames
	{
		METASOUND_PARAM(InputSignalA, "In A", "First audio input.");
		METASOUND_PARAM(InputSignalB, "In B", "Second audio input to compare against.");
		METASOUND_PARAM(OutputSignal, "Out", "Output: 1.0 where InA >= InB, else 0.0.");
	}

	class FAudioGreaterThanOrEqualToAudioOperator : public TExecutableOperator<FAudioGreaterThanOrEqualToAudioOperator>
	{
	public:
		FAudioGreaterThanOrEqualToAudioOperator(
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
			using namespace AudioGreaterThanOrEqualToAudioNodeVertexNames;

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
				Metadata.ClassName = { TEXT("UE"), TEXT("Greater Equal"), TEXT("Audio") };
				Metadata.MajorVersion = 1;
				Metadata.MinorVersion = 0;
				Metadata.DisplayName = LOCTEXT("AudioGreaterThanOrEqualToAudioNodeDisplayName", "Greater Equal (Audio >= Audio)");
				Metadata.Description = LOCTEXT("AudioGreaterThanOrEqualToAudioNodeDesc", "Outputs 1.0 where InA is greater than or equal to InB, else 0.0.");
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
			using namespace AudioGreaterThanOrEqualToAudioNodeVertexNames;

			const FInputVertexInterfaceData& InputData = InParams.InputData;

			TDataReadReference<FAudioBuffer> InA =
				InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(InputSignalA), InParams.OperatorSettings);

			TDataReadReference<FAudioBuffer> InB =
				InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(InputSignalB), InParams.OperatorSettings);

			return MakeUnique<FAudioGreaterThanOrEqualToAudioOperator>(InParams.OperatorSettings, InA, InB);
		}

		METASOUND_DISABLE_LEGACY_IO()

		virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
		{
			using namespace AudioGreaterThanOrEqualToAudioNodeVertexNames;
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputSignalA), A);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputSignalB), B);
		}

		virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
		{
			using namespace AudioGreaterThanOrEqualToAudioNodeVertexNames;
			InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputSignal), Out);
		}

		virtual void Execute()
		{
			const float* AData = A->GetData();
			const float* BData = B->GetData();
			float* OutData = Out->GetData();

			for (int32 i = 0; i < BlockSize; ++i)
			{
				OutData[i] = (AData[i] >= BData[i]) ? 1.0f : 0.0f;
			}
		}

	private:
		FAudioBufferReadRef A;
		FAudioBufferReadRef B;
		FAudioBufferWriteRef Out;
		int32 BlockSize;
	};

	class FAudioGreaterThanOrEqualToAudioNode : public FNodeFacade
	{
	public:
		FAudioGreaterThanOrEqualToAudioNode(const FNodeInitData& InitData)
			: FNodeFacade(InitData.InstanceName, InitData.InstanceID, TFacadeOperatorClass<FAudioGreaterThanOrEqualToAudioOperator>())
		{}
	};

	METASOUND_REGISTER_NODE(FAudioGreaterThanOrEqualToAudioNode);
}

#undef LOCTEXT_NAMESPACE