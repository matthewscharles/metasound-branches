// Copyright 2026 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundScaleDegreeToNoteNode.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundPrimitives.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"
#include "MetasoundBranches/Public/MetasoundCommonMacros.h"

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_ScaleDegreeToNoteNode"

namespace Metasound
{
    namespace ScaleDegreeToNoteNodeVertexNames
    {
        METASOUND_PARAM(InputUpdateTrigger, "Trigger", "Triggers output.");
        METASOUND_PARAM(InputScaleDegree, "Scale Degree", "Input scale degree (integer)." );
        METASOUND_PARAM(InputScaleArray, "Scale", "Array of scale intervals in semitones.");
        METASOUND_PARAM(InputOffset, "Offset", "Base offset in semitones.");
        METASOUND_PARAM(OutputNote, "Note", "Output note number as float.");
    }

    class FScaleDegreeToNoteNodeOperator : public TExecutableOperator<FScaleDegreeToNoteNodeOperator>
    {
    public:
        FScaleDegreeToNoteNodeOperator(
            const FOperatorSettings& InSettings,
            const FTriggerReadRef& InUpdateTrigger,
            const FInt32ReadRef& InScaleDegree,
            const TDataReadReference<TArray<float>>& InScaleArray,
            const FFloatReadRef& InOffset)
            : UpdateTrigger(InUpdateTrigger)
            , ScaleDegree(InScaleDegree)
            , ScaleArray(InScaleArray)
            , Offset(InOffset)
            , OutputNote(FFloatWriteRef::CreateNew(0.0f))
        {
        }

        static const FVertexInterface& DeclareVertexInterface()
        {
            using namespace ScaleDegreeToNoteNodeVertexNames;

            static const FVertexInterface Interface(
                FInputVertexInterface(
                    TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputUpdateTrigger)),
                    TInputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputScaleDegree)),
                    TInputDataVertex<TArray<float>>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputScaleArray)),
                    TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputOffset), 0.0f)
                ),
                FOutputVertexInterface(
                    TOutputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputNote))
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

                Metadata.ClassName = { TEXT("UE"), TEXT("Scale Degree To Note"), TEXT("Float") };
                Metadata.MajorVersion = 1;
                Metadata.MinorVersion = 0;
                Metadata.DisplayName = METASOUND_LOCTEXT("ScaleDegreeToNoteNodeDisplayName", "Scale Degree To Note");
                Metadata.Description = METASOUND_LOCTEXT("ScaleDegreeToNoteNodeDesc", "Maps a scale degree to a note number from a single octave array of semitones.");
                Metadata.Author = "Charles Matthews";
                Metadata.PromptIfMissing = PluginNodeMissingPrompt;
                Metadata.DefaultInterface = NodeInterface;
                Metadata.CategoryHierarchy = {
                    METASOUND_LOCTEXT("Custom", "Branches"),
                    METASOUND_LOCTEXT("CustomSub", "Music")
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
            using namespace ScaleDegreeToNoteNodeVertexNames;
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputUpdateTrigger), UpdateTrigger);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputScaleDegree), ScaleDegree);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputScaleArray), ScaleArray);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputOffset), Offset);
        }

        virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
        {
            using namespace ScaleDegreeToNoteNodeVertexNames;
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputNote), OutputNote);
        }

        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
        {
            using namespace ScaleDegreeToNoteNodeVertexNames;

            const FInputVertexInterfaceData& InputData = InParams.InputData;

            FTriggerReadRef UpdateTrigger = InputData.GetOrCreateDefaultDataReadReference<FTrigger>(
                METASOUND_GET_PARAM_NAME(InputUpdateTrigger), InParams.OperatorSettings);

            FInt32ReadRef ScaleDegree = InputData.GetOrCreateDefaultDataReadReference<int32>(
                METASOUND_GET_PARAM_NAME(InputScaleDegree), InParams.OperatorSettings);

            TDataReadReference<TArray<float>> ScaleArray = InputData.GetOrCreateDefaultDataReadReference<TArray<float>>(
                METASOUND_GET_PARAM_NAME(InputScaleArray), InParams.OperatorSettings);

            FFloatReadRef Offset = InputData.GetOrCreateDefaultDataReadReference<float>(
                METASOUND_GET_PARAM_NAME(InputOffset), InParams.OperatorSettings);

            return MakeUnique<FScaleDegreeToNoteNodeOperator>(InParams.OperatorSettings, UpdateTrigger, ScaleDegree, ScaleArray, Offset);
        }

        void Execute()
        {
            UpdateTrigger->ExecuteBlock(
                [](int32, int32) {},
                [this](int32, int32)
                {
                    const TArray<float>& Scale = *ScaleArray;
                    int32 NumSteps = Scale.Num();
                    int32 Degree = *ScaleDegree;

                    if (NumSteps <= 0)
                    {
                        *OutputNote = *Offset;
                        return;
                    }

                    int32 StepIndex = Degree % NumSteps;
                    if (StepIndex < 0)
                    {
                        StepIndex += NumSteps;
                    }

                    int32 Octave = (Degree - StepIndex) / NumSteps;

                    float Semitone = Scale[StepIndex] + (12.0f * static_cast<float>(Octave));
                    *OutputNote = Semitone + *Offset;
                }
            );
        }

    private:
        FTriggerReadRef UpdateTrigger;
        FInt32ReadRef ScaleDegree;
        TDataReadReference<TArray<float>> ScaleArray;
        FFloatReadRef Offset;
        FFloatWriteRef OutputNote;
    };

    class FScaleDegreeToNoteNode : public FNodeFacade
    {
    public:
        static FNodeClassMetadata CreateNodeClassMetadata()
        {
            return FScaleDegreeToNoteNodeOperator::GetNodeInfo();
        }

        FScaleDegreeToNoteNode(FNodeData InitData)
            : FNodeFacade(InitData, MakeShared<const FNodeClassMetadata>(FScaleDegreeToNoteNodeOperator::GetNodeInfo()), TFacadeOperatorClass<FScaleDegreeToNoteNodeOperator>())
        {
        }
    };

    METASOUND_REGISTER_NODE(FScaleDegreeToNoteNode);
}

#undef LOCTEXT_NAMESPACE
