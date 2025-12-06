// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundSawtoothFMOperatorNode.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundPrimitives.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundStandardNodesNames.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/DateTime.h"
#include "MetasoundBranches/Public/MetasoundCommonMacros.h"

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_SawFMOperatorNode"

namespace Metasound::MetasoundBranches
{
    namespace SawFMOperatorNodeVertexNames
    {
        METASOUND_PARAM(InputEnabled, "Enabled", "Enable or disable oscillator.");
        METASOUND_PARAM(InputBipolar, "Bi Polar", "Output bipolar signal if true, unipolar if false.");
        METASOUND_PARAM(InputSync, "Sync", "Trigger to reset phase accumulator.");
        METASOUND_PARAM(InputFreq, "Frequency", "Base frequency in Hz.");
        METASOUND_PARAM(InputFreqMod, "Frequency Modulation", "Audio rate frequency modulation input.");
        METASOUND_PARAM(InputPhase, "Phase", "Audio rate phase offset input.");
        METASOUND_PARAM(InputFeedbackFloat, "Feedback Amount", "Feedback intensity (0-1).");
        METASOUND_PARAM(InputFeedbackAudio, "Feedback Modulation", "Audio-rate feedback modulation.");
        METASOUND_PARAM(OutputAudio, "Audio Out", "Generated audio output.");
    }
    class FSawFMOperatorOperator : public TExecutableOperator<FSawFMOperatorOperator>
    {
    public:
        FSawFMOperatorOperator(
            const FOperatorSettings& InSettings,
            const FBoolReadRef& InEnabled,
            const FBoolReadRef& InBipolar,
            const FTriggerReadRef& InSync,
            const FFloatReadRef& InFreq,
            const FAudioBufferReadRef& InFreqMod,
            const FAudioBufferReadRef& InPhase,
            const FFloatReadRef& InFeedbackFloat,
            const FAudioBufferReadRef& InFeedbackAudio)
            : InputEnabled(InEnabled)
            , InputBipolar(InBipolar)
            , InputSync(InSync)
            , InputFreq(InFreq)
            , InputFreqMod(InFreqMod)
            , InputPhase(InPhase)
            , InputFeedbackFloat(InFeedbackFloat)
            , InputFeedbackAudio(InFeedbackAudio)
            , OutputAudio(FAudioBufferWriteRef::CreateNew(InSettings))
            , FeedbackState(0.f)
            , PhaseAccumulator(0.f)
        {
        }

        static const FVertexInterface& DeclareVertexInterface()
        {
            using namespace SawFMOperatorNodeVertexNames;
            static const FVertexInterface Interface(
                FInputVertexInterface(
                    TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputEnabled), true),
                    TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputBipolar), true),
                    TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputSync)),
                    TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputFreq), 440.0f),
                    TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputFreqMod)),
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
                Metadata.ClassName = { TEXT("UE"), TEXT("FM Operator"), TEXT("Audio") };
                Metadata.MajorVersion = 1;
                Metadata.MinorVersion = 0;
                Metadata.DisplayName = METASOUND_LOCTEXT("SawFMOperatorNodeDisplayName", "FM Operator");
                Metadata.Description = METASOUND_LOCTEXT("SawFMOperatorNodeDesc", "Generate a sawtooth wave with frequency modulation, phase offset and feedback.");
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
            using namespace SawFMOperatorNodeVertexNames;
            FDataReferenceCollection Inputs;
            Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputEnabled), InputEnabled);
            Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputBipolar), InputBipolar);
            Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputSync), InputSync);
            Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputFreq), InputFreq);
            Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputFreqMod), InputFreqMod);
            Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputPhase), InputPhase);
            Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputFeedbackFloat), InputFeedbackFloat);
            Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputFeedbackAudio), InputFeedbackAudio);
            return Inputs;
        }

        virtual FDataReferenceCollection GetOutputs() const override
        {
            using namespace SawFMOperatorNodeVertexNames;
            FDataReferenceCollection Outputs;
            Outputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutputAudio), OutputAudio);
            return Outputs;
        }

        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
        {
            using namespace SawFMOperatorNodeVertexNames;
            const FInputVertexInterfaceData& InputData = InParams.InputData;
            TDataReadReference<bool> InEnabled = InputData.GetOrCreateDefaultDataReadReference<bool>(METASOUND_GET_PARAM_NAME(InputEnabled), InParams.OperatorSettings);
            TDataReadReference<bool> InBipolar = InputData.GetOrCreateDefaultDataReadReference<bool>(METASOUND_GET_PARAM_NAME(InputBipolar), InParams.OperatorSettings);
            TDataReadReference<FTrigger> InSync = InputData.GetOrCreateDefaultDataReadReference<FTrigger>(METASOUND_GET_PARAM_NAME(InputSync), InParams.OperatorSettings);
            TDataReadReference<float> InFreq = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputFreq), InParams.OperatorSettings);
            TDataReadReference<FAudioBuffer> InFreqMod = InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(InputFreqMod), InParams.OperatorSettings);
            TDataReadReference<FAudioBuffer> InPhase = InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(InputPhase), InParams.OperatorSettings);
            TDataReadReference<float> InFeedbackFloat = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputFeedbackFloat), InParams.OperatorSettings);
            TDataReadReference<FAudioBuffer> InFeedbackAudio = InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(InputFeedbackAudio), InParams.OperatorSettings);
            return MakeUnique<FSawFMOperatorOperator>(InParams.OperatorSettings, InEnabled, InBipolar, InSync, InFreq, InFreqMod, InPhase, InFeedbackFloat, InFeedbackAudio);
        }

        void Execute()
        {
            const int32 NumFrames = InputPhase->Num();
            if (InputSync->IsTriggered())
            {
                PhaseAccumulator = 0.f;
            }
            float* OutAudioData = OutputAudio->GetData();
            const float* PhaseData = InputPhase->GetData();
            const float* FreqModData = InputFreqMod->GetData();
            const float* FeedbackAudioData = InputFeedbackAudio->GetData();
            float FeedbackAmount = *InputFeedbackFloat;
            bool bEnabled = *InputEnabled;
            bool bBipolar = *InputBipolar;
            float BaseFreq = *InputFreq;
            for (int32 i = 0; i < NumFrames; ++i)
            {
                float FreqModValue = FreqModData[i];
                float PhaseInc = (BaseFreq + FreqModValue) / 48000.f;
                PhaseAccumulator += PhaseInc;
                float FeedbackSignal = FeedbackAmount + FeedbackAudioData[i];
                float CurrentPhase = PhaseAccumulator + PhaseData[i] + (FeedbackSignal * FeedbackState);
                float WrappedPhase = CurrentPhase - FMath::Floor(CurrentPhase);
                float SawValue = bBipolar ? (2.f * WrappedPhase - 1.f) : WrappedPhase;
                FeedbackState = SawValue;
                OutAudioData[i] = bEnabled ? SawValue : 0.f;
            }
        }

    private:
        FBoolReadRef InputEnabled;
        FBoolReadRef InputBipolar;
        FTriggerReadRef InputSync;
        FFloatReadRef InputFreq;
        FAudioBufferReadRef InputFreqMod;
        FAudioBufferReadRef InputPhase;
        FFloatReadRef InputFeedbackFloat;
        FAudioBufferReadRef InputFeedbackAudio;
        FAudioBufferWriteRef OutputAudio;
        float FeedbackState;
        float PhaseAccumulator;
    };

    class FSawFMOperatorNode : public FNodeFacade
    {
    public:
        FSawFMOperatorNode(const FNodeInitData& InInitData)
            : FNodeFacade(InInitData.InstanceName, InInitData.InstanceID, TFacadeOperatorClass<FSawFMOperatorOperator>())
        {
        }
    };
    
    METASOUND_REGISTER_NODE(FSawFMOperatorNode);
}

#undef LOCTEXT_NAMESPACE