// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundSawtoothFMOscillatorNode.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundPrimitives.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundStandardNodesNames.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/DateTime.h"

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_SawFMOscillatorNode"

namespace Metasound
{
    namespace SawFMOscillatorNodeVertexNames
    {
        METASOUND_PARAM(InputEnabled, "Enabled", "Enable or disable oscillator.");
        METASOUND_PARAM(InputBipolar, "Bi Polar", "Output bipolar signal if true, unipolar if false.");
        METASOUND_PARAM(InputPhase, "Phase", "Audio rate phase modulation input.");
        METASOUND_PARAM(InputFeedbackFloat, "Feedback Amount", "Feedback intensity (0-1).");
        METASOUND_PARAM(InputFeedbackAudio, "Feedback Modulation", "Audio-rate feedback modulation.");
        METASOUND_PARAM(OutputAudio, "Audio Out", "Generated audio output.");
    }

    class FSawFMOscillatorOperator : public TExecutableOperator<FSawFMOscillatorOperator>
    {
    public:
        FSawFMOscillatorOperator(
            const FOperatorSettings& InSettings,
            const FBoolReadRef& InEnabled,
            const FBoolReadRef& InBipolar,
            const FAudioBufferReadRef& InPhase,
            const FFloatReadRef& InFeedbackFloat,
            const FAudioBufferReadRef& InFeedbackAudio)
            : InputEnabled(InEnabled)
            , InputBipolar(InBipolar)
            , InputPhase(InPhase)
            , InputFeedbackFloat(InFeedbackFloat)
            , InputFeedbackAudio(InFeedbackAudio)
            , OutputAudio(FAudioBufferWriteRef::CreateNew(InSettings))
            , FeedbackState(0.f)
        {
        }

        static const FVertexInterface& DeclareVertexInterface()
        {
            using namespace SawFMOscillatorNodeVertexNames;
            static const FVertexInterface Interface(
                FInputVertexInterface(
                    TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputEnabled), true),
                    TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputBipolar), true),
                    TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputPhase)),
                    TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputFeedbackFloat), 0.0f),
                    TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputFeedbackAudio))
                ),
                FOutputVertexInterface(
                    TOutputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputAudio))
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

                Metadata.ClassName = { TEXT("UE"), TEXT("FM Oscillator"), TEXT("Sawtooth") };
                Metadata.MajorVersion = 1;
                Metadata.MinorVersion = 0;
                Metadata.DisplayName = METASOUND_LOCTEXT("SawFMOscillatorNodeDisplayName", "FM Oscillator (Sawtooth)");
                Metadata.Description = METASOUND_LOCTEXT("SawFMOscillatorNodeDesc", "Generate a sawtooth wave oscillator with phase-based FM and feedback.");
                Metadata.Author = "Charles Matthews";
                Metadata.PromptIfMissing = PluginNodeMissingPrompt;
                Metadata.DefaultInterface = DeclareVertexInterface();
                Metadata.CategoryHierarchy = {
                    METASOUND_LOCTEXT("Custom", "Branches"),
                    METASOUND_LOCTEXT("CustomSub", "Shapers")
                };
                Metadata.Keywords = TArray<FText>();

                return Metadata;
            };

            static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
            return Metadata;
        }

        virtual FDataReferenceCollection GetInputs() const override
        {
            using namespace SawFMOscillatorNodeVertexNames;
            FDataReferenceCollection Inputs;
            Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputEnabled), InputEnabled);
            Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputBipolar), InputBipolar);
            Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputPhase), InputPhase);
            Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputFeedbackFloat), InputFeedbackFloat);
            Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputFeedbackAudio), InputFeedbackAudio);
            return Inputs;
        }

        virtual FDataReferenceCollection GetOutputs() const override
        {
            using namespace SawFMOscillatorNodeVertexNames;
            FDataReferenceCollection Outputs;
            Outputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutputAudio), OutputAudio);
            return Outputs;
        }

        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
        {
            using namespace SawFMOscillatorNodeVertexNames;
            const FInputVertexInterfaceData& InputData = InParams.InputData;
            TDataReadReference<bool> InEnabled = InputData.GetOrCreateDefaultDataReadReference<bool>(METASOUND_GET_PARAM_NAME(InputEnabled), InParams.OperatorSettings);
            TDataReadReference<bool> InBipolar = InputData.GetOrCreateDefaultDataReadReference<bool>(METASOUND_GET_PARAM_NAME(InputBipolar), InParams.OperatorSettings);
            TDataReadReference<FAudioBuffer> InPhase = InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(InputPhase), InParams.OperatorSettings);
            TDataReadReference<float> InFeedbackFloat = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputFeedbackFloat), InParams.OperatorSettings);
            TDataReadReference<FAudioBuffer> InFeedbackAudio = InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(InputFeedbackAudio), InParams.OperatorSettings);

            return MakeUnique<FSawFMOscillatorOperator>(InParams.OperatorSettings, InEnabled, InBipolar, InPhase, InFeedbackFloat, InFeedbackAudio);
        }

        void Execute()
        {
            const int32 NumFrames = InputPhase->Num();
            float* OutAudioData = OutputAudio->GetData();
            const float* PhaseData = InputPhase->GetData();
            const float* FeedbackAudioData = InputFeedbackAudio->GetData();
            float FeedbackAmount = *InputFeedbackFloat;
            bool bEnabled = *InputEnabled;
            bool bBipolar = *InputBipolar;

            for (int32 i = 0; i < NumFrames; ++i)
            {
                float FeedbackSignal = FeedbackAmount + FeedbackAudioData[i];

                float FeedbackPhase = PhaseData[i] + (FeedbackSignal * FeedbackState);

                float WrappedPhase = FeedbackPhase - FMath::Floor(FeedbackPhase);

                float SawValue = bBipolar ? (2.f * WrappedPhase - 1.f) : WrappedPhase;

                // Store current output for next iteration feedback
                FeedbackState = SawValue;

                OutAudioData[i] = bEnabled ? SawValue : 0.f;
            }
        }

    private:
        FBoolReadRef InputEnabled;
        FBoolReadRef InputBipolar;
        FAudioBufferReadRef InputPhase;
        FFloatReadRef InputFeedbackFloat;
        FAudioBufferReadRef InputFeedbackAudio;
        FAudioBufferWriteRef OutputAudio;
        float FeedbackState;
    };

    class FSawFMOscillatorNode : public FNodeFacade
    {
    public:
        FSawFMOscillatorNode(const FNodeInitData& InInitData)
            : FNodeFacade(InInitData.InstanceName, InInitData.InstanceID, TFacadeOperatorClass<FSawFMOscillatorOperator>())
        {
        }
    };

    METASOUND_REGISTER_NODE(FSawFMOscillatorNode);
}

#undef LOCTEXT_NAMESPACE