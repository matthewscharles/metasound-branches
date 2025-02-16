// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundTriggerOnNextFrameNode.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundPrimitives.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_TriggerOnNextFrameNode"

namespace Metasound
{
    namespace TriggerOnNextFrameNodeNames
    {
        METASOUND_PARAM(InputTrigger, "In", "Trigger input to delay.");
        METASOUND_PARAM(OutputOnTrigger, "Out", "Delay a trigger by a single frame.");
    }

    class FTriggerOnNextFrameOperator : public TExecutableOperator<FTriggerOnNextFrameOperator>
    {
    public:
        FTriggerOnNextFrameOperator(const FOperatorSettings& InSettings, const FTriggerReadRef& InTrigger)
            : InputTrigger(InTrigger)
            , OnTrigger(FTriggerWriteRef::CreateNew(InSettings))
            , OperatorSettings(InSettings)
        {
        }

        static const FVertexInterface& DeclareVertexInterface()
        {
            using namespace TriggerOnNextFrameNodeNames;
            static const FVertexInterface Interface(
                FInputVertexInterface(
                    TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTrigger))
                ),
                FOutputVertexInterface(
                    TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputOnTrigger))
                )
            );
            return Interface;
        }

        static const FNodeClassMetadata& GetNodeInfo()
        {
            auto CreateNodeClassMetadata = []() -> FNodeClassMetadata
            {
                FVertexInterface NodeInterface = DeclareVertexInterface();
                FNodeClassMetadata Metadata;
                Metadata.ClassName = { TEXT("UE"), TEXT("Trigger On Next Frame"), TEXT("Trigger") };
                Metadata.MajorVersion = 1;
                Metadata.MinorVersion = 0;
                Metadata.DisplayName = METASOUND_LOCTEXT("TriggerOnNextFrameNodeDisplayName", "Trigger On Next Frame");
                Metadata.Description = METASOUND_LOCTEXT("TriggerOnNextFrameNodeDesc", "Delays a trigger by a single audio frame, carrying to the next block if needed.");
                Metadata.Author = TEXT("Charles Matthews");
                Metadata.PromptIfMissing = PluginNodeMissingPrompt;
                Metadata.DefaultInterface = NodeInterface;
                Metadata.CategoryHierarchy = { METASOUND_LOCTEXT("Category", "Branches") };
                return Metadata;
            };
            static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
            return Metadata;
        }

        virtual FDataReferenceCollection GetInputs() const override
        {
            using namespace TriggerOnNextFrameNodeNames;
            FDataReferenceCollection Inputs;
            Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputTrigger), InputTrigger);
            return Inputs;
        }

        virtual FDataReferenceCollection GetOutputs() const override
        {
            using namespace TriggerOnNextFrameNodeNames;
            FDataReferenceCollection Outputs;
            Outputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutputOnTrigger), OnTrigger);
            return Outputs;
        }

        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
        {
            using namespace TriggerOnNextFrameNodeNames;
            const FInputVertexInterfaceData& InputData = InParams.InputData;
            TDataReadReference<FTrigger> InTrigger = InputData.GetOrCreateDefaultDataReadReference<FTrigger>(
                METASOUND_GET_PARAM_NAME(InputTrigger),
                InParams.OperatorSettings
            );
            return MakeUnique<FTriggerOnNextFrameOperator>(InParams.OperatorSettings, InTrigger);
        }

        void Execute()
        {
            OnTrigger->AdvanceBlock();
            const int32 NumFrames = OperatorSettings.GetNumFramesPerBlock();
            TArray<int32> NextCarry;

            for (int32& PendingFrame : CarryOver)
            {
                if (PendingFrame < NumFrames)
                {
                    OnTrigger->TriggerFrame(PendingFrame);
                }
                else
                {
                    NextCarry.Add(PendingFrame - NumFrames);
                }
            }

            CarryOver = NextCarry;

            InputTrigger->ExecuteBlock(
                [](int32, int32){},
                [&](int32 TriggerFrame, int32)
                {
                    const int32 Scheduled = TriggerFrame + 1;
                    if (Scheduled < NumFrames)
                    {
                        OnTrigger->TriggerFrame(Scheduled);
                    }
                    else
                    {
                        CarryOver.Add(Scheduled - NumFrames);
                    }
                }
            );
        }

    private:
        FTriggerReadRef InputTrigger;
        FTriggerWriteRef OnTrigger;
        FOperatorSettings OperatorSettings;
        TArray<int32> CarryOver;
    };

    class FTriggerOnNextFrameNode : public FNodeFacade
    {
    public:
        FTriggerOnNextFrameNode(const FNodeInitData& InitData)
            : FNodeFacade(
                InitData.InstanceName,
                InitData.InstanceID,
                TFacadeOperatorClass<FTriggerOnNextFrameOperator>()
            )
        {
        }
    };

    METASOUND_REGISTER_NODE(FTriggerOnNextFrameNode);
}

#undef LOCTEXT_NAMESPACE