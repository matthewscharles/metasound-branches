// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundTuningFromCentArrayNode.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundPrimitives.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"
#include "MetasoundBranches/Public/MetasoundCommonMacros.h"

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_TuningFromCentArrayNode"

namespace Metasound
{
    namespace TuningFromCentArrayNodeVertexNames
    {
        METASOUND_PARAM(InputUpdateTrigger, "Trigger", "Triggers output.");
        METASOUND_PARAM(InputMIDINoteNumber, "MIDI Note Number", "Input MIDI note number (integer).");
        METASOUND_PARAM(InputTuningCentsArray, "Tuning Cents Array", "Array of tuning adjustments in cents for each note in the octave.");
        METASOUND_PARAM(InputReferenceFrequency, "Reference Frequency", "Reference frequency in Hz.");
        METASOUND_PARAM(InputReferenceMIDINote, "Reference Note", "Reference MIDI note number.");
        METASOUND_PARAM(OutputFrequency, "Frequency", "Output frequency.");
    }

    class FTuningFromCentArrayNodeOperator : public TExecutableOperator<FTuningFromCentArrayNodeOperator>
    {
    public:
        FTuningFromCentArrayNodeOperator(
            const FOperatorSettings& InSettings,
            const FInt32ReadRef& InMIDINoteNumber,
            const TDataReadReference<TArray<float>>& InTuningCentsArray,
            const FFloatReadRef& InReferenceFrequency,
            const FInt32ReadRef& InReferenceMIDINote,
            const FTriggerReadRef& InUpdateTrigger)
            : UpdateTrigger(InUpdateTrigger)
            , MIDINoteNumber(InMIDINoteNumber)
            , TuningCentsArray(InTuningCentsArray)
            , ReferenceFrequency(InReferenceFrequency)
            , ReferenceMIDINote(InReferenceMIDINote)
            , OutputFrequency(FFloatWriteRef::CreateNew(0.0f))
        {
        }

        static const FVertexInterface& DeclareVertexInterface()
        {
            using namespace TuningFromCentArrayNodeVertexNames;

            static const FVertexInterface Interface(
                FInputVertexInterface(
                    TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputUpdateTrigger)),
                    TInputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputMIDINoteNumber)),
                    TInputDataVertex<TArray<float>>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTuningCentsArray)),
                    TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputReferenceFrequency), 440.0f),
                    TInputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputReferenceMIDINote), 69)
                ),
                FOutputVertexInterface(
                    TOutputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputFrequency))
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

                Metadata.ClassName = { TEXT("UE"), TEXT("Tuning From Cent Array"), TEXT("Float") };
                Metadata.MajorVersion = 1;
                Metadata.MinorVersion = 0;
                Metadata.DisplayName = METASOUND_LOCTEXT("TuningFromCentArrayNodeDisplayName", "Tuning From Cent Array");
                Metadata.Description = METASOUND_LOCTEXT("TuningFromCentArrayNodeDesc", "Generates a frequency based on custom tuning per-note, based on an array of offset values in cents.");
                Metadata.Author = "Charles Matthews";
                Metadata.PromptIfMissing = PluginNodeMissingPrompt;
                Metadata.DefaultInterface = NodeInterface;
                Metadata.CategoryHierarchy = {
                    METASOUND_LOCTEXT("Custom", "Branches"),
                    METASOUND_LOCTEXT("CustomSub", "Tuning")
                };
                Metadata.Keywords = TArray<FText>();

                return Metadata;
            };

            static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
            return Metadata;
        }
        
        METASOUND_DISABLE_LEGACY_IO()
        
        virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
        {
            using namespace TuningFromCentArrayNodeVertexNames;

            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputUpdateTrigger), UpdateTrigger);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputMIDINoteNumber), MIDINoteNumber);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputTuningCentsArray), TuningCentsArray);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputReferenceFrequency), ReferenceFrequency);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputReferenceMIDINote), ReferenceMIDINote);
        }
        
        virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
        {
            using namespace TuningFromCentArrayNodeVertexNames;
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputFrequency), OutputFrequency);
        }

        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
        {
            using namespace TuningFromCentArrayNodeVertexNames;

            const FInputVertexInterfaceData& InputData = InParams.InputData;

            TDataReadReference<int32> MIDINoteNumber = InputData.GetOrCreateDefaultDataReadReference<int32>(
                METASOUND_GET_PARAM_NAME(InputMIDINoteNumber), InParams.OperatorSettings);

            TDataReadReference<TArray<float>> TuningCentsArray = InputData.GetOrCreateDefaultDataReadReference<TArray<float>>(
                METASOUND_GET_PARAM_NAME(InputTuningCentsArray), 
                InParams.OperatorSettings
            );

            FFloatReadRef ReferenceFrequency = InputData.GetOrCreateDefaultDataReadReference<float>(
                METASOUND_GET_PARAM_NAME(InputReferenceFrequency), InParams.OperatorSettings);

            TDataReadReference<int32> ReferenceMIDINote = InputData.GetOrCreateDefaultDataReadReference<int32>(
                METASOUND_GET_PARAM_NAME(InputReferenceMIDINote), InParams.OperatorSettings);

            FTriggerReadRef UpdateTrigger = InputData.GetOrCreateDefaultDataReadReference<FTrigger>(
                METASOUND_GET_PARAM_NAME(InputUpdateTrigger), InParams.OperatorSettings);

            return MakeUnique<FTuningFromCentArrayNodeOperator>(
                InParams.OperatorSettings, MIDINoteNumber, TuningCentsArray, ReferenceFrequency, ReferenceMIDINote, UpdateTrigger
            );
        }

        void Execute()
        {
            UpdateTrigger->ExecuteBlock(
                [](int32, int32) {}, 
                [this](int32 StartFrame, int32) 
                {
                    TArray<float> LocalTuningCentsArray = *TuningCentsArray; // mutable copy

                    if (LocalTuningCentsArray.Num() < 12)
                    {
                        LocalTuningCentsArray.SetNumZeroed(12);
                    }

                    int32 midiNote = *MIDINoteNumber;
                    int32 noteInOctave = midiNote % 12;

                    float tuningAdjustmentCents = (LocalTuningCentsArray.Num() > noteInOctave) 
                                                    ? LocalTuningCentsArray[noteInOctave] 
                                                    : 0.0f;

                    float tuningAdjustmentSemitones = tuningAdjustmentCents / 100.0f;
                    
                    float adjustedNote = midiNote + tuningAdjustmentSemitones;
                    float frequency = *ReferenceFrequency * powf(2.0f, (adjustedNote - *ReferenceMIDINote) / 12.0f);

                    *OutputFrequency = frequency;
                }
            );
        }

    private:
        FTriggerReadRef UpdateTrigger;
        FInt32ReadRef MIDINoteNumber;
        TDataReadReference<TArray<float>> TuningCentsArray;
        FFloatReadRef ReferenceFrequency;
        FInt32ReadRef ReferenceMIDINote;
        FFloatWriteRef OutputFrequency;
    };

    class FTuningFromCentArrayNode : public FNodeFacade
    {
    public:
        FTuningFromCentArrayNode(const FNodeInitData& InitData)
            : FNodeFacade(InitData.InstanceName, InitData.InstanceID, TFacadeOperatorClass<FTuningFromCentArrayNodeOperator>())
        {
        }
    };
    
    METASOUND_REGISTER_NODE(FTuningFromCentArrayNode);
}

#undef LOCTEXT_NAMESPACE