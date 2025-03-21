// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundKinkFloatNode.h"
#include "MetasoundBranches/Public/Kink.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundPrimitives.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"
#include "Math/UnrealMathUtility.h"

#define LOCTEXT_NAMESPACE "MetasoundKinkFloatNode"

namespace Metasound
{
    namespace KinkFloatNodeVertexNames
    {
        METASOUND_PARAM(InputValue,  "In",    "Float input (0-1).");
        METASOUND_PARAM(InputSlope,  "Slope", "Slope factor.");
        METASOUND_PARAM(OutputValue, "Out",   "Kinked float output.");
    }

    class FKinkFloatOperator : public TExecutableOperator<FKinkFloatOperator>
    {
    public:
        FKinkFloatOperator(
            const FOperatorSettings& InSettings,
            const FFloatReadRef& InValue,
            const FFloatReadRef& InSlope
        )
            : InputValue(InValue)
            , BaseSlope(InSlope)
            , OutputValue(FFloatWriteRef::CreateNew(KinkProcess(*InValue, *InSlope)))
        {
        }

        static const FVertexInterface& DeclareVertexInterface()
        {
            using namespace KinkFloatNodeVertexNames;

            static const FVertexInterface Interface(
                FInputVertexInterface(
                    TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputValue)),
                    TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputSlope))
                ),
                FOutputVertexInterface(
                    TOutputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputValue))
                )
            );

            return Interface;
        }

        static const FNodeClassMetadata& GetNodeInfo()
        {
            auto CreateNodeClassMetadata = []() -> FNodeClassMetadata
            {
                FNodeClassMetadata Metadata;
                Metadata.ClassName = { TEXT("UE"), TEXT("Kink (Float)"), TEXT("Float") };
                Metadata.MajorVersion = 1;
                Metadata.MinorVersion = 0;
                Metadata.DisplayName = LOCTEXT("KinkFloatNodeDisplayName", "Kink (Float)");
                Metadata.Description = LOCTEXT("KinkFloatNodeDesc", "Applies a kinked slope to the float input.");
                Metadata.Author = TEXT("Charles Matthews");
                Metadata.PromptIfMissing = PluginNodeMissingPrompt;
                Metadata.DefaultInterface = DeclareVertexInterface();
                Metadata.CategoryHierarchy = {
                    METASOUND_LOCTEXT("Custom", "Branches"),
                    METASOUND_LOCTEXT("CustomSub", "Shapers")
                };
                return Metadata;
            };

            static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
            return Metadata;
        }

        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
        {
            using namespace KinkFloatNodeVertexNames;

            const FInputVertexInterfaceData& InputData = InParams.InputData;
            TDataReadReference<float> InValue =
                InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputValue), InParams.OperatorSettings);
            TDataReadReference<float> InSlope =
                InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputSlope), InParams.OperatorSettings);

            return MakeUnique<FKinkFloatOperator>(InParams.OperatorSettings, InValue, InSlope);
        }

        virtual FDataReferenceCollection GetInputs() const override
        {
            return {};
        }

        virtual FDataReferenceCollection GetOutputs() const override
        {
            using namespace KinkFloatNodeVertexNames;
            FDataReferenceCollection Outputs;
            Outputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutputValue), OutputValue);
            return Outputs;
        }

        virtual void Execute()
        {
            *OutputValue = KinkProcess(*InputValue, *BaseSlope);
        }

    private:
        FFloatReadRef InputValue;
        FFloatReadRef BaseSlope;
        FFloatWriteRef OutputValue;
    };

    class FKinkFloatNode : public FNodeFacade
    {
    public:
        FKinkFloatNode(const FNodeInitData& InitData)
            : FNodeFacade(InitData.InstanceName, InitData.InstanceID, TFacadeOperatorClass<FKinkFloatOperator>())
        {
        }
    };

    METASOUND_REGISTER_NODE(FKinkFloatNode);
}

#undef LOCTEXT_NAMESPACE