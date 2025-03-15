// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundSineFMOscillatorNode.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundPrimitives.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundStandardNodesNames.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/DateTime.h"

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_SineFMOscillatorNode"


// translation-unit local (empty namespace)
namespace
{
    static bool bCosineWavetableInitialized = false;
    static constexpr int32 WavetableSize = 1024;
    static float CosineWavetable[WavetableSize]; 

    static void GenerateWavetable()
    {
        if (!bCosineWavetableInitialized)
        {
            for (int32 i = 0; i < WavetableSize; ++i)
            {
                float Phase = (static_cast<float>(i) / WavetableSize) * TWO_PI;
                CosineWavetable[i] = FMath::Cos(Phase);
            }
            bCosineWavetableInitialized = true;
        }
    }
}

namespace Metasound
{
    namespace SineFMOscillatorNodeVertexNames
    {
        METASOUND_PARAM(InputEnabled, "Enabled", "Enable or disable oscillator.");
        METASOUND_PARAM(InputBipolar, "Bi Polar", "Output bipolar signal if true, unipolar if false.");
        METASOUND_PARAM(InputPhase, "Phase", "Audio rate phase modulation input.");
        METASOUND_PARAM(OutputAudio, "Audio Out", "Generated audio output.");
    }

    class FSineFMOscillatorOperator : public TExecutableOperator<FSineFMOscillatorOperator>
    {
    public:
        FSineFMOscillatorOperator(
            const FOperatorSettings& InSettings,
            const FBoolReadRef& InEnabled,
            const FBoolReadRef& InBipolar,
            const FAudioBufferReadRef& InPhase)
            : InputEnabled(InEnabled)
            , InputBipolar(InBipolar)
            , InputPhase(InPhase)
            , OutputAudio(FAudioBufferWriteRef::CreateNew(InSettings))
        {
			GenerateWavetable();
        }

        static const FVertexInterface& DeclareVertexInterface()
        {
            using namespace SineFMOscillatorNodeVertexNames;
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

                Metadata.ClassName = { TEXT("UE"), TEXT("FM Oscillator"), TEXT("Sine") };
                Metadata.MajorVersion = 1;
                Metadata.MinorVersion = 0;
                Metadata.DisplayName = METASOUND_LOCTEXT("SineFMOscillatorNodeDisplayName", "FM Oscillator (Sine)");
                Metadata.Description = METASOUND_LOCTEXT("SineFMOscillatorNodeDesc", "Generate a cosine wave oscillator with phase-based FM.");
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
            using namespace SineFMOscillatorNodeVertexNames;
            FDataReferenceCollection Inputs;
            Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputEnabled), InputEnabled);
            Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputBipolar), InputBipolar);
            Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputPhase), InputPhase);
            return Inputs;
        }

        virtual FDataReferenceCollection GetOutputs() const override
        {
            using namespace SineFMOscillatorNodeVertexNames;
            FDataReferenceCollection Outputs;
            Outputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutputAudio), OutputAudio);
            return Outputs;
        }

        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
        {
            using namespace SineFMOscillatorNodeVertexNames;
            const FInputVertexInterfaceData& InputData = InParams.InputData;
            TDataReadReference<bool> InEnabled = InputData.GetOrCreateDefaultDataReadReference<bool>(METASOUND_GET_PARAM_NAME(InputEnabled), InParams.OperatorSettings);
            TDataReadReference<bool> InBipolar = InputData.GetOrCreateDefaultDataReadReference<bool>(METASOUND_GET_PARAM_NAME(InputBipolar), InParams.OperatorSettings);
            TDataReadReference<FAudioBuffer> InPhase = InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(InputPhase), InParams.OperatorSettings);

            return MakeUnique<FSineFMOscillatorOperator>(InParams.OperatorSettings, InEnabled, InBipolar, InPhase);
        }

        void Execute()
		{
			const int32 NumFrames = InputPhase->Num();
			float* OutAudioData = OutputAudio->GetData();
			const float* PhaseData = InputPhase->GetData();
			bool bEnabled = *InputEnabled;
			bool bBipolar = *InputBipolar;

			// if (!bCosineWavetableInitialized) return;

			for (int32 i = 0; i < NumFrames; ++i)
			{
				float PhaseIndexF = PhaseData[i] * static_cast<float>(WavetableSize);
				int32 PhaseIndex = static_cast<int32>(PhaseIndexF) % WavetableSize;

				// int32 IndexA = PhaseIndex;
				// int32 IndexB = (IndexA + 1) % WavetableSize;
				// float Fraction = PhaseIndexF - static_cast<float>(IndexA);
				// float CosVal = FMath::Lerp(CosineWavetable[IndexA], CosineWavetable[IndexB], Fraction);

				float CosVal = CosineWavetable[PhaseIndex];

				OutAudioData[i] = bEnabled ? (bBipolar ? CosVal : (CosVal + 1.f) * 0.5f) : 0.f;
			}
		}

    private:
        FBoolReadRef InputEnabled;
        FBoolReadRef InputBipolar;
        FAudioBufferReadRef InputPhase;
        FAudioBufferWriteRef OutputAudio;
    };

    class FSineFMOscillatorNode : public FNodeFacade
    {
    public:
        FSineFMOscillatorNode(const FNodeInitData& InInitData)
            : FNodeFacade(InInitData.InstanceName, InInitData.InstanceID, TFacadeOperatorClass<FSineFMOscillatorOperator>())
        {
        }
    };

    METASOUND_REGISTER_NODE(FSineFMOscillatorNode);
}

#undef LOCTEXT_NAMESPACE