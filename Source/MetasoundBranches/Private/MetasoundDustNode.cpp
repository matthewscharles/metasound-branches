#include "MetasoundBranches/Public/MetasoundDustNode.h"
#include "MetasoundExecutableOperator.h"     // TExecutableOperator class
#include "MetasoundPrimitives.h"             // ReadRef and WriteRef descriptions for bool, int32, float, and string
#include "MetasoundNodeRegistrationMacro.h"  // METASOUND_LOCTEXT and METASOUND_REGISTER_NODE macros
#include "MetasoundStandardNodesNames.h"     // StandardNodes namespace
#include "MetasoundFacade.h"                 // FNodeFacade class, eliminates the need for a fair amount of boilerplate code
#include "MetasoundParamHelper.h"            // METASOUND_PARAM and METASOUND_GET_PARAM family of macros
#include "Math/UnrealMathUtility.h"          // For FMath functions
#include "Misc/DateTime.h"                   // For FDateTime::UtcNow()

// Required for ensuring the node is supported by all languages in engine. Must be unique per MetaSound.
#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_DustNode"

namespace Metasound
{
    // Vertex Names - define the node's inputs and outputs here
    namespace DustNodeNames
    {
        METASOUND_PARAM(InputDensity, "Density", "Density control signal (bi-polar).");
    
        METASOUND_PARAM(OutputImpulse, "Output", "Impulse output signal.");
    }

    // Operator Class - defines the way the node is described, created and executed
    class FDustOperator : public TExecutableOperator<FDustOperator>
    {
    public:
        // Constructor
        FDustOperator(
            const FAudioBufferReadRef& InDensity)
            : InputDensity(InDensity)
            , OutputImpulse(FAudioBufferWriteRef::CreateNew(InDensity->Num()))
            , RNGStream(InitialSeed()) // Initialize with a seed
        {
        }

        // Helper function for constructing vertex interface
        static const FVertexInterface& DeclareVertexInterface()
        {
            using namespace DustNodeNames;

            static const FVertexInterface Interface(
                FInputVertexInterface(
                    TInputDataVertexModel<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputDensity))
                ),
                FOutputVertexInterface(
                    TOutputDataVertexModel<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputImpulse))
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

                    Metadata.ClassName = { StandardNodes::Namespace, TEXT("Dust"), StandardNodes::AudioVariant };
                    Metadata.MajorVersion = 1;
                    Metadata.MinorVersion = 0;
                    Metadata.DisplayName = METASOUND_LOCTEXT("DustNodeDisplayName", "Dust");
                    Metadata.Description = METASOUND_LOCTEXT("DustNodeDesc", "Generates randomly timed impulse events based on an audio density control signal.");
                    Metadata.Author = PluginAuthor;
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
            using namespace DustNodeNames;

            FDataReferenceCollection InputDataReferences;

            InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputDensity), InputDensity);

            return InputDataReferences;
        }

        // Allows MetaSound graph to interact with the node's outputs
        virtual FDataReferenceCollection GetOutputs() const override
        {
            using namespace DustNodeNames;

            FDataReferenceCollection OutputDataReferences;

            OutputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutputImpulse), OutputImpulse);

            return OutputDataReferences;
        }

        // Used to instantiate a new runtime instance of the node
        static TUniquePtr<IOperator> CreateOperator(const FCreateOperatorParams& InParams, FBuildErrorArray& OutErrors)
        {
            using namespace DustNodeNames;

            const Metasound::FDataReferenceCollection& InputCollection = InParams.InputDataReferences;
            const Metasound::FInputVertexInterface& InputInterface = DeclareVertexInterface().GetInputInterface();

            TDataReadReference<FAudioBuffer> InputDensity = InputCollection.GetDataReadReferenceOrConstructWithVertexDefault<FAudioBuffer>(InputInterface, METASOUND_GET_PARAM_NAME(InputDensity), InParams.OperatorSettings);

            return MakeUnique<FDustOperator>(InputDensity);
        }

        // Primary node functionality
        void Execute()
        {
            int32 NumFrames = InputDensity->Num();

            const float* DensityData = InputDensity->GetData();
            float* OutputDataPtr = OutputImpulse->GetData();

            for (int32 i = 0; i < NumFrames; ++i)
            {
                // Calculate threshold based on density
                float Density = DensityData[i];
                float AbsDensity = FMath::Abs(Density);
                float Threshold = 1.0f - AbsDensity * 0.0009f; // 1.0 - |density| * 0.0009

                // Generate random number between 0 and 1 using FRandomStream
                float RandomValue = RNGStream.GetFraction();

                // Compare to threshold
                if (RandomValue > Threshold)
                {
                    OutputDataPtr[i] = 1.0f; // Impulse
                }
                else
                {
                    OutputDataPtr[i] = 0.0f;
                }
            }
        }

    private:

        // Inputs
        FAudioBufferReadRef InputDensity;

        // Outputs
        FAudioBufferWriteRef OutputImpulse;

        // Random number generator
        FRandomStream RNGStream;

        // Generate an initial seed for FRandomStream
        static int32 InitialSeed()
        {
            return FDateTime::UtcNow().GetTicks();
        }
    };

    // Node Class - Inheriting from FNodeFacade is recommended for nodes that have a static FVertexInterface
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