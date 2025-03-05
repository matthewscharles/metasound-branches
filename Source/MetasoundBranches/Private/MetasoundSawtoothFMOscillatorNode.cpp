// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundSawtoothFMOscillatorNode.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundPrimitives.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundStandardNodesNames.h" // StandardNodes namespace
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
        METASOUND_PARAM(OutputAudio, "Audio Out", "Generated audio output.");
    }

    class FSawFMOscillatorOperator : public TExecutableOperator<FSawFMOscillatorOperator>
    {
    public:
        FSawFMOscillatorOperator(
            const FOperatorSettings& InSettings,
            const FBoolReadRef& InEnabled,
            const FBoolReadRef& InBipolar,
            const FAudioBufferReadRef& InPhase)
            : InputEnabled(InEnabled)
            , InputBipolar(InBipolar)
            , InputPhase(InPhase)
            , OutputAudio(FAudioBufferWriteRef::CreateNew(InSettings))
        {
        }

        static const FVertexInterface& DeclareVertexInterface()
        {
            using namespace SawFMOscillatorNodeVertexNames;
            static const FVertexInterface Interface(
                FInputVertexInterface(
                    TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputEnabled), true),
                    TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputBipolar), true),
                    TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputPhase))
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
                Metadata.Description = METASOUND_LOCTEXT("SawFMOscillatorNodeDesc", "Generate a sawtooth wave oscillator with phase-based FM.");
                Metadata.Author = "Charles Matthews";
                Metadata.PromptIfMissing = PluginNodeMissingPrompt;
                Metadata.DefaultInterface = DeclareVertexInterface();
                Metadata.CategoryHierarchy = { METASOUND_LOCTEXT("Custom", "Branches") };
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

            return MakeUnique<FSawFMOscillatorOperator>(InParams.OperatorSettings, InEnabled, InBipolar, InPhase);
        }

        void Execute()
        {
            const int32 NumFrames = InputPhase->Num();
            float* OutAudioData = OutputAudio->GetData();
            const float* PhaseData = InputPhase->GetData();
            bool bEnabled = *InputEnabled;
            bool bBipolar = *InputBipolar;

            for (int32 i = 0; i < NumFrames; ++i)
            {
                float WrappedPhase = PhaseData[i] - FMath::Floor(PhaseData[i]);

                float SawValue = bBipolar ? (2.f * WrappedPhase - 1.f) : WrappedPhase; 

                OutAudioData[i] = bEnabled ? SawValue : 0.f;
            }
        }

    private:
        FBoolReadRef InputEnabled;
        FBoolReadRef InputBipolar;
        FAudioBufferReadRef InputPhase;
        FAudioBufferWriteRef OutputAudio;
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