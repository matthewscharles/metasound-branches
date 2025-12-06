// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundImpulseNode.h"
#include "MetasoundExecutableOperator.h"     
#include "MetasoundPrimitives.h"             
#include "MetasoundNodeRegistrationMacro.h"  
#include "MetasoundFacade.h"                
#include "MetasoundParamHelper.h"            
#include "MetasoundBranches/Public/MetasoundCommonMacros.h"

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_ImpulseNode"

namespace Metasound::MetasoundBranches
{
    namespace ImpulseNodeVertexNames
    {
        METASOUND_PARAM(InputTrigger, "Trigger", "Trigger input to generate an impulse.");
        METASOUND_PARAM(InputBiPolar, "Bi-Polar", "Toggle between bipolar and unipolar impulse output.");
        METASOUND_PARAM(OutputOnTrigger, "On Trigger", "Trigger output when the node is triggered.");
        METASOUND_PARAM(OutputImpulse, "Impulse Out", "Generated impulse output.");
    }

    class FImpulseOperator : public TExecutableOperator<FImpulseOperator>
    {
    public:
        FImpulseOperator(
            const FOperatorSettings& InSettings,
            const FTriggerReadRef& InTrigger,
            const FBoolReadRef& InBiPolar)
            : InputTrigger(InTrigger)
            , InputBiPolar(InBiPolar)
            , OnTrigger(FTriggerWriteRef::CreateNew(InSettings))
            , OutputImpulse(FAudioBufferWriteRef::CreateNew(InSettings))
            , SignalIsPositive(true)
        {
        }

        static const FVertexInterface& DeclareVertexInterface()
        {
            using namespace ImpulseNodeVertexNames;

            static const FVertexInterface Interface(
                FInputVertexInterface(
                    TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTrigger)),
                    TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputBiPolar), true)
                ),
                FOutputVertexInterface(
                    TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputOnTrigger)),
                    TOutputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputImpulse))
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

                    Metadata.ClassName = { TEXT("UE"), TEXT("Impulse"), TEXT("Audio") };
                    Metadata.MajorVersion = 1;
                    Metadata.MinorVersion = 1;
                    Metadata.DisplayName = METASOUND_LOCTEXT("ImpulseNodeDisplayName", "Impulse");
                    Metadata.Description = METASOUND_LOCTEXT("ImpulseNodeDesc", "Generates a single-sample impulse when triggered.");
                    Metadata.Author = "Charles Matthews";
                    Metadata.PromptIfMissing = PluginNodeMissingPrompt;
                    Metadata.DefaultInterface = DeclareVertexInterface();
                    Metadata.CategoryHierarchy = {
                        METASOUND_LOCTEXT("Custom", "Branches"),
                        METASOUND_LOCTEXT("CustomSub", "Generators")
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
            using namespace ImpulseNodeVertexNames;
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputTrigger), InputTrigger);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputBiPolar), InputBiPolar);
        }
        
        virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
        {
            using namespace ImpulseNodeVertexNames;
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputOnTrigger), OnTrigger);
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputImpulse), OutputImpulse);
        }
        
        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
        {
            using namespace ImpulseNodeVertexNames;

            const FInputVertexInterfaceData& InputData = InParams.InputData;
            const Metasound::FInputVertexInterface& InputInterface = DeclareVertexInterface().GetInputInterface();

            TDataReadReference<FTrigger> InputTrigger = InputData.GetOrCreateDefaultDataReadReference<FTrigger>(METASOUND_GET_PARAM_NAME(InputTrigger), InParams.OperatorSettings);
            TDataReadReference<bool> InputBiPolar = InputData.GetOrCreateDefaultDataReadReference<bool>(METASOUND_GET_PARAM_NAME(InputBiPolar), InParams.OperatorSettings);

            return MakeUnique<FImpulseOperator>(
                InParams.OperatorSettings,
                InputTrigger,
                InputBiPolar
            );
        }

        void Execute()
        {
            OnTrigger->AdvanceBlock();
            OutputImpulse->Zero(); // Ensure the output buffer is cleared
            
            // Initialize the output buffer to zero
            int32 NumFrames = OutputImpulse->Num();
            float* OutputDataPtr = OutputImpulse->GetData();
            FMemory::Memzero(OutputDataPtr, sizeof(float) * NumFrames);
            InputTrigger->ExecuteBlock(
                // Pre-trigger lambda (called before any triggers in the block)
                [](int32 StartFrame, int32 EndFrame)
                {
                    // No action needed before triggers
                },

                // On-trigger lambda (called for each trigger event)
                [&](int32 TriggerFrame, int32 TriggerFrameEnd)
                {
                    if (TriggerFrame < NumFrames)
                    {
                        OnTrigger->TriggerFrame(TriggerFrame);
                        if (*InputBiPolar)
                        {
                            OutputDataPtr[TriggerFrame] = SignalIsPositive ? 1.0f : -1.0f;
                            SignalIsPositive = !SignalIsPositive;
                        }
                        else
                        {
                            OutputDataPtr[TriggerFrame] = 1.0f;
                        }
                    }
                }
            );
        }

    private:

        // Inputs
        FTriggerReadRef InputTrigger;
        FBoolReadRef InputBiPolar;

        // Outputs
        FTriggerWriteRef OnTrigger;
        FAudioBufferWriteRef OutputImpulse;

        bool SignalIsPositive;

    };

    
    class FImpulseNode : public FNodeFacade
    {
    public:
        FImpulseNode(const FNodeInitData& InitData)
            : FNodeFacade(InitData.InstanceName, InitData.InstanceID, TFacadeOperatorClass<FImpulseOperator>())
        {
        }
    };

    METASOUND_REGISTER_NODE(FImpulseNode);
}

#undef LOCTEXT_NAMESPACE