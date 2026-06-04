// Copyright 2026 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundTriggerSelectConfigurableNode.h"

#include "MetasoundBranches/Public/MetasoundCommonMacros.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundParamHelper.h"
#include "MetasoundPrimitives.h"
#include "MetasoundTrigger.h"

#define LOCTEXT_NAMESPACE "MetasoundBranches_TriggerSelectConfigurable"

namespace Metasound
{
	namespace TriggerSelectConfigurableVertexNames
	{
		METASOUND_PARAM(InputTrigger, "Trigger", "Evaluates the input value and fires any matching output triggers.")
		METASOUND_PARAM(InputValue, "Value", "Input value to compare against the configured match values.")
	}

	namespace TriggerSelectConfigurablePrivate
	{
		class FTriggerSelectConfigurableOperatorData final : public TOperatorData<FTriggerSelectConfigurableOperatorData>
		{
		public:
			static const FLazyName OperatorDataTypeName;

			explicit FTriggerSelectConfigurableOperatorData(
				const EMetaSoundTriggerSelectConfigurableValueType InValueType,
				TArray<int32> InIntValues,
				TArray<float> InFloatValues,
				const float InFloatTolerance)
				: ValueType(InValueType)
				, IntValues(MoveTemp(InIntValues))
				, FloatValues(MoveTemp(InFloatValues))
				, FloatTolerance(InFloatTolerance)
			{
			}

			EMetaSoundTriggerSelectConfigurableValueType ValueType;
			TArray<int32> IntValues;
			TArray<float> FloatValues;
			float FloatTolerance;
		};

		const FLazyName FTriggerSelectConfigurableOperatorData::OperatorDataTypeName = TEXT("FTriggerSelectConfigurableOperatorData");

		int32 GetConfiguredValueCount(const FTriggerSelectConfigurableOperatorData& InData)
		{
			return (InData.ValueType == EMetaSoundTriggerSelectConfigurableValueType::Int) ? InData.IntValues.Num() : InData.FloatValues.Num();
		}

		FName MakeOutputName(const int32 InIndex)
		{
			return FName(*FString::Printf(TEXT("Match %d"), InIndex + 1));
		}

		FString MakeValueLabel(const FTriggerSelectConfigurableOperatorData& InData, const int32 InIndex)
		{
			if (InData.ValueType == EMetaSoundTriggerSelectConfigurableValueType::Int)
			{
				return InData.IntValues.IsValidIndex(InIndex) ? FString::FromInt(InData.IntValues[InIndex]) : FString::Printf(TEXT("Match %d"), InIndex + 1);
			}

			return InData.FloatValues.IsValidIndex(InIndex) ? FString::SanitizeFloat(InData.FloatValues[InIndex]) : FString::Printf(TEXT("Match %d"), InIndex + 1);
		}

		FDataVertexMetadata MakeOutputMetadata(const FTriggerSelectConfigurableOperatorData& InData, const int32 InIndex)
		{
			const FString ValueLabel = MakeValueLabel(InData, InIndex);
#if WITH_EDITOR
			return {
				FText::Format(LOCTEXT("TriggerSelectConfigurableOutputDescription", "Triggers when the input value matches configured value {0}."), FText::FromString(ValueLabel)),
				FText::FromString(ValueLabel)
			};
#else
			return {};
#endif
		}

		FVertexInterface GetVertexInterface(const FTriggerSelectConfigurableOperatorData& InData)
		{
			using namespace TriggerSelectConfigurableVertexNames;

			FInputVertexInterface InputInterface;
			InputInterface.Add(TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTrigger)));

			if (InData.ValueType == EMetaSoundTriggerSelectConfigurableValueType::Int)
			{
				InputInterface.Add(TInputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputValue), 0));
			}
			else
			{
				InputInterface.Add(TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputValue), 0.0f));
			}

			FOutputVertexInterface OutputInterface;
			const int32 NumOutputs = GetConfiguredValueCount(InData);
			for (int32 Index = 0; Index < NumOutputs; ++Index)
			{
				OutputInterface.Add(TOutputDataVertex<FTrigger>(MakeOutputName(Index), MakeOutputMetadata(InData, Index)));
			}

			return { MoveTemp(InputInterface), MoveTemp(OutputInterface) };
		}
	}

	class FTriggerSelectConfigurableOperator final : public TExecutableOperator<FTriggerSelectConfigurableOperator>
	{
	public:
		using FTriggerSelectConfigurableOperatorData = TriggerSelectConfigurablePrivate::FTriggerSelectConfigurableOperatorData;

		FTriggerSelectConfigurableOperator(
			const TSharedPtr<const FTriggerSelectConfigurableOperatorData>& InOperatorData,
			const FTriggerReadRef& InTrigger,
			const FInt32ReadRef& InIntValue,
			const FFloatReadRef& InFloatValue,
			TArray<FTriggerWriteRef>&& InOutputs)
			: OperatorData(InOperatorData)
			, Trigger(InTrigger)
			, IntValue(InIntValue)
			, FloatValue(InFloatValue)
			, Outputs(MoveTemp(InOutputs))
		{
		}

		static const FVertexInterface& DeclareVertexInterface()
		{
			static const FTriggerSelectConfigurableOperatorData DefaultData(
				EMetaSoundTriggerSelectConfigurableValueType::Int,
				TArray<int32>{0},
				TArray<float>{},
				0.001f);
			static const FVertexInterface Interface = TriggerSelectConfigurablePrivate::GetVertexInterface(DefaultData);
			return Interface;
		}

		static const FNodeClassMetadata& GetNodeInfo()
		{
			static const FNodeClassMetadata Metadata = []()
			{
				FNodeClassMetadata NodeMetadata;
				NodeMetadata.ClassName = { TEXT("UE"), TEXT("TriggerSelectConfigurable"), TEXT("Trigger") };
				NodeMetadata.MajorVersion = 1;
				NodeMetadata.MinorVersion = 0;
				NodeMetadata.DisplayName = LOCTEXT("TriggerSelectConfigurableDisplayName", "Trigger Select Configurable");
				NodeMetadata.Description = LOCTEXT("TriggerSelectConfigurableDescription", "Compares an input int or float against a configurable list of values and fires the corresponding trigger outputs on input trigger events.");
				NodeMetadata.Author = TEXT("Charles Matthews");
				NodeMetadata.PromptIfMissing = PluginNodeMissingPrompt;
				NodeMetadata.DefaultInterface = DeclareVertexInterface();
				NodeMetadata.CategoryHierarchy = {
					LOCTEXT("BranchesCategory", "Branches"),
					LOCTEXT("TriggersCategory", "Triggers")
				};
				METASOUND_BRANCHES_APPLY_NODE_STYLE(NodeMetadata);
				return NodeMetadata;
			}();

			return Metadata;
		}

		METASOUND_DISABLE_LEGACY_IO()

		static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults&)
		{
			using namespace TriggerSelectConfigurableVertexNames;

			const FTriggerSelectConfigurableOperatorData* ConfigData = CastOperatorData<const FTriggerSelectConfigurableOperatorData>(InParams.Node.GetOperatorData().Get());
			if (!ConfigData)
			{
				return MakeUnique<FNoOpOperator>();
			}

			const TSharedPtr<const FTriggerSelectConfigurableOperatorData> OperatorDataSharedPtr = StaticCastSharedPtr<const FTriggerSelectConfigurableOperatorData>(InParams.Node.GetOperatorData());

			FTriggerReadRef InTrigger = InParams.InputData.GetOrCreateDefaultDataReadReference<FTrigger>(METASOUND_GET_PARAM_NAME(InputTrigger), InParams.OperatorSettings);
			FInt32ReadRef InIntValue = TDataReadReference<int32>::CreateNew(0);
			FFloatReadRef InFloatValue = TDataReadReference<float>::CreateNew(0.0f);
			if (ConfigData->ValueType == EMetaSoundTriggerSelectConfigurableValueType::Int)
			{
				InIntValue = InParams.InputData.GetOrCreateDefaultDataReadReference<int32>(METASOUND_GET_PARAM_NAME(InputValue), InParams.OperatorSettings);
			}
			else
			{
				InFloatValue = InParams.InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InputValue), InParams.OperatorSettings);
			}

			TArray<FTriggerWriteRef> OutputTriggers;
			const int32 NumOutputs = TriggerSelectConfigurablePrivate::GetConfiguredValueCount(*ConfigData);
			OutputTriggers.Reserve(NumOutputs);
			for (int32 Index = 0; Index < NumOutputs; ++Index)
			{
				OutputTriggers.Add(FTriggerWriteRef::CreateNew(InParams.OperatorSettings));
			}

			return MakeUnique<FTriggerSelectConfigurableOperator>(
				OperatorDataSharedPtr,
				InTrigger,
				InIntValue,
				InFloatValue,
				MoveTemp(OutputTriggers));
		}

		virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
		{
			using namespace TriggerSelectConfigurableVertexNames;

			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputTrigger), Trigger);
			if (OperatorData->ValueType == EMetaSoundTriggerSelectConfigurableValueType::Int)
			{
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputValue), IntValue);
			}
			else
			{
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputValue), FloatValue);
			}
		}

		virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
		{
			for (int32 Index = 0; Index < Outputs.Num(); ++Index)
			{
				InOutVertexData.BindReadVertex(TriggerSelectConfigurablePrivate::MakeOutputName(Index), Outputs[Index]);
			}
		}

		void Execute()
		{
			for (const FTriggerWriteRef& OutputTrigger : Outputs)
			{
				OutputTrigger->AdvanceBlock();
			}

			Trigger->ExecuteBlock(
				[](int32, int32) {},
				[this](int32 StartFrame, int32)
				{
					if (OperatorData->ValueType == EMetaSoundTriggerSelectConfigurableValueType::Int)
					{
						const int32 InValue = *IntValue;
						for (int32 Index = 0; Index < Outputs.Num(); ++Index)
						{
							if (OperatorData->IntValues.IsValidIndex(Index) && InValue == OperatorData->IntValues[Index])
							{
								Outputs[Index]->TriggerFrame(StartFrame);
							}
						}
						return;
					}

					const float InValue = *FloatValue;
					for (int32 Index = 0; Index < Outputs.Num(); ++Index)
					{
						if (OperatorData->FloatValues.IsValidIndex(Index)
							&& FMath::IsNearlyEqual(InValue, OperatorData->FloatValues[Index], OperatorData->FloatTolerance))
						{
							Outputs[Index]->TriggerFrame(StartFrame);
						}
					}
				});
		}

	private:
		TSharedPtr<const FTriggerSelectConfigurableOperatorData> OperatorData;
		FTriggerReadRef Trigger;
		FInt32ReadRef IntValue;
		FFloatReadRef FloatValue;
		TArray<FTriggerWriteRef> Outputs;
	};

	using FTriggerSelectConfigurableNode = TNodeFacade<FTriggerSelectConfigurableOperator>;
	METASOUND_REGISTER_NODE_AND_CONFIGURATION(FTriggerSelectConfigurableNode, FMetaSoundTriggerSelectConfigurableNodeConfiguration);
}

FMetaSoundTriggerSelectConfigurableNodeConfiguration::FMetaSoundTriggerSelectConfigurableNodeConfiguration()
	: ValueType(EMetaSoundTriggerSelectConfigurableValueType::Int)
	, IntValues{0}
	, FloatValues{0.0f}
	, FloatTolerance(0.001f)
	, OperatorData(MakeShared<Metasound::TriggerSelectConfigurablePrivate::FTriggerSelectConfigurableOperatorData>(ValueType, IntValues, FloatValues, FloatTolerance))
{
}

TInstancedStruct<FMetasoundFrontendClassInterface> FMetaSoundTriggerSelectConfigurableNodeConfiguration::OverrideDefaultInterface(const FMetasoundFrontendClass& InClass) const
{
	Metasound::TriggerSelectConfigurablePrivate::FTriggerSelectConfigurableOperatorData InterfaceData(ValueType, IntValues, FloatValues, FloatTolerance);
	return TInstancedStruct<FMetasoundFrontendClassInterface>::Make(
		FMetasoundFrontendClassInterface::GenerateClassInterface(
			Metasound::TriggerSelectConfigurablePrivate::GetVertexInterface(InterfaceData)));
}

TSharedPtr<const Metasound::IOperatorData> FMetaSoundTriggerSelectConfigurableNodeConfiguration::GetOperatorData() const
{
	OperatorData->ValueType = ValueType;
	OperatorData->IntValues = IntValues;
	OperatorData->FloatValues = FloatValues;
	OperatorData->FloatTolerance = FloatTolerance;
	return OperatorData;
}

#undef LOCTEXT_NAMESPACE