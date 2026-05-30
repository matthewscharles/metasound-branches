// Copyright 2026 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundReverbFreezeNode.h"

#include "MetasoundAudioBuffer.h"
#include "MetasoundBranches/Public/MetasoundCommonMacros.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundParamHelper.h"
#include "MetasoundPrimitives.h"

#define LOCTEXT_NAMESPACE "MetasoundBranches_ReverbFreezeNode"

namespace Metasound
{
    namespace ReverbFreezeNodeVertexNames
    {
        METASOUND_PARAM(InputLeftSignal, "In L", "Left channel input signal.");
        METASOUND_PARAM(InputRightSignal, "In R", "Right channel input signal.");
        METASOUND_PARAM(InputRoomSize, "Room Size", "Reverb room size from 0 to 1.");
        METASOUND_PARAM(InputDamping, "Damping", "High-frequency damping from 0 to 1.");
        METASOUND_PARAM(InputWet, "Wet", "Wet level from 0 to 1.");
        METASOUND_PARAM(InputDry, "Dry", "Dry level from 0 to 1.");
        METASOUND_PARAM(InputWidth, "Width", "Stereo width from 0 to 1.");
        METASOUND_PARAM(InputFreeze, "Freeze", "Hold the reverb tail indefinitely when enabled.");

        METASOUND_PARAM(OutputLeftSignal, "Out L", "Left output signal.");
        METASOUND_PARAM(OutputRightSignal, "Out R", "Right output signal.");
    }

    class FReverbFreezeOperator : public TExecutableOperator<FReverbFreezeOperator>
    {
    public:
        FReverbFreezeOperator(
            const FOperatorSettings& InSettings,
            const FAudioBufferReadRef& InLeftSignal,
            const FAudioBufferReadRef& InRightSignal,
            const FFloatReadRef& InRoomSize,
            const FFloatReadRef& InDamping,
            const FFloatReadRef& InWet,
            const FFloatReadRef& InDry,
            const FFloatReadRef& InWidth,
            const FBoolReadRef& InFreeze)
            : InputLeftSignal(InLeftSignal)
            , InputRightSignal(InRightSignal)
            , InputRoomSize(InRoomSize)
            , InputDamping(InDamping)
            , InputWet(InWet)
            , InputDry(InDry)
            , InputWidth(InWidth)
            , InputFreeze(InFreeze)
            , OutputLeftSignal(FAudioBufferWriteRef::CreateNew(InSettings))
            , OutputRightSignal(FAudioBufferWriteRef::CreateNew(InSettings))
            , SampleRate(FMath::Max(1.0f, InSettings.GetSampleRate()))
        {
            InitializeFilters();
        }

        static const FVertexInterface& DeclareVertexInterface()
        {
            using namespace ReverbFreezeNodeVertexNames;

            static const FVertexInterface Interface(
                FInputVertexInterface(
                    TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputLeftSignal)),
                    TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputRightSignal)),
                    TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputRoomSize), 0.5f),
                    TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputDamping), 0.5f),
                    TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputWet), 0.33f),
                    TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputDry), 0.67f),
                    TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputWidth), 1.0f),
                    TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputFreeze), false)
                ),
                FOutputVertexInterface(
                    TOutputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputLeftSignal)),
                    TOutputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputRightSignal))
                )
            );

            return Interface;
        }

        static const FNodeClassMetadata& GetNodeInfo()
        {
            auto CreateNodeClassMetadata = []() -> FNodeClassMetadata
            {
                FNodeClassMetadata Metadata;
                Metadata.ClassName = { TEXT("UE"), TEXT("ReverbFreeze"), TEXT("Audio") };
                Metadata.MajorVersion = 1;
                Metadata.MinorVersion = 0;
                Metadata.DisplayName = METASOUND_LOCTEXT("ReverbFreezeDisplayName", "Reverb Freeze");
                Metadata.Description = METASOUND_LOCTEXT("ReverbFreezeDescription", "Stereo reverb using parallel comb and serial allpass filters, with freeze control.");
                Metadata.Author = TEXT("Charles Matthews");
                Metadata.PromptIfMissing = PluginNodeMissingPrompt;
                Metadata.DefaultInterface = DeclareVertexInterface();
                Metadata.CategoryHierarchy = {
                    METASOUND_LOCTEXT("Custom", "Branches"),
                    METASOUND_LOCTEXT("CustomSub", "Reverb")
                };
                METASOUND_BRANCHES_APPLY_NODE_STYLE(Metadata);
                return Metadata;
            };

            static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
            return Metadata;
        }

        METASOUND_DISABLE_LEGACY_IO()

        virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
        {
            using namespace ReverbFreezeNodeVertexNames;

            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputLeftSignal), InputLeftSignal);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputRightSignal), InputRightSignal);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputRoomSize), InputRoomSize);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputDamping), InputDamping);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputWet), InputWet);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputDry), InputDry);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputWidth), InputWidth);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputFreeze), InputFreeze);
        }

        virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
        {
            using namespace ReverbFreezeNodeVertexNames;

            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputLeftSignal), OutputLeftSignal);
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputRightSignal), OutputRightSignal);
        }

        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
        {
            using namespace ReverbFreezeNodeVertexNames;

            const FInputVertexInterfaceData& InputData = InParams.InputData;

            TDataReadReference<FAudioBuffer> InLeft = InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(InputLeftSignal), InParams.OperatorSettings);
            TDataReadReference<FAudioBuffer> InRight = InputData.GetOrCreateDefaultDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(InputRightSignal), InParams.OperatorSettings);
            TDataReadReference<float> InRoomSize = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputRoomSize), InParams.OperatorSettings);
            TDataReadReference<float> InDamping = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputDamping), InParams.OperatorSettings);
            TDataReadReference<float> InWet = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputWet), InParams.OperatorSettings);
            TDataReadReference<float> InDry = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputDry), InParams.OperatorSettings);
            TDataReadReference<float> InWidth = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputWidth), InParams.OperatorSettings);
            TDataReadReference<bool> InFreeze = InputData.GetOrCreateDefaultDataReadReference<bool>(METASOUND_GET_PARAM_NAME(InputFreeze), InParams.OperatorSettings);

            return MakeUnique<FReverbFreezeOperator>(
                InParams.OperatorSettings,
                InLeft,
                InRight,
                InRoomSize,
                InDamping,
                InWet,
                InDry,
                InWidth,
                InFreeze);
        }

        void Execute()
        {
            const int32 NumFrames = InputLeftSignal->Num();
            const float* InLeftData = InputLeftSignal->GetData();
            const float* InRightData = InputRightSignal->GetData();
            float* OutLeftData = OutputLeftSignal->GetData();
            float* OutRightData = OutputRightSignal->GetData();

            const bool bFreeze = *InputFreeze;

            const float RoomSize = FMath::Clamp(*InputRoomSize, 0.0f, 1.0f);
            const float Damping = FMath::Clamp(*InputDamping, 0.0f, 1.0f);
            const float Wet = FMath::Clamp(*InputWet, 0.0f, 1.0f);
            const float Dry = FMath::Clamp(*InputDry, 0.0f, 1.0f);
            const float Width = FMath::Clamp(*InputWidth, 0.0f, 1.0f);

            const float InputGain = bFreeze ? 0.0f : 0.015f;
            const float Feedback = bFreeze ? 1.0f : (RoomSize * 0.28f + 0.7f);
            const float Damp = bFreeze ? 0.0f : (Damping * 0.4f);

            const float Wet1 = Wet * (Width * 0.5f + 0.5f);
            const float Wet2 = Wet * ((1.0f - Width) * 0.5f);

            for (FCombFilter& Filter : CombFiltersL)
            {
                Filter.SetFeedback(Feedback);
                Filter.SetDamp(Damp);
            }
            for (FCombFilter& Filter : CombFiltersR)
            {
                Filter.SetFeedback(Feedback);
                Filter.SetDamp(Damp);
            }

            for (int32 Frame = 0; Frame < NumFrames; ++Frame)
            {
                const float InMono = (InLeftData[Frame] + InRightData[Frame]) * 0.5f * InputGain;

                float AccumL = 0.0f;
                float AccumR = 0.0f;

                for (FCombFilter& Filter : CombFiltersL)
                {
                    AccumL += Filter.Process(InMono);
                }
                for (FCombFilter& Filter : CombFiltersR)
                {
                    AccumR += Filter.Process(InMono);
                }

                for (FAllPassFilter& Filter : AllPassFiltersL)
                {
                    AccumL = Filter.Process(AccumL);
                }
                for (FAllPassFilter& Filter : AllPassFiltersR)
                {
                    AccumR = Filter.Process(AccumR);
                }

                OutLeftData[Frame] = AccumL * Wet1 + AccumR * Wet2 + InLeftData[Frame] * Dry;
                OutRightData[Frame] = AccumR * Wet1 + AccumL * Wet2 + InRightData[Frame] * Dry;
            }
        }

    private:
        class FCombFilter
        {
        public:
            void Init(const int32 InLength)
            {
                Buffer.SetNumZeroed(FMath::Max(1, InLength));
                Index = 0;
                Feedback = 0.5f;
                FilterStore = 0.0f;
                Damp1 = 0.2f;
                Damp2 = 0.8f;
            }

            void SetFeedback(const float InFeedback)
            {
                Feedback = FMath::Clamp(InFeedback, 0.0f, 1.0f);
            }

            void SetDamp(const float InDamp)
            {
                Damp1 = FMath::Clamp(InDamp, 0.0f, 1.0f);
                Damp2 = 1.0f - Damp1;
            }

            float Process(const float InSample)
            {
                const float Output = Buffer[Index];
                FilterStore = Output * Damp2 + FilterStore * Damp1;
                Buffer[Index] = InSample + FilterStore * Feedback;

                ++Index;
                if (Index >= Buffer.Num())
                {
                    Index = 0;
                }

                return Output;
            }

        private:
            TArray<float> Buffer;
            int32 Index = 0;
            float Feedback = 0.5f;
            float FilterStore = 0.0f;
            float Damp1 = 0.2f;
            float Damp2 = 0.8f;
        };

        class FAllPassFilter
        {
        public:
            void Init(const int32 InLength)
            {
                Buffer.SetNumZeroed(FMath::Max(1, InLength));
                Index = 0;
                Feedback = 0.5f;
            }

            float Process(const float InSample)
            {
                const float Buffered = Buffer[Index];
                const float Output = -InSample + Buffered;
                Buffer[Index] = InSample + Buffered * Feedback;

                ++Index;
                if (Index >= Buffer.Num())
                {
                    Index = 0;
                }

                return Output;
            }

        private:
            TArray<float> Buffer;
            int32 Index = 0;
            float Feedback = 0.5f;
        };

        void InitializeFilters()
        {
            static const int32 CombTuningsL[8] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
            static const int32 AllPassTuningsL[4] = { 556, 441, 341, 225 };
            static constexpr int32 StereoSpread = 23;

            const float Scale = SampleRate / 44100.0f;

            CombFiltersL.SetNum(8);
            CombFiltersR.SetNum(8);
            for (int32 i = 0; i < 8; ++i)
            {
                const int32 LenL = FMath::Max(1, FMath::RoundToInt(static_cast<float>(CombTuningsL[i]) * Scale));
                const int32 LenR = FMath::Max(1, FMath::RoundToInt(static_cast<float>(CombTuningsL[i] + StereoSpread) * Scale));
                CombFiltersL[i].Init(LenL);
                CombFiltersR[i].Init(LenR);
            }

            AllPassFiltersL.SetNum(4);
            AllPassFiltersR.SetNum(4);
            for (int32 i = 0; i < 4; ++i)
            {
                const int32 LenL = FMath::Max(1, FMath::RoundToInt(static_cast<float>(AllPassTuningsL[i]) * Scale));
                const int32 LenR = FMath::Max(1, FMath::RoundToInt(static_cast<float>(AllPassTuningsL[i] + StereoSpread) * Scale));
                AllPassFiltersL[i].Init(LenL);
                AllPassFiltersR[i].Init(LenR);
            }
        }

        FAudioBufferReadRef InputLeftSignal;
        FAudioBufferReadRef InputRightSignal;
        FFloatReadRef InputRoomSize;
        FFloatReadRef InputDamping;
        FFloatReadRef InputWet;
        FFloatReadRef InputDry;
        FFloatReadRef InputWidth;
        FBoolReadRef InputFreeze;

        FAudioBufferWriteRef OutputLeftSignal;
        FAudioBufferWriteRef OutputRightSignal;

        float SampleRate = 44100.0f;
        TArray<FCombFilter> CombFiltersL;
        TArray<FCombFilter> CombFiltersR;
        TArray<FAllPassFilter> AllPassFiltersL;
        TArray<FAllPassFilter> AllPassFiltersR;
    };

    class FReverbFreezeNode : public FNodeFacade
    {
    public:
        static FNodeClassMetadata CreateNodeClassMetadata()
        {
            return FReverbFreezeOperator::GetNodeInfo();
        }

        FReverbFreezeNode(FNodeData InitData)
            : FNodeFacade(InitData, MakeShared<const FNodeClassMetadata>(FReverbFreezeOperator::GetNodeInfo()), TFacadeOperatorClass<FReverbFreezeOperator>())
        {
        }
    };

    METASOUND_REGISTER_NODE(FReverbFreezeNode);
}

#undef LOCTEXT_NAMESPACE
