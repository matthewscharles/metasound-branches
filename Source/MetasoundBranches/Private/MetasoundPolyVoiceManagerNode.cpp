// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundPolyVoiceManagerNode.h"
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

#define LOCTEXT_NAMESPACE "MPolyVoiceManagerNode"

namespace Metasound
{
    namespace PolyVoiceManagerNames
    {
        METASOUND_PARAM(InputTriggerNote, "Note", "Triggers a new note allocation.");
        METASOUND_PARAM(InputNoteData, "Note Data", "Incoming float array [pitch, velocity].");
        METASOUND_PARAM(InputNumVoices, "Voices", "Number of voices (1-8).");
        METASOUND_PARAM(InputTriggerFlush, "Flush", "Flush all voices (clear).");
        METASOUND_PARAM(InputRoundRobin, "Round Robin", "True: round-robin allocation; False: LIFO.");

        METASOUND_PARAM(OutputVoice0Trig, "Trig 0", "Note trigger for voice 0.");
        METASOUND_PARAM(OutputVoice1Trig, "Trig 1", "Note trigger for voice 1.");
        METASOUND_PARAM(OutputVoice2Trig, "Trig 2", "Note trigger for voice 2.");
        METASOUND_PARAM(OutputVoice3Trig, "Trig 3", "Note trigger for voice 3.");
        METASOUND_PARAM(OutputVoice4Trig, "Trig 4", "Note trigger for voice 4.");
        METASOUND_PARAM(OutputVoice5Trig, "Trig 5", "Note trigger for voice 5.");
        METASOUND_PARAM(OutputVoice6Trig, "Trig 6", "Note trigger for voice 6.");
        METASOUND_PARAM(OutputVoice7Trig, "Trig 7", "Note trigger for voice 7.");

        METASOUND_PARAM(OutputVoice0Array, "Data 0", "Note array for voice 0.");
        METASOUND_PARAM(OutputVoice1Array, "Data 1", "Note array for voice 1.");
        METASOUND_PARAM(OutputVoice2Array, "Data 2", "Note array for voice 2.");
        METASOUND_PARAM(OutputVoice3Array, "Data 3", "Note array for voice 3.");
        METASOUND_PARAM(OutputVoice4Array, "Data 4", "Note array for voice 4.");
        METASOUND_PARAM(OutputVoice5Array, "Data 5", "Note array for voice 5.");
        METASOUND_PARAM(OutputVoice6Array, "Data 6", "Note array for voice 6.");
        METASOUND_PARAM(OutputVoice7Array, "Data 7", "Note array for voice 7.");

        METASOUND_PARAM(OutputActiveVoices, "Active Voices", "How many voices are currently in use.");
        METASOUND_PARAM(OutputOnFlush, "On Flush", "Triggers when flush occurs.");
    }

    struct FEightVoiceState
    {
        bool bActive = false;
        float Pitch = 0.f;
        float Velocity = 0.f;
    };

    class FPolyVoiceManagerOperator : public TExecutableOperator<FPolyVoiceManagerOperator>
    {
    public:
        FPolyVoiceManagerOperator(
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
            using namespace PolyVoiceManagerNames;

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
                Metadata.ClassName = {TEXT("UE"), TEXT("Voice Manager"), TEXT("Float")};
                Metadata.MajorVersion = 1;
                Metadata.MinorVersion = 1;
                Metadata.DisplayName = METASOUND_LOCTEXT("PolyVoiceManagerNodeDisplayName", "Voice Manager");
                Metadata.Description = METASOUND_LOCTEXT("PolyVoiceManagerNodeDesc", "Manages up to 8 voices with round-robin or LIFO allocation.");
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
        
        virtual void BindInputs(FInputVertexInterfaceData &InOutVertexData) override
        {
            using namespace PolyVoiceManagerNames;
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputTriggerNote), NoteTrigger);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputNoteData), NoteData);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputNumVoices), NumVoices);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputTriggerFlush), FlushTrigger);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputRoundRobin), bRoundRobin);
        }
        
        virtual void BindOutputs(FOutputVertexInterfaceData &InOutVertexData) override
        {
            using namespace PolyVoiceManagerNames;
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputVoice0Trig), VoiceTriggers[0]);
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputVoice0Array), VoiceArrays[0]);
            
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputVoice1Trig), VoiceTriggers[1]);
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputVoice1Array), VoiceArrays[1]);
            
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputVoice2Trig), VoiceTriggers[2]);
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputVoice2Array), VoiceArrays[2]);
            
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputVoice3Trig), VoiceTriggers[3]);
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputVoice3Array), VoiceArrays[3]);
            
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputVoice4Trig), VoiceTriggers[4]);
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputVoice4Array), VoiceArrays[4]);
            
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputVoice5Trig), VoiceTriggers[5]);
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputVoice5Array), VoiceArrays[5]);
            
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputVoice6Trig), VoiceTriggers[6]);
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputVoice6Array), VoiceArrays[6]);
            
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputVoice7Trig), VoiceTriggers[7]);
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputVoice7Array), VoiceArrays[7]);

            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputActiveVoices), OutputActiveVoices);
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputOnFlush), OutputOnFlush);
        }

        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams &InParams, FBuildResults &OutErrors)
        {
            using namespace PolyVoiceManagerNames;

            const FInputVertexInterfaceData& InputData = InParams.InputData;

            FTriggerReadRef NoteTrig = InputData.GetOrCreateDefaultDataReadReference<FTrigger>(METASOUND_GET_PARAM_NAME(InputTriggerNote), InParams.OperatorSettings);
            TDataReadReference<TArray<float>> NoteArr = InputData.GetOrCreateDefaultDataReadReference<TArray<float>>(METASOUND_GET_PARAM_NAME(InputNoteData), InParams.OperatorSettings);
            FInt32ReadRef NumV = InputData.GetOrCreateDefaultDataReadReference<int32>(METASOUND_GET_PARAM_NAME(InputNumVoices), InParams.OperatorSettings);
            FTriggerReadRef FlushTrig = InputData.GetOrCreateDefaultDataReadReference<FTrigger>(METASOUND_GET_PARAM_NAME(InputTriggerFlush), InParams.OperatorSettings);
            FBoolReadRef RoundRob = InputData.GetOrCreateDefaultDataReadReference<bool>(METASOUND_GET_PARAM_NAME(InputRoundRobin), InParams.OperatorSettings);

            return MakeUnique<FPolyVoiceManagerOperator>(
                InParams.OperatorSettings,
                NoteTrig,
                NoteArr,
                NumV,
                FlushTrig,
                RoundRob);
        }
        
        void Execute()
        {
            // Advance triggers
            for (int32 i = 0; i < 8; i++)
            {
                VoiceTriggers[i]->AdvanceBlock();
            }
            OutputOnFlush->AdvanceBlock();

            // 1) Handle flush
            FlushTrigger->ExecuteBlock(
                [](int32, int32) {},
                [this](int32 StartFrame, int32)
                {
                    // Clear all voices
                    for (int32 v = 0; v < 8; ++v)
                    {
                        Voices[v].bActive = false;
                        Voices[v].Pitch = 0.f;
                        Voices[v].Velocity = 0.f;

                        // Force a note-off trigger
                        (*VoiceArrays[v])[0] = Voices[v].Pitch;
                        (*VoiceArrays[v])[1] = 0.f;
                        VoiceTriggers[v]->TriggerFrame(StartFrame);
                    }
                    OutputOnFlush->TriggerFrame(StartFrame);
                });

            // 2) Process incoming pairs on NoteTrigger
            NoteTrigger->ExecuteBlock(
                [](int32, int32) {},
                [this](int32 StartFrame, int32)
                {
                    const int32 InSize = NoteData->Num();
                    if (InSize < 2)
                    {
                        return; // Not enough data
                    }

                    // i increments in steps of 2
                    for (int32 i = 0; i < InSize - 1; i += 2)
                    {
                        float Pitch = (*NoteData)[i];
                        float Velocity = (*NoteData)[i + 1];

                        if (Velocity == 0.f)
                        {
                            // note off
                            for (int32 v = 0; v < 8; v++)
                            {
                                if (Voices[v].bActive && Voices[v].Pitch == Pitch)
                                {
                                    Voices[v].bActive = false;
                                    Voices[v].Velocity = 0.f;
                                    (*VoiceArrays[v])[0] = Pitch;
                                    (*VoiceArrays[v])[1] = 0.f;
                                    VoiceTriggers[v]->TriggerFrame(StartFrame);
                                }
                            }
                        }
                        else
                        {
                            // note on
                            int32 MaxVoices = FMath::Clamp(*NumVoices, 1, 8);
                            int32 VoiceIndex = AcquireVoiceIndex(MaxVoices);

                            Voices[VoiceIndex].bActive = true;
                            Voices[VoiceIndex].Pitch = Pitch;
                            Voices[VoiceIndex].Velocity = Velocity;

                            (*VoiceArrays[VoiceIndex])[0] = Pitch;
                            (*VoiceArrays[VoiceIndex])[1] = Velocity;
                            VoiceTriggers[VoiceIndex]->TriggerFrame(StartFrame);
                        }
                    }
                }
            );

            // 3) Update ActiveCount
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
            if (*bRoundRobin)
            {
                // Round-robin approach
                for (int32 i = 0; i < MaxVoices; i++)
                {
                    // Start from NextRoundRobinIndex
                    int32 idx = (NextRoundRobinIndex + i) % MaxVoices;
                    if (!Voices[idx].bActive)
                    {
                        NextRoundRobinIndex = (idx + 1) % MaxVoices;
                        return idx;
                    }
                }
                // If no free voice, steal
                int32 StealIndex = NextRoundRobinIndex;
                NextRoundRobinIndex = (NextRoundRobinIndex + 1) % MaxVoices;
                return StealIndex;
            }
            else
            {
                // LIFO approach
                // Try to find a free voice from bottom up
                for (int32 i = 0; i < MaxVoices; i++)
                {
                    if (!Voices[i].bActive)
                    {
                        return i;
                    }
                }
                // Otherwise steal from top
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

    class FPolyVoiceManagerNode : public FNodeFacade
    {
    public:
        FPolyVoiceManagerNode(const FNodeInitData& InitData)
            : FNodeFacade(
                  InitData.InstanceName,
                  InitData.InstanceID,
                  TFacadeOperatorClass<FPolyVoiceManagerOperator>())
        {
        }
    };

    // Register the node
    METASOUND_REGISTER_NODE(FPolyVoiceManagerNode)
}

#undef LOCTEXT_NAMESPACE