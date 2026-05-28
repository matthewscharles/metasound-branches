// Copyright 2026 Charles Matthews. All Rights Reserved.

#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundParamHelper.h"
#include "MetasoundPrimitives.h"
#include "MetasoundTrigger.h"
#include "MetasoundBranches/Public/MetasoundCommonMacros.h"

#define LOCTEXT_NAMESPACE "MetasoundBranches_TriggerOnLoadNode"

namespace Metasound
{
	namespace TriggerOnLoadVertexNames
	{
		METASOUND_PARAM(InputTriggerFrame, "Trigger Frame", "Frame index within the first block to fire the trigger (0-based). Clamped to block size.")
		METASOUND_PARAM(OutputOnLoad, "On Load", "Fires once at the configured frame in the first audio block.")
	}

	class FTriggerOnLoadOperator : public TExecutableOperator<FTriggerOnLoadOperator>
	{
	public:
		FTriggerOnLoadOperator(const FOperatorSettings& InSettings, const TDataReadReference<int32>& InTriggerFrame)
			: OnLoad(FTriggerWriteRef::CreateNew(InSettings))
			, TriggerFrameInput(InTriggerFrame)
			, BlockSize(InSettings.GetNumFramesPerBlock())
			, bFired(false)
		{
		}

		static const FVertexInterface& DeclareVertexInterface()
		{
			using namespace TriggerOnLoadVertexNames;
			static const FVertexInterface Interface(
				FInputVertexInterface(
					TInputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTriggerFrame), 0)
				),
				FOutputVertexInterface(
					TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputOnLoad))
				)
			);
			return Interface;
		}

		static const FNodeClassMetadata& GetNodeInfo()
		{
			auto CreateMetadata = []() -> FNodeClassMetadata
			{
				FNodeClassMetadata Metadata;
				Metadata.ClassName = { TEXT("UE"), TEXT("TriggerOnLoad"), TEXT("") };
				Metadata.MajorVersion = 1;
				Metadata.MinorVersion = 0;
				Metadata.DisplayName = LOCTEXT("DisplayName", "Trigger On Load");
				Metadata.Description = LOCTEXT("Description", "Fires a trigger exactly once at a configurable frame within the first audio block.");
				Metadata.Author = TEXT("Charles Matthews");
				Metadata.PromptIfMissing = PluginNodeMissingPrompt;
				Metadata.DefaultInterface = DeclareVertexInterface();
				Metadata.CategoryHierarchy = {
					METASOUND_LOCTEXT("Custom", "Branches"),
					METASOUND_LOCTEXT("CustomSub", "Triggers")
				};
				METASOUND_BRANCHES_APPLY_NODE_STYLE(Metadata);
				return Metadata;
			};
			static const FNodeClassMetadata Metadata = CreateMetadata();
			return Metadata;
		}

		METASOUND_DISABLE_LEGACY_IO()

		virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
		{
			using namespace TriggerOnLoadVertexNames;
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputTriggerFrame), TriggerFrameInput);
		}

		virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
		{
			using namespace TriggerOnLoadVertexNames;
			InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputOnLoad), OnLoad);
		}

		static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
		{
			using namespace TriggerOnLoadVertexNames;
			auto InTriggerFrame = InParams.InputData.GetOrCreateDefaultDataReadReference<int32>(METASOUND_GET_PARAM_NAME(InputTriggerFrame), InParams.OperatorSettings);
			return MakeUnique<FTriggerOnLoadOperator>(InParams.OperatorSettings, InTriggerFrame);
		}

		void Execute()
		{
			OnLoad->AdvanceBlock();
			if (!bFired)
			{
				const int32 Frame = FMath::Clamp(*TriggerFrameInput, 0, BlockSize - 1);
				OnLoad->TriggerFrame(Frame);
				bFired = true;
			}
		}

	private:
		FTriggerWriteRef OnLoad;
		TDataReadReference<int32> TriggerFrameInput;
		int32 BlockSize;
		bool bFired;
	};

	using FTriggerOnLoadNode = TNodeFacade<FTriggerOnLoadOperator>;
	METASOUND_REGISTER_NODE(FTriggerOnLoadNode);
}

#undef LOCTEXT_NAMESPACE
