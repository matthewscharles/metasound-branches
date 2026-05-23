// Copyright 2026 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundBreakNoteFloatNode.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundPrimitives.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"
#include "MetasoundTime.h"
#include "MetasoundSampleCounter.h"
#include "MetasoundBranches/Public/MetasoundCommonMacros.h"

#define LOCTEXT_NAMESPACE "MetasoundBreakNoteFloatNode"

namespace Metasound
{
    namespace BreakNoteFloatNodeVertexNames
    {
        METASOUND_PARAM(InputTrigger, "Note", "Incoming note trigger.");
        METASOUND_PARAM(InputNoteData, "Note Data", "Array containing pitch and velocity.");
        METASOUND_PARAM(OutputPitch, "Pitch", "Extracted pitch value.");
        METASOUND_PARAM(OutputVelocity, "Velocity", "Extracted velocity value.");
        METASOUND_PARAM(OutputNoteOn, "Note On", "Triggers a note-on event.");
        METASOUND_PARAM(OutputNoteOff, "Note Off", "Triggers a note-off event.");
    }
    
    class FBreakNoteFloatOperator : public TExecutableOperator<FBreakNoteFloatOperator>
    {
    public:
        FBreakNoteFloatOperator(
            const FOperatorSettings& InSettings,
            const FTriggerReadRef& InTrigger,
            const TDataReadReference<TArray<float>>& InNoteData)
            : InputTrigger(InTrigger)
            , InputNoteData(InNoteData)
            , OutputPitch(FFloatWriteRef::CreateNew(0.0f))
            , OutputVelocity(FFloatWriteRef::CreateNew(0.0f))
            , OutputNoteOn(FTriggerWriteRef::CreateNew(InSettings))
            , OutputNoteOff(FTriggerWriteRef::CreateNew(InSettings))
        {
        }

        static const FVertexInterface& DeclareVertexInterface()
        {
            using namespace BreakNoteFloatNodeVertexNames;

            static const FVertexInterface Interface(
                FInputVertexInterface(
                    TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTrigger)),
                    TInputDataVertex<TArray<float>>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputNoteData))
                ),
                FOutputVertexInterface(
                    TOutputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputPitch)),
                    TOutputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputVelocity)),
                    TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputNoteOn)),
                    TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputNoteOff))
                )
            );

            return Interface;
        }
        
        static const FNodeClassMetadata& GetNodeInfo()
        {
            auto CreateNodeClassMetadata = []() -> FNodeClassMetadata
            {
                FNodeClassMetadata Metadata;
                Metadata.ClassName = { TEXT("UE"), TEXT("Break Note (Float)"), TEXT("Float") };
                Metadata.MajorVersion = 1;
                Metadata.MinorVersion = 0;
                Metadata.DisplayName = METASOUND_LOCTEXT("BreakNoteFloatDisplayName", "Break Note (Float)");
                Metadata.Description = METASOUND_LOCTEXT("BreakNoteFloatDesc", "Unpacks pitch and velocity from a note array.");
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
        
        virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
        {
            using namespace BreakNoteFloatNodeVertexNames;
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputTrigger), InputTrigger);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputNoteData), InputNoteData);
        }
        
        virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
        {
            using namespace BreakNoteFloatNodeVertexNames;
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputPitch), OutputPitch);
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputVelocity), OutputVelocity);
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputNoteOn), OutputNoteOn);
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputNoteOff), OutputNoteOff);
        }
        
        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults)
        {
            using namespace BreakNoteFloatNodeVertexNames;
            const FInputVertexInterfaceData& InputData = InParams.InputData;

            FTriggerReadRef InTrigger = InputData.GetOrCreateDefaultDataReadReference<FTrigger>(
                METASOUND_GET_PARAM_NAME(InputTrigger),
                InParams.OperatorSettings
            );
            TDataReadReference<TArray<float>> InNoteData = InputData.GetOrCreateDefaultDataReadReference<TArray<float>>(
                METASOUND_GET_PARAM_NAME(InputNoteData),
                InParams.OperatorSettings
            );

            return MakeUnique<FBreakNoteFloatOperator>(InParams.OperatorSettings, InTrigger, InNoteData);
        }
        
        virtual void Execute()
        {
            OutputNoteOn->AdvanceBlock();
            OutputNoteOff->AdvanceBlock();

            InputTrigger->ExecuteBlock(
                [](int32 StartFrame, int32 EndFrame) {},
                [this](int32 StartFrame, int32 EndFrame)
                {
                    if (InputNoteData->Num() >= 2)
                    {
                        float Pitch = (*InputNoteData)[0];
                        float Velocity = (*InputNoteData)[1];

                        *OutputPitch = Pitch;
                        *OutputVelocity = Velocity;

                        if (Velocity > 0.0f)
                        {
                            OutputNoteOn->TriggerFrame(StartFrame);
                        }
                        else
                        {
                            OutputNoteOff->TriggerFrame(StartFrame);
                        }
                    }
                }
            );
        }

    private:
        FTriggerReadRef InputTrigger;
        TDataReadReference<TArray<float>> InputNoteData;

        FFloatWriteRef OutputPitch;
        FFloatWriteRef OutputVelocity;
        FTriggerWriteRef OutputNoteOn;
        FTriggerWriteRef OutputNoteOff;
    };

    class FBreakNoteFloatNode : public FNodeFacade
    {
    public:
                static FNodeClassMetadata CreateNodeClassMetadata()
        {
            return FBreakNoteFloatOperator::GetNodeInfo();
        }
        FBreakNoteFloatNode(FNodeData InitData)
            : FNodeFacade(InitData, MakeShared<const FNodeClassMetadata>(FBreakNoteFloatOperator::GetNodeInfo()), TFacadeOperatorClass<FBreakNoteFloatOperator>())
        {
        }
    };

    METASOUND_REGISTER_NODE(FBreakNoteFloatNode);
}

#undef LOCTEXT_NAMESPACE
