// Copyright 2025 Charles Matthews. All Rights Reserved.

#pragma once

#include "MetasoundBranches/Public/ParseMidiNote.h"
#include "Interfaces/MetasoundFrontendSourceInterface.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundStandardNodesNames.h"
#include "Internationalization/Text.h"
#include "MetasoundBuilderInterface.h"
#include "MetasoundFacade.h"
#include "MetasoundNodeInterface.h"
#include "MetasoundOperatorInterface.h"
#include "MetasoundParamHelper.h"
#include "MetasoundPrimitives.h"
#include "MetasoundTrigger.h"
#include "MetasoundVertex.h"
#include <sstream>

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_ParseMidiNote"

namespace Metasound
{
    namespace ParseMidiNoteVertexNames
    {
        METASOUND_PARAM(InputTriggerParse, "Parse", "Trigger to parse the note.")
        METASOUND_PARAM(InputNoteString, "Note", "Note string.")
        METASOUND_PARAM(OutputTriggerOnParse, "On Parse", "Triggers when note is parsed.")
        METASOUND_PARAM(OutputValue, "Value", "Parsed MIDI note value.")
    }

    template<typename ElementType>
    class TParseMidiNoteOperator : public TExecutableOperator<TParseMidiNoteOperator<ElementType>>
    {
        using FValueWriteRef = TDataWriteReference<ElementType>;

    public:
        static const FVertexInterface& GetDefaultInterface()
        {
            using namespace ParseMidiNoteVertexNames;
            static const FVertexInterface DefaultInterface(
                FInputVertexInterface(
                    TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTriggerParse)),
                    TInputDataVertex<FString>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputNoteString))
                ),
                FOutputVertexInterface(
                    TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputTriggerOnParse)),
                    TOutputDataVertex<ElementType>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputValue))
                )
            );
            return DefaultInterface;
        }

        static const FNodeClassMetadata& GetNodeInfo()
        {
            auto CreateNodeClassMetadata = []() -> FNodeClassMetadata
            {
                FNodeClassMetadata Metadata;
                Metadata.ClassName = { StandardNodes::Namespace, TEXT("ParseMidiNote"), StandardNodes::AudioVariant };
                Metadata.MajorVersion = 1;
                Metadata.MinorVersion = 0;
                Metadata.DisplayName = METASOUND_LOCTEXT_FORMAT("ParseMidiNoteName", "Parse MIDI Note ({0})", GetMetasoundDataTypeDisplayText<ElementType>());
                Metadata.Description = LOCTEXT("ParseMidiNoteDesc", "Parses a note string to MIDI.");
                Metadata.Author = TEXT("Charles Matthews");
                Metadata.PromptIfMissing = PluginNodeMissingPrompt;
                Metadata.CategoryHierarchy = {
                    METASOUND_LOCTEXT("Custom", "Branches"),
                    METASOUND_LOCTEXT("CustomSub", "MIDI")
                };
                Metadata.Keywords = TArray<FText>();

                return Metadata;
            };

            static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
            return Metadata;
        }

        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults)
        {
            using namespace ParseMidiNoteVertexNames;
            const FInputVertexInterfaceData& InputData = InParams.InputData;

            TDataReadReference<FTrigger> InTriggerParse = InputData.GetOrCreateDefaultDataReadReference<FTrigger>(
                METASOUND_GET_PARAM_NAME(InputTriggerParse),
                InParams.OperatorSettings
            );

            TDataReadReference<FString> InNoteString = InputData.GetOrCreateDefaultDataReadReference<FString>(
                METASOUND_GET_PARAM_NAME(InputNoteString),
                InParams.OperatorSettings
            );

            return MakeUnique<TParseMidiNoteOperator>(
                InTriggerParse,
                InNoteString,
                InParams.OperatorSettings
            );
        }

        TParseMidiNoteOperator(
            TDataReadReference<FTrigger> InTriggerParse,
            TDataReadReference<FString> InNoteString,
            const FOperatorSettings& InSettings
        )
            : TriggerParse(InTriggerParse)
            , NoteString(InNoteString)
            , TriggerOnParse(FTriggerWriteRef::CreateNew(InSettings))
            , OutValue(TDataWriteReferenceFactory<ElementType>::CreateAny(InSettings))
        {
        }

        virtual ~TParseMidiNoteOperator() = default;

        virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
        {
            using namespace ParseMidiNoteVertexNames;
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputTriggerParse), TriggerParse);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputNoteString), NoteString);
        }

        virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
        {
            using namespace ParseMidiNoteVertexNames;
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutputTriggerOnParse), TriggerOnParse);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutputValue), OutValue);
        }

        virtual FDataReferenceCollection GetInputs() const override
        {
            checkNoEntry();
            return {};
        }

        virtual FDataReferenceCollection GetOutputs() const override
        {
            checkNoEntry();
            return {};
        }

        void Execute()
        {
            TriggerOnParse->AdvanceBlock();
            if (*TriggerParse)
            {
                float ParsedFloat = static_cast<float>(ParseMidiNote(TCHAR_TO_ANSI(**NoteString)));
                *OutValue = static_cast<ElementType>(ParsedFloat);
                TriggerParse->ExecuteBlock(
                    [](int32, int32) {},
                    [this](int32 StartFrame, int32) { TriggerOnParse->TriggerFrame(StartFrame); }
                );
            }
        }

    private:
        TDataReadReference<FTrigger> TriggerParse;
        TDataReadReference<FString> NoteString;
        TDataWriteReference<FTrigger> TriggerOnParse;
        FValueWriteRef OutValue;
    };

    template<typename ElementType>
    class TParseMidiNoteNode : public FNodeFacade
    {
    public:
        TParseMidiNoteNode(const FNodeInitData& InInitData)
            : FNodeFacade(InInitData.InstanceName, InInitData.InstanceID, TFacadeOperatorClass<TParseMidiNoteOperator<ElementType>>())
        {
        }
        virtual ~TParseMidiNoteNode() = default;
    };
}

#undef LOCTEXT_NAMESPACE