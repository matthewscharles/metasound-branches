// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundDustNode.h"
#include "MetasoundExecutableOperator.h"     // TExecutableOperator class
#include "MetasoundPrimitives.h"             // ReadRef and WriteRef descriptions for bool, int32, float, and string
#include "MetasoundNodeRegistrationMacro.h"  // METASOUND_LOCTEXT and METASOUND_REGISTER_NODE macros
#include "MetasoundFacade.h"                 // FNodeFacade class, eliminates the need for a fair amount of boilerplate code
#include "MetasoundParamHelper.h"            // METASOUND_PARAM and METASOUND_GET_PARAM family of macros
#include "Math/UnrealMathUtility.h"          // For FMath functions
#include "Misc/DateTime.h"                   // For FDateTime::UtcNow()

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_DustNode"

namespace Metasound
{
    namespace DustNodeVertexNames
    {
        METASOUND_PARAM(InputDensity, "Modulation", "Density control signal.");
        METASOUND_PARAM(InputDensityOffset, "Density", "Probability of impulse generation.");
        METASOUND_PARAM(InputEnabled, "Enabled", "Enable or disable generation.");
        METASOUND_PARAM(InputBiPolar, "Bi-Polar", "Toggle between bipolar and unipolar impulse output.");
        METASOUND_PARAM(OutputImpulse, "Impulse Out", "Generated impulse output.");
    }

    class FDustOperator : public TExecutableOperator<FDustOperator>
    {
    public:
        // Constructor
        FDustOperator(
            const FOperatorSettings& InSettings,
            const FAudioBufferReadRef& InDensity,
            const FFloatReadRef& InDensityOffset,
            const FBoolReadRef& InEnabled,
            const FBoolReadRef& InBiPolar)
            : InputDensity(InDensity)
            , InputDensityOffset(InDensityOffset)
            , InputEnabled(InEnabled)
            , InputBiPolar(InBiPolar)
            , OutputImpulse(FAudioBufferWriteRef::CreateNew(InSettings))
            , RNGStream(InitialSeed())
            , SignalIsPositive(true)
        {
        }

        static const FVertexInterface& DeclareVertexInterface()
        {
            using namespace DustNodeVertexNames;

            static const FVertexInterface Interface(
                FInputVertexInterface(
                    TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputEnabled), true),
                    TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputBiPolar), true),
                    TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputDensityOffset), 0.1f),
                    TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputDensity))
                ),
                FOutputVertexInterface(
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

                    Metadata.ClassName = { TEXT("UE"), TEXT("Dust (Audio)"), TEXT("Audio") };
                    Metadata.MajorVersion = 1;
                    Metadata.MinorVersion = 1;
                    Metadata.DisplayName = METASOUND_LOCTEXT("DustNodeDisplayName", "Dust (Audio)");
                    Metadata.Description = METASOUND_LOCTEXT("DustNodeDesc", "Generate randomly timed impulses with audio-rate modulation.");
                    Metadata.Author = "Charles Matthews";
                    Metadata.PromptIfMissing = PluginNodeMissingPrompt;
                    Metadata.DefaultInterface = DeclareVertexInterface();
                    Metadata.CategoryHierarchy = {
                        METASOUND_LOCTEXT("Custom", "Branches"),
                        METASOUND_LOCTEXT("CustomSub", "Generators")
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
            using namespace DustNodeVertexNames;

            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputDensity), InputDensity);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputDensityOffset), InputDensityOffset);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputEnabled), InputEnabled);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputBiPolar), InputBiPolar);
        }
        
        virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
        {
            using namespace DustNodeVertexNames;

            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputImpulse), OutputImpulse);
        }
        
        // Used to instantiate a new runtime instance of the node
        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
        {
            using namespace DustNodeVertexNames;

            const FInputVertexInterfaceData& InputData = InParams.InputData;
            const Metasound::FInputVertexInterface& InputInterface = DeclareVertexInterface().GetInputInterface();

            TDataReadReference<FAudioBuffer> InputDensity = InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(InputDensity), InParams.OperatorSettings);
            TDataReadReference<float> InputDensityOffset = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputDensityOffset), InParams.OperatorSettings);
            TDataReadReference<bool> InputEnabled = InputData.GetOrCreateDefaultDataReadReference<bool>(METASOUND_GET_PARAM_NAME(InputEnabled), InParams.OperatorSettings);
            TDataReadReference<bool> InputBiPolar = InputData.GetOrCreateDefaultDataReadReference<bool>(METASOUND_GET_PARAM_NAME(InputBiPolar), InParams.OperatorSettings);

            return MakeUnique<FDustOperator>(InParams.OperatorSettings, InputDensity, InputDensityOffset, InputEnabled, InputBiPolar);
        }

        void Execute()
        {
        const float* DensityData = InputDensity->GetData();
        float* OutputDataPtr = OutputImpulse->GetData();
        int32 NumFrames = InputDensity->Num();
        float InputDensityOffsetValue = *InputDensityOffset;
        bool bEnabled = *InputEnabled;
        bool bBiPolar = *InputBiPolar;

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
                    if (bBiPolar)
                    {
                        OutputDataPtr[i] = SignalIsPositive ? 1.0f : -1.0f;
                        SignalIsPositive = !SignalIsPositive; 
                    }
                    else
                    {
                        OutputDataPtr[i] = 1.0f;
                    }
                }
                else
                {
                    OutputDataPtr[i] = 0.0f;
                }
            }
            else
            {
                OutputDataPtr[i] = 0.0f; // Output zero when disabled
            }
        }
    }

    private:

        // Inputs
        FAudioBufferReadRef InputDensity;
		FFloatReadRef InputDensityOffset;
        FBoolReadRef InputEnabled;
        FBoolReadRef InputBiPolar;

        // Outputs
        FAudioBufferWriteRef OutputImpulse;

        // Random number generator
        FRandomStream RNGStream;
        
        // Toggle flag for polarity
        bool SignalIsPositive;

        // Generate an initial seed for FRandomStream
        static int32 InitialSeed()
        {
            return FDateTime::UtcNow().GetTicks();
        }
    };

    
    class FDustNode : public FNodeFacade
    {
    public:
        FDustNode(const FNodeInitData& InitData)
            : FNodeFacade(InitData.InstanceName, InitData.InstanceID, TFacadeOperatorClass<FDustOperator>())
        {
        }
    };

    // Register node
    METASOUND_REGISTER_NODE(FDustNode);
}

#undef LOCTEXT_NAMESPACE