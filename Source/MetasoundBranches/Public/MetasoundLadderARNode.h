// Copyright 2025 Charles Matthews. All Rights Reserved.

#pragma once

#include "MetasoundFrontendDocument.h"
#include "MetasoundOperatorData.h"
#include "MetasoundNode.h"
#include "UObject/ObjectMacros.h"
#include "MetasoundLadderARNode.generated.h"

USTRUCT()
struct FMetaSoundLadderARNodeConfiguration : public FMetaSoundFrontendNodeConfiguration
{
	GENERATED_BODY()

	FMetaSoundLadderARNodeConfiguration()
		: NumChannels(1)
	{
	}

	UPROPERTY(EditAnywhere, Category="General", meta=(ClampMin="1", ClampMax="32"))
	int32 NumChannels;

	virtual TInstancedStruct<FMetasoundFrontendClassInterface>
	OverrideDefaultInterface(const FMetasoundFrontendClass& InClass) const override;

	virtual TSharedPtr<const Metasound::IOperatorData>
	GetOperatorData() const override;
};