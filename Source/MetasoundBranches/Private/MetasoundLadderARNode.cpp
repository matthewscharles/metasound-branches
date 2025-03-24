// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundLadderARNode.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundPrimitives.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"
#include "Math/UnrealMathUtility.h"
#include "DSP/Filter.h"

#define LOCTEXT_NAMESPACE "MetasoundLadderARNode"

namespace Metasound
{
    namespace LadderArVertexNames
    {
        METASOUND_PARAM(InputSignal,        "In",         "Audio input to the ladder filter.");
        METASOUND_PARAM(InputCutoff,        "Cutoff",     "Base cutoff frequency.");
        METASOUND_PARAM(InputResonance,     "Resonance",  "Base resonance.");
        METASOUND_PARAM(InputCutoffMod,     "Cutoff Modulation", "Audio-rate modulation signal for cutoff frequency.");
        METASOUND_PARAM(InputResonanceMod,  "Resonance Modulation",    "Audio-rate modulation signal for resonance.");
        METASOUND_PARAM(OutputSignal,       "Out",        "Filtered audio output.");
    }

    class FLadderArOperator : public TExecutableOperator<FLadderArOperator>
    {
    public:
        FLadderArOperator(
            const FOperatorSettings& InSettings,
            const FAudioBufferReadRef& InAudioInput,
            const FFloatReadRef& InCutoff,
            const FFloatReadRef& InResonance,
            const FAudioBufferReadRef& InCutoffMod,
            const FAudioBufferReadRef& InResonanceMod
        )
            : AudioInput(InAudioInput)
            , BaseCutoff(InCutoff)
            , BaseResonance(InResonance)
            , CutoffMod(InCutoffMod)
            , ResonanceMod(InResonanceMod)
            , AudioOutput(FAudioBufferWriteRef::CreateNew(InSettings))
            , BlockSize(InSettings.GetNumFramesPerBlock())
            , SampleRate(InSettings.GetSampleRate())
            , MaxCutoffFrequency(0.5f * SampleRate)
        {
            LadderFilter.Init(SampleRate, 1);
            check(AudioOutput->Num() == BlockSize);
        }

        static const FVertexInterface& DeclareVertexInterface()
        {
            using namespace LadderArVertexNames;

            static const FVertexInterface Interface(
                FInputVertexInterface(
                    TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputSignal)),
                    TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputCutoff)),
                    TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputResonance)),
                    TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputCutoffMod)),
                    TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputResonanceMod))
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
                FNodeClassMetadata Metadata;
                Metadata.ClassName = { TEXT("UE"), TEXT("Ladder (AR)"), TEXT("Audio") };
                Metadata.MajorVersion = 1;
                Metadata.MinorVersion = 0;
                Metadata.DisplayName = LOCTEXT("LadderArDisplayName", "Ladder (AR)");
                Metadata.Description = LOCTEXT("LadderArDesc", "Ladder filter with audio-rate modulation for cutoff and resonance.");
                Metadata.Author = TEXT("Charles Matthews");
                Metadata.PromptIfMissing = PluginNodeMissingPrompt;
                Metadata.DefaultInterface = DeclareVertexInterface();
                Metadata.CategoryHierarchy = { LOCTEXT("CustomCategory", "Branches") };
                return Metadata;
            };

            static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
            return Metadata;
        }

        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
        {
            using namespace LadderArVertexNames;

            const FInputVertexInterfaceData& InputData = InParams.InputData;

            TDataReadReference<FAudioBuffer> InputSignal = InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(
                METASOUND_GET_PARAM_NAME(InputSignal), InParams.OperatorSettings);

            TDataReadReference<float> BaseCutoff = InputData.GetOrCreateDefaultDataReadReference<float>(
                METASOUND_GET_PARAM_NAME(InputCutoff), InParams.OperatorSettings);

            TDataReadReference<float> BaseResonance = InputData.GetOrCreateDefaultDataReadReference<float>(
                METASOUND_GET_PARAM_NAME(InputResonance), InParams.OperatorSettings);

            TDataReadReference<FAudioBuffer> CutoffMod = InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(
                METASOUND_GET_PARAM_NAME(InputCutoffMod), InParams.OperatorSettings);

            TDataReadReference<FAudioBuffer> ResMod = InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(
                METASOUND_GET_PARAM_NAME(InputResonanceMod), InParams.OperatorSettings);

            return MakeUnique<FLadderArOperator>(
                InParams.OperatorSettings,
                InputSignal,
                BaseCutoff,
                BaseResonance,
                CutoffMod,
                ResMod
            );
        }
        
        METASOUND_DISABLE_LEGACY_IO()
        
        virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
        {
            using namespace LadderArVertexNames;

            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputSignal), AudioInput);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputCutoff), BaseCutoff);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputResonance), BaseResonance);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputCutoffMod), CutoffMod);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputResonanceMod), ResonanceMod);
        }
        
        virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
        {
            using namespace LadderArVertexNames;

            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputSignal), AudioOutput);
        }

        virtual void Execute()
        {
            const float* InAudio        = AudioInput->GetData();
            const float* CutoffModData  = CutoffMod->GetData();
            const float* ResModData     = ResonanceMod->GetData();
            float* OutAudio             = AudioOutput->GetData();

            const int32 NumFrames = AudioInput->Num();

            for (int32 i = 0; i < NumFrames; ++i)
            {
                float CutoffVal = (*BaseCutoff) + CutoffModData[i];
                CutoffVal = FMath::Clamp(CutoffVal, 0.0f, MaxCutoffFrequency);

                float ResonanceVal = (*BaseResonance) + ResModData[i];
                ResonanceVal = FMath::Clamp(ResonanceVal, 1.0f, 10.0f);

                LadderFilter.SetFrequency(CutoffVal);
                LadderFilter.SetQ(ResonanceVal);
                LadderFilter.Update();

                float SingleInSample = InAudio[i];
                float SingleOutSample = 0.0f;

                LadderFilter.ProcessAudio(&SingleInSample, 1 /* NumSamples */, &SingleOutSample);

                OutAudio[i] = SingleOutSample;
            }
        }

    private:
        // Inputs
        FAudioBufferReadRef  AudioInput;
        FFloatReadRef        BaseCutoff;
        FFloatReadRef        BaseResonance;
        FAudioBufferReadRef  CutoffMod;
        FAudioBufferReadRef  ResonanceMod;

        // Output
        FAudioBufferWriteRef AudioOutput;

        const int32 BlockSize;
        const float SampleRate;
        const float MaxCutoffFrequency;
        Audio::FLadderFilter LadderFilter;
    };

    class FLadderArNode : public FNodeFacade
    {
    public:
        FLadderArNode(const FNodeInitData& InitData)
            : FNodeFacade(
                InitData.InstanceName,
                InitData.InstanceID,
                TFacadeOperatorClass<FLadderArOperator>()
              )
        {
        }
    };

    METASOUND_REGISTER_NODE(FLadderArNode);
}

#undef LOCTEXT_NAMESPACE