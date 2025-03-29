// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundEdgeNode.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundPrimitives.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"
#include "MetasoundTrigger.h"
#include "MetasoundBranches/Public/MetasoundCommonMacros.h"

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_Edge"

namespace Metasound
{
    namespace EdgeNames
    {
        METASOUND_PARAM(InputSignal, "In", "Input audio to monitor for edge detection.");
        METASOUND_PARAM(InputThreshold, "Slope Threshold", "Minimum slope (delta) required to detect an edge.");

        METASOUND_PARAM(OutputTriggerRise, "Rise", "Trigger on rise.");
        METASOUND_PARAM(OutputTriggerFall, "Fall", "Trigger on fall.");
    }

    class FEdgeOperator : public TExecutableOperator<FEdgeOperator>
    {
    public:
        FEdgeOperator(
            const FAudioBufferReadRef& InSignal,
            const FFloatReadRef& InThreshold,
            const FOperatorSettings& InSettings)
            : InputSignal(InSignal)
            , InputThreshold(InThreshold)
            , OutputTriggerRise(FTriggerWriteRef::CreateNew(InSettings))
            , OutputTriggerFall(FTriggerWriteRef::CreateNew(InSettings))
        {
        }

        static const FVertexInterface& DeclareVertexInterface()
        {
            using namespace EdgeNames;

            static const FVertexInterface Interface(
                FInputVertexInterface(
                    TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputSignal)),
                    TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputThreshold), 0.001f)
                ),
                FOutputVertexInterface(
                    TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputTriggerRise)),
                    TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputTriggerFall))
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

                Metadata.ClassName = { TEXT("UE"), TEXT("Edge"), TEXT("Trigger") };
                Metadata.MajorVersion = 1;
                Metadata.MinorVersion = 1;
                Metadata.DisplayName = METASOUND_LOCTEXT("EdgeNodeDisplayName", "Edge");
                Metadata.Description = METASOUND_LOCTEXT("EdgeNodeDesc", "Detect upward and downward changes in an input audio signal, using a minimum slope threshold to filter small fluctuations.");
                Metadata.Author = "Charles Matthews";
                Metadata.PromptIfMissing = PluginNodeMissingPrompt;
                Metadata.DefaultInterface = DeclareVertexInterface();
                Metadata.CategoryHierarchy = {
                    METASOUND_LOCTEXT("Custom", "Branches"),
                    METASOUND_LOCTEXT("CustomSub", "Envelopes")
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
            using namespace EdgeNames;
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputSignal), InputSignal);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputThreshold), InputThreshold);
        }

        virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
        {
            using namespace EdgeNames;
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputTriggerRise), OutputTriggerRise);
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputTriggerFall), OutputTriggerFall);
        }

        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
        {
            using namespace EdgeNames;

            const FInputVertexInterfaceData& InputData = InParams.InputData;
            const Metasound::FInputVertexInterface& InputInterface = DeclareVertexInterface().GetInputInterface();

            TDataReadReference<FAudioBuffer> InputSignal = InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(
                METASOUND_GET_PARAM_NAME(InputSignal), InParams.OperatorSettings);

            TDataReadReference<float> InputThreshold = InputData.GetOrCreateDefaultDataReadReference<float>(
                METASOUND_GET_PARAM_NAME(InputThreshold), InParams.OperatorSettings);

            return MakeUnique<FEdgeOperator>(InputSignal, InputThreshold, InParams.OperatorSettings);
        }

        virtual void Reset(const IOperator::FResetParams& InParams)
        {
            OutputTriggerRise->Reset();
            OutputTriggerFall->Reset();

            if (InputSignal->Num() > 0)
            {
                PreviousSignalValue = InputSignal->GetData()[0];
            }
            else
            {
                PreviousSignalValue = 0.0f;
            }

            PreviousIsRising = false;
        }

        void Execute()
        {
            OutputTriggerRise->AdvanceBlock();
            OutputTriggerFall->AdvanceBlock();

            const float* SignalData = InputSignal->GetData();
            int32 NumFrames = InputSignal->Num();
            const float Threshold = FMath::Max(0.0f, *InputThreshold);

            for (int32 i = 0; i < NumFrames; ++i)
            {
                float CurrentSignal = SignalData[i];
                float Delta = CurrentSignal - PreviousSignalValue;

                if (Delta > Threshold && !PreviousIsRising)
                {
                    OutputTriggerRise->TriggerFrame(i);
                    PreviousIsRising = true;
                }
                else if (Delta < -Threshold && PreviousIsRising)
                {
                    OutputTriggerFall->TriggerFrame(i);
                    PreviousIsRising = false;
                }

                PreviousSignalValue = CurrentSignal;
            }
        }

    private:
        FAudioBufferReadRef InputSignal;
        FFloatReadRef InputThreshold;

        FTriggerWriteRef OutputTriggerRise;
        FTriggerWriteRef OutputTriggerFall;

        float PreviousSignalValue = 0.0f;
        bool PreviousIsRising = false;
    };

    class FEdgeNode : public FNodeFacade
    {
    public:
        FEdgeNode(const FNodeInitData& InitData)
            : FNodeFacade(InitData.InstanceName, InitData.InstanceID, TFacadeOperatorClass<FEdgeOperator>())
        {
        }
    };

    METASOUND_REGISTER_NODE(FEdgeNode);
}

#undef LOCTEXT_NAMESPACE
