// Copyright 2026 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundMakeNoteFloatNode.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundBranches/Public/MetasoundVoiceState.h"  // Shared FVoiceState
#include "MetasoundPrimitives.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"
#include "MetasoundTime.h"
#include "MetasoundSampleCounter.h"
#include "MetasoundBranches/Public/MetasoundCommonMacros.h"

#define LOCTEXT_NAMESPACE "MetasoundMakeNoteFloatNode"

namespace Metasound
{
    namespace MakeNoteFloatNodeVertexNames
    {
        METASOUND_PARAM(InputNoteOn, "Note On", "Triggers a note-on event.");
        METASOUND_PARAM(InputNoteOff, "Note Off", "Triggers a note-off event (override).");
        METASOUND_PARAM(InputPitch, "Pitch", "Pitch value of the note.");
        METASOUND_PARAM(InputVelocity, "Velocity", "Velocity of the note.");
        METASOUND_PARAM(InputDuration, "Duration", "Duration of the note before auto note-off.");

        METASOUND_PARAM(OutputNoteOn, "Note On", "Trigger on note-on event.");
        METASOUND_PARAM(OutputNoteOff, "Note Off", "Trigger on note-off event.");
        METASOUND_PARAM(OutputArray, "Note Data", "Array containing pitch and velocity.");
        METASOUND_PARAM(OutputEvent, "Event", "Triggers on both note-on and off.");
        METASOUND_PARAM(OutputOverride, "Override", "Triggers when note-off is overridden.");
    }
    
    class FMakeNoteFloatOperator : public TExecutableOperator<FMakeNoteFloatOperator>
    {
        public:
        FMakeNoteFloatOperator(
            const FOperatorSettings& InSettings,
            const FTriggerReadRef& InNoteOn,
            const FTriggerReadRef& InNoteOff,
            const FFloatReadRef& InPitch,
            const FFloatReadRef& InVelocity,
            const FTimeReadRef&  InDuration)
            : InputNoteOn(InNoteOn)
            , InputNoteOff(InNoteOff)
            , InputPitch(InPitch)
            , InputVelocity(InVelocity)
            , InputDuration(InDuration)
            , OutputNoteOn(FTriggerWriteRef::CreateNew(InSettings))
            , OutputNoteOff(FTriggerWriteRef::CreateNew(InSettings))
            , OutputArray(TDataWriteReference<TArray<float>>::CreateNew())
            , OutputEvent(FTriggerWriteRef::CreateNew(InSettings))
            , OutputOverride(FTriggerWriteRef::CreateNew(InSettings))
            , FramesPerBlock(InSettings.GetNumFramesPerBlock())
            , SampleRate(InSettings.GetSampleRate())
            , AccumulatedSamples(0)
        {
            OutputArray->Init(0, 2);
            for (int32 i = 0; i < 128; ++i)
            {
                ActiveVoices[i] = FVoiceState{false, 0};
            }
        }

        static const FVertexInterface& DeclareVertexInterface()
        {
            using namespace MakeNoteFloatNodeVertexNames;

            static const FVertexInterface Interface(
                FInputVertexInterface(
                    TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputNoteOn)),
                    TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputNoteOff)),
                    TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputPitch)),
                    TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputVelocity)),
                    TInputDataVertex<FTime>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputDuration))
                ),
                FOutputVertexInterface(
                    TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputNoteOn)),
                    TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputNoteOff)),
                    TOutputDataVertex<TArray<float>>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputArray)),
                    TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputEvent)),
                    TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputOverride))
                )
            );

            return Interface;
        }

        static const FNodeClassMetadata& GetNodeInfo()
        {
            auto CreateNodeClassMetadata = []() -> FNodeClassMetadata
            {
                FNodeClassMetadata Metadata;
                Metadata.ClassName = { TEXT("UE"), TEXT("Make Note (Float)"), TEXT("Float") };
                Metadata.MajorVersion = 1;
                Metadata.MinorVersion = 0;
                Metadata.DisplayName = METASOUND_LOCTEXT("MakeNoteFloatDisplayName", "Make Note (Float)");
                Metadata.Description = METASOUND_LOCTEXT("MakeNoteFloatDesc", "Generates note-on and note-off events with a specified duration.");
                Metadata.Author = "Charles Matthews";
                Metadata.PromptIfMissing = PluginNodeMissingPrompt;
                Metadata.DefaultInterface = DeclareVertexInterface();
                Metadata.CategoryHierarchy = {
                    METASOUND_LOCTEXT("Custom", "Branches"),
                    METASOUND_LOCTEXT("CustomSub", "MIDI")
                };
                Metadata.Keywords = TArray<FText>(); // Keywords for searching
                
                METASOUND_BRANCHES_APPLY_NODE_STYLE(Metadata);
                
                return Metadata;
            };

            static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
            return Metadata;
        }

        METASOUND_DISABLE_LEGACY_IO()
        
        virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
        {
            using namespace MakeNoteFloatNodeVertexNames;
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputNoteOn), InputNoteOn);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputNoteOff), InputNoteOff);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputPitch), InputPitch);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputVelocity), InputVelocity);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputDuration), InputDuration);
        }
        
        virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
        {
            using namespace MakeNoteFloatNodeVertexNames;
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputNoteOn), OutputNoteOn);
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputNoteOff), OutputNoteOff);
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputArray), OutputArray);
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputEvent), OutputEvent);
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputOverride), OutputOverride);
        }
        
        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
        {
            return MakeUnique<FMakeNoteFloatOperator>(
                InParams.OperatorSettings,
                InParams.InputData.GetOrCreateDefaultDataReadReference<FTrigger>(METASOUND_GET_PARAM_NAME(MakeNoteFloatNodeVertexNames::InputNoteOn), InParams.OperatorSettings),
                InParams.InputData.GetOrCreateDefaultDataReadReference<FTrigger>(METASOUND_GET_PARAM_NAME(MakeNoteFloatNodeVertexNames::InputNoteOff), InParams.OperatorSettings),
                InParams.InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(MakeNoteFloatNodeVertexNames::InputPitch), InParams.OperatorSettings),
                InParams.InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(MakeNoteFloatNodeVertexNames::InputVelocity), InParams.OperatorSettings),
                InParams.InputData.GetOrCreateDefaultDataReadReference<FTime>(METASOUND_GET_PARAM_NAME(MakeNoteFloatNodeVertexNames::InputDuration), InParams.OperatorSettings)
            );
        }
        
        virtual void Execute()
        {
            OutputNoteOn->AdvanceBlock();
            OutputNoteOff->AdvanceBlock();
            OutputOverride->AdvanceBlock();
            OutputEvent->AdvanceBlock();

            int32 CurrentSample = AccumulatedSamples;

            // Process Note-On triggers
            InputNoteOn->ExecuteBlock(
                [](int32 StartFrame, int32 EndFrame) {},
                [this, &CurrentSample](int32 StartFrame, int32 EndFrame)
                {
                    CurrentSample += StartFrame;
                    int32 PitchIndex = static_cast<int32>(*InputPitch);

                    // Clamp to MIDI pitch range
                    if (PitchIndex >= 0 && PitchIndex < 128)
                    {
                        // Store only one note per pitch at a time
                        ActiveVoices[PitchIndex].Active = true;
                        ActiveVoices[PitchIndex].NoteEndSample = CurrentSample + static_cast<int32>(InputDuration->GetSeconds() * SampleRate);
                        ActiveVoices[PitchIndex].Pitch = *InputPitch;

                        (*OutputArray)[0] = *InputPitch;
                        (*OutputArray)[1] = *InputVelocity;
                        OutputNoteOn->TriggerFrame(StartFrame);
                        OutputEvent->TriggerFrame(StartFrame);
                    }
                }
            );

            // Process Note-Off override triggers
            InputNoteOff->ExecuteBlock(
                [](int32 StartFrame, int32 EndFrame) {},
                [this](int32 StartFrame, int32 EndFrame)
                {
                    int32 PitchIndex = static_cast<int32>(*InputPitch);
                    if (PitchIndex >= 0 && PitchIndex < 128 && ActiveVoices[PitchIndex].Active)
                    {
                        ActiveVoices[PitchIndex].Active = false;
                        (*OutputArray)[0] = *InputPitch;
                        (*OutputArray)[1] = 0.0f;
                        OutputNoteOff->TriggerFrame(StartFrame);
                        OutputOverride->TriggerFrame(StartFrame);
                        OutputEvent->TriggerFrame(StartFrame);
                    }
                }
            );

            // Process Scheduled Note-Offs
            for (int32 PitchIndex = 0; PitchIndex < 128; ++PitchIndex)
            {
                if (ActiveVoices[PitchIndex].Active && ActiveVoices[PitchIndex].NoteEndSample >= AccumulatedSamples &&
                    ActiveVoices[PitchIndex].NoteEndSample < (AccumulatedSamples + FramesPerBlock))
                {
                    int32 TriggerFrame = ActiveVoices[PitchIndex].NoteEndSample - AccumulatedSamples;
                    TriggerFrame = FMath::Clamp(TriggerFrame, 0, FramesPerBlock - 1);

                    // (*OutputArray)[0] = static_cast<float>(PitchIndex);
                    (*OutputArray)[0] = ActiveVoices[PitchIndex].Pitch;
                    (*OutputArray)[1] = 0.0f;
                    OutputNoteOff->TriggerFrame(TriggerFrame);
                    OutputEvent->TriggerFrame(TriggerFrame);

                    ActiveVoices[PitchIndex].Active = false;
                }
            }

            AccumulatedSamples += FramesPerBlock;
        }

        private:
        FTriggerReadRef InputNoteOn;
        FTriggerReadRef InputNoteOff;
        FFloatReadRef InputPitch;
        FFloatReadRef InputVelocity;
        FTimeReadRef InputDuration;
    
        FTriggerWriteRef OutputNoteOn;
        FTriggerWriteRef OutputNoteOff;
        TDataWriteReference<TArray<float>> OutputArray;
        FTriggerWriteRef OutputEvent;
        FTriggerWriteRef OutputOverride;
    
        TStaticArray<FVoiceState, 128> ActiveVoices;

        // Block / Timing
        int32 FramesPerBlock  = 0;
        float SampleRate      = 0.f;
        int32 AccumulatedSamples;
        
    };

    
    class FMakeNoteFloatNode : public FNodeFacade
    {
    public:
                static FNodeClassMetadata CreateNodeClassMetadata()
        {
            return FMakeNoteFloatOperator::GetNodeInfo();
        }
        FMakeNoteFloatNode(FNodeData InitData)
            : FNodeFacade(InitData, MakeShared<const FNodeClassMetadata>(FMakeNoteFloatOperator::GetNodeInfo()), TFacadeOperatorClass<FMakeNoteFloatOperator>())
        {
        }
    };
    
    METASOUND_REGISTER_NODE(FMakeNoteFloatNode);
}

#undef LOCTEXT_NAMESPACE