// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundAllOfBoolArrayNode.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundPrimitives.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"
#include "MetasoundBranches/Public/MetasoundCommonMacros.h"

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_AllOfBoolArray"

namespace Metasound
{
    namespace AllOfBoolArrayVertexNames
    {
        METASOUND_PARAM(InputTrigger, "Trigger", "Check the input array.");
        METASOUND_PARAM(InputBoolArray, "Input", "Array of boolean values.");
        METASOUND_PARAM(OutputResult, "All True", "True if all values in the array are true.");
    }

    class FAllOfBoolArrayOperator : public TExecutableOperator<FAllOfBoolArrayOperator>
    {
    public:
        FAllOfBoolArrayOperator(const FOperatorSettings& InSettings,
                                const FTriggerReadRef& InTrigger,
                                const TDataReadReference<TArray<bool>>& InBoolArray)
            : Trigger(InTrigger)
            , BoolArray(InBoolArray)
            , Result(FBoolWriteRef::CreateNew(false))
        {
        }

        static const FVertexInterface& DeclareVertexInterface()
        {
            using namespace AllOfBoolArrayVertexNames;

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
                Metadata.ClassName = { TEXT("UE"), TEXT("All Of"), TEXT("Bool:Array") };
                Metadata.MajorVersion = 1;
                Metadata.MinorVersion = 0;
                Metadata.DisplayName = METASOUND_LOCTEXT("AllOfBoolArrayDisplayName", "All Of (Bool:Array)");
                Metadata.Description = METASOUND_LOCTEXT("AllOfBoolArrayDesc", "Returns true if all values in the array are true.");
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
            using namespace AllOfBoolArrayVertexNames;
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputTrigger), Trigger);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputBoolArray), BoolArray);
        }

        void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
        {
            using namespace AllOfBoolArrayVertexNames;
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputResult), Result);
        }

        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
        {
            using namespace AllOfBoolArrayVertexNames;

            const FInputVertexInterfaceData& InputData = InParams.InputData;

            FTriggerReadRef Trigger = InputData.GetOrCreateDefaultDataReadReference<FTrigger>(
                METASOUND_GET_PARAM_NAME(InputTrigger), InParams.OperatorSettings);

            TDataReadReference<TArray<bool>> BoolArray = InputData.GetOrCreateDefaultDataReadReference<TArray<bool>>(
                METASOUND_GET_PARAM_NAME(InputBoolArray), InParams.OperatorSettings);

            return MakeUnique<FAllOfBoolArrayOperator>(InParams.OperatorSettings, Trigger, BoolArray);
        }

        void Execute()
        {
            Trigger->ExecuteBlock(
                [](int32, int32) {},
                [this](int32, int32)
                {
                    const TArray<bool>& Bools = *BoolArray;
                    bool bAllTrue = true;
                    for (bool b : Bools)
                    {
                        if (!b)
                        {
                            bAllTrue = false;
                            break;
                        }
                    }
                    *Result = bAllTrue;
                }
            );
        }

    private:
        FTriggerReadRef Trigger;
        TDataReadReference<TArray<bool>> BoolArray;
        FBoolWriteRef Result;
    };

    class FAllOfBoolArrayNode : public FNodeFacade
    {
    public:
        FAllOfBoolArrayNode(const FNodeInitData& InitData)
            : FNodeFacade(InitData.InstanceName, InitData.InstanceID, TFacadeOperatorClass<FAllOfBoolArrayOperator>())
        {
        }
    };

    METASOUND_REGISTER_NODE(FAllOfBoolArrayNode);
}

#undef LOCTEXT_NAMESPACE