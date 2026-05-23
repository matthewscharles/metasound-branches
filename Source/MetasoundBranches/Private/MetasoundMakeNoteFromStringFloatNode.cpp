#pragma once

#include "MetasoundBranches/Public/MetasoundMakeNoteFromStringFloatNode.h"
#include "MetasoundBranches/Public/ParseMidiNote.h"
#include "MetasoundBranches/Public/MetasoundVoiceState.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundPrimitives.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"
#include "MetasoundTime.h"
#include "MetasoundSampleCounter.h"
#include "MetasoundBranches/Public/MetasoundCommonMacros.h"

#define LOCTEXT_NAMESPACE "MetasoundMakeNoteFromStringFloatNode"

namespace Metasound
{
    namespace MakeNoteFromStringFloatNodeVertexNames
    {
        METASOUND_PARAM(InputNoteOn, "Note On", "Triggers a note-on event.");
        METASOUND_PARAM(InputNoteOff, "Note Off", "Triggers a note-off event (override).");
        METASOUND_PARAM(InputNoteString, "Note", "Note string input (e.g., C4, A#3, G^2).");
        METASOUND_PARAM(InputVelocity, "Velocity", "Velocity of the note.");
        METASOUND_PARAM(InputDuration, "Duration", "Duration of the note before auto note-off.");

        METASOUND_PARAM(OutputNoteOn, "Note On", "Triggers a note-on event.");
        METASOUND_PARAM(OutputNoteOff, "Note Off", "Triggers a note-off event.");
        METASOUND_PARAM(OutputArray, "Note Data", "Array containing parsed pitch and velocity.");
        METASOUND_PARAM(OutputEvent, "Event", "Triggers on both note-on and off.");
        METASOUND_PARAM(OutputOverride, "Override", "Triggers when note-off is overridden.");
    }
    
    class FMakeNoteFromStringFloatOperator : public TExecutableOperator<FMakeNoteFromStringFloatOperator>
    {
    public:
        FMakeNoteFromStringFloatOperator(
            const FOperatorSettings& InSettings,
            const FTriggerReadRef& InNoteOn,
            const FTriggerReadRef& InNoteOff,
            const TDataReadReference<FString>& InNoteString,
            const FFloatReadRef& InVelocity,
            const FTimeReadRef& InDuration)
            : InputNoteOn(InNoteOn)
            , InputNoteOff(InNoteOff)
            , InputNoteString(InNoteString)
            , InputVelocity(InVelocity)
            , InputDuration(InDuration)
            , OutputNoteOn(FTriggerWriteRef::CreateNew(InSettings))
            , OutputNoteOff(FTriggerWriteRef::CreateNew(InSettings))
            , OutputArray(TDataWriteReference<TArray<float>>::CreateNew(TArray<float>{0.0f, 0.0f}))
            , OutputEvent(FTriggerWriteRef::CreateNew(InSettings))
            , OutputOverride(FTriggerWriteRef::CreateNew(InSettings))
            , FramesPerBlock(InSettings.GetNumFramesPerBlock())
            , SampleRate(InSettings.GetSampleRate())
            , AccumulatedSamples(0)
        {
            for (int32 i = 0; i < 128; ++i)
            {
                ActiveVoices[i] = FVoiceState{false, 0};
            }
        }
        
        static const FVertexInterface& DeclareVertexInterface()
        {
            using namespace MakeNoteFromStringFloatNodeVertexNames;

            static const FVertexInterface Interface(
                FInputVertexInterface(
                    TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputNoteOn)),
                    TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputNoteOff)),
                    TInputDataVertex<FString>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputNoteString)),
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
                Metadata.ClassName = { TEXT("UE"), TEXT("Make Note From String (Float)"), TEXT("Float") };
                Metadata.MajorVersion = 1;
                Metadata.MinorVersion = 0;
                Metadata.DisplayName = METASOUND_LOCTEXT("MakeNoteFromStringFloatDisplayName", "Make Note From String (Float)");
                Metadata.Description = METASOUND_LOCTEXT("MakeNoteFromStringFloatDesc", "Generates note-on and note-off events with a specified duration.");
                Metadata.Author = "Charles Matthews";
                Metadata.PromptIfMissing = PluginNodeMissingPrompt;
                Metadata.DefaultInterface = DeclareVertexInterface();
                Metadata.CategoryHierarchy = {
                    METASOUND_LOCTEXT("Custom", "Branches"),
                    METASOUND_LOCTEXT("CustomSub", "MIDI")
                };
                Metadata.Keywords = TArray<FText>(); // Keywords for searching
                
                return Metadata;
            };

            static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
            return Metadata;
        }
        
        METASOUND_DISABLE_LEGACY_IO()
        
        virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
        {
            using namespace MakeNoteFromStringFloatNodeVertexNames;
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputNoteOn), InputNoteOn);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputNoteOff), InputNoteOff);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputNoteString), InputNoteString);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputVelocity), InputVelocity);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputDuration), InputDuration);
        }
        
        virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
        {
            using namespace MakeNoteFromStringFloatNodeVertexNames;
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputNoteOn), OutputNoteOn);
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputNoteOff), OutputNoteOff);
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputArray), OutputArray);
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputEvent), OutputEvent);
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputOverride), OutputOverride);
        }
        
        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
        {
            return MakeUnique<FMakeNoteFromStringFloatOperator>(
                InParams.OperatorSettings,
                InParams.InputData.GetOrCreateDefaultDataReadReference<FTrigger>(METASOUND_GET_PARAM_NAME(MakeNoteFromStringFloatNodeVertexNames::InputNoteOn), InParams.OperatorSettings),
                InParams.InputData.GetOrCreateDefaultDataReadReference<FTrigger>(METASOUND_GET_PARAM_NAME(MakeNoteFromStringFloatNodeVertexNames::InputNoteOff), InParams.OperatorSettings),
                InParams.InputData.GetOrCreateDefaultDataReadReference<FString>(METASOUND_GET_PARAM_NAME(MakeNoteFromStringFloatNodeVertexNames::InputNoteString), InParams.OperatorSettings),
                InParams.InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(MakeNoteFromStringFloatNodeVertexNames::InputVelocity), InParams.OperatorSettings),
                InParams.InputData.GetOrCreateDefaultDataReadReference<FTime>(METASOUND_GET_PARAM_NAME(MakeNoteFromStringFloatNodeVertexNames::InputDuration), InParams.OperatorSettings)
            );
        }
        

        void Execute()
        {
            OutputNoteOn->AdvanceBlock();
            OutputNoteOff->AdvanceBlock();
            OutputOverride->AdvanceBlock();
            OutputEvent->AdvanceBlock();

            InputNoteOn->ExecuteBlock(
                [](int32, int32) {},
                [this](int32 StartFrame, int32)
                {
                    float ParsedPitch = ParseMidiNote(TCHAR_TO_ANSI(**InputNoteString));
                    int32 PitchIndex = static_cast<int32>(ParsedPitch);

                    if (PitchIndex >= 0 && PitchIndex < 128)
                    {
                        ActiveVoices[PitchIndex].Active = true;
                        ActiveVoices[PitchIndex].NoteEndSample = AccumulatedSamples + static_cast<int32>(InputDuration->GetSeconds() * SampleRate);
                        ActiveVoices[PitchIndex].Pitch = ParsedPitch;

                        (*OutputArray)[0] = ParsedPitch;
                        (*OutputArray)[1] = *InputVelocity;
                        OutputNoteOn->TriggerFrame(StartFrame);
                        OutputEvent->TriggerFrame(StartFrame);
                    }
                }
            );

            // Process Note-Off triggers
            InputNoteOff->ExecuteBlock(
                [](int32, int32) {},
                [this](int32 StartFrame, int32)
                {
                    float ParsedPitch = ParseMidiNote(TCHAR_TO_ANSI(**InputNoteString));
                    int32 PitchIndex = static_cast<int32>(ParsedPitch);

                    if (PitchIndex >= 0 && PitchIndex < 128 && ActiveVoices[PitchIndex].Active)
                    {
                        ActiveVoices[PitchIndex].Active = false;
                        (*OutputArray)[0] = ParsedPitch;
                        (*OutputArray)[1] = 0.0f;
                        OutputNoteOff->TriggerFrame(StartFrame);
                        OutputOverride->TriggerFrame(StartFrame);
                    }
                }
            );

            // Process Scheduled Note-Offs
            for (int32 PitchIndex = 0; PitchIndex < 128; ++PitchIndex)
            {
                if (ActiveVoices[PitchIndex].Active &&
                    ActiveVoices[PitchIndex].NoteEndSample > AccumulatedSamples &&
                    ActiveVoices[PitchIndex].NoteEndSample <= (AccumulatedSamples + FramesPerBlock))
                {
                    int32 TriggerFrame = ActiveVoices[PitchIndex].NoteEndSample - AccumulatedSamples;
                    TriggerFrame = FMath::Clamp(TriggerFrame, 0, FramesPerBlock - 1);

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
        TDataReadReference<FString> InputNoteString;
        FFloatReadRef InputVelocity;
        FTimeReadRef InputDuration;

        FTriggerWriteRef OutputNoteOn;
        FTriggerWriteRef OutputNoteOff;
        TDataWriteReference<TArray<float>> OutputArray;
        FTriggerWriteRef OutputEvent;
        FTriggerWriteRef OutputOverride;

        TStaticArray<FVoiceState, 128> ActiveVoices;
        int32 FramesPerBlock;
        float SampleRate;
        int32 AccumulatedSamples;
    };

    class FMakeNoteFromStringFloatNode : public FNodeFacade
    {
    public:
                static FNodeClassMetadata CreateNodeClassMetadata()
        {
            return FMakeNoteFromStringFloatOperator::GetNodeInfo();
        }
        FMakeNoteFromStringFloatNode(FNodeData InitData)
            : FNodeFacade(InitData, MakeShared<const FNodeClassMetadata>(FMakeNoteFromStringFloatOperator::GetNodeInfo()), TFacadeOperatorClass<FMakeNoteFromStringFloatOperator>())
        {
        }
    };

    METASOUND_REGISTER_NODE(FMakeNoteFromStringFloatNode);
}

#undef LOCTEXT_NAMESPACE