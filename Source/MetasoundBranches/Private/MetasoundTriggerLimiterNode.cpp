// Copyright 2026 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundTriggerLimiterNode.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundPrimitives.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"
#include "MetasoundTrigger.h"
#include "MetasoundBranches/Public/MetasoundCommonMacros.h"

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_TriggerLimiter"

namespace Metasound
{
    namespace TriggerLimiterNames
    {
        METASOUND_PARAM(InputTrigger, "In", "Input trigger to limit.");
        METASOUND_PARAM(InputDeltaTime, "Delta Time", "Minimum time between triggers in seconds.");
        METASOUND_PARAM(OutputTrigger, "Out", "Limited output trigger.");
        METASOUND_PARAM(OutputOverflow, "Overflow", "Triggers that were suppressed.");
    }

    class FTriggerLimiterOperator : public TExecutableOperator<FTriggerLimiterOperator>
    {
    public:
        FTriggerLimiterOperator(
            const FTriggerReadRef& InTrigger,
            const FTimeReadRef& InDeltaTime,
            const FOperatorSettings& InSettings)
            : InputTrigger(InTrigger)
            , InputDeltaTime(InDeltaTime)
            , OutputTrigger(FTriggerWriteRef::CreateNew(InSettings))
            , OutputOverflow(FTriggerWriteRef::CreateNew(InSettings))
            , SampleRate(InSettings.GetSampleRate())
            , LastTriggerFrame(-1000000) // Ensure initial trigger can fire
        {
        }

        static const FVertexInterface& DeclareVertexInterface()
        {
            using namespace TriggerLimiterNames;

            static const FVertexInterface Interface(
                FInputVertexInterface(
                    TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTrigger)),
                    TInputDataVertex<FTime>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputDeltaTime))
                ),
                FOutputVertexInterface(
                    TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputTrigger)),
                    TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputOverflow))
                )
            );

            return Interface;
        }

        static const FNodeClassMetadata& GetNodeInfo()
        {
            auto CreateNodeClassMetadata = []() -> FNodeClassMetadata
            {
                FNodeClassMetadata Metadata;
                Metadata.ClassName = { TEXT("UE"), TEXT("TriggerLimiter"), TEXT("Trigger") };
                Metadata.MajorVersion = 1;
                Metadata.MinorVersion = 0;
                Metadata.DisplayName = METASOUND_LOCTEXT("TriggerLimiterDisplayName", "Trigger Limiter");
                Metadata.Description = METASOUND_LOCTEXT("TriggerLimiterDesc", "Limits the rate of trigger events with a minimum delta time.");
                Metadata.Author = "Charles Matthews";
                Metadata.PromptIfMissing = PluginNodeMissingPrompt;
                Metadata.DefaultInterface = DeclareVertexInterface();
                Metadata.CategoryHierarchy = {
                    METASOUND_LOCTEXT("Category", "Branches"),
                    METASOUND_LOCTEXT("Category", "Triggers")
                };
                return Metadata;
            };

            static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
            return Metadata;
        }

        METASOUND_DISABLE_LEGACY_IO()

        void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
        {
            using namespace TriggerLimiterNames;
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputTrigger), InputTrigger);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputDeltaTime), InputDeltaTime);
        }

        void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
        {
            using namespace TriggerLimiterNames;
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputTrigger), OutputTrigger);
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputOverflow), OutputOverflow);
        }

        void Reset(const IOperator::FResetParams& InParams)
        {
            OutputTrigger->Reset();
            OutputOverflow->Reset();
            LastTriggerFrame = -1000000;
        }

        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
        {
            using namespace TriggerLimiterNames;

            const FInputVertexInterfaceData& InputData = InParams.InputData;
            TDataReadReference<FTrigger> InTrigger = InputData.GetOrCreateDefaultDataReadReference<FTrigger>(
                METASOUND_GET_PARAM_NAME(InputTrigger), InParams.OperatorSettings);
            TDataReadReference<FTime> InDeltaTime = InputData.GetOrCreateDefaultDataReadReference<FTime>(
                METASOUND_GET_PARAM_NAME(InputDeltaTime), InParams.OperatorSettings);

            return MakeUnique<FTriggerLimiterOperator>(InTrigger, InDeltaTime, InParams.OperatorSettings);
        }
        
        void Execute()
        {
            OutputTrigger->AdvanceBlock();
            OutputOverflow->AdvanceBlock();

            const int32 NumFrames = InputTrigger->Num();
            const float DeltaTimeSeconds = InputDeltaTime->GetSeconds();
            const int32 DeltaSamples = FMath::Max(1, static_cast<int32>(DeltaTimeSeconds * SampleRate));

            for (int32 i = 0; i < NumFrames; ++i)
            {
                if ((*InputTrigger)[i])
                {
                    int32 CurrentFrame = i + BlockIndex;
                    if ((CurrentFrame - LastTriggerFrame) >= DeltaSamples)
                    {
                        OutputTrigger->TriggerFrame(i);
                        LastTriggerFrame = CurrentFrame;
                    }
                    else
                    {
                        OutputOverflow->TriggerFrame(i);
                    }
                }
            }

            BlockIndex += NumFrames;
        }

    private:
        // Inputs
        FTriggerReadRef InputTrigger;
        FTimeReadRef InputDeltaTime;

        // Outputs
        FTriggerWriteRef OutputTrigger;
        FTriggerWriteRef OutputOverflow;

        // Internal state
        float SampleRate;
        int32 LastTriggerFrame;
        int32 BlockIndex = 0;
    };

    class FTriggerLimiterNode : public FNodeFacade
    {
    public:
                static FNodeClassMetadata CreateNodeClassMetadata()
        {
            return FTriggerLimiterOperator::GetNodeInfo();
        }
        FTriggerLimiterNode(FNodeData InitData)
            : FNodeFacade(InitData, MakeShared<const FNodeClassMetadata>(FTriggerLimiterOperator::GetNodeInfo()), TFacadeOperatorClass<FTriggerLimiterOperator>())
        {
        }
    };

    METASOUND_REGISTER_NODE(FTriggerLimiterNode);
}

#undef LOCTEXT_NAMESPACE
