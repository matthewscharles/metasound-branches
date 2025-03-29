// Copyright 2025 Charles Matthews. All Rights Reserved.

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
        METASOUND_PARAM(InputThreshold, "Threshold", "Minimum amplitude required to detect a zero crossing.");
        METASOUND_PARAM(OutputTriggerZeroCrossing, "Zero Crossing", "Trigger on zero crossing.");
    }

    class FZeroCrossingOperator : public TExecutableOperator<FZeroCrossingOperator>
    {
    public:
        FZeroCrossingOperator(
            const FAudioBufferReadRef& InSignal,
            const FFloatReadRef& InThreshold,
            const FOperatorSettings& InSettings)
            : InputSignal(InSignal)
            , InputThreshold(InThreshold)
            , OutputTriggerZeroCrossing(FTriggerWriteRef::CreateNew(InSettings))
        {
        }

        static const FVertexInterface& DeclareVertexInterface()
        {
            using namespace ZeroCrossingVertexNames;

            static const FVertexInterface Interface(
                FInputVertexInterface(
                    TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputSignal)),
                    TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputThreshold), 0.01f)
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
                Metadata.MajorVersion = 1;
                Metadata.MinorVersion = 1;
                Metadata.DisplayName = METASOUND_LOCTEXT("ZeroCrossingNodeDisplayName", "Zero Crossing");
                Metadata.Description = METASOUND_LOCTEXT("ZeroCrossingNodeDesc", "Detect zero crossings in an input audio signal, with optional amplitude threshold to handle noise.");
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
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputThreshold), InputThreshold);
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
            const Metasound::FInputVertexInterface& InputInterface = DeclareVertexInterface().GetInputInterface();

            TDataReadReference<FAudioBuffer> InputSignal = InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(
                METASOUND_GET_PARAM_NAME(InputSignal), InParams.OperatorSettings);

            TDataReadReference<float> InputThreshold = InputData.GetOrCreateDefaultDataReadReference<float>(
                METASOUND_GET_PARAM_NAME(InputThreshold), InParams.OperatorSettings);

            return MakeUnique<FZeroCrossingOperator>(InputSignal, InputThreshold, InParams.OperatorSettings);
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
            const float Threshold = FMath::Max(0.0f, *InputThreshold);

            for (int32 i = 0; i < NumFrames; ++i)
            {
                float CurrentSignal = SignalData[i];

                bool PrevBelowNegThresh = PreviousSignalValue <= -Threshold;
                bool PrevAbovePosThresh = PreviousSignalValue >= Threshold;
                bool CurrAbovePosThresh = CurrentSignal > Threshold;
                bool CurrBelowNegThresh = CurrentSignal < -Threshold;

                bool Crossing = (PrevBelowNegThresh && CurrAbovePosThresh) ||
                                (PrevAbovePosThresh && CurrBelowNegThresh);

                if (Crossing)
                {
                    OutputTriggerZeroCrossing->TriggerFrame(i);
                }

                PreviousSignalValue = CurrentSignal;
            }
        }

    private:
        FAudioBufferReadRef InputSignal;
        FFloatReadRef InputThreshold;
        FTriggerWriteRef OutputTriggerZeroCrossing;

        float PreviousSignalValue = 0.0f;
    };

    class FZeroCrossingNode : public FNodeFacade
    {
    public:
        FZeroCrossingNode(const FNodeInitData& InitData)
            : FNodeFacade(InitData.InstanceName, InitData.InstanceID, TFacadeOperatorClass<FZeroCrossingOperator>())
        {
        }
    };

    METASOUND_REGISTER_NODE(FZeroCrossingNode);
}

#undef LOCTEXT_NAMESPACE
