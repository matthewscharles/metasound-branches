// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundMakeNoteFloatNode.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundPrimitives.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"
#include "MetasoundTime.h"
#include "MetasoundSampleCounter.h"

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

        METASOUND_PARAM(OutputNoteOn, "Note On", "Triggers a note-on event.");
        METASOUND_PARAM(OutputNoteOff, "Note Off", "Triggers a note-off event.");
        METASOUND_PARAM(OutputArray, "Note Data", "Array containing pitch and velocity.");
        METASOUND_PARAM(OutputOverride, "Override", "Triggers when note-off is overridden.");
    }
    
    struct FVoiceState
    {
        bool bActive = false;
        int32 NoteEndSample = 0;
    };


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
            , OutputOverride(FTriggerWriteRef::CreateNew(InSettings))
            , FramesPerBlock(InSettings.GetNumFramesPerBlock())
            , SampleRate(InSettings.GetSampleRate())
            , AccumulatedSamples(0)
        {
            OutputArray->Init(0, 2);
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
                Metadata.ClassName = { TEXT("UE"), TEXT("MakeNote(Float)"), TEXT("Float") };
                Metadata.MajorVersion = 1;
                Metadata.MinorVersion = 0;
                Metadata.DisplayName = METASOUND_LOCTEXT("MakeNoteFloatDisplayName", "Make Note (Float)");
                Metadata.Description = METASOUND_LOCTEXT("MakeNoteFloatDesc", "Generates note-on and note-off events with a specified duration and allows override.");
                Metadata.Author = "Charles Matthews";
                Metadata.DefaultInterface = DeclareVertexInterface();
                return Metadata;
            };

            static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
            return Metadata;
        }

        virtual FDataReferenceCollection GetInputs() const override
        {
            using namespace MakeNoteFloatNodeVertexNames;
            FDataReferenceCollection Inputs;
            Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputNoteOn), InputNoteOn);
            Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputNoteOff), InputNoteOff);
            Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputPitch), InputPitch);
            Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputVelocity), InputVelocity);
            Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputDuration), InputDuration);
            return Inputs;
        }
        
        virtual FDataReferenceCollection GetOutputs() const override
        {
            using namespace MakeNoteFloatNodeVertexNames;
            FDataReferenceCollection Outputs;
            Outputs.AddDataWriteReference(METASOUND_GET_PARAM_NAME(OutputNoteOn), OutputNoteOn);
            Outputs.AddDataWriteReference(METASOUND_GET_PARAM_NAME(OutputNoteOff), OutputNoteOff);
            Outputs.AddDataWriteReference(METASOUND_GET_PARAM_NAME(OutputArray), OutputArray);
            Outputs.AddDataWriteReference(METASOUND_GET_PARAM_NAME(OutputOverride), OutputOverride);
            return Outputs;
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
        
            // 1) Process Note On triggers in this block
            InputNoteOn->ExecuteBlock(
                [](int32 /*StartFrame*/, int32 /*EndFrame*/){},
                [this](int32 StartFrame, int32 EndFrame)
                {
                    // Calculate our block-based sample index
                    int32 CurrentSample = AccumulatedSamples + StartFrame;
                    
                    // Create a new voice entry
                    FVoiceState NewVoice;
                    NewVoice.bActive = true;
                    // Schedule note end in samples
                    NewVoice.NoteEndSample = CurrentSample + static_cast<int32>(InputDuration->GetSeconds() * SampleRate);

                    ActiveVoices.Add(NewVoice);

                    // Output pitch/velocity to the array
                    (*OutputArray)[0] = *InputPitch;
                    (*OutputArray)[1] = *InputVelocity;

                    // Fire note-on trigger at StartFrame
                    OutputNoteOn->TriggerFrame(StartFrame);
                }
            );

            // 2) Process Note Off overrides in this block
            InputNoteOff->ExecuteBlock(
                [](int32 /*StartFrame*/, int32 /*EndFrame*/){},
                [this](int32 StartFrame, int32 EndFrame)
                {
                    for (int32 i = ActiveVoices.Num() - 1; i >= 0; --i)
                    {
                        if (ActiveVoices[i].bActive)
                        {
                            ActiveVoices[i].bActive = false;
                            // Velocity = 0
                            (*OutputArray)[0] = *InputPitch;
                            (*OutputArray)[1] = 0.0f;

                            // Fire note-off & override triggers
                            OutputNoteOff->TriggerFrame(StartFrame);
                            OutputOverride->TriggerFrame(StartFrame);

                            // Remove the voice
                            ActiveVoices.RemoveAtSwap(i);
                        }
                    }
                }
            );

            // 3) Check for auto note-offs
            // By the end of this block, we've advanced AccumulatedSamples + FramesPerBlock
            // so let's test it at a block boundary.
            int32 EndOfBlockSample = AccumulatedSamples + FramesPerBlock;

            for (int32 i = ActiveVoices.Num() - 1; i >= 0; --i)
            {
                if (EndOfBlockSample >= ActiveVoices[i].NoteEndSample)
                {
                    // The note has expired
                    ActiveVoices[i].bActive = false;

                    // Output velocity = 0
                    (*OutputArray)[0] = *InputPitch;
                    (*OutputArray)[1] = 0.0f;

                    // Fire note-off. We'll do it at the block boundary (0 frame offset).
                    OutputNoteOff->TriggerFrame(0);

                    ActiveVoices.RemoveAtSwap(i);
                }
            }

            // Increment total samples processed by one block
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
        FTriggerWriteRef OutputOverride;
    
        // Voice Management
        TArray<FVoiceState> ActiveVoices;

        // Block / Timing
        int32 FramesPerBlock  = 0;
        float SampleRate      = 0.f;
        int32 AccumulatedSamples; // Runs continuously block to block
        
    };

    
    class FMakeNoteFloatNode : public FNodeFacade
    {
    public:
        FMakeNoteFloatNode(const FNodeInitData& InitData)
            : FNodeFacade(InitData.InstanceName, InitData.InstanceID, TFacadeOperatorClass<FMakeNoteFloatOperator>())
        {
        }
    };
    
    METASOUND_REGISTER_NODE(FMakeNoteFloatNode);
}

#undef LOCTEXT_NAMESPACE