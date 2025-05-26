// Copyright 2025 Charles Matthews. All Rights Reserved.

#pragma once

#include "MetasoundFrontendNode.h"
#include "MetasoundFrontendDocument.h"
#include "MetasoundNode.h"
#include "MetasoundOperatorData.h"
#include "MetasoundPrimitives.h"
#include "MetasoundTrigger.h"
#include "MetasoundShiftRegisterNode.generated.h"

USTRUCT()
struct FMetaSoundShiftRegisterNodeConfiguration : public FMetaSoundFrontendNodeConfiguration
{
	GENERATED_BODY()

	FMetaSoundShiftRegisterNodeConfiguration();

	UPROPERTY(EditAnywhere, Category = "General", meta = (ClampMin = "1", ClampMax = "32"))
	int32 NumStages;

	virtual TInstancedStruct<FMetasoundFrontendClassInterface>
	OverrideDefaultInterface(const FMetasoundFrontendClass& InClass) const override;

	/* No extra operator data required. */
	virtual TSharedPtr<const Metasound::IOperatorData> GetOperatorData() const override
	{
		return nullptr;
	}
};