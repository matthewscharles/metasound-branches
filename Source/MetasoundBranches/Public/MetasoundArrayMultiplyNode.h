// Copyright 2026 Charles Matthews. All Rights Reserved.

#pragma once

#include "Interfaces/MetasoundFrontendSourceInterface.h"
#include "Internationalization/Text.h"
#include "MetasoundArrayNodes.h"
#include "MetasoundBuilderInterface.h"
#include "MetasoundDataFactory.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundLog.h"
#include "MetasoundNodeInterface.h"
#include "MetasoundOperatorInterface.h"
#include "MetasoundParamHelper.h"
#include "MetasoundPrimitives.h"
#include "MetasoundTime.h"
#include "MetasoundTrigger.h"
#include "MetasoundVertex.h"
#include "Misc/ScopeLock.h"
#include "MetasoundArrayTypeTraits.h"
#include "MetasoundBranches/Public/MetasoundCommonMacros.h"

#define LOCTEXT_NAMESPACE "MetasoundFrontend"

namespace Metasound
{
    namespace ArrayMultiplyNodePrivate
    {
        template<typename ElementType>
        struct TMultiplierType
        {
            using Type = ElementType;
        };

        template<>
        struct TMultiplierType<FTime>
        {
            using Type = float;
        };
    }

    namespace ArrayMultiplyNodeVertexNames
    {
        METASOUND_PARAM(InputTriggerMultiply, "Multiply", "Trigger to multiply each element in the array.")
        METASOUND_PARAM(InputArray, "Array", "Input array to multiply.")
        METASOUND_PARAM(InputMultiplier, "Multiplier", "Value used to multiply each element in the array.")

        METASOUND_PARAM(OutputTriggerOnMultiply, "On Multiply", "Triggers when the multiplied array is output.")
        METASOUND_PARAM(OutputArray, "Array", "The multiplied array.")
    }

    template<typename ArrayType>
    class TArrayMultiplyOperator : public TExecutableOperator<TArrayMultiplyOperator<ArrayType>>
    {
    public:
        using FArrayDataReadReference = TDataReadReference<ArrayType>;

        using FElementType = typename MetasoundArrayNodesPrivate::TArrayElementType<ArrayType>::Type;
        using FMultiplierType = typename ArrayMultiplyNodePrivate::TMultiplierType<FElementType>::Type;
        using FMultiplierDataReadReference = TDataReadReference<FMultiplierType>;
        using FArrayDataWriteReference = TDataWriteReference<ArrayType>;

        static const FVertexInterface& GetDefaultInterface()
        {
            using namespace ArrayMultiplyNodeVertexNames;
            static const FVertexInterface DefaultInterface(
                FInputVertexInterface(
                    TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTriggerMultiply)),
                    TInputDataVertex<ArrayType>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputArray)),
                    TInputDataVertex<FMultiplierType>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputMultiplier))
                ),
                FOutputVertexInterface(
                    TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputTriggerOnMultiply)),
                    TOutputDataVertex<ArrayType>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputArray))
                )
            );
            return DefaultInterface;
        }

        static const FNodeClassMetadata& GetNodeInfo()
        {
            auto CreateNodeClassMetadata = []() -> FNodeClassMetadata
            {
                return MetasoundArrayNodesPrivate::CreateArrayNodeClassMetadata(
                    GetMetasoundDataTypeName<ArrayType>(),
                    TEXT("Multiply"),
                    METASOUND_LOCTEXT_FORMAT("ArrayOpMultiplyDisplayNamePattern", "Multiply ({0})", GetMetasoundDataTypeDisplayText<ArrayType>()),
                    LOCTEXT("ArrayOpMultiplyDesc", "Outputs the input array with each element multiplied by the multiplier when triggered."),
                    GetDefaultInterface(),
                    1,
                    0,
                    false
                );
            };

            static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
            const_cast<FNodeClassMetadata&>(Metadata).Author = TEXT("Charles Matthews");
            const_cast<FNodeClassMetadata&>(Metadata).PromptIfMissing = PluginNodeMissingPrompt;
            const_cast<FNodeClassMetadata&>(Metadata).CategoryHierarchy = {
                METASOUND_LOCTEXT("Custom", "Branches"),
                METASOUND_LOCTEXT("CustomSub", "Array")
            };
            const_cast<FNodeClassMetadata&>(Metadata).Keywords = TArray<FText>();
            return Metadata;
        }

        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults)
        {
            using namespace ArrayMultiplyNodeVertexNames;
            const FInputVertexInterfaceData& InputData = InParams.InputData;

            TDataReadReference<FTrigger> InTriggerMultiply = InputData.GetOrCreateDefaultDataReadReference<FTrigger>(
                METASOUND_GET_PARAM_NAME(InputTriggerMultiply),
                InParams.OperatorSettings
            );
            FArrayDataReadReference InInputArray = InputData.GetOrCreateDefaultDataReadReference<ArrayType>(
                METASOUND_GET_PARAM_NAME(InputArray),
                InParams.OperatorSettings
            );
            FMultiplierDataReadReference InMultiplier = InputData.GetOrCreateDefaultDataReadReference<FMultiplierType>(
                METASOUND_GET_PARAM_NAME(InputMultiplier),
                InParams.OperatorSettings
            );

            return MakeUnique<TArrayMultiplyOperator>(InParams, InTriggerMultiply, InInputArray, InMultiplier);
        }

        TArrayMultiplyOperator(
            const FBuildOperatorParams& InParams,
            const TDataReadReference<FTrigger>& InTriggerMultiply,
            const FArrayDataReadReference& InInputArray,
            const FMultiplierDataReadReference& InMultiplier)
            : TriggerMultiply(InTriggerMultiply)
            , InputArray(InInputArray)
            , Multiplier(InMultiplier)
            , TriggerOnMultiply(FTriggerWriteRef::CreateNew(InParams.OperatorSettings))
            , OutArray(TDataWriteReferenceFactory<ArrayType>::CreateAny(InParams.OperatorSettings))
        {
            *OutArray = *InputArray;
        }

        virtual ~TArrayMultiplyOperator() = default;

        virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
        {
            using namespace ArrayMultiplyNodeVertexNames;
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputTriggerMultiply), TriggerMultiply);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputArray), InputArray);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputMultiplier), Multiplier);
        }

        virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
        {
            using namespace ArrayMultiplyNodeVertexNames;
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutputTriggerOnMultiply), TriggerOnMultiply);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutputArray), OutArray);
        }

        METASOUND_DISABLE_LEGACY_IO()

        void Execute()
        {
            TriggerOnMultiply->AdvanceBlock();

            TriggerMultiply->ExecuteBlock(
                [](int32, int32) {},
                [this](int32 StartFrame, int32)
                {
                    *OutArray = *InputArray;
                    for (FElementType& Element : *OutArray)
                    {
                        Element = Element * (*Multiplier);
                    }
                    TriggerOnMultiply->TriggerFrame(StartFrame);
                }
            );
        }

    private:
        TDataReadReference<FTrigger> TriggerMultiply;
        FArrayDataReadReference InputArray;
        FMultiplierDataReadReference Multiplier;

        TDataWriteReference<FTrigger> TriggerOnMultiply;
        FArrayDataWriteReference OutArray;
    };

    template<typename ArrayType>
    class TArrayMultiplyNode : public FNodeFacade
    {
    public:
        static FNodeClassMetadata CreateNodeClassMetadata()
        {
            return TArrayMultiplyOperator<ArrayType>::GetNodeInfo();
        }

        TArrayMultiplyNode(FNodeData InInitData)
            : FNodeFacade(InInitData, MakeShared<const FNodeClassMetadata>(TArrayMultiplyOperator<ArrayType>::GetNodeInfo()), TFacadeOperatorClass<TArrayMultiplyOperator<ArrayType>>())
        {
        }

        virtual ~TArrayMultiplyNode() = default;
    };
}

#undef LOCTEXT_NAMESPACE
