// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundBreakKbmNode.h"
#include "MetasoundBranches/Public/MetasoundKbmDataType.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundPrimitives.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_BreakKbmNode"

namespace Metasound
{
    namespace BreakKbmNodeVertexNames
    {
        METASOUND_PARAM(InputTrigger, "Break KBM", "Trigger to deconstruct a KBM struct.");
        METASOUND_PARAM(InputKbmData, "KBM Data", "KBM data.");

        METASOUND_PARAM(OutputTrigger, "On Break KBM", "Triggered when KBM is broken.");
        METASOUND_PARAM(OutputMapSize, "Map Size", "Number of entries in the KBM map.");
        METASOUND_PARAM(OutputFirstNote, "First Note", "First MIDI note to retune.");
        METASOUND_PARAM(OutputLastNote, "Last Note", "Last MIDI note to retune.");
        METASOUND_PARAM(OutputMiddleNote, "Middle Note", "Middle MIDI note (1/1).");
        METASOUND_PARAM(OutputReferenceNote, "Reference Note", "MIDI note used as tuning reference.");
        METASOUND_PARAM(OutputReferenceFrequency, "Reference Frequency", "Reference frequency in Hz.");
        METASOUND_PARAM(OutputOctaveDegree, "Octave Degree", "Number of notes per octave.");
        METASOUND_PARAM(OutputScaleDegrees, "Scale Degrees", "Array of scale degrees.");
        METASOUND_PARAM(OutputCentValues, "Cent Values", "Array of cent values.");
    }

    class FBreakKbmNodeOperator : public TExecutableOperator<FBreakKbmNodeOperator>
    {
    public:
        FBreakKbmNodeOperator(
            const FOperatorSettings& InSettings,
            const FTriggerReadRef& InTrigger,
            const FKbmDataReadRef& InKbmData)
            : Trigger(InTrigger)
            , KbmData(InKbmData)
            , OnBreakTrigger(FTriggerWriteRef::CreateNew(InSettings))
            , MapSize(FInt32WriteRef::CreateNew(0))
            , FirstNote(FInt32WriteRef::CreateNew(0))
            , LastNote(FInt32WriteRef::CreateNew(0))
            , MiddleNote(FInt32WriteRef::CreateNew(0))
            , ReferenceNote(FInt32WriteRef::CreateNew(69))
            , ReferenceFrequency(FFloatWriteRef::CreateNew(440.0f))
            , OctaveDegree(FInt32WriteRef::CreateNew(12))
            , ScaleDegrees(TDataWriteReference<TArray<int32>>::CreateNew())
            , CentValues(TDataWriteReference<TArray<float>>::CreateNew())
        {
        }

        static const FVertexInterface& DeclareVertexInterface()
        {
            using namespace BreakKbmNodeVertexNames;

            static const FVertexInterface Interface(
                FInputVertexInterface(
                    TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTrigger)),
                    TInputDataVertex<MetasoundKbm::FKbmData>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputKbmData))
                ),
                FOutputVertexInterface(
                    TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputTrigger)),
                    TOutputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputMapSize)),
                    TOutputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputFirstNote)),
                    TOutputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputLastNote)),
                    TOutputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputMiddleNote)),
                    TOutputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputReferenceNote)),
                    TOutputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputReferenceFrequency)),
                    TOutputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputOctaveDegree)),
                    TOutputDataVertex<TArray<int32>>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputScaleDegrees)),
                    TOutputDataVertex<TArray<float>>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputCentValues))
                )
            );

            return Interface;
        }

        static const FNodeClassMetadata& GetNodeInfo()
        {
            auto CreateNodeClassMetadata = []() -> FNodeClassMetadata
            {
                FNodeClassMetadata Metadata;

                Metadata.ClassName = { TEXT("UE"), TEXT("Break Scala KBM"), TEXT("Scala") };
                Metadata.MajorVersion = 1;
                Metadata.MinorVersion = 0;
                Metadata.DisplayName = METASOUND_LOCTEXT("MakeKbmDisplayName", "Break Scala KBM");
                Metadata.Description = METASOUND_LOCTEXT("MakeKbmDesc", "Deconstructs a scala KBM (keyboard mapping) struct.");
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
            using namespace BreakKbmNodeVertexNames;

            FDataReferenceCollection Inputs;
            Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputTrigger), Trigger);
            Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputKbmData), KbmData);
            return Inputs;
        }

        FDataReferenceCollection GetOutputs() const override
        {
            using namespace BreakKbmNodeVertexNames;

            FDataReferenceCollection Outputs;
            Outputs.AddDataWriteReference(METASOUND_GET_PARAM_NAME(OutputTrigger), OnBreakTrigger);
            Outputs.AddDataWriteReference(METASOUND_GET_PARAM_NAME(OutputMapSize), MapSize);
            Outputs.AddDataWriteReference(METASOUND_GET_PARAM_NAME(OutputFirstNote), FirstNote);
            Outputs.AddDataWriteReference(METASOUND_GET_PARAM_NAME(OutputLastNote), LastNote);
            Outputs.AddDataWriteReference(METASOUND_GET_PARAM_NAME(OutputMiddleNote), MiddleNote);
            Outputs.AddDataWriteReference(METASOUND_GET_PARAM_NAME(OutputReferenceNote), ReferenceNote);
            Outputs.AddDataWriteReference(METASOUND_GET_PARAM_NAME(OutputReferenceFrequency), ReferenceFrequency);
            Outputs.AddDataWriteReference(METASOUND_GET_PARAM_NAME(OutputOctaveDegree), OctaveDegree);
            Outputs.AddDataWriteReference(METASOUND_GET_PARAM_NAME(OutputScaleDegrees), ScaleDegrees);
            Outputs.AddDataWriteReference(METASOUND_GET_PARAM_NAME(OutputCentValues), CentValues);
            return Outputs;
        }

        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
        {
            using namespace BreakKbmNodeVertexNames;
            const FInputVertexInterfaceData& InputData = InParams.InputData;

            return MakeUnique<FBreakKbmNodeOperator>(
                InParams.OperatorSettings,
                InputData.GetOrCreateDefaultDataReadReference<FTrigger>(METASOUND_GET_PARAM_NAME(InputTrigger), InParams.OperatorSettings),
                InputData.GetOrCreateDefaultDataReadReference<MetasoundKbm::FKbmData>(METASOUND_GET_PARAM_NAME(InputKbmData), InParams.OperatorSettings)
            );
        }

        void Execute()
        {
            OnBreakTrigger->AdvanceBlock();
            Trigger->ExecuteBlock(
                [](int32 StartFrame, int32 EndFrame) {},
                [this](int32 StartFrame, int32 EndFrame)
                {
                    *MapSize = KbmData->MapSize;
                    *FirstNote = KbmData->FirstNote;
                    *LastNote = KbmData->LastNote;
                    *MiddleNote = KbmData->MiddleNote;
                    *ReferenceNote = KbmData->ReferenceNote;
                    *ReferenceFrequency = KbmData->ReferenceFrequency;
                    *OctaveDegree = KbmData->OctaveDegree;
                    *ScaleDegrees = KbmData->ScaleDegrees;
                    *CentValues = KbmData->CentValues;

                    OnBreakTrigger->TriggerFrame(StartFrame);
                });
        }

    private:
        FTriggerReadRef Trigger;
        FKbmDataReadRef KbmData;

        FTriggerWriteRef OnBreakTrigger;
        FInt32WriteRef MapSize;
        FInt32WriteRef FirstNote;
        FInt32WriteRef LastNote;
        FInt32WriteRef MiddleNote;
        FInt32WriteRef ReferenceNote;
        FFloatWriteRef ReferenceFrequency;
        FInt32WriteRef OctaveDegree;
        TDataWriteReference<TArray<int32>> ScaleDegrees;
        TDataWriteReference<TArray<float>> CentValues;
    };

    class FBreakKbmNode : public FNodeFacade
    {
    public:
        FBreakKbmNode(const FNodeInitData& InitData)
            : FNodeFacade(InitData.InstanceName, InitData.InstanceID, TFacadeOperatorClass<FBreakKbmNodeOperator>())
        {
        }
    };

    METASOUND_REGISTER_NODE(FBreakKbmNode);
}

#undef LOCTEXT_NAMESPACE
