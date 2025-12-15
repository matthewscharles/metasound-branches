// Copyright 2025 Charles Matthews. All Rights Reserved.

#pragma once

#include "MetasoundFrontendDocument.h"
#include "Templates/SharedPointer.h"

#include "MetasoundCatPhasedLFONode.generated.h"

USTRUCT()
struct FMetaSoundCatPhasedLFONodeConfiguration : public FMetaSoundFrontendNodeConfiguration
{
	GENERATED_BODY()

	FMetaSoundCatPhasedLFONodeConfiguration() = default;

	UPROPERTY(
		EditAnywhere,
		Category = General,
		meta = (GetOptions="CatFormatOptionsHelper.GetCatFormatOptions")
	)
	FName ToType = TEXT("Stereo");

	virtual TInstancedStruct<FMetasoundFrontendClassInterface>
	OverrideDefaultInterface(const FMetasoundFrontendClass& InNodeClass) const override;

	virtual TSharedPtr<const Metasound::IOperatorData>
	GetOperatorData() const override;
};

#pragma once

