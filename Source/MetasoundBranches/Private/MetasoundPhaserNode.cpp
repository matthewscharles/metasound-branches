// Copyright 2026 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundPhaserNode.h"

#include "MetasoundAudioBuffer.h"
#include "MetasoundBranches/Public/MetasoundCommonMacros.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundParamHelper.h"
#include "MetasoundPrimitives.h"

#define LOCTEXT_NAMESPACE "MetasoundBranches_PhaserNode"

namespace Metasound
{
    namespace PhaserVertexNames
    {
        METASOUND_PARAM(InputAudio, "In", "Input audio signal.")
        METASOUND_PARAM(InputRateHz, "Rate", "LFO rate in Hz.")
        METASOUND_PARAM(InputDepth, "Depth", "Sweep depth from 0 to 1.")
        METASOUND_PARAM(InputFeedback, "Feedback", "Feedback amount from -0.99 to 0.99.")
        METASOUND_PARAM(InputMix, "Mix", "Dry/wet balance from 0 (dry) to 1 (wet).")
        METASOUND_PARAM(InputMinFrequency, "Min Frequency", "Minimum sweep frequency in Hz.")
        METASOUND_PARAM(InputMaxFrequency, "Max Frequency", "Maximum sweep frequency in Hz.")
        METASOUND_PARAM(OutputAudio, "Out", "Phaser output signal.")
    }

    namespace PhaserPrivate
    {
        static constexpr int32 MinPoles = 1;
        static constexpr int32 MaxPoles = 32;

        class FPhaserOperatorData final : public TOperatorData<FPhaserOperatorData>
        {
        public:
            static const FLazyName OperatorDataTypeName;

            explicit FPhaserOperatorData(const int32 InNumPoles)
                : NumPoles(InNumPoles)
            {
            }

            int32 NumPoles = 6;
        };

        const FLazyName FPhaserOperatorData::OperatorDataTypeName = TEXT("FPhaserOperatorData");

        int32 GetClampedNumPoles(const int32 InNumPoles)
        {
            return FMath::Clamp(InNumPoles, MinPoles, MaxPoles);
        }

        FVertexInterface GetVertexInterface()
        {
            using namespace PhaserVertexNames;

            return FVertexInterface(
                FInputVertexInterface(
                    TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputAudio)),
                    TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputRateHz), 0.25f),
                    TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputDepth), 1.0f),
                    TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputFeedback), 0.2f),
                    TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputMix), 0.5f),
                    TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputMinFrequency), 350.0f),
                    TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputMaxFrequency), 2000.0f)
                ),
                FOutputVertexInterface(
                    TOutputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputAudio))
                )
            );
        }
    }

    class FPhaserOperator : public TExecutableOperator<FPhaserOperator>
    {
    public:
        using FPhaserOperatorData = PhaserPrivate::FPhaserOperatorData;

        FPhaserOperator(
            const TSharedPtr<const FPhaserOperatorData>& InOperatorData,
            const FOperatorSettings& InSettings,
            const FAudioBufferReadRef& InAudio,
            const FFloatReadRef& InRateHz,
            const FFloatReadRef& InDepth,
            const FFloatReadRef& InFeedback,
            const FFloatReadRef& InMix,
            const FFloatReadRef& InMinFrequency,
            const FFloatReadRef& InMaxFrequency)
            : OperatorData(InOperatorData)
            , InputAudio(InAudio)
            , InputRateHz(InRateHz)
            , InputDepth(InDepth)
            , InputFeedback(InFeedback)
            , InputMix(InMix)
            , InputMinFrequency(InMinFrequency)
            , InputMaxFrequency(InMaxFrequency)
            , OutputAudio(FAudioBufferWriteRef::CreateNew(InSettings))
            , SampleRate(FMath::Max(1.0f, InSettings.GetSampleRate()))
            , NumPoles(PhaserPrivate::GetClampedNumPoles(InOperatorData->NumPoles))
        {
            AllPassState.Init(0.0f, NumPoles);
        }

        static const FVertexInterface& DeclareVertexInterface()
        {
            static const FVertexInterface Interface = PhaserPrivate::GetVertexInterface();
            return Interface;
        }

        static const FNodeClassMetadata& GetNodeInfo()
        {
            auto CreateMetadata = []() -> FNodeClassMetadata
            {
                FNodeClassMetadata Metadata;
                Metadata.ClassName = { TEXT("UE"), TEXT("Phaser"), TEXT("Audio") };
                Metadata.MajorVersion = 1;
                Metadata.MinorVersion = 0;
                Metadata.DisplayName = METASOUND_LOCTEXT("PhaserDisplayName", "Phaser");
                Metadata.Description = METASOUND_LOCTEXT("PhaserDescription", "Mono phaser with configurable pole count, LFO sweep, feedback, and dry/wet mix.");
                Metadata.Author = TEXT("Charles Matthews");
                Metadata.PromptIfMissing = PluginNodeMissingPrompt;
                Metadata.DefaultInterface = DeclareVertexInterface();
                Metadata.CategoryHierarchy = {
                    METASOUND_LOCTEXT("Custom", "Branches"),
                    METASOUND_LOCTEXT("CustomSub", "Filters")
                };
                Metadata.Keywords = {
                    METASOUND_LOCTEXT("PhaserKeyword", "Phaser"),
                    METASOUND_LOCTEXT("PoleKeyword", "Poles")
                };
                METASOUND_BRANCHES_APPLY_NODE_STYLE(Metadata);
                return Metadata;
            };

            static const FNodeClassMetadata Metadata = CreateMetadata();
            return Metadata;
        }

        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
        {
            using namespace PhaserVertexNames;
            using FPhaserOperatorData = PhaserPrivate::FPhaserOperatorData;

            const FPhaserOperatorData* ConfigData = CastOperatorData<const FPhaserOperatorData>(InParams.Node.GetOperatorData().Get());
            if (!ConfigData)
            {
                return MakeUnique<FNoOpOperator>();
            }

            const TSharedPtr<const FPhaserOperatorData>& OperatorDataSharedPtr =
                StaticCastSharedPtr<const FPhaserOperatorData>(InParams.Node.GetOperatorData());

            FAudioBufferReadRef InAudio = InParams.InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(InputAudio), InParams.OperatorSettings);
            FFloatReadRef InRateHz = InParams.InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputRateHz), InParams.OperatorSettings);
            FFloatReadRef InDepth = InParams.InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputDepth), InParams.OperatorSettings);
            FFloatReadRef InFeedback = InParams.InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputFeedback), InParams.OperatorSettings);
            FFloatReadRef InMix = InParams.InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputMix), InParams.OperatorSettings);
            FFloatReadRef InMinFrequency = InParams.InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputMinFrequency), InParams.OperatorSettings);
            FFloatReadRef InMaxFrequency = InParams.InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputMaxFrequency), InParams.OperatorSettings);

            return MakeUnique<FPhaserOperator>(
                OperatorDataSharedPtr,
                InParams.OperatorSettings,
                InAudio,
                InRateHz,
                InDepth,
                InFeedback,
                InMix,
                InMinFrequency,
                InMaxFrequency);
        }

        METASOUND_DISABLE_LEGACY_IO()

        virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
        {
            using namespace PhaserVertexNames;
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputAudio), InputAudio);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputRateHz), InputRateHz);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputDepth), InputDepth);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputFeedback), InputFeedback);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputMix), InputMix);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputMinFrequency), InputMinFrequency);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputMaxFrequency), InputMaxFrequency);
        }

        virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
        {
            using namespace PhaserVertexNames;
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputAudio), OutputAudio);
        }

        void Execute()
        {
            const float* InData = InputAudio->GetData();
            float* OutData = OutputAudio->GetData();
            const int32 NumFrames = OutputAudio->Num();

            const float RateHz = FMath::Clamp(*InputRateHz, 0.0f, 20.0f);
            const float Depth = FMath::Clamp(*InputDepth, 0.0f, 1.0f);
            const float Feedback = FMath::Clamp(*InputFeedback, -0.99f, 0.99f);
            const float Mix = FMath::Clamp(*InputMix, 0.0f, 1.0f);
            const float Nyquist = 0.5f * SampleRate;
            const float MinFrequency = FMath::Clamp(*InputMinFrequency, 20.0f, Nyquist - 20.0f);
            const float MaxFrequency = FMath::Clamp(FMath::Max(*InputMaxFrequency, MinFrequency + 1.0f), MinFrequency + 1.0f, Nyquist - 20.0f);

            const float TwoPi = 2.0f * PI;
            const float PhaseStep = (TwoPi * RateHz) / SampleRate;

            for (int32 Frame = 0; Frame < NumFrames; ++Frame)
            {
                const float Lfo01 = 0.5f * (FMath::Sin(Phase) + 1.0f);
                Phase += PhaseStep;
                if (Phase >= TwoPi)
                {
                    Phase -= TwoPi;
                }

                const float SweepFrequency = FMath::Lerp(MinFrequency, MaxFrequency, Lfo01 * Depth);
                const float W = FMath::Tan(PI * SweepFrequency / SampleRate);
                const float A = (1.0f - W) / (1.0f + W);

                const float InputSample = InData[Frame] + (Feedback * LastOutput);

                float StageSignal = InputSample;
                for (int32 PoleIndex = 0; PoleIndex < NumPoles; ++PoleIndex)
                {
                    const float PreviousState = AllPassState[PoleIndex];
                    const float Output = (-A * StageSignal) + PreviousState;
                    AllPassState[PoleIndex] = StageSignal + (A * Output);
                    StageSignal = Output;
                }

                const float PhaserSample = StageSignal;
                const float Mixed = FMath::Lerp(InData[Frame], PhaserSample, Mix);
                OutData[Frame] = Mixed;
                LastOutput = Mixed;
            }
        }

    private:
        TSharedPtr<const FPhaserOperatorData> OperatorData;

        FAudioBufferReadRef InputAudio;
        FFloatReadRef InputRateHz;
        FFloatReadRef InputDepth;
        FFloatReadRef InputFeedback;
        FFloatReadRef InputMix;
        FFloatReadRef InputMinFrequency;
        FFloatReadRef InputMaxFrequency;

        FAudioBufferWriteRef OutputAudio;

        float SampleRate = 48000.0f;
        int32 NumPoles = 6;
        float Phase = 0.0f;
        float LastOutput = 0.0f;
        TArray<float> AllPassState;
    };

    class FPhaserNode : public FNodeFacade
    {
    public:
        static FNodeClassMetadata CreateNodeClassMetadata()
        {
            return FPhaserOperator::GetNodeInfo();
        }

        explicit FPhaserNode(const FNodeData& InNodeData)
            : FNodeFacade(InNodeData, MakeShared<const FNodeClassMetadata>(FPhaserOperator::GetNodeInfo()), TFacadeOperatorClass<FPhaserOperator>())
        {
        }
    };

    METASOUND_REGISTER_NODE_AND_CONFIGURATION(FPhaserNode, FMetaSoundPhaserNodeConfiguration)
}

FMetaSoundPhaserNodeConfiguration::FMetaSoundPhaserNodeConfiguration()
    : OperatorData(MakeShared<Metasound::PhaserPrivate::FPhaserOperatorData>(NumPoles))
{
}

TInstancedStruct<FMetasoundFrontendClassInterface> FMetaSoundPhaserNodeConfiguration::OverrideDefaultInterface(const FMetasoundFrontendClass& InClass) const
{
    return TInstancedStruct<FMetasoundFrontendClassInterface>::Make(
        FMetasoundFrontendClassInterface::GenerateClassInterface(
            Metasound::PhaserPrivate::GetVertexInterface()));
}

TSharedPtr<const Metasound::IOperatorData> FMetaSoundPhaserNodeConfiguration::GetOperatorData() const
{
    OperatorData->NumPoles = FMath::Clamp(NumPoles, Metasound::PhaserPrivate::MinPoles, Metasound::PhaserPrivate::MaxPoles);
    return OperatorData;
}

#undef LOCTEXT_NAMESPACE
