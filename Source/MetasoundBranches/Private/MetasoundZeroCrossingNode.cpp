// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundZeroCrossingNode.h"
#include "MetasoundExecutableOperator.h"     // TExecutableOperator class
#include "MetasoundPrimitives.h"             // ReadRef and WriteRef for data types
#include "MetasoundNodeRegistrationMacro.h"  // METASOUND_LOCTEXT and METASOUND_REGISTER_NODE macros
#include "MetasoundFacade.h"                 // FNodeFacade class
#include "MetasoundParamHelper.h"            // METASOUND_PARAM and METASOUND_GET_PARAM family of macros
#include "MetasoundTrigger.h"                // For FTriggerWriteRef and FTrigger

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_ZeroCrossing"

namespace Metasound
{
    namespace ZeroCrossingNames
    {
        METASOUND_PARAM(InputSignal, "In", "Input audio to monitor for zero crossings.");
        METASOUND_PARAM(InputDebounce, "Debounce", "Debounce time in seconds.");
        METASOUND_PARAM(OutputTriggerZeroCrossing, "Zero Crossing", "Trigger on zero crossing.");
    }

    class FZeroCrossingOperator : public TExecutableOperator<FZeroCrossingOperator>
    {
    public:
        // Constructor
        FZeroCrossingOperator(
            const FAudioBufferReadRef& InSignal,
            const FTimeReadRef& InDebounce,
            float InSampleRate,
            const FOperatorSettings& InSettings)
            : InputSignal(InSignal)
            , InputDebounce(InDebounce)
            , OutputTriggerZeroCrossing(FTriggerWriteRef::CreateNew(InSettings))
            , PreviousSignalValue(0.0f)
            , DebounceSamples(0)
            , DebounceCounter(0)
            , SampleRate(InSampleRate)
        {
        }

        static const FVertexInterface& DeclareVertexInterface()
        {
            using namespace ZeroCrossingNames;

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
                Metadata.CategoryHierarchy = { METASOUND_LOCTEXT("Custom", "Branches") };
                Metadata.Keywords = TArray<FText>(); // Keywords for searching

                return Metadata;
            };

            static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
            return Metadata;
        }

        virtual FDataReferenceCollection GetInputs() const override
        {
            using namespace ZeroCrossingNames;

            FDataReferenceCollection InputDataReferences;
            InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputSignal), InputSignal);
            InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputDebounce), InputDebounce);

            return InputDataReferences;
        }

        virtual FDataReferenceCollection GetOutputs() const override
        {
            using namespace ZeroCrossingNames;

            FDataReferenceCollection OutputDataReferences;
            OutputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutputTriggerZeroCrossing), OutputTriggerZeroCrossing);

            return OutputDataReferences;
        }

        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
        {
            using namespace ZeroCrossingNames;

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
            // Reset trigger
            OutputTriggerZeroCrossing->Reset();

            // Initialize PreviousSignalValue
            if (InputSignal->Num() > 0)
            {
                PreviousSignalValue = InputSignal->GetData()[0];
            }
            else
            {
                PreviousSignalValue = 0.0f;
            }

            // Reset debounce counter
            DebounceCounter = 0;
        }

        void Execute()
        {
            OutputTriggerZeroCrossing->AdvanceBlock();

            const float* SignalData = InputSignal->GetData();
            int32 NumFrames = InputSignal->Num();
            const float DebounceTime = InputDebounce->GetSeconds();

            // Recalculate debounce samples if debounce time or sample rate has changed
            if (LastDebounceTime != DebounceTime || LastSampleRate != SampleRate)
            {
                DebounceSamples = FMath::RoundToInt(FMath::Clamp(DebounceTime, 0.001f, 5.0f) * SampleRate);
                LastDebounceTime = DebounceTime;
                LastSampleRate = SampleRate;
            }

            for (int32 i = 0; i < NumFrames; ++i)
            {
                float CurrentSignal = SignalData[i];

                if (DebounceCounter > 0)
                {
                    DebounceCounter--;
                }

                bool PreviousNonPositive = (PreviousSignalValue <= 0.0f);
                bool CurrentPositive = (CurrentSignal > 0.0f);
                bool PreviousNonNegative = (PreviousSignalValue >= 0.0f);
                bool CurrentNegative = (CurrentSignal < 0.0f);

                // Crossing from negative or zero to positive
                if (PreviousNonPositive && CurrentPositive && DebounceCounter <= 0)
                {
                    OutputTriggerZeroCrossing->TriggerFrame(i);
                    DebounceCounter = DebounceSamples;
                }
                // Crossing from positive or zero to negative
                else if (PreviousNonNegative && CurrentNegative && DebounceCounter <= 0)
                {
                    OutputTriggerZeroCrossing->TriggerFrame(i);
                    DebounceCounter = DebounceSamples;
                }

                PreviousSignalValue = CurrentSignal;
            }
        }

    private:
        // Inputs
        FAudioBufferReadRef InputSignal;
        FTimeReadRef InputDebounce;

        // Output
        FTriggerWriteRef OutputTriggerZeroCrossing;

        // Internal variables
        float PreviousSignalValue;
        int32 DebounceSamples;
        int32 DebounceCounter;
        float SampleRate;

        // Variables to track changes in debounce time and sample rate
        float LastDebounceTime = -1.0f;
        float LastSampleRate = -1.0f;
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