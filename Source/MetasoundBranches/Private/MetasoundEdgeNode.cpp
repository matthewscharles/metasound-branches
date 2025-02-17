// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundEdgeNode.h"
#include "MetasoundExecutableOperator.h"     // TExecutableOperator class
#include "MetasoundPrimitives.h"             // ReadRef and WriteRef descriptions for bool, int32, float, and string
#include "MetasoundNodeRegistrationMacro.h"  // METASOUND_LOCTEXT and METASOUND_REGISTER_NODE macros
#include "MetasoundFacade.h"                 // FNodeFacade class, eliminates the need for a fair amount of boilerplate code
#include "MetasoundParamHelper.h"            // METASOUND_PARAM and METASOUND_GET_PARAM family of macros
#include "MetasoundTrigger.h"                // For FTriggerWriteRef and FTrigger

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_Edge"

namespace Metasound
{
    namespace EdgeNames
    {
        METASOUND_PARAM(InputSignal, "In", "Input audio to monitor for edge detection.");
        METASOUND_PARAM(InputDebounce, "Debounce", "Debounce time in seconds.");

        METASOUND_PARAM(OutputTriggerRise, "Rise", "Trigger on rise.");
        METASOUND_PARAM(OutputTriggerFall, "Fall", "Trigger on fall.");
    }

    class FEdgeOperator : public TExecutableOperator<FEdgeOperator>
    {
    public:
        // Constructor
        FEdgeOperator(
            const FAudioBufferReadRef& InSignal,
            const FTimeReadRef& InDebounce,
            float InSampleRate,
            const FOperatorSettings& InSettings)
            : InputSignal(InSignal)
            , InputDebounce(InDebounce)
            , OutputTriggerRise(FTriggerWriteRef::CreateNew(InSettings))
            , OutputTriggerFall(FTriggerWriteRef::CreateNew(InSettings))
            , PreviousSignalValue(0.0f)
            , DebounceSamples(0)
            , DebounceCounter(0)
            , SampleRate(InSampleRate)
        {
        }

        static const FVertexInterface& DeclareVertexInterface()
        {
            using namespace EdgeNames;

            static const FVertexInterface Interface(
                FInputVertexInterface(
                    TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputSignal)),
                    TInputDataVertex<FTime>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputDebounce))
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
                Metadata.Description = METASOUND_LOCTEXT("EdgeNodeDesc", "Detect upward and downward changes in an input audio signal, with optional debounce.");
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
            using namespace EdgeNames;

            FDataReferenceCollection InputDataReferences;

            InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputSignal), InputSignal);
            InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputDebounce), InputDebounce);

            return InputDataReferences;
        }

        virtual FDataReferenceCollection GetOutputs() const override
        {
            using namespace EdgeNames;

            FDataReferenceCollection OutputDataReferences;

            OutputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutputTriggerRise), OutputTriggerRise);
            OutputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutputTriggerFall), OutputTriggerFall);

            return OutputDataReferences;
        }

        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
        {
            using namespace EdgeNames;

            const FInputVertexInterfaceData& InputData = InParams.InputData;
            const Metasound::FInputVertexInterface& InputInterface = DeclareVertexInterface().GetInputInterface();

            TDataReadReference<FAudioBuffer> InputSignal = InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(
                METASOUND_GET_PARAM_NAME(InputSignal), InParams.OperatorSettings);

            TDataReadReference<FTime> InputDebounce = InputData.GetOrCreateDefaultDataReadReference<FTime>(
                METASOUND_GET_PARAM_NAME(InputDebounce), InParams.OperatorSettings);

            float SampleRate = InParams.OperatorSettings.GetSampleRate();

            return MakeUnique<FEdgeOperator>(InputSignal, InputDebounce, SampleRate, InParams.OperatorSettings);
        }
        
        virtual void Reset(const IOperator::FResetParams& InParams)
        {
            // Reset triggers
            OutputTriggerRise->Reset();
            OutputTriggerFall->Reset();

            // Initialize PreviousSignalValue to the first sample of the incoming signal to prevent false triggers
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
            OutputTriggerRise->AdvanceBlock();
            OutputTriggerFall->AdvanceBlock();

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

                // Decrement debounce counter
                if (DebounceCounter > 0)
                {
                    DebounceCounter--;
                }

                // Detect rising edge
                if (CurrentSignal > PreviousSignalValue && !PreviousIsRising && DebounceCounter <= 0)
                {
                    // Rising edge detected
                    OutputTriggerRise->TriggerFrame(i);
                    DebounceCounter = DebounceSamples;
                    PreviousIsRising = true;
                }
                // Detect falling edge
                else if (CurrentSignal < PreviousSignalValue && PreviousIsRising && DebounceCounter <= 0)
                {
                    // Falling edge detected
                    OutputTriggerFall->TriggerFrame(i);
                    DebounceCounter = DebounceSamples;
                    PreviousIsRising = false;
                }

                // Update previous signal value
                PreviousSignalValue = CurrentSignal;
            }
        }

    private:
        // Inputs
        FAudioBufferReadRef InputSignal;
        FTimeReadRef InputDebounce;

        // Outputs
        FTriggerWriteRef OutputTriggerRise;
        FTriggerWriteRef OutputTriggerFall;

        // Internal variables
        float PreviousSignalValue;
        bool PreviousIsRising = false;
        int32 DebounceSamples;
        int32 DebounceCounter;
        float SampleRate;
        
        // Variables to track changes in debounce time and sample rate
        float LastDebounceTime = -1.0f;
        float LastSampleRate = -1.0f;
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