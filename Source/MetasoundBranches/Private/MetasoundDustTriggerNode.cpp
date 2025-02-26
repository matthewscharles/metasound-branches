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

// Required for ensuring the node is supported by all languages in engine. Must be unique per MetaSound.
#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_DustTriggerNode"

namespace Metasound
{
    // Vertex Names - define the node's inputs and outputs here
    namespace DustTriggerNodeVertexNames
    {
        METASOUND_PARAM(InputDensity, "Modulation", "Input density control signal.");
        METASOUND_PARAM(InputDensityOffset, "Density", "Probability of trigger generation.");
        METASOUND_PARAM(InputEnabled, "Enabled", "Enable or disable generation.");
        METASOUND_PARAM(OutputTrigger, "Trigger Out", "Generated trigger output.");
    }

    // Operator Class - defines the way the node is described, created and executed
    class FDustTriggerOperator : public TExecutableOperator<FDustTriggerOperator>
    {
    public:
        // Constructor
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

        // Helper function for constructing vertex interface
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

        // Retrieves necessary metadata about the node
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
                    Metadata.CategoryHierarchy = { METASOUND_LOCTEXT("Custom", "Branches") };
                    Metadata.Keywords = TArray<FText>(); // Add relevant keywords if necessary

                    return Metadata;
                };

            static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
            return Metadata;
        }

        // Allows MetaSound graph to interact with the node's inputs
        virtual FDataReferenceCollection GetInputs() const override
        {
            using namespace DustTriggerNodeVertexNames;
            FDataReferenceCollection Inputs;
            Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputDensity), InputDensity);
            Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputDensityOffset), InputDensityOffset);
            Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputEnabled), InputEnabled);
            return Inputs;
        }

        // Allows MetaSound graph to interact with the node's outputs
        virtual FDataReferenceCollection GetOutputs() const override
        {
            using namespace DustTriggerNodeVertexNames;

            FDataReferenceCollection OutputDataReferences;

            OutputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutputTrigger), OutputTrigger);

            return OutputDataReferences;
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

        // Primary node functionality
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

    // Node Class - Inheriting from FNodeFacade is recommended for nodes that have a static FVertexInterface
    class FDustTriggerNode : public FNodeFacade
    {
    public:
        FDustTriggerNode(const FNodeInitData& InitData)
            : FNodeFacade(InitData.InstanceName, InitData.InstanceID, TFacadeOperatorClass<FDustTriggerOperator>())
        {
        }
    };

    // Register node
    METASOUND_REGISTER_NODE(FDustTriggerNode);
}

#undef LOCTEXT_NAMESPACE