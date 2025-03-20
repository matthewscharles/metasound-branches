// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundTuningFromArrayNode.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundPrimitives.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_ TuningFromArrayNode"

namespace Metasound
{
    namespace  TuningFromArrayNodeVertexNames
    {
        METASOUND_PARAM(InputMIDINoteNumber, "MIDI Note Number", "Input MIDI note number (integer).");
        METASOUND_PARAM(InputTuningCentsArray, "Tuning Cents Array", "Array of tuning adjustments in cents for each note in the octave.");
        METASOUND_PARAM(OutputFrequency, "Frequency", "Output frequency.");
    }

    class FTuningFromArrayNodeOperator : public TExecutableOperator<FTuningFromArrayNodeOperator>
    {
    public:
        FTuningFromArrayNodeOperator(
            const FOperatorSettings& InSettings,
            const FInt32ReadRef& InMIDINoteNumber,
            const TDataReadReference<TArray<float>>& InTuningCentsArray)
            : MIDINoteNumber(InMIDINoteNumber)
            , TuningCentsArray(InTuningCentsArray)
            , OutputFrequency(FFloatWriteRef::CreateNew(0.0f))
        {
           
        }

        static const FVertexInterface& DeclareVertexInterface()
        {
            using namespace  TuningFromArrayNodeVertexNames;

            static const FVertexInterface Interface(
                FInputVertexInterface(
                    TInputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputMIDINoteNumber)),
                    TInputDataVertex<TArray<float>>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTuningCentsArray))
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

                Metadata.ClassName = { TEXT("UE"), TEXT("Tuning From Array"), TEXT("Float") };
                Metadata.MajorVersion = 1;
                Metadata.MinorVersion = 0;
                Metadata.DisplayName = METASOUND_LOCTEXT(" TuningFromArrayNodeDisplayName", "Tuning From Array");
                Metadata.Description = METASOUND_LOCTEXT(" TuningFromArrayNodeDesc", "Generates a frequency based on custom tuning per-note using an array.");
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
            using namespace  TuningFromArrayNodeVertexNames;

            FDataReferenceCollection InputDataReferences;
            
            InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputMIDINoteNumber), MIDINoteNumber);
            InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputTuningCentsArray), TuningCentsArray);

            return InputDataReferences;
        }

        virtual FDataReferenceCollection GetOutputs() const override
        {
            using namespace  TuningFromArrayNodeVertexNames;

            FDataReferenceCollection OutputDataReferences;
            OutputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutputFrequency), OutputFrequency);

            return OutputDataReferences;
        }

        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
        {
            using namespace  TuningFromArrayNodeVertexNames;

            const FInputVertexInterfaceData& InputData = InParams.InputData;
            const FInputVertexInterface& InputInterface = DeclareVertexInterface().GetInputInterface();

            TDataReadReference<int32> MIDINoteNumber = InputData.GetOrCreateDefaultDataReadReference<int32>(
                METASOUND_GET_PARAM_NAME(InputMIDINoteNumber), InParams.OperatorSettings);

            TDataReadReference<TArray<float>> TuningCentsArray = InputData.GetOrCreateDefaultDataReadReference<TArray<float>>(
                METASOUND_GET_PARAM_NAME(InputTuningCentsArray), 
                InParams.OperatorSettings
            );

            if (TuningCentsArray->Num() < 12)
            {
                TArray<float> MutableArray = *TuningCentsArray; // Create a mutable copy
                MutableArray.SetNumZeroed(12);
                TuningCentsArray = TDataReadReference<TArray<float>>::CreateNew(MutableArray);
            }

            return MakeUnique<FTuningFromArrayNodeOperator>(InParams.OperatorSettings, MIDINoteNumber, TuningCentsArray);
        }

        void Execute()
        {
            int32 midiNote = *MIDINoteNumber;
            int32 noteInOctave = midiNote % 12;

            // Ensure safe access within the array bounds
            float tuningAdjustmentCents = (TuningCentsArray->Num() > noteInOctave) ? (*TuningCentsArray)[noteInOctave] : 0.0f;
            float tuningAdjustmentSemitones = tuningAdjustmentCents / 100.0f;
            float adjustedNote = midiNote + tuningAdjustmentSemitones;
            float frequency = 440.0f * powf(2.0f, (adjustedNote - 69.0f) / 12.0f);

            *OutputFrequency = frequency;
        }

    private:
        FInt32ReadRef MIDINoteNumber;
        TDataReadReference<TArray<float>> TuningCentsArray;
        FFloatWriteRef OutputFrequency;
    };

    class FTuningFromArrayNode : public FNodeFacade
    {
    public:
        FTuningFromArrayNode(const FNodeInitData& InitData)
            : FNodeFacade(InitData.InstanceName, InitData.InstanceID, TFacadeOperatorClass<FTuningFromArrayNodeOperator>())
        {
        }
    };

    METASOUND_REGISTER_NODE(FTuningFromArrayNode);
}

#undef LOCTEXT_NAMESPACE