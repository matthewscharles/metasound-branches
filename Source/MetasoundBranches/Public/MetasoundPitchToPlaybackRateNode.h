// Copyright 2026 Charles Matthews. All Rights Reserved.

#pragma once

#include "MetasoundFrontendDocument.h"
#include "MetasoundOperatorData.h"

#include "MetasoundPitchToPlaybackRateNode.generated.h"

UENUM()
enum class EMetaSoundPitchToPlaybackRateTuningMode : uint8
{
	EqualTemperament12 UMETA(DisplayName = "12-TET"),
	EDO UMETA(DisplayName = "EDO"),
	TuningCents UMETA(DisplayName = "Tuning Cents")
};

namespace Metasound::PitchToPlaybackRatePrivate
{
	class FPitchToPlaybackRateOperatorData;
}

USTRUCT()
struct FMetaSoundPitchToPlaybackRateNodeConfiguration : public FMetaSoundFrontendNodeConfiguration
{
	GENERATED_BODY()

	FMetaSoundPitchToPlaybackRateNodeConfiguration();

	UPROPERTY(EditAnywhere, Category = General)
	EMetaSoundPitchToPlaybackRateTuningMode TuningMode = EMetaSoundPitchToPlaybackRateTuningMode::EqualTemperament12;

	virtual TInstancedStruct<FMetasoundFrontendClassInterface> OverrideDefaultInterface(const FMetasoundFrontendClass& InClass) const override;

	virtual TSharedPtr<const Metasound::IOperatorData> GetOperatorData() const override;

private:
	mutable TSharedPtr<Metasound::PitchToPlaybackRatePrivate::FPitchToPlaybackRateOperatorData> OperatorData;
};
