// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundTuningNode.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundPrimitives.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_TuningNode"

namespace Metasound
{
    namespace TuningNodeVertexNames
    {
        METASOUND_PARAM(InputUpdateTrigger, "Trigger", "Triggers tuning update.");
        METASOUND_PARAM(InputMIDINoteNumber, "MIDI Note Number", "Input MIDI note number (integer).");
        METASOUND_PARAM(InputReferenceFrequency, "Reference Frequency", "Reference frequency in Hz.");
        METASOUND_PARAM(InputReferenceMIDINote, "Reference Note", "Reference MIDI note number.");
        METASOUND_PARAM(InputTuningCents0, "Cents 0", "C adjustment in cents");
        METASOUND_PARAM(InputTuningCents1, "Cents 1", "C♯ / D♭ adjustment in cents");
        METASOUND_PARAM(InputTuningCents2, "Cents 2", "D adjustment in cents");
        METASOUND_PARAM(InputTuningCents3, "Cents 3", "D♯ / E♭ adjustment in cents");
        METASOUND_PARAM(InputTuningCents4, "Cents 4", "E adjustment in cents");
        METASOUND_PARAM(InputTuningCents5, "Cents 5", "F adjustment in cents");
        METASOUND_PARAM(InputTuningCents6, "Cents 6", "F♯ / G♭ adjustment in cents");
        METASOUND_PARAM(InputTuningCents7, "Cents 7", "G adjustment in cents");
        METASOUND_PARAM(InputTuningCents8, "Cents 8", "G♯ / A♭ adjustment in cents");
        METASOUND_PARAM(InputTuningCents9, "Cents 9", "A adjustment in cents");
        METASOUND_PARAM(InputTuningCents10, "Cents 10", "A♯ / B♭ adjustment in cents");
        METASOUND_PARAM(InputTuningCents11, "Cents 11", "B adjustment in cents");
        METASOUND_PARAM(OutputFrequency, "Frequency", "Output frequency.");
    }

    class FTuningNodeOperator : public TExecutableOperator<FTuningNodeOperator>
    {
    public:
        FTuningNodeOperator(
            const FOperatorSettings& InSettings,
            const FTriggerReadRef& InUpdateTrigger,
            const FInt32ReadRef& InMIDINoteNumber,
            const FFloatReadRef& InReferenceFrequency,
            const FInt32ReadRef& InReferenceMIDINote,
            const FFloatReadRef& InTuningCents0,
            const FFloatReadRef& InTuningCents1,
            const FFloatReadRef& InTuningCents2,
            const FFloatReadRef& InTuningCents3,
            const FFloatReadRef& InTuningCents4,
            const FFloatReadRef& InTuningCents5,
            const FFloatReadRef& InTuningCents6,
            const FFloatReadRef& InTuningCents7,
            const FFloatReadRef& InTuningCents8,
            const FFloatReadRef& InTuningCents9,
            const FFloatReadRef& InTuningCents10,
            const FFloatReadRef& InTuningCents11)
            : UpdateTrigger(InUpdateTrigger)
            , MIDINoteNumber(InMIDINoteNumber)
            , ReferenceFrequency(InReferenceFrequency)
            , ReferenceMIDINote(InReferenceMIDINote)
            , TuningCents0(InTuningCents0)
            , TuningCents1(InTuningCents1)
            , TuningCents2(InTuningCents2)
            , TuningCents3(InTuningCents3)
            , TuningCents4(InTuningCents4)
            , TuningCents5(InTuningCents5)
            , TuningCents6(InTuningCents6)
            , TuningCents7(InTuningCents7)
            , TuningCents8(InTuningCents8)
            , TuningCents9(InTuningCents9)
            , TuningCents10(InTuningCents10)
            , TuningCents11(InTuningCents11)
            , OutputFrequency(FFloatWriteRef::CreateNew(0.0f))
        {
        }

        static const FVertexInterface& DeclareVertexInterface()
        {
            using namespace TuningNodeVertexNames;

            static const FVertexInterface Interface(
                FInputVertexInterface(
                    TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputUpdateTrigger)),
                    TInputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputMIDINoteNumber)),
                    TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputReferenceFrequency), 440.0f),
                    TInputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputReferenceMIDINote), 69),
                    TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTuningCents0)),
                    TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTuningCents1)),
                    TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTuningCents2)),
                    TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTuningCents3)),
                    TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTuningCents4)),
                    TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTuningCents5)),
                    TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTuningCents6)),
                    TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTuningCents7)),
                    TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTuningCents8)),
                    TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTuningCents9)),
                    TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTuningCents10)),
                    TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTuningCents11))
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

                Metadata.ClassName = { TEXT("UE"), TEXT("Tuning"), TEXT("Float") };
                Metadata.MajorVersion = 1;
                Metadata.MinorVersion = 1;
                Metadata.DisplayName = METASOUND_LOCTEXT("TuningNodeDisplayName", "Tuning");
                Metadata.Description = METASOUND_LOCTEXT("TuningNodeDesc", "Generates a frequency based on custom tuning per-note.");
                Metadata.Author = "Charles Matthews";
                Metadata.PromptIfMissing = PluginNodeMissingPrompt;
                Metadata.DefaultInterface = NodeInterface;
                Metadata.CategoryHierarchy = { METASOUND_LOCTEXT("Custom", "Branches") };
                Metadata.Keywords = TArray<FText>();

                return Metadata;
            };

            static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
            return Metadata;
        }
        
        virtual FDataReferenceCollection GetInputs() const override
        {
            using namespace TuningNodeVertexNames;

            FDataReferenceCollection InputDataReferences;

            InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputUpdateTrigger), UpdateTrigger);
            InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputMIDINoteNumber), MIDINoteNumber);
            InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputReferenceFrequency), ReferenceFrequency);
            InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputReferenceMIDINote), ReferenceMIDINote);
            InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputTuningCents0), TuningCents0);
            InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputTuningCents1), TuningCents1);
            InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputTuningCents2), TuningCents2);
            InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputTuningCents3), TuningCents3);
            InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputTuningCents4), TuningCents4);
            InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputTuningCents5), TuningCents5);
            InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputTuningCents6), TuningCents6);
            InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputTuningCents7), TuningCents7);
            InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputTuningCents8), TuningCents8);
            InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputTuningCents9), TuningCents9);
            InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputTuningCents10), TuningCents10);
            InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputTuningCents11), TuningCents11);

            return InputDataReferences;
        }

        virtual FDataReferenceCollection GetOutputs() const override
        {
            using namespace TuningNodeVertexNames;

            FDataReferenceCollection OutputDataReferences;
            OutputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutputFrequency), OutputFrequency);
            return OutputDataReferences;
        }

        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
        {
            using namespace TuningNodeVertexNames;

            const FInputVertexInterfaceData& InputData = InParams.InputData;

            TDataReadReference<FTrigger> UpdateTrigger = InputData.GetOrCreateDefaultDataReadReference<FTrigger>(
                METASOUND_GET_PARAM_NAME(InputUpdateTrigger), InParams.OperatorSettings);

            TDataReadReference<int32> MIDINoteNumber = InputData.GetOrCreateDefaultDataReadReference<int32>(
                METASOUND_GET_PARAM_NAME(InputMIDINoteNumber), InParams.OperatorSettings);

            TDataReadReference<float> ReferenceFrequency = InputData.GetOrCreateDefaultDataReadReference<float>(
                METASOUND_GET_PARAM_NAME(InputReferenceFrequency), InParams.OperatorSettings);

            TDataReadReference<int32> ReferenceMIDINote = InputData.GetOrCreateDefaultDataReadReference<int32>(
                METASOUND_GET_PARAM_NAME(InputReferenceMIDINote), InParams.OperatorSettings);

            TDataReadReference<float> TuningCents0 = InputData.GetOrCreateDefaultDataReadReference<float>(
                METASOUND_GET_PARAM_NAME(InputTuningCents0), InParams.OperatorSettings);
            TDataReadReference<float> TuningCents1 = InputData.GetOrCreateDefaultDataReadReference<float>(
                METASOUND_GET_PARAM_NAME(InputTuningCents1), InParams.OperatorSettings);
            TDataReadReference<float> TuningCents2 = InputData.GetOrCreateDefaultDataReadReference<float>(
                METASOUND_GET_PARAM_NAME(InputTuningCents2), InParams.OperatorSettings);
            TDataReadReference<float> TuningCents3 = InputData.GetOrCreateDefaultDataReadReference<float>(
                METASOUND_GET_PARAM_NAME(InputTuningCents3), InParams.OperatorSettings);
            TDataReadReference<float> TuningCents4 = InputData.GetOrCreateDefaultDataReadReference<float>(
                METASOUND_GET_PARAM_NAME(InputTuningCents4), InParams.OperatorSettings);
            TDataReadReference<float> TuningCents5 = InputData.GetOrCreateDefaultDataReadReference<float>(
                METASOUND_GET_PARAM_NAME(InputTuningCents5), InParams.OperatorSettings);
            TDataReadReference<float> TuningCents6 = InputData.GetOrCreateDefaultDataReadReference<float>(
                METASOUND_GET_PARAM_NAME(InputTuningCents6), InParams.OperatorSettings);
            TDataReadReference<float> TuningCents7 = InputData.GetOrCreateDefaultDataReadReference<float>(
                METASOUND_GET_PARAM_NAME(InputTuningCents7), InParams.OperatorSettings);
            TDataReadReference<float> TuningCents8 = InputData.GetOrCreateDefaultDataReadReference<float>(
                METASOUND_GET_PARAM_NAME(InputTuningCents8), InParams.OperatorSettings);
            TDataReadReference<float> TuningCents9 = InputData.GetOrCreateDefaultDataReadReference<float>(
                METASOUND_GET_PARAM_NAME(InputTuningCents9), InParams.OperatorSettings);
            TDataReadReference<float> TuningCents10 = InputData.GetOrCreateDefaultDataReadReference<float>(
                METASOUND_GET_PARAM_NAME(InputTuningCents10), InParams.OperatorSettings);
            TDataReadReference<float> TuningCents11 = InputData.GetOrCreateDefaultDataReadReference<float>(
                METASOUND_GET_PARAM_NAME(InputTuningCents11), InParams.OperatorSettings);

            return MakeUnique<FTuningNodeOperator>(
                InParams.OperatorSettings,
                UpdateTrigger,
                MIDINoteNumber,
                ReferenceFrequency,
                ReferenceMIDINote,
                TuningCents0,
                TuningCents1,
                TuningCents2,
                TuningCents3,
                TuningCents4,
                TuningCents5,
                TuningCents6,
                TuningCents7,
                TuningCents8,
                TuningCents9,
                TuningCents10,
                TuningCents11
            );
        }

        void Execute()
        {
            UpdateTrigger->ExecuteBlock(
                [](int32, int32) {},
                [this](int32 StartFrame, int32)
                {
                    int32 midiNote = *MIDINoteNumber;
                    int32 noteInOctave = midiNote % 12;

                    float tuningCentsArray[12] = {
                        *TuningCents0,
                        *TuningCents1,
                        *TuningCents2,
                        *TuningCents3,
                        *TuningCents4,
                        *TuningCents5,
                        *TuningCents6,
                        *TuningCents7,
                        *TuningCents8,
                        *TuningCents9,
                        *TuningCents10,
                        *TuningCents11
                    };

                    float tuningAdjustmentCents = tuningCentsArray[noteInOctave];
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
        FFloatReadRef ReferenceFrequency;
        FInt32ReadRef ReferenceMIDINote;
        FFloatReadRef TuningCents0;
        FFloatReadRef TuningCents1;
        FFloatReadRef TuningCents2;
        FFloatReadRef TuningCents3;
        FFloatReadRef TuningCents4;
        FFloatReadRef TuningCents5;
        FFloatReadRef TuningCents6;
        FFloatReadRef TuningCents7;
        FFloatReadRef TuningCents8;
        FFloatReadRef TuningCents9;
        FFloatReadRef TuningCents10;
        FFloatReadRef TuningCents11;
        FFloatWriteRef OutputFrequency;
    };

    class FTuningNode : public FNodeFacade
    {
    public:
        FTuningNode(const FNodeInitData& InitData)
            : FNodeFacade(InitData.InstanceName, InitData.InstanceID, TFacadeOperatorClass<FTuningNodeOperator>())
        {
        }
    };
    
    METASOUND_REGISTER_NODE(FTuningNode);
}

#undef LOCTEXT_NAMESPACE