// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundRoundFloatNode.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundPrimitives.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"
#include "MetasoundBranches/Public/MetasoundCommonMacros.h"

#define LOCTEXT_NAMESPACE "MetasoundRoundFloatNode"

namespace Metasound
{
    namespace RoundFloatNodeVertexNames
    {
        METASOUND_PARAM(InputSignal, "In", "Float to round.");
        METASOUND_PARAM(OutputSignal, "Out", "Rounded float.");
    }

    class FRoundFloatOperator : public TExecutableOperator<FRoundFloatOperator>
    {
    public:
        FRoundFloatOperator(
            const FOperatorSettings& InSettings,
            const FFloatReadRef& InSignal)
            : InputSignal(InSignal)
            , OutputSignal(FFloatWriteRef::CreateNew(0.0f))
        {
        }

        static const FVertexInterface& DeclareVertexInterface()
        {
            using namespace RoundFloatNodeVertexNames;

            static const FVertexInterface Interface(
                FInputVertexInterface(
                    TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputSignal))
                ),
                FOutputVertexInterface(
                    TOutputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputSignal))
                )
            );

            return Interface;
        }

        static const FNodeClassMetadata& GetNodeInfo()
        {
            auto CreateNodeClassMetadata = []() -> FNodeClassMetadata
            {
                FNodeClassMetadata Metadata;
                Metadata.ClassName = { TEXT("UE"), TEXT("Round"), TEXT("Float") };
                Metadata.MajorVersion = 1;
                Metadata.MinorVersion = 0;
                Metadata.DisplayName = METASOUND_LOCTEXT("RoundFloatDisplayName", "Round");
                Metadata.Description = METASOUND_LOCTEXT("RoundFloatDesc", "Rounds the input to the nearest whole number, returned as a float.");
                Metadata.Author = "Charles Matthews";
                Metadata.PromptIfMissing = PluginNodeMissingPrompt;
                Metadata.DefaultInterface = DeclareVertexInterface();
                Metadata.CategoryHierarchy = {
                    METASOUND_LOCTEXT("Custom", "Branches"),
                    METASOUND_LOCTEXT("CustomSub", "Math")
                };
                return Metadata;
            };

            static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
            return Metadata;
        }

        METASOUND_DISABLE_LEGACY_IO()

        virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
        {
            using namespace RoundFloatNodeVertexNames;
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputSignal), InputSignal);
        }

        virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
        {
            using namespace RoundFloatNodeVertexNames;
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputSignal), OutputSignal);
        }

        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
        {
            using namespace RoundFloatNodeVertexNames;

            const FInputVertexInterfaceData& InputData = InParams.InputData;

            TDataReadReference<float> InputSignal = InputData.GetOrCreateDefaultDataReadReference<float>(
                METASOUND_GET_PARAM_NAME(InputSignal),
                InParams.OperatorSettings
            );

            return MakeUnique<FRoundFloatOperator>(InParams.OperatorSettings, InputSignal);
        }

        virtual void Execute() override
        {
            *OutputSignal = FMath::RoundToFloat(*InputSignal);
        }

    private:
        FFloatReadRef InputSignal;
        FFloatWriteRef OutputSignal;
    };

    class FRoundFloatNode : public FNodeFacade
    {
    public:
        FRoundFloatNode(const FNodeInitData& InitData)
            : FNodeFacade(InitData.InstanceName, InitData.InstanceID, TFacadeOperatorClass<FRoundFloatOperator>())
        {
        }
    };

    METASOUND_REGISTER_NODE(FRoundFloatNode);
}

#undef LOCTEXT_NAMESPACE