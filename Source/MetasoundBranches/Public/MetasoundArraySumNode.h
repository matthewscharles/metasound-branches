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
#include "Misc/ScopeLock.h"
#include "MetasoundArrayTypeTraits.h"
#include <numeric>

#define LOCTEXT_NAMESPACE "MetasoundFrontend"

namespace Metasound
{
    namespace ArraySumNodeVertexNames
    {
        METASOUND_PARAM(InputTriggerSum, "Sum", "Trigger to sum the array.")
        METASOUND_PARAM(InputArray, "Array", "Input array to sum.")
        
        METASOUND_PARAM(OutputTriggerOnSum, "On Sum", "Triggers when the sum is output.")
        METASOUND_PARAM(OutputSum, "Sum", "The sum of the array elements.")
    }

    template<typename ArrayType>
    class TArraySumOperator : public TExecutableOperator<TArraySumOperator<ArrayType>>
    {
    public:
        using FArrayDataReadReference = TDataReadReference<ArrayType>;
        
        // Retrieve the element type from the array
        using FElementType = typename MetasoundArrayNodesPrivate::TArrayElementType<ArrayType>::Type;
        using FElementTypeWriteReference = TDataWriteReference<FElementType>;

        static const FVertexInterface& GetDefaultInterface()
        {
            using namespace ArraySumNodeVertexNames;
            static const FVertexInterface DefaultInterface(
                FInputVertexInterface(
                    TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTriggerSum)),
                    TInputDataVertex<ArrayType>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputArray))
                ),
                FOutputVertexInterface(
                    TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputTriggerOnSum)),
                    TOutputDataVertex<FElementType>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputSum))
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
                    TEXT("Sum"),  // OperatorName
                    METASOUND_LOCTEXT_FORMAT("ArrayOpSumDisplayNamePattern", "Sum ({0})", GetMetasoundDataTypeDisplayText<ArrayType>()),  // DisplayName
                    LOCTEXT("ArrayOpSumDesc", "Outputs the sum of all elements in the input array when triggered."),  // Description
                    GetDefaultInterface(), // VertexInterface
                    1,  // Major Version
                    0,  // Minor Version
                    false // IsDeprecated
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
            using namespace ArraySumNodeVertexNames;
            const FInputVertexInterfaceData& InputData = InParams.InputData;

            TDataReadReference<FTrigger> InTriggerSum = InputData.GetOrCreateDefaultDataReadReference<FTrigger>(
                METASOUND_GET_PARAM_NAME(InputTriggerSum),
                InParams.OperatorSettings
            );
            FArrayDataReadReference InInputArray = InputData.GetOrCreateDefaultDataReadReference<ArrayType>(
                METASOUND_GET_PARAM_NAME(InputArray),
                InParams.OperatorSettings
            );

            return MakeUnique<TArraySumOperator>(InParams, InTriggerSum, InInputArray);
        }

        TArraySumOperator(
            const FBuildOperatorParams& InParams,
            const TDataReadReference<FTrigger>& InTriggerSum,
            const FArrayDataReadReference& InInputArray)
            : TriggerSum(InTriggerSum)
            , InputArray(InInputArray)
            , TriggerOnSum(FTriggerWriteRef::CreateNew(InParams.OperatorSettings))
            , OutSum(TDataWriteReferenceFactory<FElementType>::CreateAny(InParams.OperatorSettings))
        {
        }

        virtual ~TArraySumOperator() = default;

        virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
        {
            using namespace ArraySumNodeVertexNames;
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputTriggerSum), TriggerSum);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputArray), InputArray);
        }

        virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
        {
            using namespace ArraySumNodeVertexNames;
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutputTriggerOnSum), TriggerOnSum);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutputSum), OutSum);
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
            TriggerOnSum->AdvanceBlock();

            const ArrayType& ArrayRef = *InputArray;

            FElementType Sum = std::accumulate(ArrayRef.begin(), ArrayRef.end(), FElementType{});

            *OutSum = Sum;

            TriggerSum->ExecuteBlock(
                [](int32, int32) {},
                [this](int32 StartFrame, int32) { TriggerOnSum->TriggerFrame(StartFrame); }
            );
        }

    private:
        TDataReadReference<FTrigger> TriggerSum;
        FArrayDataReadReference InputArray;

        TDataWriteReference<FTrigger> TriggerOnSum;
        TDataWriteReference<FElementType> OutSum;
    };

    template<typename ArrayType>
    class TArraySumNode : public FNodeFacade
    {
    public:
        TArraySumNode(const FNodeInitData& InInitData)
            : FNodeFacade(InInitData.InstanceName, InInitData.InstanceID, TFacadeOperatorClass<TArraySumOperator<ArrayType>>())
        {
        }
        virtual ~TArraySumNode() = default;
    };
}

#undef LOCTEXT_NAMESPACE