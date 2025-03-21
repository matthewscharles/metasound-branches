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

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_ArrayMake"

namespace Metasound
{
	namespace ArrayMakeNodeVertexNames
	{
		METASOUND_PARAM(InputTriggerMake, "Make", "Trigger to make the array.")
		METASOUND_PARAM(InputValue0, "0", "Element 0.")
		METASOUND_PARAM(InputValue1, "1", "Element 1.")
		METASOUND_PARAM(InputValue2, "2", "Element 2.")
		METASOUND_PARAM(InputValue3, "3", "Element 3.")
		METASOUND_PARAM(InputValue4, "4", "Element 4.")
		METASOUND_PARAM(InputValue5, "5", "Element 5.")
		METASOUND_PARAM(InputValue6, "6", "Element 6.")
		METASOUND_PARAM(InputValue7, "7", "Element 7.")
		METASOUND_PARAM(OutputTriggerOnMake, "On Make", "Triggers when array is made.")
		METASOUND_PARAM(OutputArray, "Array", "The output array.")
	}

	template<typename ElementType>
	class TArrayMakeOperator : public TExecutableOperator<TArrayMakeOperator<ElementType>>
	{
		using FArrayType = TArray<ElementType>;
		using FArrayDataWriteReference = TDataWriteReference<FArrayType>;

	public:
		static const FVertexInterface& GetDefaultInterface()
		{
			using namespace ArrayMakeNodeVertexNames;
			static const FVertexInterface DefaultInterface(
				FInputVertexInterface(
					TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTriggerMake)),
					TInputDataVertex<ElementType>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputValue0)),
					TInputDataVertex<ElementType>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputValue1)),
					TInputDataVertex<ElementType>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputValue2)),
					TInputDataVertex<ElementType>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputValue3)),
					TInputDataVertex<ElementType>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputValue4)),
					TInputDataVertex<ElementType>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputValue5)),
					TInputDataVertex<ElementType>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputValue6)),
					TInputDataVertex<ElementType>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputValue7))
				),
				FOutputVertexInterface(
					TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputTriggerOnMake)),
					TOutputDataVertex<FArrayType>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputArray))
				)
			);
			return DefaultInterface;
		}

		static const FNodeClassMetadata& GetNodeInfo()
		{
			auto CreateNodeClassMetadata = []() -> FNodeClassMetadata
			{
				FName DataTypeName = GetMetasoundDataTypeName<FArrayType>();
				FName OperatorName = TEXT("Make Array");
				FText NodeDisplayName = METASOUND_LOCTEXT_FORMAT("ArrayOpMakeDisplayNamePattern", "Make Array ({0})", GetMetasoundDataTypeDisplayText<FArrayType>());
				const FText NodeDescription = LOCTEXT("ArrayOpMakeDesc", "Creates an array from the given inputs.");
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
			const_cast<FNodeClassMetadata&>(Metadata).CategoryHierarchy = {
				METASOUND_LOCTEXT("Custom", "Branches"),
				METASOUND_LOCTEXT("CustomSub", "Array")
			};
			const_cast<FNodeClassMetadata&>(Metadata).Keywords = TArray<FText>();
			return Metadata;
		}

		static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults)
		{
			using namespace ArrayMakeNodeVertexNames;
			const FInputVertexInterfaceData& InputData = InParams.InputData;

			TDataReadReference<FTrigger> InTriggerMake = InputData.GetOrCreateDefaultDataReadReference<FTrigger>(
				METASOUND_GET_PARAM_NAME(InputTriggerMake),
				InParams.OperatorSettings
			);

			auto InValue0 = InputData.GetOrCreateDefaultDataReadReference<ElementType>(
				METASOUND_GET_PARAM_NAME(InputValue0),
				InParams.OperatorSettings
			);

			auto InValue1 = InputData.GetOrCreateDefaultDataReadReference<ElementType>(
				METASOUND_GET_PARAM_NAME(InputValue1),
				InParams.OperatorSettings
			);

			auto InValue2 = InputData.GetOrCreateDefaultDataReadReference<ElementType>(
				METASOUND_GET_PARAM_NAME(InputValue2),
				InParams.OperatorSettings
			);

			auto InValue3 = InputData.GetOrCreateDefaultDataReadReference<ElementType>(
				METASOUND_GET_PARAM_NAME(InputValue3),
				InParams.OperatorSettings
			);

			auto InValue4 = InputData.GetOrCreateDefaultDataReadReference<ElementType>(
				METASOUND_GET_PARAM_NAME(InputValue4),
				InParams.OperatorSettings
			);

			auto InValue5 = InputData.GetOrCreateDefaultDataReadReference<ElementType>(
				METASOUND_GET_PARAM_NAME(InputValue5),
				InParams.OperatorSettings
			);

			auto InValue6 = InputData.GetOrCreateDefaultDataReadReference<ElementType>(
				METASOUND_GET_PARAM_NAME(InputValue6),
				InParams.OperatorSettings
			);

			auto InValue7 = InputData.GetOrCreateDefaultDataReadReference<ElementType>(
				METASOUND_GET_PARAM_NAME(InputValue7),
				InParams.OperatorSettings
			);

			return MakeUnique<TArrayMakeOperator>(
				InParams,
				InTriggerMake,
				InValue0,
				InValue1,
				InValue2,
				InValue3,
				InValue4,
				InValue5,
				InValue6,
				InValue7
			);
		}

		TArrayMakeOperator(
			const FBuildOperatorParams& InParams,
			TDataReadReference<FTrigger> InTriggerMake,
			TDataReadReference<ElementType> InValue0,
			TDataReadReference<ElementType> InValue1,
			TDataReadReference<ElementType> InValue2,
			TDataReadReference<ElementType> InValue3,
			TDataReadReference<ElementType> InValue4,
			TDataReadReference<ElementType> InValue5,
			TDataReadReference<ElementType> InValue6,
			TDataReadReference<ElementType> InValue7)
			: TriggerMake(InTriggerMake)
			, Value0(InValue0)
			, Value1(InValue1)
			, Value2(InValue2)
			, Value3(InValue3)
			, Value4(InValue4)
			, Value5(InValue5)
			, Value6(InValue6)
			, Value7(InValue7)
			, TriggerOnMake(FTriggerWriteRef::CreateNew(InParams.OperatorSettings))
			, OutArray(TDataWriteReferenceFactory<FArrayType>::CreateAny(InParams.OperatorSettings))
		{
		}

		virtual ~TArrayMakeOperator() = default;

		virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
		{
			using namespace ArrayMakeNodeVertexNames;
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputTriggerMake), TriggerMake);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputValue0), Value0);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputValue1), Value1);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputValue2), Value2);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputValue3), Value3);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputValue4), Value4);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputValue5), Value5);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputValue6), Value6);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputValue7), Value7);
		}

		virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
		{
			using namespace ArrayMakeNodeVertexNames;
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutputTriggerOnMake), TriggerOnMake);
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
			TriggerOnMake->AdvanceBlock();

			if (*TriggerMake)
			{
				FArrayType TempArray;
				TempArray.Add(*Value0);
				TempArray.Add(*Value1);
				TempArray.Add(*Value2);
				TempArray.Add(*Value3);
				TempArray.Add(*Value4);
				TempArray.Add(*Value5);
				TempArray.Add(*Value6);
				TempArray.Add(*Value7);
				*OutArray = TempArray;

				TriggerMake->ExecuteBlock(
					[](int32, int32) {},
					[this](int32 StartFrame, int32) { TriggerOnMake->TriggerFrame(StartFrame); }
				);
			}
		}

	private:
		TDataReadReference<FTrigger> TriggerMake;
		TDataReadReference<ElementType> Value0;
		TDataReadReference<ElementType> Value1;
		TDataReadReference<ElementType> Value2;
		TDataReadReference<ElementType> Value3;
		TDataReadReference<ElementType> Value4;
		TDataReadReference<ElementType> Value5;
		TDataReadReference<ElementType> Value6;
		TDataReadReference<ElementType> Value7;
		TDataWriteReference<FTrigger> TriggerOnMake;
		FArrayDataWriteReference OutArray;
	};

	template<typename ElementType>
	class TArrayMakeNode : public FNodeFacade
	{
	public:
		TArrayMakeNode(const FNodeInitData& InInitData)
			: FNodeFacade(InInitData.InstanceName, InInitData.InstanceID, TFacadeOperatorClass<TArrayMakeOperator<ElementType>>())
		{
		}
		virtual ~TArrayMakeNode() = default;
	};
}

#undef LOCTEXT_NAMESPACE