// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundMakeNoteFloatNode.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundPrimitives.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"

#define LOCTEXT_NAMESPACE "MetasoundMakeNoteFloatNode"

namespace Metasound
{
    namespace MakeNoteFloatNodeVertexNames
    {
        METASOUND_PARAM(InputNoteOn, "Note On", "Triggers a note-on event.");
        METASOUND_PARAM(InputNoteOff, "Note Off", "Triggers a note-off event (override).");
        METASOUND_PARAM(InputPitch, "Pitch", "Pitch value of the note.");
        METASOUND_PARAM(InputVelocity, "Velocity", "Velocity of the note.");
        METASOUND_PARAM(InputDuration, "Duration", "Duration of the note before auto note-off.");

        METASOUND_PARAM(OutputNoteOn, "Note On", "Triggers a note-on event.");
        METASOUND_PARAM(OutputNoteOff, "Note Off", "Triggers a note-off event.");
        METASOUND_PARAM(OutputArray, "Note Data", "Array containing pitch and velocity.");
        METASOUND_PARAM(OutputOverride, "Override", "Triggers when note-off is overridden.");
    }

    class FMakeNoteFloatOperator : public TExecutableOperator<FMakeNoteFloatOperator>
    {
    public:
        FMakeNoteFloatOperator(
            const FOperatorSettings& InSettings,
            const FTriggerReadRef& InNoteOn,
            const FTriggerReadRef& InNoteOff,
            const FFloatReadRef& InPitch,
            const FFloatReadRef& InVelocity,
            const FTimeReadRef& InDuration)
            : InputNoteOn(InNoteOn)
            , InputNoteOff(InNoteOff)
            , InputPitch(InPitch)
            , InputVelocity(InVelocity)
            , InputDuration(InDuration)
            , OutputNoteOn(FTriggerWriteRef::CreateNew(InSettings))
            , OutputNoteOff(FTriggerWriteRef::CreateNew(InSettings))
            , OutputArray(TDataWriteReference<TArray<float>>::CreateNew())
            , OutputOverride(FTriggerWriteRef::CreateNew(InSettings))
            , Active(false)
            , NoteEndTime(0.0)
        {
            OutputArray->Init(0, 2);
        }

        static const FVertexInterface& DeclareVertexInterface()
        {
            using namespace MakeNoteFloatNodeVertexNames;

            static const FVertexInterface Interface(
                FInputVertexInterface(
                    TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputNoteOn)),
                    TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputNoteOff)),
                    TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputPitch)),
                    TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputVelocity)),
                    TInputDataVertex<FTime>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputDuration))
                ),
                FOutputVertexInterface(
                    TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputNoteOn)),
                    TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputNoteOff)),
                    TOutputDataVertex<TArray<float>>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputArray)),
                    TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputOverride))
                )
            );

            return Interface;
        }

        static const FNodeClassMetadata& GetNodeInfo()
        {
            auto CreateNodeClassMetadata = []() -> FNodeClassMetadata
            {
                FNodeClassMetadata Metadata;
                Metadata.ClassName = { TEXT("UE"), TEXT("MakeNote(Float)"), TEXT("Float") };
                Metadata.MajorVersion = 1;
                Metadata.MinorVersion = 0;
                Metadata.DisplayName = METASOUND_LOCTEXT("MakeNoteFloatDisplayName", "Make Note (Float)");
                Metadata.Description = METASOUND_LOCTEXT("MakeNoteFloatDesc", "Generates note-on and note-off events with a specified duration and allows override.");
                Metadata.Author = "Charles Matthews";
                Metadata.DefaultInterface = DeclareVertexInterface();
                return Metadata;
            };

            static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
            return Metadata;
        }

        virtual FDataReferenceCollection GetInputs() const override
        {
            using namespace MakeNoteFloatNodeVertexNames;
            FDataReferenceCollection Inputs;
            Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputNoteOn), InputNoteOn);
            Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputNoteOff), InputNoteOff);
            Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputPitch), InputPitch);
            Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputVelocity), InputVelocity);
            Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputDuration), InputDuration);
            return Inputs;
        }
        
        virtual FDataReferenceCollection GetOutputs() const override
        {
            using namespace MakeNoteFloatNodeVertexNames;
            FDataReferenceCollection Outputs;
            Outputs.AddDataWriteReference(METASOUND_GET_PARAM_NAME(OutputNoteOn), OutputNoteOn);
            Outputs.AddDataWriteReference(METASOUND_GET_PARAM_NAME(OutputNoteOff), OutputNoteOff);
            Outputs.AddDataWriteReference(METASOUND_GET_PARAM_NAME(OutputArray), OutputArray);
            Outputs.AddDataWriteReference(METASOUND_GET_PARAM_NAME(OutputOverride), OutputOverride);
            return Outputs;
        }
        
        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
        {
            return MakeUnique<FMakeNoteFloatOperator>(
                InParams.OperatorSettings,
                InParams.InputData.GetOrCreateDefaultDataReadReference<FTrigger>(METASOUND_GET_PARAM_NAME(MakeNoteFloatNodeVertexNames::InputNoteOn), InParams.OperatorSettings),
                InParams.InputData.GetOrCreateDefaultDataReadReference<FTrigger>(METASOUND_GET_PARAM_NAME(MakeNoteFloatNodeVertexNames::InputNoteOff), InParams.OperatorSettings),
                InParams.InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(MakeNoteFloatNodeVertexNames::InputPitch), InParams.OperatorSettings),
                InParams.InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(MakeNoteFloatNodeVertexNames::InputVelocity), InParams.OperatorSettings),
                InParams.InputData.GetOrCreateDefaultDataReadReference<FTime>(METASOUND_GET_PARAM_NAME(MakeNoteFloatNodeVertexNames::InputDuration), InParams.OperatorSettings)
            );
        }
        
        virtual void Execute()
        {
            OutputNoteOn->AdvanceBlock();
            OutputNoteOff->AdvanceBlock();
            OutputOverride->AdvanceBlock();
            
            int32 CurrentFrame = 0;
            if (InputNoteOn->IsTriggeredInBlock())
            {
                Active = true;
                NoteEndTime = FPlatformTime::Seconds() + InputDuration->GetSeconds();
                (*OutputArray) = { *InputPitch, *InputVelocity };
                OutputNoteOn->TriggerFrame(CurrentFrame);
            }

            if (InputNoteOff->IsTriggeredInBlock() && Active)
            {
                Active = false;
                (*OutputArray) = { *InputPitch, 0.0f };
                OutputNoteOff->TriggerFrame(CurrentFrame);
                OutputOverride->TriggerFrame(CurrentFrame);
            }

            if (Active && FPlatformTime::Seconds() >= NoteEndTime)
            {
                Active = false;
                (*OutputArray) = { *InputPitch, 0.0f };
                OutputNoteOff->TriggerFrame(CurrentFrame);
            }
        }


        
        private:
        FTriggerReadRef InputNoteOn;
        FTriggerReadRef InputNoteOff;
        FFloatReadRef InputPitch;
        FFloatReadRef InputVelocity;
        FTimeReadRef InputDuration;
    
        FTriggerWriteRef OutputNoteOn;
        FTriggerWriteRef OutputNoteOff;
        TDataWriteReference<TArray<float>> OutputArray;
        FTriggerWriteRef OutputOverride;
    
        bool Active;
        double NoteEndTime;
        
    };

    
    class FMakeNoteFloatNode : public FNodeFacade
    {
    public:
        FMakeNoteFloatNode(const FNodeInitData& InitData)
            : FNodeFacade(InitData.InstanceName, InitData.InstanceID, TFacadeOperatorClass<FMakeNoteFloatOperator>())
        {
        }
    };
    
    METASOUND_REGISTER_NODE(FMakeNoteFloatNode);
}

#undef LOCTEXT_NAMESPACE