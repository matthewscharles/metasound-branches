// Copyright 2026 Charles Matthews. All Rights Reserved.

#pragma once

#include "MetasoundFrontendDocument.h"
#include "MetasoundOperatorData.h"

#include "MetasoundTriggerSelectConfigurableNode.generated.h"

UENUM()
enum class EMetaSoundTriggerSelectConfigurableValueType : uint8
{
	Int UMETA(DisplayName = "Int"),
	Float UMETA(DisplayName = "Float")
};

namespace Metasound::TriggerSelectConfigurablePrivate
{
	class FTriggerSelectConfigurableOperatorData;
}

USTRUCT()
struct FMetaSoundTriggerSelectConfigurableNodeConfiguration : public FMetaSoundFrontendNodeConfiguration
{
	GENERATED_BODY()

	FMetaSoundTriggerSelectConfigurableNodeConfiguration();

	UPROPERTY(EditAnywhere, Category = General)
	EMetaSoundTriggerSelectConfigurableValueType ValueType = EMetaSoundTriggerSelectConfigurableValueType::Int;

	UPROPERTY(EditAnywhere, Category = General, meta = (EditCondition = "ValueType == EMetaSoundTriggerSelectConfigurableValueType::Int", EditConditionHides))
	TArray<int32> IntValues;

	UPROPERTY(EditAnywhere, Category = General, meta = (EditCondition = "ValueType == EMetaSoundTriggerSelectConfigurableValueType::Float", EditConditionHides))
	TArray<float> FloatValues;

	UPROPERTY(EditAnywhere, Category = General, meta = (ClampMin = "0.0", EditCondition = "ValueType == EMetaSoundTriggerSelectConfigurableValueType::Float", EditConditionHides))
	float FloatTolerance = 0.001f;

	virtual TInstancedStruct<FMetasoundFrontendClassInterface> OverrideDefaultInterface(const FMetasoundFrontendClass& InClass) const override;

	virtual TSharedPtr<const Metasound::IOperatorData> GetOperatorData() const override;

private:
	mutable TSharedPtr<Metasound::TriggerSelectConfigurablePrivate::FTriggerSelectConfigurableOperatorData> OperatorData;
};