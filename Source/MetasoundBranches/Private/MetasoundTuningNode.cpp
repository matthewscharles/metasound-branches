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
        METASOUND_PARAM(InputMIDINoteNumber, "MIDI Note Number", "Input MIDI note number (integer).");
        METASOUND_PARAM(InputTuningCents0, "+/- Cents C", "Tuning adjustment for note 0 in cents.");
        METASOUND_PARAM(InputTuningCents1, "+/- Cents C♯ / D♭", "Tuning adjustment for note 1 in cents.");
        METASOUND_PARAM(InputTuningCents2, "+/- Cents D", "Tuning adjustment for note 2 in cents.");
        METASOUND_PARAM(InputTuningCents3, "+/- Cents D♯ / E♭", "Tuning adjustment for note 3 in cents.");
        METASOUND_PARAM(InputTuningCents4, "+/- Cents E", "Tuning adjustment for note 4 in cents.");
        METASOUND_PARAM(InputTuningCents5, "+/- Cents F", "Tuning adjustment for note 5 in cents.");
        METASOUND_PARAM(InputTuningCents6, "+/- Cents F♯ / G♭", "Tuning adjustment for note 6 in cents.");
        METASOUND_PARAM(InputTuningCents7, "+/- Cents G", "Tuning adjustment for note 7 in cents.");
        METASOUND_PARAM(InputTuningCents8, "+/- Cents G♯ / A♭", "Tuning adjustment for note 8 in cents.");
        METASOUND_PARAM(InputTuningCents9, "+/- Cents A", "Tuning adjustment for note 9 in cents.");
        METASOUND_PARAM(InputTuningCents10, "+/- Cents A♯ / B♭", "Tuning adjustment for note 10 in cents.");
        METASOUND_PARAM(InputTuningCents11, "+/- Cents B", "Tuning adjustment for note 11 in cents.");
        METASOUND_PARAM(OutputFrequency, "Frequency", "Output frequency.");
    }

    class FTuningNodeOperator : public TExecutableOperator<FTuningNodeOperator>
    {
    public:
        FTuningNodeOperator(
            const FOperatorSettings& InSettings,
            const FInt32ReadRef& InMIDINoteNumber,
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
            : MIDINoteNumber(InMIDINoteNumber)
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
                    TInputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputMIDINoteNumber)),
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
                Metadata.MinorVersion = 0;
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

            InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputMIDINoteNumber), MIDINoteNumber);
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
            const FInputVertexInterface& InputInterface = DeclareVertexInterface().GetInputInterface();

            TDataReadReference<int32> MIDINoteNumber = InputData.GetOrCreateDefaultDataReadReference<int32>(METASOUND_GET_PARAM_NAME(InputMIDINoteNumber), InParams.OperatorSettings);

            TDataReadReference<float> TuningCents0 = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputTuningCents0), InParams.OperatorSettings);
            TDataReadReference<float> TuningCents1 = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputTuningCents1), InParams.OperatorSettings);
            TDataReadReference<float> TuningCents2 = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputTuningCents2), InParams.OperatorSettings);
            TDataReadReference<float> TuningCents3 = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputTuningCents3), InParams.OperatorSettings);
            TDataReadReference<float> TuningCents4 = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputTuningCents4), InParams.OperatorSettings);
            TDataReadReference<float> TuningCents5 = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputTuningCents5), InParams.OperatorSettings);
            TDataReadReference<float> TuningCents6 = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputTuningCents6), InParams.OperatorSettings);
            TDataReadReference<float> TuningCents7 = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputTuningCents7), InParams.OperatorSettings);
            TDataReadReference<float> TuningCents8 = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputTuningCents8), InParams.OperatorSettings);
            TDataReadReference<float> TuningCents9 = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputTuningCents9), InParams.OperatorSettings);
            TDataReadReference<float> TuningCents10 = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputTuningCents10), InParams.OperatorSettings);
            TDataReadReference<float> TuningCents11 = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputTuningCents11), InParams.OperatorSettings);

            return MakeUnique<FTuningNodeOperator>(
                InParams.OperatorSettings,
                MIDINoteNumber,
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
            float frequency = 440.0f * powf(2.0f, (adjustedNote - 69.0f) / 12.0f);
            *OutputFrequency = frequency;
        }

    private:
        FInt32ReadRef MIDINoteNumber;
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