// Copyright 2026 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundGreatestCommonDivisorIntNode.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundPrimitives.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"
#include "MetasoundBranches/Public/MetasoundCommonMacros.h"

#define LOCTEXT_NAMESPACE "MetasoundGreatestCommonDivisorIntNode"

namespace Metasound
{
    namespace GCDIntNodeVertexNames
    {
        METASOUND_PARAM(InputA, "In A", "First integer.");
        METASOUND_PARAM(InputB, "In B", "Second integer.");
        METASOUND_PARAM(OutputValue, "Out", "Greatest common divisor of the two integers.");
    }

    class FGreatestCommonDivisorIntOperator : public TExecutableOperator<FGreatestCommonDivisorIntOperator>
    {
    public:
        FGreatestCommonDivisorIntOperator(
            const FOperatorSettings& InSettings,
            const FInt32ReadRef& InA,
            const FInt32ReadRef& InB)
            : InputA(InA)
            , InputB(InB)
            , OutputValue(FInt32WriteRef::CreateNew(0))
        {
        }

        static const FVertexInterface& DeclareVertexInterface()
        {
            using namespace GCDIntNodeVertexNames;

            static const FVertexInterface Interface(
                FInputVertexInterface(
                    TInputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputA)),
                    TInputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputB))
                ),
                FOutputVertexInterface(
                    TOutputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputValue))
                )
            );

            return Interface;
        }

        static const FNodeClassMetadata& GetNodeInfo()
        {
            auto CreateNodeClassMetadata = []() -> FNodeClassMetadata
            {
                FNodeClassMetadata Metadata;
                Metadata.ClassName = { TEXT("UE"), TEXT("GreatestCommonDivisor"), TEXT("Int32") };
                Metadata.MajorVersion = 1;
                Metadata.MinorVersion = 0;
                Metadata.DisplayName = METASOUND_LOCTEXT("GCDIntDisplayName", "Greatest Common Divisor");
                Metadata.Description = METASOUND_LOCTEXT("GCDIntDesc", "Returns the greatest common divisor of two integers.");
                Metadata.Author = "Charles Matthews";
                Metadata.PromptIfMissing = PluginNodeMissingPrompt;
                Metadata.DefaultInterface = DeclareVertexInterface();
                Metadata.CategoryHierarchy = {
                    METASOUND_LOCTEXT("Custom", "Branches"),
                    METASOUND_LOCTEXT("CustomSub", "Math")
                };

                Metadata.Keywords = {
                    METASOUND_LOCTEXT("GCDKeyword1", "Euclidean")
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
            using namespace GCDIntNodeVertexNames;
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputA), InputA);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputB), InputB);
        }

        virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
        {
            using namespace GCDIntNodeVertexNames;
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputValue), OutputValue);
        }

        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
        {
            using namespace GCDIntNodeVertexNames;

            const FInputVertexInterfaceData& InputData = InParams.InputData;

            TDataReadReference<int32> A = InputData.GetOrCreateDefaultDataReadReference<int32>(METASOUND_GET_PARAM_NAME(InputA), InParams.OperatorSettings);
            TDataReadReference<int32> B = InputData.GetOrCreateDefaultDataReadReference<int32>(METASOUND_GET_PARAM_NAME(InputB), InParams.OperatorSettings);

            return MakeUnique<FGreatestCommonDivisorIntOperator>(InParams.OperatorSettings, A, B);
        }

        virtual void Execute()
        {
            *OutputValue = FMath::GreatestCommonDivisor(*InputA, *InputB);
        }

    private:
        FInt32ReadRef InputA;
        FInt32ReadRef InputB;
        FInt32WriteRef OutputValue;
    };

    class FGreatestCommonDivisorIntNode : public FNodeFacade
    {
    public:
                static FNodeClassMetadata CreateNodeClassMetadata()
        {
            return FGreatestCommonDivisorIntOperator::GetNodeInfo();
        }
        FGreatestCommonDivisorIntNode(FNodeData InitData)
            : FNodeFacade(InitData, MakeShared<const FNodeClassMetadata>(FGreatestCommonDivisorIntOperator::GetNodeInfo()), TFacadeOperatorClass<FGreatestCommonDivisorIntOperator>())
        {
        }
    };

    METASOUND_REGISTER_NODE(FGreatestCommonDivisorIntNode);
}

#undef LOCTEXT_NAMESPACE