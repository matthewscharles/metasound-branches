// Copyright 2025 Charles Matthews. All Rights Reserved.

#pragma once

#include "MetasoundFrontendDocument.h"
#include "MetasoundOperatorData.h"
#include "MetasoundTime.h"
#include "MetasoundDataReference.h"
#include "MetasoundNode.h"

#include "MetasoundSlewConfigurableNode.generated.h"

UENUM()
enum class ESlewConfigurableMode : uint8
{
	Control UMETA(DisplayName = "Float"),
	Audio   UMETA(DisplayName = "Audio")
};

USTRUCT()
struct FMetaSoundSlewConfigurableNodeConfiguration : public FMetaSoundFrontendNodeConfiguration
{
	GENERATED_BODY()

	FMetaSoundSlewConfigurableNodeConfiguration()
		: NumPins(1)
		, SlewMode(ESlewConfigurableMode::Audio)
	{}

	UPROPERTY(EditAnywhere, Category = "General", meta = (ClampMin = "1", ClampMax = "32"))
	int32 NumPins;

	UPROPERTY(EditAnywhere, Category = "General")
	ESlewConfigurableMode SlewMode;

	virtual TInstancedStruct<FMetasoundFrontendClassInterface>
	OverrideDefaultInterface(const FMetasoundFrontendClass& InClass) const override;

	virtual TSharedPtr<const Metasound::IOperatorData>
	GetOperatorData() const override;
};

struct FSlewConfigurableOperatorData : public Metasound::TOperatorData<FSlewConfigurableOperatorData>
{
	static const FLazyName OperatorDataTypeName;
	ESlewConfigurableMode Mode;
	int32                 NumPins;

	FSlewConfigurableOperatorData(ESlewConfigurableMode InMode, int32 InNum)
		: Mode(InMode), NumPins(InNum)
	{}
};