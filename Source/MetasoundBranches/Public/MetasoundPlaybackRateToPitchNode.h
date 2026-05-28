// Copyright 2026 Charles Matthews. All Rights Reserved.

#pragma once

#include "MetasoundFrontendDocument.h"
#include "MetasoundOperatorData.h"

#include "MetasoundPlaybackRateToPitchNode.generated.h"

UENUM()
enum class EMetaSoundPlaybackRateToPitchTuningMode : uint8
{
	EqualTemperament12 UMETA(DisplayName = "12-TET"),
	EDO UMETA(DisplayName = "EDO"),
	TuningCents UMETA(DisplayName = "Tuning Cents")
};

namespace Metasound::PlaybackRateToPitchPrivate
{
	class FPlaybackRateToPitchOperatorData;
}

USTRUCT()
struct FMetaSoundPlaybackRateToPitchNodeConfiguration : public FMetaSoundFrontendNodeConfiguration
{
	GENERATED_BODY()

	FMetaSoundPlaybackRateToPitchNodeConfiguration();

	UPROPERTY(EditAnywhere, Category = General)
	EMetaSoundPlaybackRateToPitchTuningMode TuningMode = EMetaSoundPlaybackRateToPitchTuningMode::EqualTemperament12;

	virtual TInstancedStruct<FMetasoundFrontendClassInterface> OverrideDefaultInterface(const FMetasoundFrontendClass& InClass) const override;

	virtual TSharedPtr<const Metasound::IOperatorData> GetOperatorData() const override;

private:
	mutable TSharedPtr<Metasound::PlaybackRateToPitchPrivate::FPlaybackRateToPitchOperatorData> OperatorData;
};
