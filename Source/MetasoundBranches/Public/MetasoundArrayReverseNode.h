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
#include "MetasoundTime.h"

#define LOCTEXT_NAMESPACE "MetasoundFrontend"

namespace Metasound
{
    namespace MetasoundArrayNodesPrivate
    {
        template<>
        struct TArrayElementType<TArray<FTime>>
        {
            using Type = FTime;
        };
    }
	namespace ArrayReverseNodeVertexNames
	{
		METASOUND_PARAM(InputTriggerReverse, "Reverse", "Trigger to reverse the array.")
		METASOUND_PARAM(InputArray, "Array", "Input array to reverse.")

		METASOUND_PARAM(OutputTriggerOnReverse, "On Reverse", "Triggers when the reversed array is output.")
		METASOUND_PARAM(OutputReversedArray, "Array", "The reversed array.")
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
					TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTriggerReverse)),
					TInputDataVertex<ArrayType>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputArray))
				),
				FOutputVertexInterface(
					TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputTriggerOnReverse)),
					TOutputDataVertex<ArrayType>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputReversedArray))
				)
			);
			return DefaultInterface;
		}

        static const FNodeClassMetadata& GetNodeInfo()
        {
            auto CreateNodeClassMetadata = []() -> FNodeClassMetadata
            {
                return MetasoundArrayNodesPrivate::CreateArrayNodeClassMetadata(
                    GetMetasoundDataTypeName<ArrayType>(),  // DataTypeName
                    TEXT("Reverse"),  // OperatorName
                    METASOUND_LOCTEXT_FORMAT("ArrayOpReverseArrayDisplayNamePattern", "Reverse ({0})", GetMetasoundDataTypeDisplayText<ArrayType>()), // DisplayName
                    LOCTEXT("ReverseArrayDesc", "Outputs a reversed copy of the input array when triggered."), // Description
                    GetDefaultInterface(), // VertexInterface
                    1,  // Major Version
                    0,  // Minor Version
                    false // IsDeprecated
                );
            };
        
            static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
        
            // Ensure the additional properties are set explicitly
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

			TDataReadReference<FTrigger> InTriggerReverse = InputData.GetOrCreateDefaultDataReadReference<FTrigger>(METASOUND_GET_PARAM_NAME(InputTriggerReverse), InParams.OperatorSettings);
			FArrayDataReadReference InInputArray = InputData.GetOrCreateDefaultDataReadReference<ArrayType>(METASOUND_GET_PARAM_NAME(InputArray), InParams.OperatorSettings);

			return MakeUnique<TArrayReverseOperator>(InParams, InTriggerReverse, InInputArray);
		}

		TArrayReverseOperator(
			const FBuildOperatorParams& InParams,
			const TDataReadReference<FTrigger>& InTriggerReverse,
			const FArrayDataReadReference& InInputArray)
			: TriggerReverse(InTriggerReverse)
			, InputArray(InInputArray)
			, TriggerOnReverse(FTriggerWriteRef::CreateNew(InParams.OperatorSettings))
			, OutReversedArray(TDataWriteReferenceFactory<ArrayType>::CreateAny(InParams.OperatorSettings))
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
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutputReversedArray), OutReversedArray);
		}

		virtual FDataReferenceCollection GetInputs() const override
		{
			// This should never be called. Bind(...) is called instead. This method
			// exists as a stop-gap until the API can be deprecated and removed.
			checkNoEntry();
			return {};
		}

		virtual FDataReferenceCollection GetOutputs() const override
		{
			// This should never be called. Bind(...) is called instead. This method
			// exists as a stop-gap until the API can be deprecated and removed.
			checkNoEntry();
			return {};
		}

		void Execute()
		{
			TriggerOnReverse->AdvanceBlock();

			const ArrayType& InputArrayRef = *InputArray;
			ArrayType& ReversedArrayRef = *OutReversedArray;

			ReversedArrayRef.Reset();

			const int32 ArraySize = InputArrayRef.Num();
			if (ArraySize > 0)
			{
				ReversedArrayRef.Reserve(ArraySize);
				for (int32 i = ArraySize - 1; i >= 0; --i)
				{
					ReversedArrayRef.Add(InputArrayRef[i]);
				}
			}

			TriggerReverse->ExecuteBlock(
				[](int32, int32) {},
				[this](int32 StartFrame, int32)
				{
					TriggerOnReverse->TriggerFrame(StartFrame);
				}
			);
		}

	private:
		// Input trigger and array.
		TDataReadReference<FTrigger> TriggerReverse;
		FArrayDataReadReference InputArray;

		// Output trigger and reversed array.
		TDataWriteReference<FTrigger> TriggerOnReverse;
		TDataWriteReference<ArrayType> OutReversedArray;
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