// Copyright 2026 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundSineFMOscillatorNode.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundPrimitives.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundStandardNodesNames.h" // StandardNodes namespace
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/DateTime.h"
#include "MetasoundBranches/Public/MetasoundCommonMacros.h"

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_SineFMOscillatorNode"

namespace Metasound
{
    namespace SineFMOscillatorNodeVertexNames
    {
        METASOUND_PARAM(InputEnabled, "Enabled", "Enable or disable oscillator.");
        METASOUND_PARAM(InputBipolar, "Bi Polar", "Output bipolar signal if true, unipolar if false.");
        METASOUND_PARAM(InputPhase, "Phase", "Audio rate phase modulation input.");
        METASOUND_PARAM(InputFeedbackFloat, "Feedback Amount", "Feedback intensity (0-1).");
        METASOUND_PARAM(InputFeedbackAudio, "Feedback Modulation", "Audio-rate feedback modulation.");
        METASOUND_PARAM(OutputAudio, "Audio Out", "Generated audio output.");
    }

    class FSineFMOscillatorOperator : public TExecutableOperator<FSineFMOscillatorOperator>
    {
    public:
        FSineFMOscillatorOperator(
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
            using namespace SineFMOscillatorNodeVertexNames;
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

                Metadata.ClassName = { TEXT("UE"), TEXT("Function"), TEXT("Cosine") };
                Metadata.MajorVersion = 1;
                Metadata.MinorVersion = 0;
                Metadata.DisplayName = METASOUND_LOCTEXT("SineFMOscillatorNodeDisplayName", "Function (Cosine)");
                Metadata.Description = METASOUND_LOCTEXT("SineFMOscillatorNodeDesc", "Generate a consine wave with audio rate phase control and feedback.");
                Metadata.Author = "Charles Matthews";
                Metadata.PromptIfMissing = PluginNodeMissingPrompt;
                Metadata.DefaultInterface = DeclareVertexInterface();
                Metadata.CategoryHierarchy = {
                    METASOUND_LOCTEXT("Custom", "Branches"),
                    METASOUND_LOCTEXT("CustomSub", "Shapers")
                };
                Metadata.Keywords = TArray<FText>();

                METASOUND_BRANCHES_APPLY_NODE_STYLE(Metadata);

                return Metadata;
            };

            static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
            return Metadata;
        }
        
        METASOUND_DISABLE_LEGACY_IO()
        
        virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
        {
            using namespace SineFMOscillatorNodeVertexNames;
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputEnabled), InputEnabled);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputBipolar), InputBipolar);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputPhase), InputPhase);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputFeedbackFloat), InputFeedbackFloat);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputFeedbackAudio), InputFeedbackAudio);
        }
        
        virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
        {
            using namespace SineFMOscillatorNodeVertexNames;
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputAudio), OutputAudio);
        }

        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
        {
            using namespace SineFMOscillatorNodeVertexNames;
            const FInputVertexInterfaceData& InputData = InParams.InputData;
            TDataReadReference<bool> InEnabled = InputData.GetOrCreateDefaultDataReadReference<bool>(METASOUND_GET_PARAM_NAME(InputEnabled), InParams.OperatorSettings);
            TDataReadReference<bool> InBipolar = InputData.GetOrCreateDefaultDataReadReference<bool>(METASOUND_GET_PARAM_NAME(InputBipolar), InParams.OperatorSettings);
            TDataReadReference<FAudioBuffer> InPhase = InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(InputPhase), InParams.OperatorSettings);
            TDataReadReference<float> InFeedbackFloat = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputFeedbackFloat), InParams.OperatorSettings);
            TDataReadReference<FAudioBuffer> InFeedbackAudio = InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(InputFeedbackAudio), InParams.OperatorSettings);

            return MakeUnique<FSineFMOscillatorOperator>(InParams.OperatorSettings, InEnabled, InBipolar, InPhase, InFeedbackFloat, InFeedbackAudio);
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
            const float TwoPi = 2.f * PI;

            for (int32 i = 0; i < NumFrames; ++i)
            {
                float FeedbackSignal = FeedbackAmount + FeedbackAudioData[i];

                float FeedbackPhase = PhaseData[i] + (FeedbackSignal * FeedbackState);

                float CosineValue = FMath::Cos(FeedbackPhase * TwoPi);

                FeedbackState = CosineValue;

                OutAudioData[i] = bEnabled ? (bBipolar ? CosineValue : (CosineValue + 1.f) * 0.5f) : 0.f;
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

    class FSineFMOscillatorNode : public FNodeFacade
    {
    public:
                static FNodeClassMetadata CreateNodeClassMetadata()
        {
            return FSineFMOscillatorOperator::GetNodeInfo();
        }
        FSineFMOscillatorNode(FNodeData InInitData)
            : FNodeFacade(InInitData, MakeShared<const FNodeClassMetadata>(FSineFMOscillatorOperator::GetNodeInfo()), TFacadeOperatorClass<FSineFMOscillatorOperator>())
        {
        }
    };

    METASOUND_REGISTER_NODE(FSineFMOscillatorNode);
}

#undef LOCTEXT_NAMESPACE