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
        METASOUND_PARAM(InputDebounce, "Debounce", "Debounce time in seconds.");
        METASOUND_PARAM(OutputTriggerZeroCrossing, "Zero Crossing", "Trigger on zero crossing.");
    }

    class FZeroCrossingOperator : public TExecutableOperator<FZeroCrossingOperator>
    {
    public:
        FZeroCrossingOperator(
            const FAudioBufferReadRef& InSignal,
            const FTimeReadRef& InDebounce,
            float InSampleRate,
            const FOperatorSettings& InSettings)
            : InputSignal(InSignal)
            , InputDebounce(InDebounce)
            , OutputTriggerZeroCrossing(FTriggerWriteRef::CreateNew(InSettings))
            , SampleRate(InSampleRate)
        {
        }

        static const FVertexInterface& DeclareVertexInterface()
        {
            using namespace ZeroCrossingVertexNames;

            static const FVertexInterface Interface(
                FInputVertexInterface(
                    TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputSignal)),
                    TInputDataVertex<FTime>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputDebounce))
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
                Metadata.Description = METASOUND_LOCTEXT("ZeroCrossingNodeDesc", "Detect zero crossings in an input audio signal, with optional debounce.");
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
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputDebounce), InputDebounce);
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

            TDataReadReference<FTime> InputDebounce = InputData.GetOrCreateDefaultDataReadReference<FTime>(
                METASOUND_GET_PARAM_NAME(InputDebounce), InParams.OperatorSettings);

            float SampleRate = InParams.OperatorSettings.GetSampleRate();

            return MakeUnique<FZeroCrossingOperator>(InputSignal, InputDebounce, SampleRate, InParams.OperatorSettings);
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

            LastTriggerFrame = -1000000;
            BlockIndex = 0;
        }

        void Execute()
        {
            OutputTriggerZeroCrossing->AdvanceBlock();

            const float* SignalData = InputSignal->GetData();
            int32 NumFrames = InputSignal->Num();
            const float DebounceTime = InputDebounce->GetSeconds();

            int32 DebounceSamples = FMath::Max(1, static_cast<int32>(FMath::Clamp(DebounceTime, 0.001f, 5.0f) * SampleRate));

            for (int32 i = 0; i < NumFrames; ++i)
            {
                float CurrentSignal = SignalData[i];
                int32 CurrentFrame = BlockIndex + i;

                bool PreviousNonPositive = (PreviousSignalValue <= 0.0f);
                bool CurrentPositive = (CurrentSignal > 0.0f);
                bool PreviousNonNegative = (PreviousSignalValue >= 0.0f);
                bool CurrentNegative = (CurrentSignal < 0.0f);

                bool Crossing = (PreviousNonPositive && CurrentPositive) || (PreviousNonNegative && CurrentNegative);

                if (Crossing && (CurrentFrame - LastTriggerFrame) >= DebounceSamples)
                {
                    OutputTriggerZeroCrossing->TriggerFrame(i);
                    LastTriggerFrame = CurrentFrame;
                }

                PreviousSignalValue = CurrentSignal;
            }

            BlockIndex += NumFrames;
        }

    private:
        FAudioBufferReadRef InputSignal;
        FTimeReadRef InputDebounce;

        FTriggerWriteRef OutputTriggerZeroCrossing;

        float SampleRate;
        float PreviousSignalValue = 0.0f;

        int32 LastTriggerFrame = -1000000;
        int32 BlockIndex = 0;
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
