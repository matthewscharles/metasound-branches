// Copyright 2025 Charles Matthews. All Rights Reserved.

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
#include "MetasoundTrigger.h"
#include "MetasoundVertex.h"
#include "MetasoundArrayTypeTraits.h"
#include "MetasoundTime.h"

#define LOCTEXT_NAMESPACE "MetasoundFrontend"

namespace Metasound
{
    namespace ArrayReverseNodeVertexNames
    {
        METASOUND_PARAM(InputTriggerReverse,   "Reverse",       "Trigger to reverse the array.")
        METASOUND_PARAM(InputArray,            "Array",         "Array to reverse.")
        METASOUND_PARAM(OutputTriggerOnReverse,"On Reverse",    "Triggers when the array is reversed.")
        METASOUND_PARAM(OutputArray,           "Array",         "The reversed array.")
    }

    template<typename ArrayType>
    class TArrayReverseOperator : public TExecutableOperator<TArrayReverseOperator<ArrayType>>
    {
    public:
        using FArrayReadRef  = TDataReadReference<ArrayType>;
        using FArrayWriteRef = TDataWriteReference<ArrayType>;
        using FTriggerReadRef  = TDataReadReference<FTrigger>;
        using FTriggerWriteRef = TDataWriteReference<FTrigger>;

        static const FVertexInterface& GetDefaultInterface()
        {
            using namespace ArrayReverseNodeVertexNames;
            static const FVertexInterface DefaultInterface(
                FInputVertexInterface(
                    TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTriggerReverse)),
                    TInputDataVertex<ArrayType>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputArray))
                ),
                FOutputVertexInterface(
                    TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputTriggerOnReverse)),
                    TOutputDataVertex<ArrayType>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputArray))
                )
            );
            return DefaultInterface;
        }

        static const FNodeClassMetadata& GetNodeInfo()
        {
            auto CreateNodeClassMetadata = []() -> FNodeClassMetadata
            {
                FNodeClassMetadata Metadata = MetasoundArrayNodesPrivate::CreateArrayNodeClassMetadata(
                    GetMetasoundDataTypeName<ArrayType>(),
                    TEXT("Reverse"),
                    METASOUND_LOCTEXT_FORMAT("ArrayReverseNodeName", "Reverse ({0})", GetMetasoundDataTypeDisplayText<ArrayType>()),
                    LOCTEXT("ArrayReverseNodeDesc", "Reverses the input array on trigger."),
                    GetDefaultInterface(),
                    1,
                    0,
                    false
                );
                const_cast<FNodeClassMetadata&>(Metadata).Author = TEXT("Charles Matthews");
                const_cast<FNodeClassMetadata&>(Metadata).PromptIfMissing = PluginNodeMissingPrompt;
                const_cast<FNodeClassMetadata&>(Metadata).CategoryHierarchy = { LOCTEXT("Category", "Branches") };
                return Metadata;
            };
            static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
            return Metadata;
        }

        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults)
        {
            using namespace ArrayReverseNodeVertexNames;
            const FInputVertexInterfaceData& InputData = InParams.InputData;

            TDataReadReference<FTrigger> InTriggerReverse = InputData.GetOrCreateDefaultDataReadReference<FTrigger>(
                METASOUND_GET_PARAM_NAME(InputTriggerReverse),
                InParams.OperatorSettings
            );

            FArrayReadRef InInputArray = InputData.GetOrCreateDefaultDataReadReference<ArrayType>(
                METASOUND_GET_PARAM_NAME(InputArray),
                InParams.OperatorSettings
            );

            return MakeUnique<TArrayReverseOperator>(InParams, InTriggerReverse, InInputArray);
        }

        TArrayReverseOperator(
            const FBuildOperatorParams& InParams,
            const FTriggerReadRef& InTriggerReverse,
            const FArrayReadRef& InInputArray
        )
            : TriggerReverse(InTriggerReverse)
            , InputArray(InInputArray)
            , TriggerOnReverse(FTriggerWriteRef::CreateNew(InParams.OperatorSettings))
            , OutArray(TDataWriteReferenceFactory<ArrayType>::CreateAny(InParams.OperatorSettings))
        {
        }

        virtual ~TArrayReverseOperator() = default;

        virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
        {
            using namespace ArrayReverseNodeVertexNames;
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputTriggerReverse), TriggerReverse);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputArray), InputArray);
        }

        virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
        {
            using namespace ArrayReverseNodeVertexNames;
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutputTriggerOnReverse), TriggerOnReverse);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutputArray), OutArray);
        }

        virtual FDataReferenceCollection GetInputs() const override
        {
            checkNoEntry();
            return {};
        }

        virtual FDataReferenceCollection GetOutputs() const override
        {
            checkNoEntry();
            return {};
        }

        void Execute()
        {
            TriggerOnReverse->AdvanceBlock();
            
        
            TriggerReverse->ExecuteBlock(
                [](int32, int32) {}, // No-op before triggers
                [this](int32 TriggerFrame, int32) // Per-trigger event
                {
                    *OutArray = *InputArray;
                    Algo::Reverse(*OutArray);        
                    TriggerOnReverse->TriggerFrame(TriggerFrame); 
                    UE_LOG(LogTemp, Warning, TEXT("MetaSound: Reversed Array at Frame %d"), TriggerFrame);
                }
            );
        }

    private:
        TDataReadReference<FTrigger> TriggerReverse;
        FArrayReadRef InputArray;
        FTriggerWriteRef TriggerOnReverse;
        FArrayWriteRef OutArray;
    };

    template<typename ArrayType>
    class TArrayReverseNode : public FNodeFacade
    {
    public:
        TArrayReverseNode(const FNodeInitData& InInitData)
            : FNodeFacade(
                InInitData.InstanceName,
                InInitData.InstanceID,
                TFacadeOperatorClass<TArrayReverseOperator<ArrayType>>()
            )
        {
        }

        virtual ~TArrayReverseNode() = default;
    };
}

#undef LOCTEXT_NAMESPACE