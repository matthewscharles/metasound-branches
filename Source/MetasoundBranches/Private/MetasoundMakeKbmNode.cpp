// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundMakeKbmNode.h"
#include "MetasoundBranches/Public/MetasoundKbmDataType.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundPrimitives.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_MakeKbmNode"

namespace Metasound
{
    namespace MakeKbmNodeVertexNames
    {
        METASOUND_PARAM(InputTrigger, "Make KBM", "Trigger to create KBM struct.");
        METASOUND_PARAM(InputMapSize, "Map Size", "Number of entries in the KBM map.");
        METASOUND_PARAM(InputFirstNote, "First Note", "First MIDI note to retune.");
        METASOUND_PARAM(InputLastNote, "Last Note", "Last MIDI note to retune.");
        METASOUND_PARAM(InputMiddleNote, "Middle Note", "Middle MIDI note (1/1).");
        METASOUND_PARAM(InputReferenceNote, "Reference Note", "MIDI note used as tuning reference.");
        METASOUND_PARAM(InputReferenceFrequency, "Reference Frequency", "Reference frequency in Hz.");
        METASOUND_PARAM(InputOctaveDegree, "Octave Degree", "Number of notes per octave.");
        METASOUND_PARAM(InputPeriodRatio, "Period Ratio", "Ratio of octave.");
        METASOUND_PARAM(InputScaleDegrees, "Scale Degrees", "Array of scale degrees (-1 = unassigned).");
        METASOUND_PARAM(InputCentValues, "Cent Values", "Array of cent values per MIDI note.");

        METASOUND_PARAM(OutputTrigger, "On Make KBM", "Triggered when KBM struct is created.");
        METASOUND_PARAM(OutputKbmData, "KBM Data", "The generated KBM struct.");
    }

    class FMakeKbmNodeOperator : public TExecutableOperator<FMakeKbmNodeOperator>
    {
    public:
        FMakeKbmNodeOperator(
            const FOperatorSettings& InSettings,
            const FTriggerReadRef& InTrigger,
            const FInt32ReadRef& InMapSize,
            const FInt32ReadRef& InFirstNote,
            const FInt32ReadRef& InLastNote,
            const FInt32ReadRef& InMiddleNote,
            const FInt32ReadRef& InReferenceNote,
            const FFloatReadRef& InReferenceFrequency,
            const FInt32ReadRef& InOctaveDegree,
            const FFloatReadRef& InPeriodRatio,
            const TDataReadReference<TArray<int32>>& InScaleDegrees,
            const TDataReadReference<TArray<float>>& InCentValues)
            : Trigger(InTrigger)
            , MapSize(InMapSize)
            , FirstNote(InFirstNote)
            , LastNote(InLastNote)
            , MiddleNote(InMiddleNote)
            , ReferenceNote(InReferenceNote)
            , ReferenceFrequency(InReferenceFrequency)
            , OctaveDegree(InOctaveDegree)
            , PeriodRatio(FFloatReadRef::CreateNew(2.0f))
            , ScaleDegrees(InScaleDegrees)
            , CentValues(InCentValues)
            , OnMakeTrigger(FTriggerWriteRef::CreateNew(InSettings))
            , KbmData(FKbmDataWriteRef::CreateNew())
        {
        }

        static const FVertexInterface& DeclareVertexInterface()
        {
            using namespace MakeKbmNodeVertexNames;

            static const FVertexInterface Interface(
                FInputVertexInterface(
                    TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTrigger)),
                    TInputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputMapSize)),
                    TInputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputFirstNote)),
                    TInputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputLastNote)),
                    TInputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputMiddleNote)),
                    TInputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputReferenceNote)),
                    TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputReferenceFrequency)),
                    TInputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputOctaveDegree)),
                    TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputPeriodRatio), 2.0f),
                    TInputDataVertex<TArray<int32>>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputScaleDegrees)),
                    TInputDataVertex<TArray<float>>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputCentValues))
                ),
                FOutputVertexInterface(
                    TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputTrigger)),
                    TOutputDataVertex<MetasoundKbm::FKbmData>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputKbmData))
                )
            );

            return Interface;
        }

        static const FNodeClassMetadata& GetNodeInfo()
        {
            auto CreateNodeClassMetadata = []() -> FNodeClassMetadata
            {
                FNodeClassMetadata Metadata;

                Metadata.ClassName = { TEXT("UE"), TEXT("Make Scala KBM"), TEXT("Scala") };
                Metadata.MajorVersion = 1;
                Metadata.MinorVersion = 0;
                Metadata.DisplayName = METASOUND_LOCTEXT("MakeKbmDisplayName", "Make Scala KBM");
                Metadata.Description = METASOUND_LOCTEXT("MakeKbmDesc", "Constructs a KBM (keyboard mapper) struct.");
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
            using namespace MakeKbmNodeVertexNames;

            FDataReferenceCollection Inputs;
            Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputTrigger), Trigger);
            Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputMapSize), MapSize);
            Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputFirstNote), FirstNote);
            Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputLastNote), LastNote);
            Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputMiddleNote), MiddleNote);
            Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputReferenceNote), ReferenceNote);
            Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputReferenceFrequency), ReferenceFrequency);
            Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputOctaveDegree), OctaveDegree);
            Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputPeriodRatio), PeriodRatio);
            Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputScaleDegrees), ScaleDegrees);
            Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputCentValues), CentValues);
            return Inputs;
        }

        FDataReferenceCollection GetOutputs() const override
        {
            using namespace MakeKbmNodeVertexNames;

            FDataReferenceCollection Outputs;
            Outputs.AddDataWriteReference(METASOUND_GET_PARAM_NAME(OutputTrigger), OnMakeTrigger);
            Outputs.AddDataWriteReference(METASOUND_GET_PARAM_NAME(OutputKbmData), KbmData);
            return Outputs;
        }

        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
        {
            using namespace MakeKbmNodeVertexNames;
            const FInputVertexInterfaceData& InputData = InParams.InputData;

            return MakeUnique<FMakeKbmNodeOperator>(
                InParams.OperatorSettings,
                InputData.GetOrCreateDefaultDataReadReference<FTrigger>(METASOUND_GET_PARAM_NAME(InputTrigger), InParams.OperatorSettings),
                InputData.GetOrCreateDefaultDataReadReference<int32>(METASOUND_GET_PARAM_NAME(InputMapSize), InParams.OperatorSettings),
                InputData.GetOrCreateDefaultDataReadReference<int32>(METASOUND_GET_PARAM_NAME(InputFirstNote), InParams.OperatorSettings),
                InputData.GetOrCreateDefaultDataReadReference<int32>(METASOUND_GET_PARAM_NAME(InputLastNote), InParams.OperatorSettings),
                InputData.GetOrCreateDefaultDataReadReference<int32>(METASOUND_GET_PARAM_NAME(InputMiddleNote), InParams.OperatorSettings),
                InputData.GetOrCreateDefaultDataReadReference<int32>(METASOUND_GET_PARAM_NAME(InputReferenceNote), InParams.OperatorSettings),
                InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputReferenceFrequency), InParams.OperatorSettings),
                InputData.GetOrCreateDefaultDataReadReference<int32>(METASOUND_GET_PARAM_NAME(InputOctaveDegree), InParams.OperatorSettings),
                InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputPeriodRatio), InParams.OperatorSettings),
                InputData.GetOrCreateDefaultDataReadReference<TArray<int32>>(METASOUND_GET_PARAM_NAME(InputScaleDegrees), InParams.OperatorSettings),
                InputData.GetOrCreateDefaultDataReadReference<TArray<float>>(METASOUND_GET_PARAM_NAME(InputCentValues), InParams.OperatorSettings)
            );
        }

        void Execute()
        {
            OnMakeTrigger->AdvanceBlock();
            Trigger->ExecuteBlock(
                [](int32 StartFrame, int32 EndFrame) {}, // Optional pre-pass lambda
                [this](int32 StartFrame, int32 EndFrame)
                {
                    MetasoundKbm::FKbmData NewKbm;
                    NewKbm.MapSize = *MapSize;
                    NewKbm.FirstNote = *FirstNote;
                    NewKbm.LastNote = *LastNote;
                    NewKbm.MiddleNote = *MiddleNote;
                    NewKbm.ReferenceNote = *ReferenceNote;
                    NewKbm.ReferenceFrequency = *ReferenceFrequency;
                    NewKbm.OctaveDegree = *OctaveDegree;
                    NewKbm.PeriodRatio = *PeriodRatio;
                    NewKbm.ScaleDegrees = *ScaleDegrees;
                    NewKbm.CentValues = *CentValues;
                    *KbmData = NewKbm;

                    OnMakeTrigger->TriggerFrame(StartFrame);
                }
            );
        }

    private:
        FTriggerReadRef Trigger;
        FInt32ReadRef MapSize;
        FInt32ReadRef FirstNote;
        FInt32ReadRef LastNote;
        FInt32ReadRef MiddleNote;
        FInt32ReadRef ReferenceNote;
        FFloatReadRef ReferenceFrequency;
        FInt32ReadRef OctaveDegree;
        FFloatReadRef PeriodRatio;
        TDataReadReference<TArray<int32>> ScaleDegrees;
        TDataReadReference<TArray<float>> CentValues;

        FTriggerWriteRef OnMakeTrigger;
        FKbmDataWriteRef KbmData;
    };

    class FMakeKbmNode : public FNodeFacade
    {
    public:
        FMakeKbmNode(const FNodeInitData& InitData)
            : FNodeFacade(InitData.InstanceName, InitData.InstanceID, TFacadeOperatorClass<FMakeKbmNodeOperator>())
        {
        }
    };

    METASOUND_REGISTER_NODE(FMakeKbmNode);
}

#undef LOCTEXT_NAMESPACE
