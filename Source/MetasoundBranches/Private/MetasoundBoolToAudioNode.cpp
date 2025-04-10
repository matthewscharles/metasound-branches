// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundBoolToAudioNode.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundPrimitives.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundStandardNodesNames.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"
#include "MetasoundBranches/Public/MetasoundCommonMacros.h"

#define LOCTEXT_NAMESPACE "MetasoundBoolToAudioNode"

namespace Metasound
{
    namespace BoolToAudioNodeVertexNames
    {
        METASOUND_PARAM(InputBool, "Value", "Boolean input to convert to audio.");
        METASOUND_PARAM(OutputSignal, "Out", "Audio signal.");
    }

    class FBoolToAudioOperator : public TExecutableOperator<FBoolToAudioOperator>
    {
    public:
        FBoolToAudioOperator(const FOperatorSettings& InSettings, const FBoolReadRef& InBool)
            : InputBool(InBool)
            , OutputSignal(FAudioBufferWriteRef::CreateNew(InSettings))
        {
        }

        static const FVertexInterface& DeclareVertexInterface()
        {
            using namespace BoolToAudioNodeVertexNames;

            static const FVertexInterface Interface(
                FInputVertexInterface(
                    TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputBool))
                ),
                FOutputVertexInterface(
                    TOutputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputSignal))
                )
            );

            return Interface;
        }

        static const FNodeClassMetadata& GetNodeInfo()
        {
            auto CreateNodeClassMetadata = []() -> FNodeClassMetadata
            {
                FNodeClassMetadata Metadata;
                Metadata.ClassName = { TEXT("UE"), TEXT("BoolToAudio"), TEXT("Audio") };
                Metadata.MajorVersion = 2;
                Metadata.MinorVersion = 0;
                Metadata.DisplayName = METASOUND_LOCTEXT("BoolToAudioDisplayName", "Bool To Audio");
                Metadata.Description = METASOUND_LOCTEXT("BoolToAudioDesc", "Converts a boolean value (block rate) to audio signal (0 or 1).");
                Metadata.Author = "Charles Matthews";
                Metadata.PromptIfMissing = PluginNodeMissingPrompt;
                Metadata.DefaultInterface = DeclareVertexInterface();
                Metadata.CategoryHierarchy = { 
                    METASOUND_LOCTEXT("Custom", "Branches"),
                    METASOUND_LOCTEXT("Custom_Conversions", "Conversions")
                };

                FNodeDisplayStyle DisplayStyle;
                DisplayStyle.ImageName = TEXT("MetasoundEditor.Graph.Node.Conversion");
                DisplayStyle.bShowName = false;
                DisplayStyle.bShowInputNames = false;
                DisplayStyle.bShowOutputNames = false;
                Metadata.DisplayStyle = DisplayStyle;

                return Metadata;
            };

            static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
            return Metadata;
        }

        METASOUND_DISABLE_LEGACY_IO()
        
        virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
        {
            using namespace BoolToAudioNodeVertexNames;
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputBool), InputBool);
        }

        virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
        {
            using namespace BoolToAudioNodeVertexNames;
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutputSignal), OutputSignal);
        }
        
        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
        {
            using namespace BoolToAudioNodeVertexNames;

            const FInputVertexInterfaceData& InputData = InParams.InputData;

            TDataReadReference<bool> InputBool = InputData.GetOrCreateDefaultDataReadReference<bool>(
                METASOUND_GET_PARAM_NAME(InputBool),
                InParams.OperatorSettings
            );

            return MakeUnique<FBoolToAudioOperator>(InParams.OperatorSettings, InputBool);
        }

        virtual void Execute()
        {
            const float OutputValue = *InputBool ? 1.0f : 0.0f;
            float* OutputDataPtr = OutputSignal->GetData();
            const int32 NumFrames = OutputSignal->Num();

            for (int32 i = 0; i < NumFrames; ++i)
            {
                OutputDataPtr[i] = OutputValue;
            }
        }

    private:
        FBoolReadRef InputBool;
        FAudioBufferWriteRef OutputSignal;
    };

    class FBoolToAudioNode : public FNodeFacade
    {
    public:
        FBoolToAudioNode(const FNodeInitData& InitData)
            : FNodeFacade(InitData.InstanceName, InitData.InstanceID, TFacadeOperatorClass<FBoolToAudioOperator>())
        {
        }
    };

    METASOUND_REGISTER_NODE(FBoolToAudioNode);
}

#undef LOCTEXT_NAMESPACE