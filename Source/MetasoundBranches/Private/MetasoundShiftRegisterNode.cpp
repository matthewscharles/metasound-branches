// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundShiftRegisterNode.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundPrimitives.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"
#include "MetasoundBranches/Public/MetasoundCommonMacros.h"

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_ShiftRegister"

namespace Metasound
{
    namespace ShiftRegisterNodeVertexNames
    {
        METASOUND_PARAM(InputSignal, "In", "Input float to the shift register.")
        METASOUND_PARAM(InputTrigger, "Trigger", "Trigger.")
        METASOUND_PARAM(InputReset, "Reset", "Reset trigger for the shift register.")
        METASOUND_PARAM(InputDefault, "Default", "Default float for the shift register.")
        METASOUND_PARAM(InputReverse, "Reverse", "Reverse flag for the shift register.")
        METASOUND_PARAM(InputOutputOnReset, "Output On Reset", "When true, a reset will immediately output default values.")
        METASOUND_PARAM(OutputTrigger, "On Trigger", "Output trigger following the shift.")
        METASOUND_PARAM(OutputReset, "On Reset", "Output trigger when Output On Reset is true.")
        METASOUND_PARAM(OutputDirectionChange, "On Direction Change", "Output trigger on direction change.")
        METASOUND_PARAM(OutputSignal1, "Stage 1", "Shifted output at stage 1.")
        METASOUND_PARAM(OutputSignal2, "Stage 2", "Shifted output at stage 2.")
        METASOUND_PARAM(OutputSignal3, "Stage 3", "Shifted output at stage 3.")
        METASOUND_PARAM(OutputSignal4, "Stage 4", "Shifted output at stage 4.")
        METASOUND_PARAM(OutputSignal5, "Stage 5", "Shifted output at stage 5.")
        METASOUND_PARAM(OutputSignal6, "Stage 6", "Shifted output at stage 6.")
        METASOUND_PARAM(OutputSignal7, "Stage 7", "Shifted output at stage 7.")
        METASOUND_PARAM(OutputSignal8, "Stage 8", "Shifted output at stage 8.")
    }

    class FShiftRegisterOperator : public TExecutableOperator<FShiftRegisterOperator>
    {
    public:
        FShiftRegisterOperator(
            const FOperatorSettings& InSettings,
            const FFloatReadRef& InInputSignal,
            const FTriggerReadRef& InInputTrigger,
            const FTriggerReadRef& InInputReset,
            const FFloatReadRef& InInputDefault,
            const TDataReadReference<bool>& InInputReverse,
            const TDataReadReference<bool>& InInputOutputOnReset)
            : InputSignal(InInputSignal)
            , InputTrigger(InInputTrigger)
            , InputReset(InInputReset)
            , InputDefault(InInputDefault)
            , InputReverse(InInputReverse)
            , InputOutputOnReset(InInputOutputOnReset)
            , OutputTrigger(FTriggerWriteRef::CreateNew(InSettings))
            , OutputResetTrigger(FTriggerWriteRef::CreateNew(InSettings))
            , OutputDirectionChangeTrigger(FTriggerWriteRef::CreateNew(InSettings))
            , OutputSignal1(FFloatWriteRef::CreateNew(0.0f))
            , OutputSignal2(FFloatWriteRef::CreateNew(0.0f))
            , OutputSignal3(FFloatWriteRef::CreateNew(0.0f))
            , OutputSignal4(FFloatWriteRef::CreateNew(0.0f))
            , OutputSignal5(FFloatWriteRef::CreateNew(0.0f))
            , OutputSignal6(FFloatWriteRef::CreateNew(0.0f))
            , OutputSignal7(FFloatWriteRef::CreateNew(0.0f))
            , OutputSignal8(FFloatWriteRef::CreateNew(0.0f))
            , ShiftedValue1(0.0f)
            , ShiftedValue2(0.0f)
            , ShiftedValue3(0.0f)
            , ShiftedValue4(0.0f)
            , ShiftedValue5(0.0f)
            , ShiftedValue6(0.0f)
            , ShiftedValue7(0.0f)
            , ShiftedValue8(0.0f)
            , bReverseState(false)
            , bResetPending(false)
        {
        }

        static const FVertexInterface& DeclareVertexInterface()
        {
            using namespace ShiftRegisterNodeVertexNames;
            static const FVertexInterface Interface(
                FInputVertexInterface(
                    TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputSignal)),
                    TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTrigger)),
                    TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputReset)),
                    TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputDefault)),
                    TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputReverse)),
                    TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputOutputOnReset))
                ),
                FOutputVertexInterface(
                    TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputTrigger)),
                    TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputReset)),
                    TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputDirectionChange)),
                    TOutputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputSignal1)),
                    TOutputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputSignal2)),
                    TOutputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputSignal3)),
                    TOutputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputSignal4)),
                    TOutputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputSignal5)),
                    TOutputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputSignal6)),
                    TOutputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputSignal7)),
                    TOutputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputSignal8))
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
                Metadata.ClassName = { TEXT("UE"), TEXT("Shift Register"), TEXT("Float") };
                Metadata.MajorVersion = 1;
                Metadata.MinorVersion = 1;
                Metadata.DisplayName = METASOUND_LOCTEXT("ShiftRegisterNodeDisplayName", "Shift Register");
                Metadata.Description = METASOUND_LOCTEXT("ShiftRegisterNodeDesc", "Shift register node with eight stages and additional reset/output options.");
                Metadata.Author = "Charles Matthews";
                Metadata.PromptIfMissing = PluginNodeMissingPrompt;
                Metadata.DefaultInterface = NodeInterface;
                Metadata.CategoryHierarchy = {
                    METASOUND_LOCTEXT("Custom", "Branches"),
                    METASOUND_LOCTEXT("CustomSub", "Modulation")
                };
                Metadata.Keywords = TArray<FText>();
                return Metadata;
            };
            static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
            return Metadata;
        }

        METASOUND_DISABLE_LEGACY_IO()

        virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
        {
            using namespace ShiftRegisterNodeVertexNames;
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputSignal), InputSignal);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputTrigger), InputTrigger);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputReset), InputReset);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputDefault), InputDefault);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputReverse), InputReverse);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputOutputOnReset), InputOutputOnReset);
        }

        virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
        {
            using namespace ShiftRegisterNodeVertexNames;
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputTrigger), OutputTrigger);
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputReset), OutputResetTrigger);
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputDirectionChange), OutputDirectionChangeTrigger);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutputSignal1), OutputSignal1);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutputSignal2), OutputSignal2);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutputSignal3), OutputSignal3);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutputSignal4), OutputSignal4);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutputSignal5), OutputSignal5);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutputSignal6), OutputSignal6);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutputSignal7), OutputSignal7);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutputSignal8), OutputSignal8);
        }

        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
        {
            using namespace ShiftRegisterNodeVertexNames;
            const FInputVertexInterfaceData& InputData = InParams.InputData;
            TDataReadReference<float> InputSignal = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputSignal), InParams.OperatorSettings);
            TDataReadReference<FTrigger> InputTrigger = InputData.GetOrCreateDefaultDataReadReference<FTrigger>(METASOUND_GET_PARAM_NAME(InputTrigger), InParams.OperatorSettings);
            TDataReadReference<FTrigger> InputReset = InputData.GetOrCreateDefaultDataReadReference<FTrigger>(METASOUND_GET_PARAM_NAME(InputReset), InParams.OperatorSettings);
            TDataReadReference<float> InputDefault = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputDefault), InParams.OperatorSettings);
            TDataReadReference<bool> InputReverse = InputData.GetOrCreateDefaultDataReadReference<bool>(METASOUND_GET_PARAM_NAME(InputReverse), InParams.OperatorSettings);
            TDataReadReference<bool> InputOutputOnReset = InputData.GetOrCreateDefaultDataReadReference<bool>(METASOUND_GET_PARAM_NAME(InputOutputOnReset), InParams.OperatorSettings);
            return MakeUnique<FShiftRegisterOperator>(
                InParams.OperatorSettings,
                InputSignal,
                InputTrigger,
                InputReset,
                InputDefault,
                InputReverse,
                InputOutputOnReset
            );
        }

        void Execute()
        {
            OutputTrigger->AdvanceBlock();
            OutputResetTrigger->AdvanceBlock();
            OutputDirectionChangeTrigger->AdvanceBlock();
        
            bool bResetOccurred = false;
            bool bTriggerOccurred = false;
            int32 ResetFrame = INDEX_NONE;
        
            InputReset->ExecuteBlock(
                [](int32, int32) {},
                [&](int32 StartFrame, int32 EndFrame)
                {
                    bResetOccurred = true;
                    ResetFrame = StartFrame;
                    ShiftedValue1 = *InputDefault;
                    ShiftedValue2 = *InputDefault;
                    ShiftedValue3 = *InputDefault;
                    ShiftedValue4 = *InputDefault;
                    ShiftedValue5 = *InputDefault;
                    ShiftedValue6 = *InputDefault;
                    ShiftedValue7 = *InputDefault;
                    ShiftedValue8 = *InputDefault;
        
                    if (*InputOutputOnReset)
                    {
                        OutputResetTrigger->TriggerFrame(StartFrame);
                    }
                    else
                    {
                        bResetPending = true;
                    }
                }
            );
        
            InputTrigger->ExecuteBlock(
                [](int32, int32) {},
                [&](int32 StartFrame, int32 EndFrame)
                {
                    bTriggerOccurred = true;
                    if (bResetPending)
                    {
                        OutputResetTrigger->TriggerFrame(StartFrame);
                        bResetPending = false;
                    }
        
                    if (*InputReverse)
                    {
                        ShiftedValue1 = ShiftedValue2;
                        ShiftedValue2 = ShiftedValue3;
                        ShiftedValue3 = ShiftedValue4;
                        ShiftedValue4 = ShiftedValue5;
                        ShiftedValue5 = ShiftedValue6;
                        ShiftedValue6 = ShiftedValue7;
                        ShiftedValue7 = ShiftedValue8;
                        ShiftedValue8 = *InputSignal;
                    }
                    else
                    {
                        ShiftedValue8 = ShiftedValue7;
                        ShiftedValue7 = ShiftedValue6;
                        ShiftedValue6 = ShiftedValue5;
                        ShiftedValue5 = ShiftedValue4;
                        ShiftedValue4 = ShiftedValue3;
                        ShiftedValue3 = ShiftedValue2;
                        ShiftedValue2 = ShiftedValue1;
                        ShiftedValue1 = *InputSignal;
                    }
                    OutputTrigger->TriggerFrame(StartFrame);
                }
            );
        
            bool CurrentReverse = *InputReverse;
            if (CurrentReverse != bReverseState)
            {
                OutputDirectionChangeTrigger->TriggerFrame(0);
                bReverseState = CurrentReverse;
            }
        
            if (bTriggerOccurred || (bResetOccurred && *InputOutputOnReset))
            {
                *OutputSignal1 = ShiftedValue1;
                *OutputSignal2 = ShiftedValue2;
                *OutputSignal3 = ShiftedValue3;
                *OutputSignal4 = ShiftedValue4;
                *OutputSignal5 = ShiftedValue5;
                *OutputSignal6 = ShiftedValue6;
                *OutputSignal7 = ShiftedValue7;
                *OutputSignal8 = ShiftedValue8;
            }
        }

    private:
        FFloatReadRef InputSignal;
        FTriggerReadRef InputTrigger;
        FTriggerReadRef InputReset;
        FFloatReadRef InputDefault;
        TDataReadReference<bool> InputReverse;
        TDataReadReference<bool> InputOutputOnReset;
        FTriggerWriteRef OutputTrigger;
        FTriggerWriteRef OutputResetTrigger;
        FTriggerWriteRef OutputDirectionChangeTrigger;
        FFloatWriteRef OutputSignal1;
        FFloatWriteRef OutputSignal2;
        FFloatWriteRef OutputSignal3;
        FFloatWriteRef OutputSignal4;
        FFloatWriteRef OutputSignal5;
        FFloatWriteRef OutputSignal6;
        FFloatWriteRef OutputSignal7;
        FFloatWriteRef OutputSignal8;
        float ShiftedValue1;
        float ShiftedValue2;
        float ShiftedValue3;
        float ShiftedValue4;
        float ShiftedValue5;
        float ShiftedValue6;
        float ShiftedValue7;
        float ShiftedValue8;
        bool bReverseState;
        bool bResetPending;
    };

    class FShiftRegisterNode : public FNodeFacade
    {
    public:
        FShiftRegisterNode(const FNodeInitData& InitData)
            : FNodeFacade(InitData.InstanceName, InitData.InstanceID, TFacadeOperatorClass<FShiftRegisterOperator>())
        {
        }
    };

    METASOUND_REGISTER_NODE(FShiftRegisterNode);
}

#undef LOCTEXT_NAMESPACE