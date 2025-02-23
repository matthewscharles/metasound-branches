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

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_StringToSequence"

/** Fully generic bracket group struct with a local repeat factor. */
template<typename T>
struct FBracketGroup
{
	/** The parsed values inside the bracket. */
	TArray<T> Values;

	/** How many times to repeat bracketed values locally. Defaults to 1. */
	int32 Repeats = 1;
};

namespace Metasound
{
	namespace StringToSequenceNodeVertexNames
	{
		// Input Pins
		METASOUND_PARAM(InputTriggerSplit,    "Load",             "Trigger to load the string into sequence elements.")
		METASOUND_PARAM(InputTriggerNext,     "Next",             "Trigger to output the next step in the sequence.")
		METASOUND_PARAM(InputTriggerReset,    "Reset",            "Reset the sequence and repeats to 0 on next trigger.")
		METASOUND_PARAM(InputString,          "String",           "The input string to be split.")
		METASOUND_PARAM(InputDelimiter,       "Delimiter",        "Delimiter string used to split the input (for non-bracket logic).")
		METASOUND_PARAM(InputNumRepeats,      "Num Repeats",      "Number of full sequence iterations before stopping (if not looping).")
		METASOUND_PARAM(InputLoop,            "Loop",             "If true, once the sequence is complete, it loops forever (Num Repeats resets on each overall loop).")
		METASOUND_PARAM(InputOverflow,        "Trigger Overflow", "If true and Loop=false, Next will continue to trigger On Finished instead of doing nothing.")

		// Output Pins
		METASOUND_PARAM(OutputTriggerOnSplit,  "On Load",          "Triggers when the sequence is ready.")
		METASOUND_PARAM(OutputTriggerNext,     "On Next",          "Triggers when the sequence outputs the next element.")
		METASOUND_PARAM(OutputValue,           "Value",            "Current sequence value.")
		METASOUND_PARAM(OutputPosition,        "Position",         "Current sequence position (0-based).")
		METASOUND_PARAM(OutputTimeMultiplier,  "Time Multiplier",  "Current sub-step fraction = 1 / (GroupSize * Repeats).")
		METASOUND_PARAM(OutputTriggerOnRepeat, "On Repeat",        "Triggers immediately at the start of each pass (position 0).")
		METASOUND_PARAM(OutputRepeatCount,     "Repeat Count",     "How many times the sequence has started a new pass.")
		METASOUND_PARAM(OutputLength,          "Length",           "Number of bracket groups in the sequence.")
		METASOUND_PARAM(OutputTriggerOnEnd,    "On Finished",      "Triggers when the sequence has finished all repeats (if Loop=false).")
	}

	template<typename ElementType>
	class TStringToSequenceOperator : public TExecutableOperator<TStringToSequenceOperator<ElementType>>
	{
	public:
		using FBracketedGroups = TArray<FBracketGroup<ElementType>>;

		using FStringReadRef  = TDataReadReference<FString>;
		using FTriggerReadRef = TDataReadReference<FTrigger>;
		using FInt32ReadRef   = TDataReadReference<int32>;
		using FBoolReadRef    = TDataReadReference<bool>;

		using FTriggerWriteRef   = TDataWriteReference<FTrigger>;
		using FElementWriteRef   = TDataWriteReference<ElementType>;
		using FInt32WriteRef     = TDataWriteReference<int32>;
		using FFloatWriteRef     = TDataWriteReference<float>;

		static const FVertexInterface& GetDefaultInterface()
		{
			using namespace StringToSequenceNodeVertexNames;

			static const FVertexInterface DefaultInterface(
				FInputVertexInterface(
					TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTriggerSplit)),
					TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTriggerNext)),
					TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTriggerReset)),
					TInputDataVertex<FString>( METASOUND_GET_PARAM_NAME_AND_METADATA(InputString)),
					TInputDataVertex<FString>( METASOUND_GET_PARAM_NAME_AND_METADATA(InputDelimiter)),
					TInputDataVertex<int32>(   METASOUND_GET_PARAM_NAME_AND_METADATA(InputNumRepeats)),
					TInputDataVertex<bool>(    METASOUND_GET_PARAM_NAME_AND_METADATA(InputLoop)),
					TInputDataVertex<bool>(    METASOUND_GET_PARAM_NAME_AND_METADATA(InputOverflow))
				),
				FOutputVertexInterface(
					TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputTriggerOnSplit)),
					TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputTriggerNext)),
					TOutputDataVertex<ElementType>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputValue)),
					TOutputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputPosition)),
					TOutputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputTimeMultiplier)),
					TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputTriggerOnRepeat)),
					TOutputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputRepeatCount)),
					TOutputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputLength)),
					TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputTriggerOnEnd))
				)
			);

			return DefaultInterface;
		}

		static const FNodeClassMetadata& GetNodeInfo()
		{
			auto CreateNodeClassMetadata = []() -> FNodeClassMetadata
			{
				FName DataTypeName = GetMetasoundDataTypeName<ElementType>();
				FName OperatorName = TEXT("String To Sequence");
				FText NodeDisplayName = METASOUND_LOCTEXT_FORMAT(
					"StringToSequenceDisplayName",
					"String To Sequence ({0})",
					GetMetasoundDataTypeDisplayText<ElementType>()
				);

				const FText NodeDescription = LOCTEXT(
					"StringToSequenceDesc",
					"Splits a string into bracketed groups (if any) and single tokens. Brackets may end with xN to repeat that bracket N times. "
					"Each bracket counts as one position, but can produce multiple sub-steps. A 'Repeat' triggers immediately when a new pass begins at position=0."
				);

				// todo: add sub-step division count
				
				FVertexInterface NodeInterface = GetDefaultInterface();

				FNodeClassMetadata Metadata;
				Metadata.ClassName = {TEXT("Metasound"), OperatorName, DataTypeName};
				Metadata.MajorVersion = 1;
				Metadata.MinorVersion = 0;
				Metadata.DisplayName = NodeDisplayName;
				Metadata.Description = NodeDescription;
				Metadata.Author = TEXT("Charles Matthews");
				Metadata.DefaultInterface = NodeInterface;
				Metadata.CategoryHierarchy = { LOCTEXT("Custom", "Branches") };
				Metadata.Keywords = TArray<FText>();
				Metadata.PromptIfMissing = PluginNodeMissingPrompt;
				return Metadata;
			};

			static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
			return Metadata;
		}

		static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults)
		{
			using namespace StringToSequenceNodeVertexNames;
			const FInputVertexInterfaceData& InputData = InParams.InputData;

			TDataReadReference<FTrigger> InTriggerSplit =
				InputData.GetOrCreateDefaultDataReadReference<FTrigger>(
					METASOUND_GET_PARAM_NAME(InputTriggerSplit),
					InParams.OperatorSettings
				);

			TDataReadReference<FTrigger> InTriggerNext =
				InputData.GetOrCreateDefaultDataReadReference<FTrigger>(
					METASOUND_GET_PARAM_NAME(InputTriggerNext),
					InParams.OperatorSettings
				);

			TDataReadReference<FTrigger> InTriggerReset =
				InputData.GetOrCreateDefaultDataReadReference<FTrigger>(
					METASOUND_GET_PARAM_NAME(InputTriggerReset),
					InParams.OperatorSettings
				);

			TDataReadReference<FString> InString =
				InputData.GetOrCreateDefaultDataReadReference<FString>(
					METASOUND_GET_PARAM_NAME(InputString),
					InParams.OperatorSettings
				);

			TDataReadReference<FString> InDelimiter =
				InputData.GetOrCreateDefaultDataReadReference<FString>(
					METASOUND_GET_PARAM_NAME(InputDelimiter),
					InParams.OperatorSettings
				);

			TDataReadReference<int32> InNumRepeats =
				InputData.GetOrCreateDefaultDataReadReference<int32>(
					METASOUND_GET_PARAM_NAME(InputNumRepeats),
					InParams.OperatorSettings
				);

			TDataReadReference<bool> InLoop =
				InputData.GetOrCreateDefaultDataReadReference<bool>(
					METASOUND_GET_PARAM_NAME(InputLoop),
					InParams.OperatorSettings
				);

			TDataReadReference<bool> InOverflow =
				InputData.GetOrCreateDefaultDataReadReference<bool>(
					METASOUND_GET_PARAM_NAME(InputOverflow),
					InParams.OperatorSettings
				);

			return MakeUnique<TStringToSequenceOperator<ElementType>>(
				InParams,
				InTriggerSplit,
				InTriggerNext,
				InTriggerReset,
				InString,
				InDelimiter,
				InNumRepeats,
				InLoop,
				InOverflow
			);
		}

		TStringToSequenceOperator(
			const FBuildOperatorParams& InParams,
			FTriggerReadRef InTriggerSplit,
			FTriggerReadRef InTriggerNext,
			FTriggerReadRef InTriggerReset,
			FStringReadRef  InString,
			FStringReadRef  InDelimiter,
			FInt32ReadRef   InNumRepeats,
			FBoolReadRef    bInLoop,
			FBoolReadRef    bInOverflow)
			: TriggerSplit(InTriggerSplit)
			, TriggerNext(InTriggerNext)
			, TriggerReset(InTriggerReset)
			, InputString(InString)
			, Delimiter(InDelimiter)
			, NumRepeats(InNumRepeats)
			, bLoop(bInLoop)
			, bOverflow(bInOverflow)

			// Outputs
			, OnSplit(FTriggerWriteRef::CreateNew(InParams.OperatorSettings))
			, OnNext(FTriggerWriteRef::CreateNew(InParams.OperatorSettings))
			, OutValue(TDataWriteReferenceFactory<ElementType>::CreateAny(InParams.OperatorSettings))
			, OutPosition(TDataWriteReferenceFactory<int32>::CreateAny(InParams.OperatorSettings))
			, OutTimeMultiplier(TDataWriteReferenceFactory<float>::CreateAny(InParams.OperatorSettings))
			, OnRepeat(FTriggerWriteRef::CreateNew(InParams.OperatorSettings))
			, OutRepeatCount(TDataWriteReferenceFactory<int32>::CreateAny(InParams.OperatorSettings))
			, OutLength(TDataWriteReferenceFactory<int32>::CreateAny(InParams.OperatorSettings))
			, OnEnd(FTriggerWriteRef::CreateNew(InParams.OperatorSettings))
		{
			*OutValue          = ElementType{};
			*OutPosition       = 0;
			*OutTimeMultiplier = 1.0f;
			*OutRepeatCount    = 0;
			*OutLength         = 0;
		}

		virtual ~TStringToSequenceOperator() = default;

		virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
		{
			using namespace StringToSequenceNodeVertexNames;
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputTriggerSplit), TriggerSplit);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputTriggerNext),  TriggerNext);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputTriggerReset), TriggerReset);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputString),       InputString);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputDelimiter),    Delimiter);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputNumRepeats),   NumRepeats);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputLoop),         bLoop);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputOverflow),     bOverflow);
		}

		virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
		{
			using namespace StringToSequenceNodeVertexNames;
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutputTriggerOnSplit),  OnSplit);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutputTriggerNext),     OnNext);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutputValue),           OutValue);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutputPosition),        OutPosition);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutputTimeMultiplier),  OutTimeMultiplier);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutputTriggerOnRepeat), OnRepeat);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutputRepeatCount),     OutRepeatCount);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutputLength),          OutLength);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutputTriggerOnEnd),    OnEnd);
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
			// Advance triggers
			OnSplit->AdvanceBlock();
			OnNext->AdvanceBlock();
			OnRepeat->AdvanceBlock();
			OnEnd->AdvanceBlock();

			// If "Reset" triggered:
			if (*TriggerReset)
			{
				TriggerReset->ExecuteBlock(
					[](int32, int32){},
					[this](int32 StartFrame, int32)
					{
						ResetSequence();
					}
				);
			}

			// If "Load" triggered:
			if (*TriggerSplit)
			{
				TriggerSplit->ExecuteBlock(
					[](int32, int32){},
					[this](int32 StartFrame, int32)
					{
						ParseBracketedGroups();
						ResetSequence(); 

						// Update length = number of bracket groups
						*OutLength = ParsedGroups.Num();

						// Fire "On Load"
						OnSplit->TriggerFrame(StartFrame);
					}
				);
			}

			// If "Next" triggered:
			if (*TriggerNext)
			{
				TriggerNext->ExecuteBlock(
					[](int32, int32){},
					[this](int32 StartFrame, int32)
					{
						// If the entire sequence is complete
						if (bSequenceComplete)
						{
							// If overflow is true, re-fire OnEnd each time
							if (*bOverflow)
							{
								OnEnd->TriggerFrame(StartFrame);
							}
							return;
						}

						if (ParsedGroups.Num() == 0)
						{
							// No data
							return;
						}

						// If we are at groupIndex=0 and subIndex=0, that means
						// we just started a new pass => Fire OnRepeat immediately
						if (CurrentGroupIndex == 0 && CurrentSubIndex == 0)
						{
							++CurrentRepeatCount;
							*OutRepeatCount = CurrentRepeatCount;
							OnRepeat->TriggerFrame(StartFrame);
						}

						// Output the current bracket group sub-step
						FBracketGroup<ElementType>& CurrentGroup = ParsedGroups[CurrentGroupIndex];
						const int32 NumValues = CurrentGroup.Values.Num();
						const int32 EffectiveGroupSize = NumValues * CurrentGroup.Repeats;

						// SubIndex in the repeated bracket
						const int32 LocalIndex = (CurrentSubIndex % NumValues);

						// Output the actual value
						*OutValue = CurrentGroup.Values[LocalIndex];

						// The top-level position is the bracket group index
						*OutPosition = CurrentGroupIndex;

						// Time for a single sub-step
						if (EffectiveGroupSize > 0)
						{
							*OutTimeMultiplier = 1.0f / (float)EffectiveGroupSize;
						}
						else
						{
							*OutTimeMultiplier = 1.0f;
						}

						OnNext->TriggerFrame(StartFrame);

						// Advance the sub-index
						++CurrentSubIndex;

						// If we've output all sub-steps for this bracket group
						if (CurrentSubIndex >= EffectiveGroupSize)
						{
							CurrentSubIndex = 0;
							++CurrentGroupIndex;

							// If we've gone beyond the last bracket group, we've finished a pass
							if (CurrentGroupIndex >= ParsedGroups.Num())
							{
								// Reset for the next pass
								CurrentGroupIndex = 0;

								// Check if we've hit the max overall repeats
								const bool bReachedMax = (CurrentRepeatCount >= *NumRepeats);
								if (!*bLoop && bReachedMax)
								{
									// If not looping, we're done
									OnEnd->TriggerFrame(StartFrame);
									bSequenceComplete = true;
								}
								else if (*bLoop && bReachedMax)
								{
									// If looping, reset the pass count
									CurrentRepeatCount = 0;
									*OutRepeatCount = 0;
								}
							}
						}
					}
				);
			}
		}

	private:
		// Reset all sequence state
		void ResetSequence()
		{
			CurrentGroupIndex  = 0;
			CurrentSubIndex    = 0;
			bSequenceComplete  = false;
			// Reset restarts from pass 0, otherwise can set CurrentRepeatCount
			CurrentRepeatCount = 0;
			*OutRepeatCount = 0;
		}

		/**
		 * Parses the input string for bracketed sections. 
		 * If a bracket section ends with "xN", we set `Repeats = N`.
		 * E.g. "[2 3] x3" => bracket with {2,3}, repeated 3 times.
		 * Then we store it as one bracket group with Repeats=3.
		 */
		void ParseBracketedGroups()
		{
			ParsedGroups.Reset();

			// Currently ignoring Delimiter pin for bracket logic,
			// whitespace parse for demonstration
			TArray<FString> Tokens;
			InputString->ParseIntoArrayWS(Tokens);

			int32 i = 0;
			while (i < Tokens.Num())
			{
				const FString& Token = Tokens[i];
				if (Token.StartsWith("["))
				{
					FBracketGroup<ElementType> Bracket;
					FString Trimmed = Token;
					Trimmed.RemoveFromStart("[");
					bool bEndFound = false;

					// If this token ends with ']', might also have xN after that
					bool bCheckForRepeat = false;

					if (Trimmed.EndsWith("]"))
					{
						Trimmed.RemoveFromEnd("]");
						bEndFound = true;
						bCheckForRepeat = true; 
					}

					// Parse the first chunk if not empty
					if (!Trimmed.IsEmpty())
					{
						ElementType Parsed{};
						if (!LexTryParseString(Parsed, *Trimmed))
						{
							// fails => default
						}
						Bracket.Values.Add(Parsed);
					}
					++i;

					// Read until we see a ']' if not found yet
					while (!bEndFound && i < Tokens.Num())
					{
						FString NextTok = Tokens[i];
						if (NextTok.EndsWith("]"))
						{
							NextTok.RemoveFromEnd("]");
							bEndFound = true;
							bCheckForRepeat = true;
						}

						if (!NextTok.IsEmpty())
						{
							ElementType SubParsed{};
							if (!LexTryParseString(SubParsed, *NextTok))
							{
								// parse fails => default
							}
							Bracket.Values.Add(SubParsed);
						}
						++i;
					}

					// After the bracket is fully parsed, check if the next token 
					// is something like x3 to set bracket.Repeats=3
					if (bCheckForRepeat && i < Tokens.Num())
					{
						// e.g. if the next token is "x3"
						const FString& PotentialRepeat = Tokens[i];
						if (PotentialRepeat.StartsWith("x") && PotentialRepeat.Len() > 1)
						{
							FString NumberPart = PotentialRepeat.Mid(1); // remove 'x'
							int32 LocalRepeat = 1;
							if (LexTryParseString(LocalRepeat, *NumberPart))
							{
								Bracket.Repeats = FMath::Max(LocalRepeat, 1);
								// we've consumed this token
								++i;
							}
						}
					}

					ParsedGroups.Add(MoveTemp(Bracket));
				}
				else
				{
					// single token => single group
					FBracketGroup<ElementType> SingleGrp;
					ElementType Parsed{};
					if (!LexTryParseString(Parsed, *Token))
					{
						// fails => default
					}
					SingleGrp.Values.Add(Parsed);

					ParsedGroups.Add(MoveTemp(SingleGrp));
					++i;

					//* check if next token is xN 
					//* e.g. "0 x2" => 0 repeated 2 times
					// if (i < Tokens.Num())
					// {
					// 	const FString& PotentialRepeat = Tokens[i];
					// 	if (PotentialRepeat.StartsWith("x") && PotentialRepeat.Len() > 1)
					// 	{
					// 		FString NumberPart = PotentialRepeat.Mid(1);
					// 		int32 LocalRepeat = 1;
					// 		if (LexTryParseString(LocalRepeat, *NumberPart))
					// 		{
					// 			SingleGrp.Repeats = FMath::Max(LocalRepeat, 1);
					// 			// Overwrite the bracket in place
					// 			ParsedGroups.Last().Repeats = SingleGrp.Repeats;
					// 			++i;
					// 		}
					// 	}
					// }
				}
			}
		}

	private:
		FTriggerReadRef TriggerSplit;
		FTriggerReadRef TriggerNext;
		FTriggerReadRef TriggerReset;
		FStringReadRef  InputString;
		FStringReadRef  Delimiter;
		FInt32ReadRef   NumRepeats;
		FBoolReadRef    bLoop;
		FBoolReadRef    bOverflow;

		FTriggerWriteRef  OnSplit;       
		FTriggerWriteRef  OnNext;        
		TDataWriteReference<ElementType> OutValue; 
		TDataWriteReference<int32>       OutPosition;
		TDataWriteReference<float>       OutTimeMultiplier;
		FTriggerWriteRef  OnRepeat;      
		TDataWriteReference<int32>       OutRepeatCount;
		TDataWriteReference<int32>       OutLength;
		FTriggerWriteRef  OnEnd;         

		/** All bracket groups, each with its local Repeats factor. */
		FBracketedGroups ParsedGroups;

		/** Index of the bracket group we’re on. */
		int32 CurrentGroupIndex  = 0;

		/** Which sub-step inside the repeated bracket group. */
		int32 CurrentSubIndex    = 0;

		/** Count how many times we have begun a new pass. */
		int32 CurrentRepeatCount = 0;

		/** If the entire sequence is complete (non-loop). */
		bool  bSequenceComplete  = false;
	};
	
	template<typename ElementType>
	class TStringToSequenceNode : public FNodeFacade
	{
	public:
		TStringToSequenceNode(const FNodeInitData& InInitData)
			: FNodeFacade(
				InInitData.InstanceName,
				InInitData.InstanceID,
				TFacadeOperatorClass<TStringToSequenceOperator<ElementType>>()
			)
		{
		}

		virtual ~TStringToSequenceNode() = default;
	};
}

#undef LOCTEXT_NAMESPACE