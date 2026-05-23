// Copyright 2026 Charles Matthews. All Rights Reserved.

#pragma once

#include "MetasoundFrontendDocument.h"
#include "MetasoundNode.h"
#include "MetasoundOperatorData.h"
#include "MetasoundPrimitives.h"
#include "MetasoundTrigger.h"
#include "MetasoundClockDividerNode.generated.h"

USTRUCT()
struct FMetaSoundClockDividerNodeConfiguration : public FMetaSoundFrontendNodeConfiguration
{
	GENERATED_BODY()

	FMetaSoundClockDividerNodeConfiguration();

	/* How many output pins (1-32). */
	UPROPERTY(EditAnywhere, Category = "General", meta = (ClampMin = "1", ClampMax = "32"))
	int32 NumDivisions;

	/* Added to 1-based index before applying multiplier. */
	UPROPERTY(EditAnywhere, Category = "General", meta = (ClampMin = "0", ClampMax = "128"))
	int32 Offset;

	/* Multiplies the index (after offset) to yield each division value. */
	UPROPERTY(EditAnywhere, Category = "General", meta = (ClampMin = "1", ClampMax = "128"))
	int32 Multiplier;

	virtual TInstancedStruct<FMetasoundFrontendClassInterface>
	OverrideDefaultInterface(const FMetasoundFrontendClass& InClass) const override;

	virtual TSharedPtr<const Metasound::IOperatorData> GetOperatorData() const override;
};