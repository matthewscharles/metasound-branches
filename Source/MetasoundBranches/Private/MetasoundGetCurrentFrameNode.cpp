// Copyright 2026 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundGetCurrentFrameNode.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundPrimitives.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"
#include "MetasoundBranches/Public/MetasoundCommonMacros.h"

#define LOCTEXT_NAMESPACE "MetasoundGetCurrentFrameNode"

namespace Metasound
{
    namespace GetCurrentFrameNodeVertexNames
    {
        METASOUND_PARAM(InputTrigger, "Get Frame", "Trigger to retrieve the current audio frame.");
        METASOUND_PARAM(OutputFrame, "Current Frame", "Outputs the current audio frame index.");
    }

    class FGetCurrentFrameOperator : public TExecutableOperator<FGetCurrentFrameOperator>
    {
    public:
        FGetCurrentFrameOperator(const FOperatorSettings& InSettings, const FTriggerReadRef& InTrigger)
            : InputTrigger(InTrigger)
            , OutputFrame(FInt32WriteRef::CreateNew(0))
        {
        }

        static const FVertexInterface& DeclareVertexInterface()
        {
            using namespace GetCurrentFrameNodeVertexNames;

            static const FVertexInterface Interface(
                FInputVertexInterface(
                    TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTrigger))
                ),
                FOutputVertexInterface(
                    TOutputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputFrame))
                )
            );
            return Interface;
        }

        static const FNodeClassMetadata& GetNodeInfo()
        {
            auto CreateNodeClassMetadata = []() -> FNodeClassMetadata
            {
                FNodeClassMetadata Metadata;
                Metadata.ClassName = { TEXT("UE"), TEXT("Get Current Frame"), TEXT("Utility") };
                Metadata.MajorVersion = 1;
                Metadata.MinorVersion = 0;
                Metadata.DisplayName = METASOUND_LOCTEXT("GetCurrentFrameDisplayName", "Get Current Frame");
                Metadata.Description = METASOUND_LOCTEXT("GetCurrentFrameDesc", "Outputs the current audio frame index on trigger.");
                Metadata.Author = "Charles Matthews";
                Metadata.PromptIfMissing = PluginNodeMissingPrompt;
                Metadata.DefaultInterface = DeclareVertexInterface();
                Metadata.CategoryHierarchy = {
                    METASOUND_LOCTEXT("Custom", "Branches"),
                    METASOUND_LOCTEXT("CustomSub", "Time")
                };
                return Metadata;
            };
            static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
            return Metadata;
        }
        
        METASOUND_DISABLE_LEGACY_IO()
        
        virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
        {
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(GetCurrentFrameNodeVertexNames::InputTrigger), InputTrigger);
        }
        
        virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
        {
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(GetCurrentFrameNodeVertexNames::OutputFrame), OutputFrame);
        }

        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
        {
            const FInputVertexInterfaceData& InputData = InParams.InputData;
            TDataReadReference<FTrigger> InputTrigger = InputData.GetOrCreateDefaultDataReadReference<FTrigger>(
                METASOUND_GET_PARAM_NAME(GetCurrentFrameNodeVertexNames::InputTrigger), InParams.OperatorSettings);

            return MakeUnique<FGetCurrentFrameOperator>(InParams.OperatorSettings, InputTrigger);
        }

        virtual void Execute()
        {
            InputTrigger->ExecuteBlock(
                [](int32 StartFrame, int32 EndFrame) {},
                [&](int32 TriggerFrame, int32 TriggerFrameEnd)
                {
                    *OutputFrame = TriggerFrame;
                }
            );
        }

    private:
        FTriggerReadRef InputTrigger;
        FInt32WriteRef OutputFrame;
    };

    class FGetCurrentFrameNode : public FNodeFacade
    {
    public:
                static FNodeClassMetadata CreateNodeClassMetadata()
        {
            return FGetCurrentFrameOperator::GetNodeInfo();
        }
        FGetCurrentFrameNode(FNodeData InitData)
            : FNodeFacade(InitData, MakeShared<const FNodeClassMetadata>(FGetCurrentFrameOperator::GetNodeInfo()), TFacadeOperatorClass<FGetCurrentFrameOperator>())
        {
        }
    };

    METASOUND_REGISTER_NODE(FGetCurrentFrameNode);
}

#undef LOCTEXT_NAMESPACE
