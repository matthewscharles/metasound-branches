// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundDeltaTimeNode.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundPrimitives.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"
#include "MetasoundBranches/Public/MetasoundCommonMacros.h"

#define LOCTEXT_NAMESPACE "MetasoundDeltaTimeNode"

namespace Metasound::MetasoundBranches
{
    namespace DeltaTimeNodeVertexNames
    {
        METASOUND_PARAM(InputTrigger, "Trigger", "Trigger input to calculate delta time.");
        METASOUND_PARAM(InputReset, "Reset", "Trigger input to reset the stored time without outputting a delta.");
        METASOUND_PARAM(OutputOnTrigger, "On Trigger", "Trigger output when the delta time is calculated.");
        METASOUND_PARAM(OutputOnReset, "On Reset", "Trigger output when the time is reset.");
        METASOUND_PARAM(OutputDelta, "Delta", "Time difference between consecutive triggers in seconds.");
    }

    class FDeltaTimeOperator : public TExecutableOperator<FDeltaTimeOperator>
    {
    public:
        FDeltaTimeOperator(const FOperatorSettings& InSettings, const FTriggerReadRef& InTrigger, const FTriggerReadRef& InReset)
            : InputTrigger(InTrigger)
            , InputReset(InReset)
            , OutputOnTrigger(FTriggerWriteRef::CreateNew(InSettings))
            , OutputOnReset(FTriggerWriteRef::CreateNew(InSettings))
            , OutputDelta(FTimeWriteRef::CreateNew(FTime(0.0)))
            , LastGlobalFrameIndex(-1)
            , CurrentBlockStartFrame(0)
            , BlockSize(InSettings.GetNumFramesPerBlock())
            , SampleRate(InSettings.GetSampleRate())
        {
        }

        static const FVertexInterface& DeclareVertexInterface()
        {
            using namespace DeltaTimeNodeVertexNames;
            static const FVertexInterface Interface(
                FInputVertexInterface(
                    TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTrigger)),
                    TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputReset))
                ),
                FOutputVertexInterface(
                    TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputOnTrigger)),
                    TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputOnReset)),
                    TOutputDataVertex<FTime>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputDelta))
                )
            );
            return Interface;
        }

        static const FNodeClassMetadata& GetNodeInfo()
        {
            auto CreateNodeClassMetadata = []() -> FNodeClassMetadata
            {
                FNodeClassMetadata Metadata;
                Metadata.ClassName = { TEXT("UE"), TEXT("Delta Time"), TEXT("Time") };
                Metadata.MajorVersion = 1;
                Metadata.MinorVersion = 0;
                Metadata.DisplayName = METASOUND_LOCTEXT("DeltaTimeDisplayName", "Delta Time");
                Metadata.Description = METASOUND_LOCTEXT("DeltaTimeDesc", "Outputs the elapsed time between consecutive triggers.");
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
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(DeltaTimeNodeVertexNames::InputTrigger), InputTrigger);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(DeltaTimeNodeVertexNames::InputReset), InputReset);
        }

        virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
        {
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(DeltaTimeNodeVertexNames::OutputOnTrigger), OutputOnTrigger);
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(DeltaTimeNodeVertexNames::OutputOnReset), OutputOnReset);
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(DeltaTimeNodeVertexNames::OutputDelta), OutputDelta);
        }
        
        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
        {
            const FInputVertexInterfaceData& InputData = InParams.InputData;
            TDataReadReference<FTrigger> InputTrigger = InputData.GetOrCreateDefaultDataReadReference<FTrigger>(
                METASOUND_GET_PARAM_NAME(DeltaTimeNodeVertexNames::InputTrigger), InParams.OperatorSettings);
            TDataReadReference<FTrigger> InputReset = InputData.GetOrCreateDefaultDataReadReference<FTrigger>(
                METASOUND_GET_PARAM_NAME(DeltaTimeNodeVertexNames::InputReset), InParams.OperatorSettings);
            return MakeUnique<FDeltaTimeOperator>(InParams.OperatorSettings, InputTrigger, InputReset);
        }

        virtual void Execute()
        {
            OutputOnTrigger->AdvanceBlock();
            OutputOnReset->AdvanceBlock();

            InputReset->ExecuteBlock(
                [](int32 StartFrame, int32 EndFrame) {},
                [&](int32 TriggerFrame, int32 TriggerFrameEnd)
                {
                    int32 GlobalResetIndex = CurrentBlockStartFrame + TriggerFrame;
                    LastGlobalFrameIndex = GlobalResetIndex;
                    OutputOnReset->TriggerFrame(TriggerFrame);
                }
            );

            InputTrigger->ExecuteBlock(
                [](int32 StartFrame, int32 EndFrame) {},
                [&](int32 TriggerFrame, int32 TriggerFrameEnd)
                {
                    int32 GlobalTriggerIndex = CurrentBlockStartFrame + TriggerFrame;
                    if (LastGlobalFrameIndex >= 0)
                    {
                        int32 FrameDelta = GlobalTriggerIndex - LastGlobalFrameIndex;
                        double TimeDelta = static_cast<double>(FrameDelta) / SampleRate;
                        *OutputDelta = FTime(TimeDelta);
                    }
                    LastGlobalFrameIndex = GlobalTriggerIndex;
                    OutputOnTrigger->TriggerFrame(TriggerFrame);
                }
            );

            CurrentBlockStartFrame += BlockSize;
        }

    private:
        FTriggerReadRef InputTrigger;
        FTriggerReadRef InputReset;
        
        FTriggerWriteRef OutputOnTrigger;
        FTriggerWriteRef OutputOnReset;
        FTimeWriteRef OutputDelta;
        
        int32 LastGlobalFrameIndex;
        int32 CurrentBlockStartFrame;
        int32 BlockSize;
        float SampleRate;
    };

    class FDeltaTimeNode : public FNodeFacade
    {
    public:
        FDeltaTimeNode(const FNodeInitData& InitData)
            : FNodeFacade(InitData.InstanceName, InitData.InstanceID, TFacadeOperatorClass<FDeltaTimeOperator>())
        {
        }
    };

    METASOUND_REGISTER_NODE(FDeltaTimeNode);
}

#undef LOCTEXT_NAMESPACE

