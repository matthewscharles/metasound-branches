// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundZeroCrossingNode.h"

#include "MetasoundExecutableOperator.h"
#include "MetasoundPrimitives.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundParamHelper.h"
#include "MetasoundTrigger.h"
#include "MetasoundBranches/Public/MetasoundCommonMacros.h"

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_ZeroCrossing"

namespace Metasound::MetasoundBranches
{
    namespace ZeroCrossingVertexNames
    {
        METASOUND_PARAM(InputSignal, "In", "Input audio to monitor for zero crossings.");
        METASOUND_PARAM(OutputTriggerZeroCrossing, "Zero Crossing", "Trigger on zero crossing.");
    }

    class FZeroCrossingOperator : public TExecutableOperator<FZeroCrossingOperator>
    {
    public:
        FZeroCrossingOperator(const FAudioBufferReadRef& InSignal,
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

        const FNodeClassMetadata& FZeroCrossingOperator::GetNodeInfo()
        {
            static const FNodeClassMetadata Metadata = FZeroCrossingNode::CreateNodeClassMetadata();
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

        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams,
                                                    FBuildResults& OutErrors)
        {
            using namespace ZeroCrossingVertexNames;

            const FInputVertexInterfaceData& InputData = InParams.InputData;

            TDataReadReference<FAudioBuffer> InputSignal =
                InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(
                    METASOUND_GET_PARAM_NAME(InputSignal),
                    InParams.OperatorSettings);

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
            const int32 NumFrames = InputSignal->Num();

            for (int32 i = 0; i < NumFrames; ++i)
            {
                const float CurrentSignal = SignalData[i];

                const bool PreviousNonPositive = (PreviousSignalValue <= 0.0f);
                const bool CurrentPositive     = (CurrentSignal > 0.0f);
                const bool PreviousNonNegative = (PreviousSignalValue >= 0.0f);
                const bool CurrentNegative     = (CurrentSignal < 0.0f);

                const bool bCrossing =
                    (PreviousNonPositive && CurrentPositive) ||
                    (PreviousNonNegative && CurrentNegative);

                if (bCrossing)
                {
                    OutputTriggerZeroCrossing->TriggerFrame(i);
                }

                PreviousSignalValue = CurrentSignal;
            }
        }

    private:
        FAudioBufferReadRef InputSignal;
        FTriggerWriteRef    OutputTriggerZeroCrossing;

        float PreviousSignalValue = 0.0f;
    };

    FNodeClassMetadata FZeroCrossingNode::CreateNodeClassMetadata()
    {
        using namespace ZeroCrossingVertexNames;

        FNodeClassMetadata Metadata;

        Metadata.ClassName = { TEXT("UE"), TEXT("Zero Crossing"), TEXT("Trigger") };
        Metadata.MajorVersion = 2;
        Metadata.MinorVersion = 0;

        Metadata.DisplayName =
            METASOUND_LOCTEXT("ZeroCrossingNodeDisplayName", "Zero Crossing");
        Metadata.Description =
            METASOUND_LOCTEXT("ZeroCrossingNodeDesc", "Detect zero crossings in an input audio signal.");
        Metadata.Author = TEXT("Charles Matthews");
        Metadata.PromptIfMissing = PluginNodeMissingPrompt;

        Metadata.DefaultInterface = FZeroCrossingOperator::DeclareVertexInterface();

        Metadata.CategoryHierarchy = {
            METASOUND_LOCTEXT("Custom",   "Branches"),
            METASOUND_LOCTEXT("CustomSub", "Envelopes")
        };

        Metadata.Keywords = TArray<FText>();

        return Metadata;
    }
    
    FZeroCrossingNode::FZeroCrossingNode(FNodeData InNodeData,
                                         TSharedRef<const FNodeClassMetadata> InClassMetadata)
        : FNodeFacade(InNodeData,
                      InClassMetadata,
                      TFacadeOperatorClass<FZeroCrossingOperator>())
    {
    }

    // Registration under the same namespace
    METASOUND_REGISTER_NODE(FZeroCrossingNode);
}

#undef LOCTEXT_NAMESPACE