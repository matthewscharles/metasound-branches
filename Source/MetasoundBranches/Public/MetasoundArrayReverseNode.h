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
        METASOUND_PARAM(InputTriggerForward, "Forward", "Trigger to store the current input array.")
        METASOUND_PARAM(InputTriggerReverse, "Reverse", "Trigger to reverse the stored array.")
        METASOUND_PARAM(InputArray, "Array", "Input array to store or reverse.")

        METASOUND_PARAM(OutputTriggerOnChange, "On Change", "Triggers when the output array is updated.")
        METASOUND_PARAM(OutputArray, "Array", "The stored or reversed array.")
    }

    template<typename ArrayType>
    class TArrayReverseOperator : public TExecutableOperator<TArrayReverseOperator<ArrayType>>
    {
    public:
        using FArrayDataReadReference = TDataReadReference<ArrayType>;
        using FArrayDataWriteReference = TDataWriteReference<ArrayType>;

        static const FVertexInterface& GetDefaultInterface()
        {
            using namespace ArrayReverseNodeVertexNames;
            static const FVertexInterface DefaultInterface(
                FInputVertexInterface(
                    TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTriggerForward)),
                    TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTriggerReverse)),
                    TInputDataVertex<ArrayType>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputArray))
                ),
                FOutputVertexInterface(
                    TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputTriggerOnChange)),
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
                    TEXT("HoldReverse"),  
                    METASOUND_LOCTEXT_FORMAT("ArrayOpHoldReverseArrayDisplayNamePattern", "Hold/Reverse ({0})", GetMetasoundDataTypeDisplayText<ArrayType>()), 
                    LOCTEXT("HoldReverseArrayDesc", "Stores an array, allows reversing it or updating with a new one on trigger."),  
                    GetDefaultInterface(),  
                    1,  
                    0,  
                    false 
                );
            };

            static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();

            const_cast<FNodeClassMetadata&>(Metadata).Author = TEXT("Charles Matthews");
            const_cast<FNodeClassMetadata&>(Metadata).PromptIfMissing = PluginNodeMissingPrompt;
            const_cast<FNodeClassMetadata&>(Metadata).CategoryHierarchy = { LOCTEXT("Custom", "Branches") };
            const_cast<FNodeClassMetadata&>(Metadata).Keywords = TArray<FText>();

            return Metadata;
        }

        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults)
        {
            using namespace ArrayReverseNodeVertexNames;
            const FInputVertexInterfaceData& InputData = InParams.InputData;

            TDataReadReference<FTrigger> InTriggerForward = InputData.GetOrCreateDefaultDataReadReference<FTrigger>(
                METASOUND_GET_PARAM_NAME(InputTriggerForward),
                InParams.OperatorSettings
            );

            TDataReadReference<FTrigger> InTriggerReverse = InputData.GetOrCreateDefaultDataReadReference<FTrigger>(
                METASOUND_GET_PARAM_NAME(InputTriggerReverse),
                InParams.OperatorSettings
            );

            FArrayDataReadReference InInputArray = InputData.GetOrCreateDefaultDataReadReference<ArrayType>(
                METASOUND_GET_PARAM_NAME(InputArray),
                InParams.OperatorSettings
            );

            return MakeUnique<TArrayReverseOperator>(InParams, InTriggerForward, InTriggerReverse, InInputArray);
        }

        TArrayReverseOperator(
            const FBuildOperatorParams& InParams,
            const TDataReadReference<FTrigger>& InTriggerForward,
            const TDataReadReference<FTrigger>& InTriggerReverse,
            const FArrayDataReadReference& InInputArray)
            : TriggerForward(InTriggerForward)
            , TriggerReverse(InTriggerReverse)
            , InputArray(InInputArray)
            , TriggerOnChange(FTriggerWriteRef::CreateNew(InParams.OperatorSettings))
            , OutStoredArray(TDataWriteReferenceFactory<ArrayType>::CreateAny(InParams.OperatorSettings))
        {
            // Initialize the stored array with the input array.
            *OutStoredArray = *InputArray;
        }

        virtual ~TArrayReverseOperator() = default;

        virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
        {
            using namespace ArrayReverseNodeVertexNames;
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputTriggerForward), TriggerForward);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputTriggerReverse), TriggerReverse);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputArray), InputArray);
        }

        virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
        {
            using namespace ArrayReverseNodeVertexNames;
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutputTriggerOnChange), TriggerOnChange);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutputArray), OutStoredArray);
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
            TriggerOnChange->AdvanceBlock();

            bool bChanged = false;

            if (*TriggerForward)
            {
                // Store the current input array as the output.
                *OutStoredArray = *InputArray;
                bChanged = true;
            }
            else if (*TriggerReverse)
            {
                // Reverse the currently stored array.
                Algo::Reverse(*OutStoredArray);
                bChanged = true;
            }

            if (bChanged)
            {
                TriggerOnChange->ExecuteBlock(
                    [](int32, int32) {},
                    [this](int32 StartFrame, int32) { TriggerOnChange->TriggerFrame(StartFrame); }
                );
            }
        }

    private:
        TDataReadReference<FTrigger> TriggerForward;
        TDataReadReference<FTrigger> TriggerReverse;
        FArrayDataReadReference InputArray;

        TDataWriteReference<FTrigger> TriggerOnChange;
        TDataWriteReference<ArrayType> OutStoredArray;
    };

    template<typename ArrayType>
    class TArrayReverseNode : public FNodeFacade
    {
    public:
        TArrayReverseNode(const FNodeInitData& InInitData)
            : FNodeFacade(InInitData.InstanceName, InInitData.InstanceID, TFacadeOperatorClass<TArrayReverseOperator<ArrayType>>())
        {
        }

        virtual ~TArrayReverseNode() = default;
    };
}

#undef LOCTEXT_NAMESPACE