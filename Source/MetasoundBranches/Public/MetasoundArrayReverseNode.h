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
#include "Algo/Reverse.h"

#define LOCTEXT_NAMESPACE "MetasoundFrontend"

namespace Metasound
{
    namespace ArrayReverseNodeVertexNames
    {
        METASOUND_PARAM(InputArray,  "Array In",  "The array to reverse each block.")
        METASOUND_PARAM(InputWaitForChange, "Wait For Change", "Hold output if reversed array equals last output.")
        METASOUND_PARAM(OutputArray, "Array Out", "The reversed array.")
    }

    template<typename ArrayType>
    class TArrayReverseOperator : public TExecutableOperator<TArrayReverseOperator<ArrayType>>
    {
    public:
        using FArrayReadRef  = TDataReadReference<ArrayType>;
        using FArrayWriteRef = TDataWriteReference<ArrayType>;
        using FBoolReadRef   = TDataReadReference<bool>;

        static const FVertexInterface& GetDefaultInterface()
        {
            using namespace ArrayReverseNodeVertexNames;
            static const FVertexInterface DefaultInterface(
                FInputVertexInterface(
                    TInputDataVertex<ArrayType>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputArray)),
                    TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputWaitForChange))
                ),
                FOutputVertexInterface(
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
                    LOCTEXT("ArrayReverseNodeDesc", "Reverses the input array each block."),
                    GetDefaultInterface(),
                    1,
                    0,
                    false
                );
                Metadata.Author = TEXT("Charles Matthews");
                Metadata.PromptIfMissing = PluginNodeMissingPrompt;
                Metadata.CategoryHierarchy = { LOCTEXT("Category", "Branches") };
                return Metadata;
            };
            static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
            return Metadata;
        }

        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults)
        {
            using namespace ArrayReverseNodeVertexNames;
            const FInputVertexInterfaceData& InputData = InParams.InputData;
            FArrayReadRef InInputArray = InputData.GetOrCreateDefaultDataReadReference<ArrayType>(
                METASOUND_GET_PARAM_NAME(InputArray),
                InParams.OperatorSettings
            );
            FBoolReadRef InWaitForChange = InputData.GetOrCreateDefaultDataReadReference<bool>(
                METASOUND_GET_PARAM_NAME(InputWaitForChange),
                InParams.OperatorSettings
            );
            return MakeUnique<TArrayReverseOperator>(InParams, InInputArray, InWaitForChange);
        }

        TArrayReverseOperator(const FBuildOperatorParams& InParams, const FArrayReadRef& InInputArray, const FBoolReadRef& InWaitForChange)
            : InputArray(InInputArray)
            , WaitForChange(InWaitForChange)
            , OutArray(TDataWriteReferenceFactory<ArrayType>::CreateAny(InParams.OperatorSettings))
        {
            *OutArray = *InputArray;
            Algo::Reverse(*OutArray);
            // UE_LOG(LogTemp, Warning, TEXT("Reverse Operator constructed. Initial reversed array: %s"), *ArrayToString(*OutArray));
        }

        virtual ~TArrayReverseOperator() = default;

        virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
        {
            using namespace ArrayReverseNodeVertexNames;
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputArray), InputArray);
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputWaitForChange), WaitForChange);
        }

        virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
        {
            using namespace ArrayReverseNodeVertexNames;
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

        template <typename T>
        FString ArrayToString(const TArray<T>& InArray)
        {
            FString ArrayString;
            for (const T& Value : InArray)
            {
                ArrayString += LexToString(Value) + TEXT(" ");
            }
            return ArrayString.TrimEnd();
        }

        void Execute()
        {
            ArrayType NewReversed = *InputArray;
            Algo::Reverse(NewReversed);
            if (*WaitForChange)
            {
                if (NewReversed != *OutArray)
                {
                    *OutArray = NewReversed;
                    // UE_LOG(LogTemp, Warning, TEXT("Reverse: Updated output to: %s"), *ArrayToString(*OutArray));
                }
                else
                {
                    // UE_LOG(LogTemp, Warning, TEXT("Reverse: Holding output, no change detected."));
                }
            }
            else
            {
                *OutArray = NewReversed;
                // UE_LOG(LogTemp, Warning, TEXT("Reverse: Updated output to: %s"), *ArrayToString(*OutArray));
            }
        }

    private:
        FArrayReadRef InputArray;
        FBoolReadRef WaitForChange;
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