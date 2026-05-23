// Copyright 2026 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundGetRangePctNode.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundPrimitives.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"
#include "MetasoundBranches/Public/MetasoundCommonMacros.h"

#define LOCTEXT_NAMESPACE "MetasoundGetRangePctNode"

namespace Metasound
{
    namespace GetRangePctNodeVertexNames
    {
        METASOUND_PARAM(InputA, "In A", "Minimum edge of the range.");
        METASOUND_PARAM(InputB, "In B", "Maximum edge of the range.");
        METASOUND_PARAM(InputX, "In X", "Value to evaluate.");
        METASOUND_PARAM(OutputSignal, "Out", "Normalized position of X between A and B.");
    }

    class FGetRangePctOperator : public TExecutableOperator<FGetRangePctOperator>
    {
    public:
        FGetRangePctOperator(
            const FOperatorSettings& InSettings,
            const FFloatReadRef& InA,
            const FFloatReadRef& InB,
            const FFloatReadRef& InX)
            : InputA(InA)
            , InputB(InB)
            , InputX(InX)
            , OutputSignal(FFloatWriteRef::CreateNew(0.0f))
        {
        }

        static const FVertexInterface& DeclareVertexInterface()
        {
            using namespace GetRangePctNodeVertexNames;

            static const FVertexInterface Interface(
                FInputVertexInterface(
                    TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputA)),
                    TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputB)),
                    TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputX))
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
                Metadata.ClassName = { TEXT("UE"), TEXT("GetRangePct"), TEXT("Float") };
                Metadata.MajorVersion = 1;
                Metadata.MinorVersion = 0;
                Metadata.DisplayName = METASOUND_LOCTEXT("GetRangePctFloatDisplayName", "Get Range Percent");
                Metadata.Description = METASOUND_LOCTEXT("GetRangePctFloatDesc", "Returns the normalized value of X between A and B (0–1).");
                Metadata.Author = "Charles Matthews";
                Metadata.PromptIfMissing = PluginNodeMissingPrompt;
                Metadata.DefaultInterface = DeclareVertexInterface();
                Metadata.CategoryHierarchy = {
                    METASOUND_LOCTEXT("Custom", "Branches"),
                    METASOUND_LOCTEXT("CustomSub", "Math")
                };

                Metadata.Keywords = {
                    METASOUND_LOCTEXT("RangePctKeyword", "Normalize"),
                    METASOUND_LOCTEXT("RangePctKeyword2", "Range"),
                    METASOUND_LOCTEXT("RangePctKeyword3", "Remap"),
					METASOUND_LOCTEXT("RangePctKeyword3", "GetRangePct")
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
            using namespace GetRangePctNodeVertexNames;
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputA), InputA);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputB), InputB);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputX), InputX);
        }

        virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
        {
            using namespace GetRangePctNodeVertexNames;
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputSignal), OutputSignal);
        }

        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
        {
            using namespace GetRangePctNodeVertexNames;

            const FInputVertexInterfaceData& InputData = InParams.InputData;

            TDataReadReference<float> A = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputA), InParams.OperatorSettings);
            TDataReadReference<float> B = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputB), InParams.OperatorSettings);
            TDataReadReference<float> X = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputX), InParams.OperatorSettings);

            return MakeUnique<FGetRangePctOperator>(InParams.OperatorSettings, A, B, X);
        }

        virtual void Execute()
        {
            *OutputSignal = FMath::GetRangePct(*InputA, *InputB, *InputX);
        }

    private:
        FFloatReadRef InputA;
        FFloatReadRef InputB;
        FFloatReadRef InputX;
        FFloatWriteRef OutputSignal;
    };

    class FGetRangePctNode : public FNodeFacade
    {
    public:
                static FNodeClassMetadata CreateNodeClassMetadata()
        {
            return FGetRangePctOperator::GetNodeInfo();
        }
        FGetRangePctNode(FNodeData InitData)
            : FNodeFacade(InitData, MakeShared<const FNodeClassMetadata>(FGetRangePctOperator::GetNodeInfo()), TFacadeOperatorClass<FGetRangePctOperator>())
        {
        }
    };

    METASOUND_REGISTER_NODE(FGetRangePctNode);
}

#undef LOCTEXT_NAMESPACE