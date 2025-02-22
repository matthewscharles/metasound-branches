// Copyright 2025 Charles Matthews. All Rights Reserved.

#pragma once

#include "Interfaces/MetasoundFrontendSourceInterface.h"
#include "Internationalization/Text.h"
#include "MetasoundArrayNodes.h"
#include "MetasoundBuilderInterface.h"
#include "MetasoundFacade.h"
#include "MetasoundNodeInterface.h"
#include "MetasoundOperatorInterface.h"
#include "MetasoundParamHelper.h"
#include "MetasoundPrimitives.h"
#include "MetasoundTrigger.h"
#include "MetasoundVertex.h"
#include "MetasoundArrayTypeTraits.h"
#include <sstream>

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_ArrayJoin"

namespace Metasound
{
	namespace ArrayJoinNodeVertexNames
	{
		METASOUND_PARAM(InputTriggerJoin, "Trigger", "Trigger to join the array elements.")
		METASOUND_PARAM(InputArray, "Array", "Input array to join.")
		METASOUND_PARAM(InputDelimiter, "Delimiter", "Delimiter string to insert between array elements.")

		METASOUND_PARAM(OutputTriggerOnJoin, "On Trigger", "Triggers when the converted string is output.")
		METASOUND_PARAM(OutputJoinedString, "String", "The joined string.")
	}

	template<typename ElementType>
	class TArrayJoinOperator : public TExecutableOperator<TArrayJoinOperator<ElementType>>
	{
	public:
		using FArrayType = TArray<ElementType>;

		using FArrayDataReadReference = TDataReadReference<FArrayType>;
		using FDelimiterReadReference = TDataReadReference<FString>;
		using FOutputStringWriteReference = TDataWriteReference<FString>;

		static const FVertexInterface& GetDefaultInterface()
		{
			using namespace ArrayJoinNodeVertexNames;
			static const FVertexInterface DefaultInterface(
				FInputVertexInterface(
					TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTriggerJoin)),
					TInputDataVertex<FArrayType>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputArray)),
					TInputDataVertex<FString>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputDelimiter))
				),
				FOutputVertexInterface(
					TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputTriggerOnJoin)),
					TOutputDataVertex<FString>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputJoinedString))
				)
			);
			return DefaultInterface;
		}

		static const FNodeClassMetadata& GetNodeInfo()
		{
			auto CreateNodeClassMetadata = []() -> FNodeClassMetadata
			{
				// Use the array type's name as the third element to differentiate specializations.
				FName DataTypeName = GetMetasoundDataTypeName<FArrayType>();
				FName OperatorName = TEXT("Array To String");
				FText NodeDisplayName = METASOUND_LOCTEXT_FORMAT("ArrayOpJoinDisplayNamePattern", "Array ({0}) To String", GetMetasoundDataTypeDisplayText<FArrayType>());
				const FText NodeDescription = LOCTEXT("ArrayOpJoinDesc", "Joins the elements of an array into a single string using a specified delimiter.");
				FVertexInterface NodeInterface = GetDefaultInterface();

				return MetasoundArrayNodesPrivate::CreateArrayNodeClassMetadata(
					DataTypeName,
					OperatorName,
					NodeDisplayName,
					NodeDescription,
					NodeInterface,
					1, 0, false
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
			using namespace ArrayJoinNodeVertexNames;
			const FInputVertexInterfaceData& InputData = InParams.InputData;

			TDataReadReference<FTrigger> InTriggerJoin = InputData.GetOrCreateDefaultDataReadReference<FTrigger>(
				METASOUND_GET_PARAM_NAME(InputTriggerJoin),
				InParams.OperatorSettings
			);

			FArrayDataReadReference InInputArray = InputData.GetOrCreateDefaultDataReadReference<FArrayType>(
				METASOUND_GET_PARAM_NAME(InputArray),
				InParams.OperatorSettings
			);

			TDataReadReference<FString> InDelimiter = InputData.GetOrCreateDefaultDataReadReference<FString>(
				METASOUND_GET_PARAM_NAME(InputDelimiter),
				InParams.OperatorSettings
			);

			return MakeUnique<TArrayJoinOperator>(InParams, InTriggerJoin, InInputArray, InDelimiter);
		}

		TArrayJoinOperator(
			const FBuildOperatorParams& InParams,
			TDataReadReference<FTrigger> InTriggerJoin,
			FArrayDataReadReference InInputArray,
			TDataReadReference<FString> InDelimiter)
			: TriggerJoin(InTriggerJoin)
			, InputArray(InInputArray)
			, Delimiter(InDelimiter)
			, TriggerOnJoin(FTriggerWriteRef::CreateNew(InParams.OperatorSettings))
			, OutJoinedString(TDataWriteReferenceFactory<FString>::CreateAny(InParams.OperatorSettings))
		{
		}

		virtual ~TArrayJoinOperator() = default;

		virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
		{
			using namespace ArrayJoinNodeVertexNames;
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputTriggerJoin), TriggerJoin);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputArray), InputArray);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputDelimiter), Delimiter);
		}

		virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
		{
			using namespace ArrayJoinNodeVertexNames;
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutputTriggerOnJoin), TriggerOnJoin);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutputJoinedString), OutJoinedString);
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
            TriggerOnJoin->AdvanceBlock();

            if (*TriggerJoin)
            {
                if (InputArray->Num() == 0)
                {
                    *OutJoinedString = "";
                    return;
                }

                FString JoinedStr;
                const int32 Count = InputArray->Num();
                for (int32 i = 0; i < Count; ++i)
                {
                    JoinedStr += LexToString((*InputArray)[i]);
                    if (i < Count - 1)
                    {
                        JoinedStr += *Delimiter;
                    }
                }

                *OutJoinedString = JoinedStr;

                TriggerJoin->ExecuteBlock(
                    [](int32, int32) {},
                    [this](int32 StartFrame, int32) { TriggerOnJoin->TriggerFrame(StartFrame); }
                );
            }
        }

	private:
		TDataReadReference<FTrigger> TriggerJoin;
		FArrayDataReadReference InputArray;
		TDataReadReference<FString> Delimiter;

		TDataWriteReference<FTrigger> TriggerOnJoin;
		TDataWriteReference<FString> OutJoinedString;
	};

    template<typename ElementType>
    class TArrayJoinNode : public FNodeFacade
    {
    public:
        TArrayJoinNode(const FNodeInitData& InInitData)
            : FNodeFacade(InInitData.InstanceName, InInitData.InstanceID, TFacadeOperatorClass<TArrayJoinOperator<ElementType>>())
        {
        }
    
        virtual ~TArrayJoinNode() = default;
    };
	
}

#undef LOCTEXT_NAMESPACE