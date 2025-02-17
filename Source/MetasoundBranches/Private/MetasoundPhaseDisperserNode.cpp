// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundPhaseDisperserNode.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundPrimitives.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"
#include "Math/UnrealMathUtility.h"

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_PhaseDisperserNode"

namespace Metasound
{
    namespace PhaseDisperserNodeVertexNames
    {
        METASOUND_PARAM(InputSignal, "In", "Incoming audio.");
        METASOUND_PARAM(OutputSignal, "Out", "Phase-dispersed audio.");

        METASOUND_PARAM(NumFilters, "Stages", "Number of allpass filter stages to apply (maximum 128).");
    }

    class FPhaseDisperserOperator : public TExecutableOperator<FPhaseDisperserOperator>
    {
    public:
        // Maximum number of allowed allpass filters
        static constexpr int32 MaxAllowedFilters = 128;

        FPhaseDisperserOperator(const FAudioBufferReadRef& InSignal, const TDataReadReference<int32>& InNumFilters)
            : InputSignal(InSignal)
            , NumFilters(InNumFilters)
            , OutputSignal(FAudioBufferWriteRef::CreateNew(InSignal->Num()))
        {
            AllPassFilters.SetNum(MaxAllowedFilters);
            for (int32 i = 0; i < MaxAllowedFilters; ++i)
            {
                AllPassFilters[i].Init();
            }
        }

        static const FVertexInterface& DeclareVertexInterface()
        {
            using namespace PhaseDisperserNodeVertexNames;

            static const FVertexInterface Interface(
                FInputVertexInterface(
                    TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputSignal)),
                    TInputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(NumFilters)) 
                ),
                FOutputVertexInterface(
                    TOutputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputSignal))
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
                Metadata.ClassName = { TEXT("UE"), TEXT("PhaseDisperser"), TEXT("Audio") };
                Metadata.MajorVersion = 1;
                Metadata.MinorVersion = 0;
                Metadata.DisplayName = METASOUND_LOCTEXT("PhaseDisperserNodeDisplayName", "Phase Disperser");
                Metadata.Description = METASOUND_LOCTEXT("PhaseDisperserNodeDesc", "A chain of allpass filters acting as a phase disperser to soften transients.");
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
            using namespace PhaseDisperserNodeVertexNames;

            FDataReferenceCollection InputDataReferences;
            InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputSignal), InputSignal);
            InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(NumFilters), NumFilters);

            return InputDataReferences;
        }

        virtual FDataReferenceCollection GetOutputs() const override
        {
            using namespace PhaseDisperserNodeVertexNames;

            FDataReferenceCollection OutputDataReferences;
            OutputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutputSignal), OutputSignal);

            return OutputDataReferences;
        }

        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
        {
            using namespace PhaseDisperserNodeVertexNames;

            const FInputVertexInterfaceData& InputData = InParams.InputData;
            const FInputVertexInterface& InputInterface = DeclareVertexInterface().GetInputInterface();

            TDataReadReference<FAudioBuffer> InputSignal = InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(
                METASOUND_GET_PARAM_NAME(InputSignal), InParams.OperatorSettings);

            TDataReadReference<int32> NumFiltersRef = InputData.GetOrCreateDefaultDataReadReference<int32>(
                METASOUND_GET_PARAM_NAME(NumFilters), InParams.OperatorSettings);

            int32 ClampedNumFilters = FMath::Clamp(*NumFiltersRef, 1, MaxAllowedFilters);
          
            return MakeUnique<FPhaseDisperserOperator>(InputSignal, NumFiltersRef);
        }

        void Execute()
        {
            int32 NumFrames = InputSignal->Num();
            const float* InputData = InputSignal->GetData();
            float* OutputData = OutputSignal->GetData();

            // Temporary buffer to hold intermediate results
            TArray<float> TempBuffer;
            TempBuffer.SetNumZeroed(NumFrames);

            // Initial copy of input to temp buffer
            FMemory::Memcpy(TempBuffer.GetData(), InputData, NumFrames * sizeof(float));

            int32 CurrentNumFilters = FMath::Clamp(*NumFilters, 1, MaxAllowedFilters);

            for (int32 i = 0; i < CurrentNumFilters; ++i)
            {
                AllPassFilters[i].ProcessBuffer(TempBuffer.GetData(), NumFrames);
            }

            FMemory::Memcpy(OutputData, TempBuffer.GetData(), NumFrames * sizeof(float));
        }

    private:
        class FAllPassFilter
        {
        public:
            void Init(float InFeedback = 0.5f)
            {
                DelayBuffer.SetNumZeroed(2); // For D = 1
                WriteIndex = 0;
                Feedback = InFeedback;
            }

            void ProcessBuffer(float* InOutBuffer, int32 NumSamples)
            {
                for (int32 i = 0; i < NumSamples; ++i)
                {
                    float InSample = InOutBuffer[i];
                    float DelayedSample = DelayBuffer[WriteIndex];

                    // Allpass difference equation: y[n] = -a * x[n] + x[n-D] + a * y[n-D]
                    float OutSample = -Feedback * InSample + DelayedSample;
                    DelayBuffer[WriteIndex] = InSample + Feedback * OutSample;

                    InOutBuffer[i] = OutSample;

                    // Update write index for D = 1
                    WriteIndex = (WriteIndex + 1) % 2;
                }
            }

        private:
            TArray<float> DelayBuffer;
            int32 WriteIndex;
            float Feedback;
        };

        // Inputs
        FAudioBufferReadRef InputSignal;
        FInt32ReadRef NumFilters;

        // Outputs
        FAudioBufferWriteRef OutputSignal;

        // Allpass filters
        TArray<FAllPassFilter> AllPassFilters;
    };

    class FPhaseDisperserNode : public FNodeFacade
    {
    public:
        FPhaseDisperserNode(const FNodeInitData& InitData)
            : FNodeFacade(InitData.InstanceName, InitData.InstanceID, TFacadeOperatorClass<FPhaseDisperserOperator>())
        {
        }
    };

    METASOUND_REGISTER_NODE(FPhaseDisperserNode);
}

#undef LOCTEXT_NAMESPACE