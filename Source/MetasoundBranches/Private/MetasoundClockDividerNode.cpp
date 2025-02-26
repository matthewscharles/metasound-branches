// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundClockDividerNode.h"
#include "MetasoundExecutableOperator.h"     // TExecutableOperator class
#include "MetasoundPrimitives.h"             // ReadRef and WriteRef descriptions for bool, int32, float, and string
#include "MetasoundNodeRegistrationMacro.h"  // METASOUND_LOCTEXT and METASOUND_REGISTER_NODE macros
#include "MetasoundFacade.h"                 // FNodeFacade class, eliminates the need for a fair amount of boilerplate code
#include "MetasoundParamHelper.h"            // METASOUND_PARAM and METASOUND_GET_PARAM family of macros

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_ClockDividerNode"

namespace Metasound
{
    namespace ClockDividerNodeVertexNames
    {
        METASOUND_PARAM(InputTrigger, "Trigger", "Input trigger to the clock divider.");
        METASOUND_PARAM(InputReset, "Reset", "Reset the clock divider.");

        METASOUND_PARAM(OutputTrigger1, "1", "Output trigger for division 1.");
        METASOUND_PARAM(OutputTrigger2, "2", "Output trigger for division 2.");
        METASOUND_PARAM(OutputTrigger3, "3", "Output trigger for division 3.");
        METASOUND_PARAM(OutputTrigger4, "4", "Output trigger for division 4.");
        METASOUND_PARAM(OutputTrigger5, "5", "Output trigger for division 5.");
        METASOUND_PARAM(OutputTrigger6, "6", "Output trigger for division 6.");
        METASOUND_PARAM(OutputTrigger7, "7", "Output trigger for division 7.");
        METASOUND_PARAM(OutputTrigger8, "8", "Output trigger for division 8.");
    }

    class FClockDividerOperator : public TExecutableOperator<FClockDividerOperator>
    {
    public:
        FClockDividerOperator(
            const FOperatorSettings& InSettings,
            const FTriggerReadRef& InInputTrigger,
            const FTriggerReadRef& InInputReset)
            : InputTrigger(InInputTrigger)
            , InputReset(InInputReset)
            , OutputTrigger1(FTriggerWriteRef::CreateNew(InSettings))
            , OutputTrigger2(FTriggerWriteRef::CreateNew(InSettings))
            , OutputTrigger3(FTriggerWriteRef::CreateNew(InSettings))
            , OutputTrigger4(FTriggerWriteRef::CreateNew(InSettings))
            , OutputTrigger5(FTriggerWriteRef::CreateNew(InSettings))
            , OutputTrigger6(FTriggerWriteRef::CreateNew(InSettings))
            , OutputTrigger7(FTriggerWriteRef::CreateNew(InSettings))
            , OutputTrigger8(FTriggerWriteRef::CreateNew(InSettings))
            , Counter(0)
        {
        }

        static const FVertexInterface& DeclareVertexInterface()
        {
            using namespace ClockDividerNodeVertexNames;

            static const FVertexInterface Interface(
                FInputVertexInterface(
                    TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTrigger)),
                    TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputReset))
                ),
                FOutputVertexInterface(
                    TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputTrigger1)),
                    TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputTrigger2)),
                    TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputTrigger3)),
                    TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputTrigger4)),
                    TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputTrigger5)),
                    TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputTrigger6)),
                    TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputTrigger7)),
                    TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputTrigger8))
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

                    Metadata.ClassName = { TEXT("UE"), TEXT("Clock Divider"), TEXT("Trigger") };
                    Metadata.MajorVersion = 1;
                    Metadata.MinorVersion = 1;
                    Metadata.DisplayName = METASOUND_LOCTEXT("ClockDividerNodeDisplayName", "Clock Divider");
                    Metadata.Description = METASOUND_LOCTEXT("ClockDividerNodeDesc", "Divides an input trigger into multiple outputs.");
                    Metadata.Author = "Charles Matthews";
                    Metadata.PromptIfMissing = PluginNodeMissingPrompt;
                    Metadata.DefaultInterface = NodeInterface;
                    Metadata.CategoryHierarchy = { METASOUND_LOCTEXT("Custom", "Branches") };
                    Metadata.Keywords = TArray<FText>();

                    return Metadata;
                };

            static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
            return Metadata;
        }

        virtual FDataReferenceCollection GetInputs() const override
        {
            using namespace ClockDividerNodeVertexNames;

            FDataReferenceCollection InputDataReferences;

            InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputTrigger), InputTrigger);
            InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputReset), InputReset);

            return InputDataReferences;
        }

        virtual FDataReferenceCollection GetOutputs() const override
        {
            using namespace ClockDividerNodeVertexNames;

            FDataReferenceCollection OutputDataReferences;

            OutputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutputTrigger1), OutputTrigger1);
            OutputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutputTrigger2), OutputTrigger2);
            OutputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutputTrigger3), OutputTrigger3);
            OutputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutputTrigger4), OutputTrigger4);
            OutputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutputTrigger5), OutputTrigger5);
            OutputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutputTrigger6), OutputTrigger6);
            OutputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutputTrigger7), OutputTrigger7);
            OutputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutputTrigger8), OutputTrigger8);

            return OutputDataReferences;
        }

        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
        {
            using namespace ClockDividerNodeVertexNames;

            const FInputVertexInterfaceData& InputData = InParams.InputData;
            const Metasound::FInputVertexInterface& InputInterface = DeclareVertexInterface().GetInputInterface();

            TDataReadReference<FTrigger> InputTrigger = InputData.GetOrCreateDefaultDataReadReference<FTrigger>(METASOUND_GET_PARAM_NAME(InputTrigger), InParams.OperatorSettings);
            TDataReadReference<FTrigger> InputReset = InputData.GetOrCreateDefaultDataReadReference<FTrigger>(METASOUND_GET_PARAM_NAME(InputReset), InParams.OperatorSettings);

            return MakeUnique<FClockDividerOperator>(
                InParams.OperatorSettings,
                InputTrigger,
                InputReset
            );
        }

        void Execute()
        {
            // Handle reset
            InputReset->ExecuteBlock(
                // Pre-trigger lambda
                [](int32 StartFrame, int32 EndFrame)
                {
                    // No action needed before triggers
                },

                // On-trigger lambda
                [&](int32 StartFrame, int32 EndFrame)
                {
                    Counter = 0;
                }
            );

            // Initialize output triggers
            OutputTrigger1->AdvanceBlock();
            OutputTrigger2->AdvanceBlock();
            OutputTrigger3->AdvanceBlock();
            OutputTrigger4->AdvanceBlock();
            OutputTrigger5->AdvanceBlock();
            OutputTrigger6->AdvanceBlock();
            OutputTrigger7->AdvanceBlock();
            OutputTrigger8->AdvanceBlock();

            // Handle input trigger
            InputTrigger->ExecuteBlock(
                // Pre-trigger lambda
                [](int32 StartFrame, int32 EndFrame)
                {
                    // No action needed before triggers
                },

                // On-trigger lambda
                [&](int32 StartFrame, int32 EndFrame)
                {
                    Counter = (Counter + 1) % 8;

                    switch (Counter)
                    {
                        case 0:
                            OutputTrigger1->TriggerFrame(StartFrame);  
                            break;

                        case 1:
                            OutputTrigger1->TriggerFrame(StartFrame);  
                            OutputTrigger2->TriggerFrame(StartFrame);
                            break;

                        case 2:
                            OutputTrigger1->TriggerFrame(StartFrame); 
                            OutputTrigger3->TriggerFrame(StartFrame);
                            break;

                        case 3:
                            OutputTrigger1->TriggerFrame(StartFrame);  
                            OutputTrigger2->TriggerFrame(StartFrame);
                            OutputTrigger4->TriggerFrame(StartFrame);
                            break;

                        case 4:
                            OutputTrigger1->TriggerFrame(StartFrame);  
                            OutputTrigger5->TriggerFrame(StartFrame);
                            break;

                        case 5:
                            OutputTrigger1->TriggerFrame(StartFrame);  
                            OutputTrigger2->TriggerFrame(StartFrame);
                            OutputTrigger3->TriggerFrame(StartFrame);
                            OutputTrigger6->TriggerFrame(StartFrame);
                            break;

                        case 6:
                            OutputTrigger1->TriggerFrame(StartFrame);
                            OutputTrigger7->TriggerFrame(StartFrame);
                            break;

                        case 7:
                            OutputTrigger1->TriggerFrame(StartFrame);
                            OutputTrigger2->TriggerFrame(StartFrame);
                            OutputTrigger4->TriggerFrame(StartFrame);
                            OutputTrigger8->TriggerFrame(StartFrame);
                            break;

                        default:
                            break;
                    }
                }
            );
        }

    private:
        FTriggerReadRef InputTrigger;
        FTriggerReadRef InputReset;

        FTriggerWriteRef OutputTrigger1;
        FTriggerWriteRef OutputTrigger2;
        FTriggerWriteRef OutputTrigger3;
        FTriggerWriteRef OutputTrigger4;
        FTriggerWriteRef OutputTrigger5;
        FTriggerWriteRef OutputTrigger6;
        FTriggerWriteRef OutputTrigger7;
        FTriggerWriteRef OutputTrigger8;

        int32 Counter;
    };

    class FClockDividerNode : public FNodeFacade
    {
    public:
        FClockDividerNode(const FNodeInitData& InitData)
            : FNodeFacade(InitData.InstanceName, InitData.InstanceID, TFacadeOperatorClass<FClockDividerOperator>())
        {
        }
    };

    METASOUND_REGISTER_NODE(FClockDividerNode);
}

#undef LOCTEXT_NAMESPACE