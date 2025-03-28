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
        METASOUND_PARAM(InputDebounce, "Debounce", "Debounce time in seconds.");

        METASOUND_PARAM(OutputTriggerRise, "Rise", "Trigger on rise.");
        METASOUND_PARAM(OutputTriggerFall, "Fall", "Trigger on fall.");
    }

    class FEdgeOperator : public TExecutableOperator<FEdgeOperator>
    {
    public:
        FEdgeOperator(
            const FAudioBufferReadRef& InSignal,
            const FTimeReadRef& InDebounce,
            float InSampleRate,
            const FOperatorSettings& InSettings)
            : InputSignal(InSignal)
            , InputDebounce(InDebounce)
            , OutputTriggerRise(FTriggerWriteRef::CreateNew(InSettings))
            , OutputTriggerFall(FTriggerWriteRef::CreateNew(InSettings))
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
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputDebounce), InputDebounce);
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

            TDataReadReference<FTime> InputDebounce = InputData.GetOrCreateDefaultDataReadReference<FTime>(
                METASOUND_GET_PARAM_NAME(InputDebounce), InParams.OperatorSettings);

            float SampleRate = InParams.OperatorSettings.GetSampleRate();

            return MakeUnique<FEdgeOperator>(InputSignal, InputDebounce, SampleRate, InParams.OperatorSettings);
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

            LastTriggerFrame = -1000000;
            BlockIndex = 0;
        }

        void Execute()
        {
            OutputTriggerRise->AdvanceBlock();
            OutputTriggerFall->AdvanceBlock();

            const float* SignalData = InputSignal->GetData();
            int32 NumFrames = InputSignal->Num();
            const float DebounceTime = InputDebounce->GetSeconds();

            int32 DebounceSamples = FMath::Max(1, static_cast<int32>(FMath::Clamp(DebounceTime, 0.001f, 5.0f) * SampleRate));

            for (int32 i = 0; i < NumFrames; ++i)
            {
                float CurrentSignal = SignalData[i];
                int32 CurrentFrame = BlockIndex + i;

                if (CurrentSignal > PreviousSignalValue && !PreviousIsRising)
                {
                    if ((CurrentFrame - LastTriggerFrame) >= DebounceSamples)
                    {
                        OutputTriggerRise->TriggerFrame(i);
                        LastTriggerFrame = CurrentFrame;
                        PreviousIsRising = true;
                    }
                }
                else if (CurrentSignal < PreviousSignalValue && PreviousIsRising)
                {
                    if ((CurrentFrame - LastTriggerFrame) >= DebounceSamples)
                    {
                        OutputTriggerFall->TriggerFrame(i);
                        LastTriggerFrame = CurrentFrame;
                        PreviousIsRising = false;
                    }
                }

                PreviousSignalValue = CurrentSignal;
            }

            BlockIndex += NumFrames;
        }

    private:
        FAudioBufferReadRef InputSignal;
        FTimeReadRef InputDebounce;

        FTriggerWriteRef OutputTriggerRise;
        FTriggerWriteRef OutputTriggerFall;

        float SampleRate;
        float PreviousSignalValue = 0.0f;
        bool PreviousIsRising = false;

        int32 LastTriggerFrame = -1000000;
        int32 BlockIndex = 0;
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
