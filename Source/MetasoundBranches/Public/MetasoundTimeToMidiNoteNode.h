// Copyright 2026 Charles Matthews. All Rights Reserved.

#pragma once

#include "MetasoundFrontendDocument.h"
#include "MetasoundOperatorData.h"

#include "MetasoundTimeToMidiNoteNode.generated.h"

UENUM()
enum class EMetaSoundTimeToMidiNoteTuningMode : uint8
{
	EqualTemperament12 UMETA(DisplayName = "12-TET"),
	EDO UMETA(DisplayName = "EDO"),
	TuningCents UMETA(DisplayName = "Tuning Cents")
};

namespace Metasound::TimeToMidiNotePrivate
{
	class FTimeToMidiNoteOperatorData;
}

USTRUCT()
struct FMetaSoundTimeToMidiNoteNodeConfiguration : public FMetaSoundFrontendNodeConfiguration
{
	GENERATED_BODY()

	FMetaSoundTimeToMidiNoteNodeConfiguration();

	UPROPERTY(EditAnywhere, Category = General)
	EMetaSoundTimeToMidiNoteTuningMode TuningMode = EMetaSoundTimeToMidiNoteTuningMode::EqualTemperament12;

	virtual TInstancedStruct<FMetasoundFrontendClassInterface> OverrideDefaultInterface(const FMetasoundFrontendClass& InClass) const override;

	virtual TSharedPtr<const Metasound::IOperatorData> GetOperatorData() const override;

private:
	mutable TSharedPtr<Metasound::TimeToMidiNotePrivate::FTimeToMidiNoteOperatorData> OperatorData;
};
