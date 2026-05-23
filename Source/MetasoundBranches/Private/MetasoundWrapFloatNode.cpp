// Copyright 2026 Charles Matthews. All Rights Reserved.

#pragma once

#include "MetasoundBranches/Public/MetasoundWrapFloatNode.h"
#include "MetasoundBranches/Public/Wrap.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundPrimitives.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"
#include "Math/UnrealMathUtility.h"
#include "MetasoundBranches/Public/MetasoundCommonMacros.h"

#define LOCTEXT_NAMESPACE "MetasoundWrapFloatNode"

namespace Metasound
{
    namespace WrapFloatNodeVertexNames
    {
        METASOUND_PARAM(InputSignal, "In", "Float value to wrap.");
        METASOUND_PARAM(InputHigh, "High", "Upper threshold for wrapping.");
        METASOUND_PARAM(InputLow, "Low", "Lower threshold for wrapping.");
        METASOUND_PARAM(OutputSignal, "Out", "Wrapped float value.");
    }

    class FWrapFloatOperator : public TExecutableOperator<FWrapFloatOperator>
    {
    public:
        FWrapFloatOperator(
            const FOperatorSettings& InSettings,
            const FFloatReadRef& InSignal,
            const FFloatReadRef& InHigh,
            const FFloatReadRef& InLow)
            : InputSignal(InSignal)
            , InputHigh(InHigh)
            , InputLow(InLow)
            , OutputSignal(FFloatWriteRef::CreateNew(*InSignal))
        {
        }

        static const FVertexInterface& DeclareVertexInterface()
        {
            using namespace WrapFloatNodeVertexNames;

            static const FVertexInterface Interface(
                FInputVertexInterface(
                    TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputSignal)),
                    TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputHigh)),
                    TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputLow))
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
                Metadata.ClassName = { TEXT("UE"), TEXT("Wrap (Float)"), TEXT("Float") };
                Metadata.MajorVersion = 1;
                Metadata.MinorVersion = 1;
                Metadata.DisplayName = METASOUND_LOCTEXT("WrapFloatDisplayName", "Wrap (Float)");
                Metadata.Description = METASOUND_LOCTEXT("WrapFloatDesc", "Wraps a float value within a given range.");
                Metadata.Author = "Charles Matthews";
                Metadata.PromptIfMissing = PluginNodeMissingPrompt;
                Metadata.DefaultInterface = DeclareVertexInterface();
                Metadata.CategoryHierarchy = {
                    METASOUND_LOCTEXT("Custom", "Branches"),
                    METASOUND_LOCTEXT("CustomSub", "Shapers")
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
            using namespace WrapFloatNodeVertexNames;
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputSignal), InputSignal);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputHigh), InputHigh);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputLow), InputLow);
        }
        
        virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
        {
            using namespace WrapFloatNodeVertexNames;
            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputSignal), OutputSignal);
        }

        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
        {
            using namespace WrapFloatNodeVertexNames;

            const FInputVertexInterfaceData& InputData = InParams.InputData;

            TDataReadReference<float> InputSignal = InputData.GetOrCreateDefaultDataReadReference<float>(
                METASOUND_GET_PARAM_NAME(InputSignal), InParams.OperatorSettings);

            TDataReadReference<float> InputHigh = InputData.GetOrCreateDefaultDataReadReference<float>(
                METASOUND_GET_PARAM_NAME(InputHigh), InParams.OperatorSettings);

            TDataReadReference<float> InputLow = InputData.GetOrCreateDefaultDataReadReference<float>(
                METASOUND_GET_PARAM_NAME(InputLow), InParams.OperatorSettings);

            return MakeUnique<FWrapFloatOperator>(InParams.OperatorSettings, InputSignal, InputHigh, InputLow);
        }
        
        virtual void Execute()
        {
            *OutputSignal = PerformWrap(*InputSignal, *InputLow, *InputHigh);
        }

    private:
        FFloatReadRef InputSignal;
        FFloatReadRef InputHigh;
        FFloatReadRef InputLow;
        FFloatWriteRef OutputSignal;
    };

    class FWrapFloatNode : public FNodeFacade
    {
    public:
                static FNodeClassMetadata CreateNodeClassMetadata()
        {
            return FWrapFloatOperator::GetNodeInfo();
        }
        FWrapFloatNode(FNodeData InitData)
            : FNodeFacade(InitData, MakeShared<const FNodeClassMetadata>(FWrapFloatOperator::GetNodeInfo()), TFacadeOperatorClass<FWrapFloatOperator>())
        {
        }
    };

    METASOUND_REGISTER_NODE(FWrapFloatNode);
}

#undef LOCTEXT_NAMESPACE
