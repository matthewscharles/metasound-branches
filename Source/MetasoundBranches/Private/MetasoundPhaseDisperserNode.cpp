// Copyright 2026 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundPhaseDisperserNode.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundPrimitives.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"
#include "Math/UnrealMathUtility.h"
#include "MetasoundBranches/Public/MetasoundCommonMacros.h"

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_PhaseDisperserNode"

namespace Metasound
{
    namespace PhaseDisperserNodeVertexNames
    {
        METASOUND_PARAM(InputSignal, "In", "Incoming audio.");
        METASOUND_PARAM(OutputSignal, "Out", "Phase-dispersed audio.");
        METASOUND_PARAM(NumFilters, "Stages", "Number of allpass filter stages to apply.");
        METASOUND_PARAM(MaximumStages, "Maximum Stages", "Upper bound on the number of filters. Evaluated once on instantiation.");
    }

    class FPhaseDisperserOperator : public TExecutableOperator<FPhaseDisperserOperator>
    {
    public:
        FPhaseDisperserOperator(
            const FOperatorSettings& InSettings,
            const FAudioBufferReadRef& InSignal,
            const FInt32ReadRef& InNumFilters,
            const int32 InMaxStages)
            : InputSignal(InSignal)
            , NumFilters(InNumFilters)
            , OutputSignal(FAudioBufferWriteRef::CreateNew(InSettings))
            , MaxStages(FMath::Clamp(InMaxStages, 1, 1024)) // Clamp here to protect memory
        {
            // Allocate filter array up to MaxStages
            AllPassFilters.SetNum(MaxStages);
            for (int32 i = 0; i < MaxStages; ++i)
            {
                AllPassFilters[i].Init();
            }

            // Preallocate temp buffer
            TempBuffer.SetNumUninitialized(InSettings.GetNumFramesPerBlock());
        }

        static const FVertexInterface& DeclareVertexInterface()
        {
            using namespace PhaseDisperserNodeVertexNames;

            static const FVertexInterface Interface(
                FInputVertexInterface(
                    TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputSignal)),
                    TInputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(NumFilters), 16),
                    TInputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(MaximumStages), 128)
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
                Metadata.MinorVersion = 1;
                Metadata.DisplayName = METASOUND_LOCTEXT("PhaseDisperserNodeDisplayName", "Phase Disperser");
                Metadata.Description = METASOUND_LOCTEXT("PhaseDisperserNodeDesc", "A chain of allpass filters acting as a phase disperser to soften transients.");
                Metadata.Author = "Charles Matthews";
                Metadata.PromptIfMissing = PluginNodeMissingPrompt;
                Metadata.DefaultInterface = NodeInterface;
                Metadata.CategoryHierarchy = {
                    METASOUND_LOCTEXT("Custom", "Branches"),
                    METASOUND_LOCTEXT("CustomSub", "Filters")
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
            using namespace PhaseDisperserNodeVertexNames;
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputSignal), InputSignal);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(NumFilters), NumFilters);
        }

        virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
        {
            using namespace PhaseDisperserNodeVertexNames;
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputSignal), OutputSignal);
        }

        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
        {
            using namespace PhaseDisperserNodeVertexNames;

            const FInputVertexInterfaceData& InputData = InParams.InputData;

            TDataReadReference<FAudioBuffer> InputSignal = InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(
                METASOUND_GET_PARAM_NAME(InputSignal), InParams.OperatorSettings);

            TDataReadReference<int32> NumFiltersRef = InputData.GetOrCreateDefaultDataReadReference<int32>(
                METASOUND_GET_PARAM_NAME(NumFilters), InParams.OperatorSettings);

            TDataReadReference<int32> MaxStagesRef = InputData.GetOrCreateDefaultDataReadReference<int32>(
                METASOUND_GET_PARAM_NAME(MaximumStages), InParams.OperatorSettings);

            return MakeUnique<FPhaseDisperserOperator>(InParams.OperatorSettings, InputSignal, NumFiltersRef, *MaxStagesRef);
        }

        void Execute()
        {
            const int32 NumFrames = InputSignal->Num();
            const float* InputData = InputSignal->GetData();
            float* OutputData = OutputSignal->GetData();

            FMemory::Memcpy(TempBuffer.GetData(), InputData, NumFrames * sizeof(float));

            int32 CurrentNumFilters = FMath::Clamp(*NumFilters, 1, MaxStages);

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
                DelayBuffer.SetNumZeroed(2); // D = 1
                WriteIndex = 0;
                Feedback = InFeedback;
            }

            void ProcessBuffer(float* InOutBuffer, int32 NumSamples)
            {
                for (int32 i = 0; i < NumSamples; ++i)
                {
                    float InSample = InOutBuffer[i];
                    float DelayedSample = DelayBuffer[WriteIndex];

                    float OutSample = -Feedback * InSample + DelayedSample;
                    DelayBuffer[WriteIndex] = InSample + Feedback * OutSample;

                    InOutBuffer[i] = OutSample;
                    WriteIndex = (WriteIndex + 1) % 2;
                }
            }

        private:
            TArray<float> DelayBuffer;
            int32 WriteIndex = 0;
            float Feedback = 0.5f;
        };

        // Inputs
        FAudioBufferReadRef InputSignal;
        FInt32ReadRef NumFilters;

        // Outputs
        FAudioBufferWriteRef OutputSignal;

        // Internal state
        int32 MaxStages;
        TArray<FAllPassFilter> AllPassFilters;
        TArray<float> TempBuffer;
    };

    class FPhaseDisperserNode : public FNodeFacade
    {
    public:
                static FNodeClassMetadata CreateNodeClassMetadata()
        {
            return FPhaseDisperserOperator::GetNodeInfo();
        }
        FPhaseDisperserNode(FNodeData InitData)
            : FNodeFacade(InitData, MakeShared<const FNodeClassMetadata>(FPhaseDisperserOperator::GetNodeInfo()), TFacadeOperatorClass<FPhaseDisperserOperator>())
        {
        }
    };

    METASOUND_REGISTER_NODE(FPhaseDisperserNode);
}

#undef LOCTEXT_NAMESPACE