// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundKbmProcessorNode.h"
#include "MetasoundBranches/Public/MetasoundKbmDataType.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundPrimitives.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"
#include "MetasoundBranches/Public/MetasoundCommonMacros.h"

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
        METASOUND_PARAM(OutputOctave, "Octave", "Octave index relative to middle note.");
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
            , Octave(FInt32WriteRef::CreateNew(0))
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
                    TOutputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputScaleDegree)),
                    TOutputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputOctave))
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
                Metadata.Description = METASOUND_LOCTEXT("KbmProcessorDesc", "Looks up a frequency, scale degree, and octave using Scala KBM data.");
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
        
        METASOUND_DISABLE_LEGACY_IO()
        
        virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
        {
            using namespace KbmProcessorNodeVertexNames;
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputTrigger), Trigger);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputNote), Note);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputKbmData), KbmData);
        }
        
        virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
        {
            using namespace KbmProcessorNodeVertexNames;
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputTrigger), OnMappedTrigger);
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputUnmappedTrigger), OnUnmappedTrigger);
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputFrequency), Frequency);
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputScaleDegree), ScaleDegree);
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputOctave), Octave);
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
                [](int32, int32){},
                [this](int32 StartFrame, int32)
                {
                    const int32 NoteIndex = *Note;
                    const int32 MinNote = KbmData->FirstNote;
                    const int32 MaxNote = KbmData->LastNote;
                    const int32 MidNote = KbmData->MiddleNote;
                    const float MidFreq = KbmData->ReferenceFrequency;
                    const int32 OctaveDegree = KbmData->OctaveDegree;
                    if (NoteIndex < MinNote || NoteIndex > MaxNote) 
                    {
                        *Frequency = 0.0f;
                        *ScaleDegree = -1;
                        *Octave = 0;
                        OnUnmappedTrigger->TriggerFrame(StartFrame);
                        return;
                    }
                    const int32 WrappedNoteIndex = NoteIndex % OctaveDegree;
                    if (!KbmData->ScaleDegrees.IsValidIndex(WrappedNoteIndex) || KbmData->ScaleDegrees[WrappedNoteIndex] < 0)
                    {
                        *Frequency = 0.0f;
                        *ScaleDegree = -1;
                        *Octave = 0;
                        OnUnmappedTrigger->TriggerFrame(StartFrame);
                        return;
                    }
                    const float Cents = KbmData->CentValues.IsValidIndex(WrappedNoteIndex) ? KbmData->CentValues[WrappedNoteIndex] : 0.0f;
                    const int32 NoteOffset = NoteIndex - MidNote;
                    const int32 ThisOctave = NoteOffset / OctaveDegree;
                    const float Hz = MidFreq * powf(KbmData->PeriodRatio, (static_cast<float>(NoteIndex - MidNote) / OctaveDegree)) * powf(2.0f, Cents / 1200.0f);
                    *Frequency = Hz;
                    *ScaleDegree = KbmData->ScaleDegrees[WrappedNoteIndex];
                    *Octave = ThisOctave;
                    OnMappedTrigger->TriggerFrame(StartFrame);
                }
            );
        }

    private:
        FTriggerReadRef Trigger;
        FInt32ReadRef Note;
        FKbmDataReadRef KbmData;
        FTriggerWriteRef OnMappedTrigger;
        FTriggerWriteRef OnUnmappedTrigger;
        FFloatWriteRef Frequency;
        FInt32WriteRef ScaleDegree;
        FInt32WriteRef Octave;
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