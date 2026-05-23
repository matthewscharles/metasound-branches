// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundAnyOfBoolArrayNode.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundPrimitives.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"
#include "MetasoundBranches/Public/MetasoundCommonMacros.h"

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_AnyOfBoolArray"

namespace Metasound
{
    namespace AnyOfBoolArrayVertexNames
    {
        METASOUND_PARAM(InputTrigger, "Trigger", "Check the input array.");
        METASOUND_PARAM(InputBoolArray, "Input", "Array of boolean values.");
        METASOUND_PARAM(OutputResult, "Any True", "True if any value in the array is true.");
    }

    class FAnyOfBoolArrayOperator : public TExecutableOperator<FAnyOfBoolArrayOperator>
    {
    public:
        FAnyOfBoolArrayOperator(const FOperatorSettings& InSettings,
                                const FTriggerReadRef& InTrigger,
                                const TDataReadReference<TArray<bool>>& InBoolArray)
            : Trigger(InTrigger)
            , BoolArray(InBoolArray)
            , Result(FBoolWriteRef::CreateNew(false))
        {
        }

        static const FVertexInterface& DeclareVertexInterface()
        {
            using namespace AnyOfBoolArrayVertexNames;

            static const FVertexInterface Interface(
                FInputVertexInterface(
                    TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTrigger)),
                    TInputDataVertex<TArray<bool>>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputBoolArray))
                ),
                FOutputVertexInterface(
                    TOutputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputResult))
                )
            );

            return Interface;
        }

        static const FNodeClassMetadata& GetNodeInfo()
        {
            auto CreateNodeClassMetadata = []() -> FNodeClassMetadata
            {
                FVertexInterface Interface = DeclareVertexInterface();

                FNodeClassMetadata Metadata;
                Metadata.ClassName = { TEXT("UE"), TEXT("Any Of"), TEXT("Bool:Array") };
                Metadata.MajorVersion = 1;
                Metadata.MinorVersion = 0;
                Metadata.DisplayName = METASOUND_LOCTEXT("AnyOfBoolArrayDisplayName", "Any Of (Bool:Array)");
                Metadata.Description = METASOUND_LOCTEXT("AnyOfBoolArrayDesc", "Returns true if any value in the array is true.");
                Metadata.Author = TEXT("Charles Matthews");
                Metadata.DefaultInterface = Interface;
                Metadata.CategoryHierarchy = {
                    METASOUND_LOCTEXT("Custom", "Branches"),
                    METASOUND_LOCTEXT("CustomSub", "Array")
                };
                return Metadata;
            };

            static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
            return Metadata;
        }

        METASOUND_DISABLE_LEGACY_IO()

        void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
        {
            using namespace AnyOfBoolArrayVertexNames;
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputTrigger), Trigger);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputBoolArray), BoolArray);
        }

        void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
        {
            using namespace AnyOfBoolArrayVertexNames;
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputResult), Result);
        }

        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
        {
            using namespace AnyOfBoolArrayVertexNames;

            const FInputVertexInterfaceData& InputData = InParams.InputData;

            FTriggerReadRef Trigger = InputData.GetOrCreateDefaultDataReadReference<FTrigger>(
                METASOUND_GET_PARAM_NAME(InputTrigger), InParams.OperatorSettings);

            TDataReadReference<TArray<bool>> BoolArray = InputData.GetOrCreateDefaultDataReadReference<TArray<bool>>(
                METASOUND_GET_PARAM_NAME(InputBoolArray), InParams.OperatorSettings);

            return MakeUnique<FAnyOfBoolArrayOperator>(InParams.OperatorSettings, Trigger, BoolArray);
        }

        void Execute()
        {
            Trigger->ExecuteBlock(
                [](int32, int32) {},
                [this](int32, int32)
                {
                    const TArray<bool>& Bools = *BoolArray;
                    bool bAnyTrue = false;
                    for (bool b : Bools)
                    {
                        if (b)
                        {
                            bAnyTrue = true;
                            break;
                        }
                    }
                    *Result = bAnyTrue;
                }
            );
        }

    private:
        FTriggerReadRef Trigger;
        TDataReadReference<TArray<bool>> BoolArray;
        FBoolWriteRef Result;
    };

    class FAnyOfBoolArrayNode : public FNodeFacade
    {
    public:
                static FNodeClassMetadata CreateNodeClassMetadata()
        {
            return FAnyOfBoolArrayOperator::GetNodeInfo();
        }
        FAnyOfBoolArrayNode(FNodeData InitData)
            : FNodeFacade(InitData, MakeShared<const FNodeClassMetadata>(FAnyOfBoolArrayOperator::GetNodeInfo()), TFacadeOperatorClass<FAnyOfBoolArrayOperator>())
        {
        }
    };

    METASOUND_REGISTER_NODE(FAnyOfBoolArrayNode);
}

#undef LOCTEXT_NAMESPACE
