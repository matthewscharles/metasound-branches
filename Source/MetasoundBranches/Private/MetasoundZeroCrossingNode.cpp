// Copyright 2026 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundZeroCrossingNode.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundPrimitives.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"
#include "MetasoundTrigger.h"
#include "MetasoundBranches/Public/MetasoundCommonMacros.h"

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_ZeroCrossing"

namespace Metasound
{
    namespace ZeroCrossingVertexNames
    {
        METASOUND_PARAM(InputSignal, "In", "Input audio to monitor for zero crossings.");
        METASOUND_PARAM(OutputTriggerZeroCrossing, "Zero Crossing", "Trigger on zero crossing.");
    }

    class FZeroCrossingOperator : public TExecutableOperator<FZeroCrossingOperator>
    {
    public:
        FZeroCrossingOperator(
            const FAudioBufferReadRef& InSignal,
            const FOperatorSettings& InSettings)
            : InputSignal(InSignal)
            , OutputTriggerZeroCrossing(FTriggerWriteRef::CreateNew(InSettings))
        {
        }

        static const FVertexInterface& DeclareVertexInterface()
        {
            using namespace ZeroCrossingVertexNames;

            static const FVertexInterface Interface(
                FInputVertexInterface(
                    TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputSignal))
                ),
                FOutputVertexInterface(
                    TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputTriggerZeroCrossing))
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

                Metadata.ClassName = { TEXT("UE"), TEXT("Zero Crossing"), TEXT("Trigger") };
                Metadata.MajorVersion = 2;
                Metadata.MinorVersion = 0;
                Metadata.DisplayName = METASOUND_LOCTEXT("ZeroCrossingNodeDisplayName", "Zero Crossing");
                Metadata.Description = METASOUND_LOCTEXT("ZeroCrossingNodeDesc", "Detect zero crossings in an input audio signal.");
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
            using namespace ZeroCrossingVertexNames;
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputSignal), InputSignal);
        }

        virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
        {
            using namespace ZeroCrossingVertexNames;
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputTriggerZeroCrossing), OutputTriggerZeroCrossing);
        }

        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
        {
            using namespace ZeroCrossingVertexNames;

            const FInputVertexInterfaceData& InputData = InParams.InputData;

            TDataReadReference<FAudioBuffer> InputSignal = InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(
                METASOUND_GET_PARAM_NAME(InputSignal), InParams.OperatorSettings);

            return MakeUnique<FZeroCrossingOperator>(InputSignal, InParams.OperatorSettings);
        }

        virtual void Reset(const IOperator::FResetParams& InParams)
        {
            OutputTriggerZeroCrossing->Reset();

            if (InputSignal->Num() > 0)
            {
                PreviousSignalValue = InputSignal->GetData()[0];
            }
            else
            {
                PreviousSignalValue = 0.0f;
            }
        }

        void Execute()
        {
            OutputTriggerZeroCrossing->AdvanceBlock();

            const float* SignalData = InputSignal->GetData();
            int32 NumFrames = InputSignal->Num();

            for (int32 i = 0; i < NumFrames; ++i)
            {
                float CurrentSignal = SignalData[i];

                bool PreviousNonPositive = (PreviousSignalValue <= 0.0f);
                bool CurrentPositive = (CurrentSignal > 0.0f);
                bool PreviousNonNegative = (PreviousSignalValue >= 0.0f);
                bool CurrentNegative = (CurrentSignal < 0.0f);

                bool Crossing = (PreviousNonPositive && CurrentPositive) || (PreviousNonNegative && CurrentNegative);

                if (Crossing)
                {
                    OutputTriggerZeroCrossing->TriggerFrame(i);
                }

                PreviousSignalValue = CurrentSignal;
            }
        }

    private:
        FAudioBufferReadRef InputSignal;
        FTriggerWriteRef OutputTriggerZeroCrossing;

        float PreviousSignalValue = 0.0f;
    };

    class FZeroCrossingNode : public FNodeFacade
    {
    public:
                static FNodeClassMetadata CreateNodeClassMetadata()
        {
            return FZeroCrossingOperator::GetNodeInfo();
        }
        FZeroCrossingNode(FNodeData InitData)
            : FNodeFacade(InitData, MakeShared<const FNodeClassMetadata>(FZeroCrossingOperator::GetNodeInfo()), TFacadeOperatorClass<FZeroCrossingOperator>())
        {
        }
    };

    METASOUND_REGISTER_NODE(FZeroCrossingNode);
}

#undef LOCTEXT_NAMESPACE
