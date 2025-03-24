// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundDustTriggerNode.h"
#include "MetasoundExecutableOperator.h"     // TExecutableOperator class
#include "MetasoundPrimitives.h"             // ReadRef and WriteRef descriptions for bool, int32, float, and string
#include "MetasoundNodeRegistrationMacro.h"  // METASOUND_LOCTEXT and METASOUND_REGISTER_NODE macros
#include "MetasoundFacade.h"                 // FNodeFacade class, eliminates the need for a fair amount of boilerplate code
#include "MetasoundParamHelper.h"            // METASOUND_PARAM and METASOUND_GET_PARAM family of macros
#include "Math/UnrealMathUtility.h"          // For FMath functions
#include "Misc/DateTime.h"                   // For FDateTime::UtcNow()
#include "MetasoundTrigger.h"                // For FTrigger classes

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_DustTriggerNode"

namespace Metasound
{
    namespace DustTriggerNodeVertexNames
    {
        METASOUND_PARAM(InputDensity, "Modulation", "Input density control signal.");
        METASOUND_PARAM(InputDensityOffset, "Density", "Probability of trigger generation.");
        METASOUND_PARAM(InputEnabled, "Enabled", "Enable or disable generation.");
        METASOUND_PARAM(OutputTrigger, "Trigger Out", "Generated trigger output.");
    }

    class FDustTriggerOperator : public TExecutableOperator<FDustTriggerOperator>
    {
    public:
        FDustTriggerOperator(
            const FOperatorSettings& InSettings,
            const FAudioBufferReadRef& InDensity,
            const FFloatReadRef& InDensityOffset,
            const FBoolReadRef& InEnabled)
            : InputDensity(InDensity)
            , InputDensityOffset(InDensityOffset)
            , InputEnabled(InEnabled)
            , OutputTrigger(FTriggerWriteRef::CreateNew(InSettings))
            , RNGStream(InitialSeed())
        {
        }

        static const FVertexInterface& DeclareVertexInterface()
        {
            using namespace DustTriggerNodeVertexNames;

            static const FVertexInterface Interface(
                FInputVertexInterface(
                    TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputEnabled), true),
                    TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputDensityOffset), 0.1f),
                    TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputDensity))
                ),
                FOutputVertexInterface(
                    TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputTrigger))
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

                    Metadata.ClassName = { TEXT("UE"), TEXT("Dust (Trigger)"), TEXT("Trigger") };
                    Metadata.MajorVersion = 1;
                    Metadata.MinorVersion = 1;
                    Metadata.DisplayName = METASOUND_LOCTEXT("DustTriggerNodeDisplayName", "Dust (Trigger)");
                    Metadata.Description = METASOUND_LOCTEXT("DustTriggerNodeDesc", "Generate randomly timed trigger events, with audio-rate modulation.");
                    Metadata.Author = "Charles Matthews";
                    Metadata.PromptIfMissing = PluginNodeMissingPrompt;
                    Metadata.DefaultInterface = DeclareVertexInterface();
                    Metadata.CategoryHierarchy = {
                        METASOUND_LOCTEXT("Custom", "Branches"),
                        METASOUND_LOCTEXT("CustomSub", "Triggers")
                    };
                    Metadata.Keywords = TArray<FText>(); // Add relevant keywords if necessary

                    return Metadata;
                };

            static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
            return Metadata;
        }

        METASOUND_DISABLE_LEGACY_IO()
        
        virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
        {
            using namespace DustTriggerNodeVertexNames;

            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputDensity), InputDensity);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputDensityOffset), InputDensityOffset);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputEnabled), InputEnabled);
        }
        
        virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
        {
            using namespace DustTriggerNodeVertexNames;

            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputTrigger), OutputTrigger);
        }

        // Used to instantiate a new runtime instance of the node
        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
        {
            using namespace DustTriggerNodeVertexNames;

            const FInputVertexInterfaceData& InputData = InParams.InputData;
            const Metasound::FInputVertexInterface& InputInterface = DeclareVertexInterface().GetInputInterface();

            TDataReadReference<FAudioBuffer> InputDensity = InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(InputDensity), InParams.OperatorSettings);
            TDataReadReference<float> InputDensityOffset = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputDensityOffset), InParams.OperatorSettings);
            TDataReadReference<bool> InputEnabled = InputData.GetOrCreateDefaultDataReadReference<bool>(METASOUND_GET_PARAM_NAME(InputEnabled), InParams.OperatorSettings);

            return MakeUnique<FDustTriggerOperator>(InParams.OperatorSettings, InputDensity, InputDensityOffset, InputEnabled);
        }

        void Execute()
        {
            OutputTrigger->AdvanceBlock();
            const float* DensityData = InputDensity->GetData();
            int32 NumFrames = InputDensity->Num();
            float InputDensityOffsetValue = *InputDensityOffset;
            bool bEnabled = *InputEnabled;

            for (int32 i = 0; i < NumFrames; ++i)
            {
                if (bEnabled)
                {
                    float Density = DensityData[i];
                    float AbsDensity = FMath::Abs(Density) + InputDensityOffsetValue;
                    float Threshold = 1.0f - AbsDensity * 0.0009f;

                    float RandomValue = RNGStream.GetFraction();

                    if (RandomValue > Threshold)
                    {
                        OutputTrigger->TriggerFrame(i);
                    }
                }
            }
        }

    private:

        // Inputs
        FAudioBufferReadRef InputDensity;
        FFloatReadRef InputDensityOffset;
        FBoolReadRef InputEnabled;

        // Output
        FTriggerWriteRef OutputTrigger;

        // Random number generator
        FRandomStream RNGStream;

        // Generate an initial seed for FRandomStream
        static int32 InitialSeed()
        {
            return FDateTime::UtcNow().GetTicks();
        }
    };

    
    class FDustTriggerNode : public FNodeFacade
    {
    public:
        FDustTriggerNode(const FNodeInitData& InitData)
            : FNodeFacade(InitData.InstanceName, InitData.InstanceID, TFacadeOperatorClass<FDustTriggerOperator>())
        {
        }
    };

    METASOUND_REGISTER_NODE(FDustTriggerNode);
}

#undef LOCTEXT_NAMESPACE