// Copyright 2026 Charles Matthews. All Rights Reserved.

#pragma once

#include "MetasoundBranches/Public/MetasoundFoldFloatNode.h"
#include "MetasoundBranches/Public/Fold.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundPrimitives.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"
#include "Math/UnrealMathUtility.h"
#include "MetasoundBranches/Public/MetasoundCommonMacros.h"

#define LOCTEXT_NAMESPACE "MetasoundFoldFloatNode"

namespace Metasound
{
    namespace FoldFloatNodeVertexNames
    {
        METASOUND_PARAM(InputSignal, "In", "Float value to fold.");
        METASOUND_PARAM(InputHigh, "High", "Upper threshold for folding.");
        METASOUND_PARAM(InputLow, "Low", "Lower threshold for folding.");
        METASOUND_PARAM(OutputSignal, "Out", "Folded float value.");
    }

    class FFoldFloatOperator : public TExecutableOperator<FFoldFloatOperator>
    {
    public:
        FFoldFloatOperator(
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
            using namespace FoldFloatNodeVertexNames;

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
                Metadata.ClassName = { TEXT("UE"), TEXT("Fold (Float)"), TEXT("Float") };
                Metadata.MajorVersion = 1;
                Metadata.MinorVersion = 1;
                Metadata.DisplayName = METASOUND_LOCTEXT("FoldFloatDisplayName", "Fold (Float)");
                Metadata.Description = METASOUND_LOCTEXT("FoldFloatDesc", "Folds a float value within a given range.");
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
            using namespace FoldFloatNodeVertexNames;

            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputSignal), InputSignal);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputHigh), InputHigh);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputLow), InputLow);
        }
        
        virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
        {
            using namespace FoldFloatNodeVertexNames;

            InOutVertexData.BindWriteVertex(METASOUND_GET_PARAM_NAME(OutputSignal), OutputSignal);
        }
        
        
        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutErrors)
        {
            using namespace FoldFloatNodeVertexNames;

            const FInputVertexInterfaceData& InputData = InParams.InputData;

            TDataReadReference<float> InputSignal = InputData.GetOrCreateDefaultDataReadReference<float>(
                METASOUND_GET_PARAM_NAME(InputSignal), InParams.OperatorSettings);

            TDataReadReference<float> InputHigh = InputData.GetOrCreateDefaultDataReadReference<float>(
                METASOUND_GET_PARAM_NAME(InputHigh), InParams.OperatorSettings);

            TDataReadReference<float> InputLow = InputData.GetOrCreateDefaultDataReadReference<float>(
                METASOUND_GET_PARAM_NAME(InputLow), InParams.OperatorSettings);

            return MakeUnique<FFoldFloatOperator>(InParams.OperatorSettings, InputSignal, InputHigh, InputLow);
        }
        
        virtual void Execute()
        {
            *OutputSignal = PerformFold(*InputSignal, *InputLow, *InputHigh);
        }

    private:
        FFloatReadRef InputSignal;
        FFloatReadRef InputHigh;
        FFloatReadRef InputLow;
        FFloatWriteRef OutputSignal;
    };

    class FFoldFloatNode : public FNodeFacade
    {
    public:
                static FNodeClassMetadata CreateNodeClassMetadata()
        {
            return FFoldFloatOperator::GetNodeInfo();
        }
        FFoldFloatNode(FNodeData InitData)
            : FNodeFacade(InitData, MakeShared<const FNodeClassMetadata>(FFoldFloatOperator::GetNodeInfo()), TFacadeOperatorClass<FFoldFloatOperator>())
        {
        }
    };

    METASOUND_REGISTER_NODE(FFoldFloatNode);
}

#undef LOCTEXT_NAMESPACE
