// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundLowPassARNode.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundPrimitives.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"
#include "Math/UnrealMathUtility.h"

#define LOCTEXT_NAMESPACE "MetasoundLowPassARNode"

namespace Metasound
{
    class FLowPassArFilter
    {
    public:
        void Init(float InSampleRate)
        {
            SampleRate = InSampleRate;
            Reset();
        }
    
        void Reset()
        {
            FMemory::Memzero(Stage, sizeof(Stage));
            Frequency = 1000.0f;
            Resonance = 1.0f;
            UpdateCoefficients();
        }
    
        void SetFrequency(float InFrequency)
        {
            Frequency = FMath::Clamp(InFrequency, 20.0f, 0.45f * SampleRate);
            UpdateCoefficients();
        }
    
        void SetResonance(float InResonance)
        {
            Resonance = FMath::Clamp(InResonance, 0.0f, 10.0f);
            ScaledResonance = 4.0f * (Resonance / 10.0f);
        }
    
        float Process(float InSample)
        {
            float Feedback = ScaledResonance * (Stage[3]); 
            float Input = InSample - Feedback;
    
            for (int i = 0; i < 4; ++i)
            {
                Stage[i] += Alpha * (Input - Stage[i]);
                Input = Stage[i];
            }
    
            return Stage[3];
        }
    
    private:
        void UpdateCoefficients()
        {
            float x = Frequency / SampleRate;
            Alpha = (3.14159265359f * x) / (1.0f + (3.14159265359f * x));
        }
    
        float SampleRate;
        float Frequency;
        float Resonance;
        float ScaledResonance;
        float Alpha;
        float Stage[4];
    };

    namespace LowPassArVertexNames
    {
        METASOUND_PARAM(InputSignal,        "In",         "Audio input to the filter.");
        METASOUND_PARAM(InputCutoff,        "Cutoff",     "Base cutoff frequency.");
        METASOUND_PARAM(InputResonance,     "Resonance",  "Base resonance.");
        METASOUND_PARAM(InputCutoffMod,     "Cutoff Modulation", "Audio-rate modulation signal for cutoff frequency.");
        METASOUND_PARAM(InputResonanceMod,  "Resonance Modulation",    "Audio-rate modulation signal for resonance.");
        METASOUND_PARAM(OutputSignal,       "Out",        "Filtered audio output.");
    }

    class FLowPassArOperator : public TExecutableOperator<FLowPassArOperator>
    {
    public:
        FLowPassArOperator(
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
            // , MaxCutoffFrequency(0.5f * SampleRate)
        {
            LowPassFilter.Init(SampleRate);
        }

        static const FVertexInterface& DeclareVertexInterface()
        {
            using namespace LowPassArVertexNames;

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
                Metadata.ClassName = { TEXT("UE"), TEXT("Low Pass Filter (AR)"), TEXT("Audio") };
                Metadata.MajorVersion = 1;
                Metadata.MinorVersion = 0;
                Metadata.DisplayName = LOCTEXT("LowPassArDisplayName", "Low Pass Filter (AR)");
                Metadata.Description = LOCTEXT("LowPassArDesc", "Low pass (ladder) filter with audio-rate modulation for cutoff and resonance.");
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
            using namespace LowPassArVertexNames;

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

            return MakeUnique<FLowPassArOperator>(
                InParams.OperatorSettings,
                InputSignal,
                BaseCutoff,
                BaseResonance,
                CutoffMod,
                ResMod
            );
        }

        virtual FDataReferenceCollection GetInputs() const override
        {
            FDataReferenceCollection InputData;
            return InputData;
        }

        virtual FDataReferenceCollection GetOutputs() const override
        {
            using namespace LowPassArVertexNames;

            FDataReferenceCollection OutputData;
            OutputData.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutputSignal), AudioOutput);
            return OutputData;
        }

        virtual void Execute()
        {
            const float* InAudio = AudioInput->GetData();
            const float* CutoffModData = CutoffMod->GetData();
            const float* ResModData = ResonanceMod->GetData();
            float* OutAudio = AudioOutput->GetData();

            for (int32 i = 0; i < BlockSize; ++i)
            {
                float CutoffVal = (*BaseCutoff) + CutoffModData[i];
                float ResonanceVal = (*BaseResonance) + ResModData[i];

                LowPassFilter.SetFrequency(CutoffVal);
                LowPassFilter.SetResonance(ResonanceVal);

                OutAudio[i] = LowPassFilter.Process(InAudio[i]);
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
        
        FLowPassArFilter LowPassFilter;
        const int32 BlockSize;
        const float SampleRate;
        // const float MaxCutoffFrequency;
    };

    class FLowPassArNode : public FNodeFacade
    {
    public:
        FLowPassArNode(const FNodeInitData& InitData)
            : FNodeFacade(
                InitData.InstanceName,
                InitData.InstanceID,
                TFacadeOperatorClass<FLowPassArOperator>()
              )
        {
        }
    };

    METASOUND_REGISTER_NODE(FLowPassArNode);
}

#undef LOCTEXT_NAMESPACE