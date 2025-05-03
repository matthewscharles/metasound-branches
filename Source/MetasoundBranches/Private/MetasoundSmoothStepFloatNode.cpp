// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundSmoothStepFloatNode.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundPrimitives.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"
#include "MetasoundBranches/Public/MetasoundCommonMacros.h"

#define LOCTEXT_NAMESPACE "MetasoundSmoothStepFloatNode"

namespace Metasound
{
    namespace SmoothStepFloatNodeVertexNames
    {
        METASOUND_PARAM(InputA, "In A", "Minimum edge of the interpolation range.");
        METASOUND_PARAM(InputB, "In B", "Maximum edge of the interpolation range.");
        METASOUND_PARAM(InputX, "In X", "Value to interpolate.");
        METASOUND_PARAM(OutputSignal, "Out", "Smoothed step result between 0 and 1.");
    }

    class FSmoothStepFloatOperator : public TExecutableOperator<FSmoothStepFloatOperator>
    {
    public:
        FSmoothStepFloatOperator(
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
            using namespace SmoothStepFloatNodeVertexNames;

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
                Metadata.ClassName = { TEXT("UE"), TEXT("SmoothStep"), TEXT("Float") };
                Metadata.MajorVersion = 1;
                Metadata.MinorVersion = 0;
                Metadata.DisplayName = METASOUND_LOCTEXT("SmoothStepFloatDisplayName", "SmoothStep");
                Metadata.Description = METASOUND_LOCTEXT("SmoothStepFloatDesc", "Performs smooth Hermite interpolation between 0 and 1 when input X is between A and B.");
                Metadata.Author = "Charles Matthews";
                Metadata.PromptIfMissing = PluginNodeMissingPrompt;
                Metadata.DefaultInterface = DeclareVertexInterface();
                Metadata.CategoryHierarchy = {
                    METASOUND_LOCTEXT("Custom", "Branches"),
                    METASOUND_LOCTEXT("CustomSub", "Math")
                };

                Metadata.Keywords = {
                    METASOUND_LOCTEXT("SmoothStepKeyword", "Smooth"),
                    METASOUND_LOCTEXT("SmoothStepKeyword2", "Interpolation"),
                    METASOUND_LOCTEXT("SmoothStepKeyword3", "Hermite")
                };

                return Metadata;
            };

            static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
            return Metadata;
        }

        METASOUND_DISABLE_LEGACY_IO()

        virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
        {
            using namespace SmoothStepFloatNodeVertexNames;
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputA), InputA);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputB), InputB);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputX), InputX);
        }

        virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
        {
            using namespace SmoothStepFloatNodeVertexNames;
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputSignal), OutputSignal);
        }

        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
        {
            using namespace SmoothStepFloatNodeVertexNames;

            const FInputVertexInterfaceData& InputData = InParams.InputData;

            TDataReadReference<float> A = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputA), InParams.OperatorSettings);
            TDataReadReference<float> B = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputB), InParams.OperatorSettings);
            TDataReadReference<float> X = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputX), InParams.OperatorSettings);

            return MakeUnique<FSmoothStepFloatOperator>(InParams.OperatorSettings, A, B, X);
        }

        virtual void Execute()
        {
            *OutputSignal = FMath::SmoothStep(*InputA, *InputB, *InputX);
        }

    private:
        FFloatReadRef InputA;
        FFloatReadRef InputB;
        FFloatReadRef InputX;
        FFloatWriteRef OutputSignal;
    };

    class FSmoothStepFloatNode : public FNodeFacade
    {
    public:
        FSmoothStepFloatNode(const FNodeInitData& InitData)
            : FNodeFacade(InitData.InstanceName, InitData.InstanceID, TFacadeOperatorClass<FSmoothStepFloatOperator>())
        {
        }
    };

    METASOUND_REGISTER_NODE(FSmoothStepFloatNode);
}

#undef LOCTEXT_NAMESPACE