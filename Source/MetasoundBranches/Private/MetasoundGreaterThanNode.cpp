// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundGreaterThanNode.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"
#include "MetasoundPrimitives.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundStandardNodesCategories.h"
#include "MetasoundBranches/Public/MetasoundCommonMacros.h"

#define LOCTEXT_NAMESPACE "MetasoundGreaterThanNode"

namespace Metasound
{
	namespace GreaterThanNodeVertexNames
	{
		METASOUND_PARAM(InputSignal,  "In",        "Audio input.");
		METASOUND_PARAM(Threshold,    "Threshold", "Float threshold to compare input against.");
		METASOUND_PARAM(OutputSignal, "Out",       "Output signal: 1.0 if input > threshold, else 0.0.");
	}

	class FGreaterThanOperator : public TExecutableOperator<FGreaterThanOperator>
	{
	public:
		FGreaterThanOperator(
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
			using namespace GreaterThanNodeVertexNames;

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
				Metadata.ClassName = { TEXT("UE"), TEXT("GreaterThan"), TEXT("Audio") };
				Metadata.MajorVersion = 1;
				Metadata.MinorVersion = 0;
				Metadata.DisplayName = LOCTEXT("GreaterThanNodeDisplayName", "Greater Than");
				Metadata.Description = LOCTEXT("GreaterThanNodeDesc", "Outputs 1.0 where input > threshold, else 0.0.");
				Metadata.Author = TEXT("Charles Matthews");
				Metadata.PromptIfMissing = PluginNodeMissingPrompt;
				Metadata.DefaultInterface = DeclareVertexInterface();
				Metadata.CategoryHierarchy = {
					METASOUND_LOCTEXT("Custom", "Branches"),
					METASOUND_LOCTEXT("CustomSub", "Math")
				};

				FNodeDisplayStyle DisplayStyle;
				DisplayStyle.ImageName = TEXT("MetasoundEditor.Graph.Node.Custom.GreaterThan");
				// DisplayStyle.ImageName = TEXT("MetasoundEditor.Graph.Node.Math.Add");
				DisplayStyle.bShowName = false;
				DisplayStyle.bShowInputNames = false;
				DisplayStyle.bShowOutputNames = false;
				Metadata.DisplayStyle = DisplayStyle;
				
				//* troubleshooting
				
			// 	if (const ISlateStyle* S = FSlateStyleRegistry::FindSlateStyle("MetasoundBranchesStyle"))
			// 	{
			// 		UE_LOG(LogTemp, Log, TEXT("From node: Found style set: %s"), *S->GetStyleSetName().ToString());

			// 		const FSlateBrush* Brush = S->GetBrush("MetasoundEditor.Graph.Node.Custom.GreaterThan");
			// 		if (Brush)
			// 		{
			// 			UE_LOG(LogTemp, Log, TEXT("From node: Found brush with resource name: %s"), *Brush->GetResourceName().ToString());
			// 		}
			// 		else
			// 		{
			// 			UE_LOG(LogTemp, Warning, TEXT("From node: Brush key not found in style set."));
			// 		}
			// 	}
			// 	else
			// 	{
			// 		UE_LOG(LogTemp, Warning, TEXT("From node: Could not find MetasoundBranchesStyle in registry."));
			// 	}

				return Metadata;
			};

			static const FNodeClassMetadata Metadata = CreateMetadata();
			return Metadata;
		}

		static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
		{
			using namespace GreaterThanNodeVertexNames;

			const FInputVertexInterfaceData& InputData = InParams.InputData;

			TDataReadReference<FAudioBuffer> InAudio =
				InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(InputSignal), InParams.OperatorSettings);

			TDataReadReference<float> InThreshold =
				InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(Threshold), InParams.OperatorSettings);

			return MakeUnique<FGreaterThanOperator>(InParams.OperatorSettings, InAudio, InThreshold);
		}

		METASOUND_DISABLE_LEGACY_IO()

		virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
		{
			using namespace GreaterThanNodeVertexNames;
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputSignal), AudioIn);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Threshold), Threshold);
		}

		virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
		{
			using namespace GreaterThanNodeVertexNames;
			InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputSignal), AudioOut);
		}

		virtual void Execute()
		{
			const float* InData = AudioIn->GetData();
			float* OutData = AudioOut->GetData();
			const float CompareThreshold = *Threshold;

			for (int32 i = 0; i < BlockSize; ++i)
			{
				OutData[i] = (InData[i] > CompareThreshold) ? 1.0f : 0.0f;
			}
		}

	private:
		FAudioBufferReadRef AudioIn;
		FFloatReadRef Threshold;
		FAudioBufferWriteRef AudioOut;
		int32 BlockSize;
	};

	class FGreaterThanNode : public FNodeFacade
	{
	public:
		FGreaterThanNode(const FNodeInitData& InitData)
			: FNodeFacade(InitData.InstanceName, InitData.InstanceID, TFacadeOperatorClass<FGreaterThanOperator>())
		{}
	};

	METASOUND_REGISTER_NODE(FGreaterThanNode);
}

#undef LOCTEXT_NAMESPACE