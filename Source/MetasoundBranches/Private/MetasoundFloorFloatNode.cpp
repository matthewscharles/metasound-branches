// Copyright 2026 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundFloorFloatNode.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundPrimitives.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"
#include "MetasoundBranches/Public/MetasoundCommonMacros.h"

#define LOCTEXT_NAMESPACE "MetasoundFloorFloatNode"

namespace Metasound
{
    namespace FloorFloatNodeVertexNames
    {
        METASOUND_PARAM(InputSignal, "In", "Input value to be floored.");
        METASOUND_PARAM(OutputSignal, "Out", "Resulting value after flooring.");
    }

    class FFloorFloatOperator : public TExecutableOperator<FFloorFloatOperator>
    {
    public:
        FFloorFloatOperator(
            const FOperatorSettings& InSettings,
            const FFloatReadRef& InSignal)
            : InputSignal(InSignal)
            , OutputSignal(FFloatWriteRef::CreateNew(0.0f))
        {
        }

        static const FVertexInterface& DeclareVertexInterface()
        {
            using namespace FloorFloatNodeVertexNames;

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
                Metadata.ClassName = { TEXT("UE"), TEXT("Floor"), TEXT("Float") };
                Metadata.MajorVersion = 1;
                Metadata.MinorVersion = 0;
                Metadata.DisplayName = METASOUND_LOCTEXT("FloorFloatDisplayName", "Floor");
                Metadata.Description = METASOUND_LOCTEXT("FloorFloatDesc", "Returns the largest integer less than or equal to the input, as a float.");
                Metadata.Author = "Charles Matthews";
                Metadata.PromptIfMissing = PluginNodeMissingPrompt;
                Metadata.DefaultInterface = DeclareVertexInterface();
                Metadata.CategoryHierarchy = {
                    METASOUND_LOCTEXT("Custom", "Branches"),
                    METASOUND_LOCTEXT("CustomSub", "Math")
                };
                
                Metadata.Keywords = {
					METASOUND_LOCTEXT("GreaterThanKeyword", "Int"),
					METASOUND_LOCTEXT("GreaterThanKeyword2", "Down")
				};
                
                METASOUND_BRANCHES_APPLY_NODE_STYLE(Metadata);
                
                return Metadata;
            };

            static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
            return Metadata;
        }

        METASOUND_DISABLE_LEGACY_IO()

        virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
        {
            using namespace FloorFloatNodeVertexNames;
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputSignal), InputSignal);
        }

        virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
        {
            using namespace FloorFloatNodeVertexNames;
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputSignal), OutputSignal);
        }

        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
        {
            using namespace FloorFloatNodeVertexNames;

            const FInputVertexInterfaceData& InputData = InParams.InputData;

            TDataReadReference<float> InputSignal = InputData.GetOrCreateDefaultDataReadReference<float>(
                METASOUND_GET_PARAM_NAME(InputSignal),
                InParams.OperatorSettings
            );

            return MakeUnique<FFloorFloatOperator>(InParams.OperatorSettings, InputSignal);
        }

        virtual void Execute()
        {
            *OutputSignal = FMath::FloorToFloat(*InputSignal);
        }

    private:
        FFloatReadRef InputSignal;
        FFloatWriteRef OutputSignal;
    };

    class FFloorFloatNode : public FNodeFacade
    {
    public:
                static FNodeClassMetadata CreateNodeClassMetadata()
        {
            return FFloorFloatOperator::GetNodeInfo();
        }
        FFloorFloatNode(FNodeData InitData)
            : FNodeFacade(InitData, MakeShared<const FNodeClassMetadata>(FFloorFloatOperator::GetNodeInfo()), TFacadeOperatorClass<FFloorFloatOperator>())
        {
        }
    };

    METASOUND_REGISTER_NODE(FFloorFloatNode);
}

#undef LOCTEXT_NAMESPACE