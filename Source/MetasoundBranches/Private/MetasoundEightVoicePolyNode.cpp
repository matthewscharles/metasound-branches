// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundEightVoicePolyNode.h"
#include "CoreMinimal.h"
#include "MetasoundBuilderInterface.h"
#include "MetasoundDataReferenceCollection.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundNode.h"
#include "MetasoundNodeInterface.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundOperatorInterface.h"
#include "MetasoundPrimitives.h"
#include "MetasoundParamHelper.h"
#include "MetasoundTime.h"

#define LOCTEXT_NAMESPACE "MEightVoicePolyNode"

namespace Metasound
{
    namespace EightVoicePolyNames
    {
        METASOUND_PARAM(InputTriggerNote, "Note", "Triggers a new note allocation.");
        METASOUND_PARAM(InputNoteData, "Note Data", "Incoming float array [pitch, velocity].");
        METASOUND_PARAM(InputNumVoices, "Voices", "Number of voices (1-8).");
        METASOUND_PARAM(InputTriggerFlush, "Flush", "Flush all voices (clear).");
        METASOUND_PARAM(InputRoundRobin, "Round Robin", "True: round-robin allocation; False: LIFO.");

        METASOUND_PARAM(OutputVoice0Trig, "0 Trig", "Note trigger for voice 0.");
        METASOUND_PARAM(OutputVoice1Trig, "1 Trig", "Note trigger for voice 1.");
        METASOUND_PARAM(OutputVoice2Trig, "2 Trig", "Note trigger for voice 2.");
        METASOUND_PARAM(OutputVoice3Trig, "3 Trig", "Note trigger for voice 3.");
        METASOUND_PARAM(OutputVoice4Trig, "4 Trig", "Note trigger for voice 4.");
        METASOUND_PARAM(OutputVoice5Trig, "5 Trig", "Note trigger for voice 5.");
        METASOUND_PARAM(OutputVoice6Trig, "6 Trig", "Note trigger for voice 6.");
        METASOUND_PARAM(OutputVoice7Trig, "7 Trig", "Note trigger for voice 7.");

        METASOUND_PARAM(OutputVoice0Array, "0 Data", "Note array for voice 0.");
        METASOUND_PARAM(OutputVoice1Array, "1 Data", "Note array for voice 1.");
        METASOUND_PARAM(OutputVoice2Array, "2 Data", "Note array for voice 2.");
        METASOUND_PARAM(OutputVoice3Array, "3 Data", "Note array for voice 3.");
        METASOUND_PARAM(OutputVoice4Array, "4 Data", "Note array for voice 4.");
        METASOUND_PARAM(OutputVoice5Array, "5 Data", "Note array for voice 5.");
        METASOUND_PARAM(OutputVoice6Array, "6 Data", "Note array for voice 6.");
        METASOUND_PARAM(OutputVoice7Array, "7 Data", "Note array for voice 7.");

        METASOUND_PARAM(OutputActiveVoices, "Active Voices", "How many voices are currently in use.");
        METASOUND_PARAM(OutputOnFlush, "On Flush", "Triggers when flush occurs.");
    }

    struct FEightVoiceState
    {
        bool bActive = false;
        float Pitch = 0.f;
        float Velocity = 0.f;
    };

    class FEightVoicePolyOperator : public TExecutableOperator<FEightVoicePolyOperator>
    {
    public:
        FEightVoicePolyOperator(
            const FOperatorSettings &InSettings,
            const FTriggerReadRef &InNoteTrigger,
            const TDataReadReference<TArray<float>> &InNoteData,
            const FInt32ReadRef &InNumVoices,
            const FTriggerReadRef &InFlushTrigger,
            const FBoolReadRef &InRoundRobin)
            : NoteTrigger(InNoteTrigger)
            , NoteData(InNoteData)
            , NumVoices(InNumVoices)
            , FlushTrigger(InFlushTrigger)
            , bRoundRobin(InRoundRobin)
            , VoiceTriggers{
                FTriggerWriteRef::CreateNew(InSettings), // 0
                FTriggerWriteRef::CreateNew(InSettings), // 1
                FTriggerWriteRef::CreateNew(InSettings), // 2
                FTriggerWriteRef::CreateNew(InSettings), // 3
                FTriggerWriteRef::CreateNew(InSettings), // 4
                FTriggerWriteRef::CreateNew(InSettings), // 5
                FTriggerWriteRef::CreateNew(InSettings), // 6
                FTriggerWriteRef::CreateNew(InSettings)  // 7
            },
              VoiceArrays{TDataWriteReference<TArray<float>>::CreateNew(), TDataWriteReference<TArray<float>>::CreateNew(), TDataWriteReference<TArray<float>>::CreateNew(), TDataWriteReference<TArray<float>>::CreateNew(), TDataWriteReference<TArray<float>>::CreateNew(), TDataWriteReference<TArray<float>>::CreateNew(), TDataWriteReference<TArray<float>>::CreateNew(), TDataWriteReference<TArray<float>>::CreateNew()}, OutputActiveVoices(FInt32WriteRef::CreateNew(0)), OutputOnFlush(FTriggerWriteRef::CreateNew(InSettings)), NextRoundRobinIndex(0)
        {
            // Initialize each TArray to size 2  (pitch/velocity)
            for (int32 i = 0; i < 8; i++)
            {
                VoiceArrays[i]->Init(0, 2);
            }

            // Initialize all voices (inactive)
            for (int32 i = 0; i < 8; ++i)
            {
                Voices[i].bActive = false;
                Voices[i].Pitch = 0.f;
                Voices[i].Velocity = 0.f;
            }
        }
        
        static const FVertexInterface& DeclareVertexInterface()
        {
            using namespace EightVoicePolyNames;

            static const FVertexInterface Interface(
                FInputVertexInterface(
                    TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTriggerNote)),
                    TInputDataVertex<TArray<float>>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputNoteData)),
                    TInputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputNumVoices), 8),
                    TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTriggerFlush)),
                    TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputRoundRobin), true)),
                FOutputVertexInterface(
                    TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputVoice0Trig)),
                    TOutputDataVertex<TArray<float>>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputVoice0Array)),
                    
                    TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputVoice1Trig)),
                    TOutputDataVertex<TArray<float>>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputVoice1Array)),
                    
                    TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputVoice2Trig)),
                    TOutputDataVertex<TArray<float>>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputVoice2Array)),
                    
                    TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputVoice3Trig)),
                    TOutputDataVertex<TArray<float>>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputVoice3Array)),
                    
                    TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputVoice4Trig)),
                    TOutputDataVertex<TArray<float>>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputVoice4Array)),
                    
                    TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputVoice5Trig)),
                    TOutputDataVertex<TArray<float>>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputVoice5Array)),
                    
                    TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputVoice6Trig)),
                    TOutputDataVertex<TArray<float>>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputVoice6Array)),
                    
                    TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputVoice7Trig)),
                    TOutputDataVertex<TArray<float>>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputVoice7Array)),


                    TOutputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputActiveVoices)),
                    TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputOnFlush))));

            return Interface;
        }
        
        // Metadata about the node
        static const FNodeClassMetadata& GetNodeInfo()
        {
            auto CreateNodeClassMetadata = []() -> FNodeClassMetadata
            {
                FNodeClassMetadata Metadata;
                Metadata.ClassName = {TEXT("UE"), TEXT("EightVoicePoly"), TEXT("Float")};
                Metadata.MajorVersion = 1;
                Metadata.MinorVersion = 1;
                Metadata.DisplayName = LOCTEXT("EightVoicePolyNodeDisplayName", "Eight-Voice Poly");
                Metadata.Description = LOCTEXT("EightVoicePolyNodeDesc", "Manages up to 8 voices with round-robin or LIFO allocation.");
                Metadata.Author = "Charles Matthews";
                Metadata.PromptIfMissing = PluginNodeMissingPrompt;
                Metadata.DefaultInterface = DeclareVertexInterface();
                Metadata.CategoryHierarchy = {METASOUND_LOCTEXT("Custom", "Branches")};
                Metadata.Keywords = TArray<FText>(); // Keywords for searching

                return Metadata;
            };

            static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
            return Metadata;
        }

        virtual FDataReferenceCollection GetInputs() const override
        {
            using namespace EightVoicePolyNames;
            FDataReferenceCollection Inputs;
            Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputTriggerNote), NoteTrigger);
            Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputNoteData), NoteData);
            Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputNumVoices), NumVoices);
            Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputTriggerFlush), FlushTrigger);
            Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputRoundRobin), bRoundRobin);
            return Inputs;
        }

        virtual FDataReferenceCollection GetOutputs() const override
        {
            using namespace EightVoicePolyNames;
            FDataReferenceCollection Outputs;
            Outputs.AddDataWriteReference(METASOUND_GET_PARAM_NAME(OutputVoice0Trig), VoiceTriggers[0]);
            Outputs.AddDataWriteReference(METASOUND_GET_PARAM_NAME(OutputVoice0Array), VoiceArrays[0]);
            
            Outputs.AddDataWriteReference(METASOUND_GET_PARAM_NAME(OutputVoice1Trig), VoiceTriggers[1]);
            Outputs.AddDataWriteReference(METASOUND_GET_PARAM_NAME(OutputVoice1Array), VoiceArrays[1]);
            
            Outputs.AddDataWriteReference(METASOUND_GET_PARAM_NAME(OutputVoice2Trig), VoiceTriggers[2]);
            Outputs.AddDataWriteReference(METASOUND_GET_PARAM_NAME(OutputVoice2Array), VoiceArrays[2]);
            
            Outputs.AddDataWriteReference(METASOUND_GET_PARAM_NAME(OutputVoice3Trig), VoiceTriggers[3]);
            Outputs.AddDataWriteReference(METASOUND_GET_PARAM_NAME(OutputVoice3Array), VoiceArrays[3]);
            
            Outputs.AddDataWriteReference(METASOUND_GET_PARAM_NAME(OutputVoice4Trig), VoiceTriggers[4]);
            Outputs.AddDataWriteReference(METASOUND_GET_PARAM_NAME(OutputVoice4Array), VoiceArrays[4]);
            
            Outputs.AddDataWriteReference(METASOUND_GET_PARAM_NAME(OutputVoice5Trig), VoiceTriggers[5]);
            Outputs.AddDataWriteReference(METASOUND_GET_PARAM_NAME(OutputVoice5Array), VoiceArrays[5]);
            
            Outputs.AddDataWriteReference(METASOUND_GET_PARAM_NAME(OutputVoice6Trig), VoiceTriggers[6]);
            Outputs.AddDataWriteReference(METASOUND_GET_PARAM_NAME(OutputVoice6Array), VoiceArrays[6]);
            
            Outputs.AddDataWriteReference(METASOUND_GET_PARAM_NAME(OutputVoice7Trig), VoiceTriggers[7]);
            Outputs.AddDataWriteReference(METASOUND_GET_PARAM_NAME(OutputVoice7Array), VoiceArrays[7]);

            Outputs.AddDataWriteReference(METASOUND_GET_PARAM_NAME(OutputActiveVoices), OutputActiveVoices);
            Outputs.AddDataWriteReference(METASOUND_GET_PARAM_NAME(OutputOnFlush), OutputOnFlush);
            return Outputs;
        }

        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams &InParams, FBuildResults &OutErrors)
        {
            using namespace EightVoicePolyNames;

            const FInputVertexInterfaceData& InputData = InParams.InputData;

            FTriggerReadRef NoteTrig = InputData.GetOrCreateDefaultDataReadReference<FTrigger>(METASOUND_GET_PARAM_NAME(InputTriggerNote), InParams.OperatorSettings);
            TDataReadReference<TArray<float>> NoteArr = InputData.GetOrCreateDefaultDataReadReference<TArray<float>>(METASOUND_GET_PARAM_NAME(InputNoteData), InParams.OperatorSettings);
            FInt32ReadRef NumV = InputData.GetOrCreateDefaultDataReadReference<int32>(METASOUND_GET_PARAM_NAME(InputNumVoices), InParams.OperatorSettings);
            FTriggerReadRef FlushTrig = InputData.GetOrCreateDefaultDataReadReference<FTrigger>(METASOUND_GET_PARAM_NAME(InputTriggerFlush), InParams.OperatorSettings);
            FBoolReadRef RoundRob = InputData.GetOrCreateDefaultDataReadReference<bool>(METASOUND_GET_PARAM_NAME(InputRoundRobin), InParams.OperatorSettings);

            return MakeUnique<FEightVoicePolyOperator>(
                InParams.OperatorSettings,
                NoteTrig,
                NoteArr,
                NumV,
                FlushTrig,
                RoundRob);
        }
        // Audio block execution
        virtual void Execute()
        {
            // Advance triggers each block
            for (int32 i = 0; i < 8; i++)
            {
                VoiceTriggers[i]->AdvanceBlock();
            }
            OutputOnFlush->AdvanceBlock();

            // 1) Handle note flush
            FlushTrigger->ExecuteBlock(
                [](int32 StartFrame, int32 EndFrame) {},
                [this](int32 StartFrame, int32 EndFrame)
                {
                    // Clear all voices
                    for (int32 v = 0; v < 8; ++v)
                    {
                        Voices[v].bActive = false;
                        Voices[v].Pitch = 0.f;
                        Voices[v].Velocity = 0.f;

                        // Fire note-off for all voices
                        (*VoiceArrays[v])[0] = Voices[v].Pitch;
                        (*VoiceArrays[v])[1] = 0.0f;
                        VoiceTriggers[v]->TriggerFrame(StartFrame);
                    }
                    // Fire flush trigger
                    OutputOnFlush->TriggerFrame(StartFrame);
                });

            // 2) Handle note triggers (now checks for zero-velocity note-offs)
            NoteTrigger->ExecuteBlock(
                [](int32 StartFrame, int32 EndFrame) {},
                [this](int32 StartFrame, int32 EndFrame)
                {
                    // Ensure the incoming array has at least two elements
                    if (NoteData->Num() >= 2)
                    {
                        float Pitch = (*NoteData)[0];
                        float Velocity = (*NoteData)[1];

                    // Check if this is a note-off event (velocity == 0)
                    if (Velocity == 0.0f)
                    {
                        bool FoundActiveNote = false;

                        // Find all active voices that match this pitch
                        for (int32 i = 0; i < 8; i++)
                        {
                            if (Voices[i].bActive && Voices[i].Pitch == Pitch)
                            {
                                Voices[i].bActive = false;
                                Voices[i].Velocity = 0.0f;

                                // Output note-off data
                                (*VoiceArrays[i])[0] = Pitch;
                                (*VoiceArrays[i])[1] = 0.0f;

                                // Trigger note-off event
                                VoiceTriggers[i]->TriggerFrame(StartFrame);

                                FoundActiveNote = true;
                            }
                        }

                        return; 
                    }
                        else // Normal note-on behavior
                        {
                            // Determine max voices user wants
                            int32 MaxVoices = FMath::Clamp(*NumVoices, 1, 8);

                            // Acquire a voice index
                            int32 VoiceIndex = AcquireVoiceIndex(MaxVoices);

                            // Store new note data
                            Voices[VoiceIndex].bActive = true;
                            Voices[VoiceIndex].Pitch = Pitch;
                            Voices[VoiceIndex].Velocity = Velocity;

                            // Output to the array
                            (*VoiceArrays[VoiceIndex])[0] = Pitch;
                            (*VoiceArrays[VoiceIndex])[1] = Velocity;

                            // Trigger the voice
                            VoiceTriggers[VoiceIndex]->TriggerFrame(StartFrame);
                        }
                    }
                });

            // 3) Calculate number of active voices
            int32 ActiveCount = 0;
            for (int32 i = 0; i < 8; i++)
            {
                if (Voices[i].bActive)
                {
                    ActiveCount++;
                }
            }
            *OutputActiveVoices = ActiveCount;
        }

        // Acquire a voice index based on Round-Robin or LIFO
        int32 AcquireVoiceIndex(int32 MaxVoices)
        {
            // 1) Try finding a free voice
            for (int32 i = 0; i < MaxVoices; i++)
            {
                int32 index = (*bRoundRobin ? (NextRoundRobinIndex + i) % MaxVoices : i);
                if (!Voices[index].bActive)
                {
                    if (*bRoundRobin)
                    {
                        NextRoundRobinIndex = (index + 1) % MaxVoices;
                    }
                    return index;
                }
            }

            // 2) No free voice found -> voice theft
            if (*bRoundRobin)
            {
                // Round-robin: just pick NextRoundRobinIndex
                int32 StealIndex = NextRoundRobinIndex;
                NextRoundRobinIndex = (NextRoundRobinIndex + 1) % MaxVoices;
                return StealIndex;
            }
            else
            {
                // LIFO: steal the highest voice index used
                // i.e., from the top if any
                return MaxVoices - 1;
            }
        }

    private:
        // Inputs
        FTriggerReadRef NoteTrigger;
        TDataReadReference<TArray<float>> NoteData;
        FInt32ReadRef NumVoices;
        FTriggerReadRef FlushTrigger;
        FBoolReadRef bRoundRobin;

        // Outputs
        FTriggerWriteRef VoiceTriggers[8];
        TDataWriteReference<TArray<float>> VoiceArrays[8];
        FInt32WriteRef OutputActiveVoices;
        FTriggerWriteRef OutputOnFlush;

        // Internal
        FEightVoiceState Voices[8];
        int32 NextRoundRobinIndex;
    };

    class FEightVoicePolyNode : public FNodeFacade
    {
    public:
        FEightVoicePolyNode(const FNodeInitData& InitData)
            : FNodeFacade(
                  InitData.InstanceName,
                  InitData.InstanceID,
                  TFacadeOperatorClass<FEightVoicePolyOperator>())
        {
        }
    };

    // Register the node
    METASOUND_REGISTER_NODE(FEightVoicePolyNode)
}

#undef LOCTEXT_NAMESPACE