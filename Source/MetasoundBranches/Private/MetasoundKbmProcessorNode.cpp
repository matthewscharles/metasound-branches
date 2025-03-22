// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundKbmProcessorNode.h"
#include "MetasoundBranches/Public/MetasoundKbmDataType.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundPrimitives.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_KbmProcessorNode"

namespace Metasound
{
    namespace KbmProcessorNodeVertexNames
    {
        METASOUND_PARAM(InputTrigger, "Process Note", "Trigger to calculate frequency from KBM.");
        METASOUND_PARAM(InputNote, "Note", "MIDI note to process.");
        METASOUND_PARAM(InputKbmData, "KBM Data", "KBM tuning data.");

        METASOUND_PARAM(OutputTrigger, "On Mapped", "Fires if note was mapped.");
        METASOUND_PARAM(OutputUnmappedTrigger, "On Unmapped", "Fires if note is not mapped.");
        METASOUND_PARAM(OutputFrequency, "Frequency", "Calculated frequency.");
        METASOUND_PARAM(OutputScaleDegree, "Scale Degree", "Scale degree index.");
    }

    class FKbmProcessorNodeOperator : public TExecutableOperator<FKbmProcessorNodeOperator>
    {
    public:
        FKbmProcessorNodeOperator(
            const FOperatorSettings& InSettings,
            const FTriggerReadRef& InTrigger,
            const FInt32ReadRef& InNote,
            const FKbmDataReadRef& InKbmData)
            : Trigger(InTrigger)
            , Note(InNote)
            , KbmData(InKbmData)
            , OnMappedTrigger(FTriggerWriteRef::CreateNew(InSettings))
            , OnUnmappedTrigger(FTriggerWriteRef::CreateNew(InSettings))
            , Frequency(FFloatWriteRef::CreateNew(0.0f))
            , ScaleDegree(FInt32WriteRef::CreateNew(-1))
        {
        }

        static const FVertexInterface& DeclareVertexInterface()
        {
            using namespace KbmProcessorNodeVertexNames;

            static const FVertexInterface Interface(
                FInputVertexInterface(
                    TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTrigger)),
                    TInputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputNote)),
                    TInputDataVertex<MetasoundKbm::FKbmData>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputKbmData))
                ),
                FOutputVertexInterface(
                    TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputTrigger)),
                    TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputUnmappedTrigger)),
                    TOutputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputFrequency)),
                    TOutputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputScaleDegree))
                )
            );

            return Interface;
        }

        static const FNodeClassMetadata& GetNodeInfo()
        {
            auto CreateNodeClassMetadata = []() -> FNodeClassMetadata
            {
                FNodeClassMetadata Metadata;

                Metadata.ClassName = { TEXT("UE"), TEXT("Scala KBM Processor"), TEXT("Scala") };
                Metadata.MajorVersion = 1;
                Metadata.MinorVersion = 0;
                Metadata.DisplayName = METASOUND_LOCTEXT("KbmProcessorDisplayName", "Scala KBM Processor");
                Metadata.Description = METASOUND_LOCTEXT("KbmProcessorDesc", "Looks up a frequency and scale degree using Scala KBM data.");
                Metadata.Author = "Charles Matthews";
                Metadata.DefaultInterface = DeclareVertexInterface();
                Metadata.CategoryHierarchy = {
                    METASOUND_LOCTEXT("Custom", "Branches"),
                    METASOUND_LOCTEXT("CustomSub", "Tuning")
                };

                return Metadata;
            };

            static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
            return Metadata;
        }
        
        FDataReferenceCollection GetInputs() const override
        {
            using namespace KbmProcessorNodeVertexNames;

            FDataReferenceCollection Inputs;
            Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputTrigger), Trigger);
            Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputNote), Note);
            Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputKbmData), KbmData);
            return Inputs;
        }

        FDataReferenceCollection GetOutputs() const override
        {
            using namespace KbmProcessorNodeVertexNames;

            FDataReferenceCollection Outputs;
            Outputs.AddDataWriteReference(METASOUND_GET_PARAM_NAME(OutputTrigger), OnMappedTrigger);
            Outputs.AddDataWriteReference(METASOUND_GET_PARAM_NAME(OutputUnmappedTrigger), OnUnmappedTrigger);
            Outputs.AddDataWriteReference(METASOUND_GET_PARAM_NAME(OutputFrequency), Frequency);
            Outputs.AddDataWriteReference(METASOUND_GET_PARAM_NAME(OutputScaleDegree), ScaleDegree);
            return Outputs;
        }

        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
        {
            using namespace KbmProcessorNodeVertexNames;
            const FInputVertexInterfaceData& InputData = InParams.InputData;

            return MakeUnique<FKbmProcessorNodeOperator>(
                InParams.OperatorSettings,
                InputData.GetOrCreateDefaultDataReadReference<FTrigger>(METASOUND_GET_PARAM_NAME(InputTrigger), InParams.OperatorSettings),
                InputData.GetOrCreateDefaultDataReadReference<int32>(METASOUND_GET_PARAM_NAME(InputNote), InParams.OperatorSettings),
                InputData.GetOrCreateDefaultDataReadReference<MetasoundKbm::FKbmData>(METASOUND_GET_PARAM_NAME(InputKbmData), InParams.OperatorSettings)
            );
        }

        void Execute()
        {
            OnMappedTrigger->AdvanceBlock();
            OnUnmappedTrigger->AdvanceBlock();
            Trigger->ExecuteBlock(
                [](int32 StartFrame, int32 EndFrame) {},
                [this](int32 StartFrame, int32 EndFrame)
                {
                    const int32 NoteIndex = *Note;
                    const int32 MinNote = KbmData->FirstNote;
                    const int32 MaxNote = KbmData->LastNote;
                    const int32 RefNote = KbmData->ReferenceNote;
                    const float RefFreq = KbmData->ReferenceFrequency;
                    const int32 OctaveDegree = KbmData->OctaveDegree;
                    const float PeriodRatio = 2.0f; // for now ...

                    if (NoteIndex < MinNote || NoteIndex > MaxNote || !KbmData->ScaleDegrees.IsValidIndex(NoteIndex % OctaveDegree) || KbmData->ScaleDegrees[NoteIndex % OctaveDegree] < 0)
                    {
                        *Frequency = 0.0f;
                        *ScaleDegree = -1;
                        OnUnmappedTrigger->TriggerFrame(StartFrame);
                        return;
                    }

                    const int32 WrappedNoteIndex = NoteIndex % OctaveDegree;
                    const float Cents = KbmData->CentValues.IsValidIndex(WrappedNoteIndex) ? KbmData->CentValues[WrappedNoteIndex] : 0.0f;
                    const float Hz = RefFreq * powf(PeriodRatio, (Cents / 1200.0f) + (NoteIndex / OctaveDegree));

                    *Frequency = Hz;
                    *ScaleDegree = KbmData->ScaleDegrees[WrappedNoteIndex];
                    OnMappedTrigger->TriggerFrame(StartFrame);
                });
        }

    private:
        FTriggerReadRef Trigger;
        FInt32ReadRef Note;
        FKbmDataReadRef KbmData;

        FTriggerWriteRef OnMappedTrigger;
        FTriggerWriteRef OnUnmappedTrigger;
        FFloatWriteRef Frequency;
        FInt32WriteRef ScaleDegree;
    };

    class FKbmProcessorNode : public FNodeFacade
    {
    public:
        FKbmProcessorNode(const FNodeInitData& InitData)
            : FNodeFacade(InitData.InstanceName, InitData.InstanceID, TFacadeOperatorClass<FKbmProcessorNodeOperator>())
        {
        }
    };

    METASOUND_REGISTER_NODE(FKbmProcessorNode);
}

#undef LOCTEXT_NAMESPACE