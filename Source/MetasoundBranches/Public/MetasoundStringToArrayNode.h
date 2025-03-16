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

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_StringToArray"

namespace Metasound
{
	namespace StringToArrayNodeVertexNames
	{
		METASOUND_PARAM(InputTriggerSplit, "Split",          "Trigger to split the string into array elements.")
		METASOUND_PARAM(InputString,       "String",         "The input string to be split.")
		METASOUND_PARAM(InputDelimiter,    "Delimiter",      "Delimiter string used to split the input.")

		METASOUND_PARAM(OutputTriggerOnSplit, "On Split",    "Triggers when the split is completed.")
		METASOUND_PARAM(OutputArray,         "Array",        "The resulting array after splitting the string.")
	}

	template<typename ElementType>
	class TArraySplitOperator : public TExecutableOperator<TArraySplitOperator<ElementType>>
	{
	public:
		using FArrayType = TArray<ElementType>;

		using FStringReadRef       = TDataReadReference<FString>;
		using FArrayWriteRef       = TDataWriteReference<FArrayType>;
		using FTriggerReadRef      = TDataReadReference<FTrigger>;

		static const FVertexInterface& GetDefaultInterface()
		{
			using namespace StringToArrayNodeVertexNames;

			static const FVertexInterface DefaultInterface(
				FInputVertexInterface(
					TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTriggerSplit)),
					TInputDataVertex<FString>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputString)),
					TInputDataVertex<FString>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputDelimiter))
				),
				FOutputVertexInterface(
					TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputTriggerOnSplit)),
					TOutputDataVertex<FArrayType>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputArray))
				)
			);

			return DefaultInterface;
		}

		static const FNodeClassMetadata& GetNodeInfo()
		{
			auto CreateNodeClassMetadata = []() -> FNodeClassMetadata
			{
				FName DataTypeName = GetMetasoundDataTypeName<TArray<ElementType>>();
				FName OperatorName = TEXT("String To Array");
				FText NodeDisplayName = METASOUND_LOCTEXT_FORMAT("ArrayOpSplitDisplayNamePattern", "String To Array ({0})", GetMetasoundDataTypeDisplayText<TArray<ElementType>>());
				const FText NodeDescription = LOCTEXT("ArrayOpSplitDesc", "Splits a string into an array of elements using a specified delimiter.");

				FVertexInterface NodeInterface = GetDefaultInterface();

				FNodeClassMetadata Metadata = MetasoundArrayNodesPrivate::CreateArrayNodeClassMetadata(
					DataTypeName,
					OperatorName,
					NodeDisplayName,
					NodeDescription,
					NodeInterface,
					1 /* MajorVersion */,
					0 /* MinorVersion */,
					false /* bIsDeprecated */
				);

				Metadata.Author = TEXT("Charles Matthews");
				Metadata.PromptIfMissing = PluginNodeMissingPrompt;
				Metadata.CategoryHierarchy = { LOCTEXT("Custom", "Branches") };
				Metadata.Keywords = TArray<FText>();

				return Metadata;
			};

			static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
			return Metadata;
		}

		static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults)
		{
			using namespace StringToArrayNodeVertexNames;

			const FInputVertexInterfaceData& InputData = InParams.InputData;

			FTriggerReadRef InTriggerSplit = InputData.GetOrCreateDefaultDataReadReference<FTrigger>(
				METASOUND_GET_PARAM_NAME(InputTriggerSplit),
				InParams.OperatorSettings
			);

			FStringReadRef InString = InputData.GetOrCreateDefaultDataReadReference<FString>(
				METASOUND_GET_PARAM_NAME(InputString),
				InParams.OperatorSettings
			);

			FStringReadRef InDelimiter = InputData.GetOrCreateDefaultDataReadReference<FString>(
				METASOUND_GET_PARAM_NAME(InputDelimiter),
				InParams.OperatorSettings
			);

			return MakeUnique<TArraySplitOperator>(
				InParams,
				InTriggerSplit,
				InString,
				InDelimiter
			);
		}

		TArraySplitOperator(
			const FBuildOperatorParams& InParams,
			FTriggerReadRef InTriggerSplit,
			FStringReadRef  InString,
			FStringReadRef  InDelimiter)
			: TriggerSplit(InTriggerSplit)
			, InputString(InString)
			, Delimiter(InDelimiter)
			, TriggerOnSplit(FTriggerWriteRef::CreateNew(InParams.OperatorSettings))
			, OutputArray(TDataWriteReferenceFactory<FArrayType>::CreateAny(InParams.OperatorSettings))
		{
		}

		virtual ~TArraySplitOperator() = default;

		virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
		{
			using namespace StringToArrayNodeVertexNames;
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputTriggerSplit), TriggerSplit);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputString), InputString);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputDelimiter), Delimiter);
		}

		virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
		{
			using namespace StringToArrayNodeVertexNames;
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutputTriggerOnSplit), TriggerOnSplit);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutputArray), OutputArray);
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
			TriggerOnSplit->AdvanceBlock();

			if (*TriggerSplit)
			{
				// Split the string into tokens using the specified delimiter
				TArray<FString> Tokens;
				InputString->ParseIntoArray(Tokens, **Delimiter);

				// Empty array for parsed elements
				FArrayType NewArray;
				NewArray.Reserve(Tokens.Num());

				for (const FString& Token : Tokens)
				{
					ElementType ParsedElement{};
					// Attempt to parse the token to templated type
					if (!LexTryParseString(ParsedElement, *Token))
					{
						
						// Fail: ParsedElement stays at default val
					}
					NewArray.Add(ParsedElement);
				}

				*OutputArray = MoveTemp(NewArray);


				TriggerSplit->ExecuteBlock(
					[](int32, int32) {},
					[this](int32 StartFrame, int32 EndFrame)
					{
						TriggerOnSplit->TriggerFrame(StartFrame);
					}
				);
			}
		}

	private:
		// Input read references
		FTriggerReadRef TriggerSplit;
		FStringReadRef  InputString;
		FStringReadRef  Delimiter;

		// Output write references
		FTriggerWriteRef    TriggerOnSplit;
		TDataWriteReference<FArrayType> OutputArray;
	};

	template<typename ElementType>
	class TArraySplitNode : public FNodeFacade
	{
	public:
		TArraySplitNode(const FNodeInitData& InInitData)
			: FNodeFacade(InInitData.InstanceName, InInitData.InstanceID, TFacadeOperatorClass<TArraySplitOperator<ElementType>>())
		{
		}

		virtual ~TArraySplitNode() = default;
	};
}

#undef LOCTEXT_NAMESPACE