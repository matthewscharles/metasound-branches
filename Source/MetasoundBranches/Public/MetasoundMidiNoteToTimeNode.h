// Copyright 2026 Charles Matthews. All Rights Reserved.

#pragma once

#include "MetasoundFrontendDocument.h"
#include "MetasoundOperatorData.h"

#include "MetasoundMidiNoteToTimeNode.generated.h"

UENUM()
enum class EMetaSoundMidiNoteToTimeTuningMode : uint8
{
	EqualTemperament12 UMETA(DisplayName = "12-TET"),
	EDO UMETA(DisplayName = "EDO"),
	TuningCents UMETA(DisplayName = "Tuning Cents")
};

namespace Metasound::MidiNoteToTimePrivate
{
	class FMidiNoteToTimeOperatorData;
}

USTRUCT()
struct FMetaSoundMidiNoteToTimeNodeConfiguration : public FMetaSoundFrontendNodeConfiguration
{
	GENERATED_BODY()

	FMetaSoundMidiNoteToTimeNodeConfiguration();

	UPROPERTY(EditAnywhere, Category = General)
	EMetaSoundMidiNoteToTimeTuningMode TuningMode = EMetaSoundMidiNoteToTimeTuningMode::EqualTemperament12;

	virtual TInstancedStruct<FMetasoundFrontendClassInterface> OverrideDefaultInterface(const FMetasoundFrontendClass& InClass) const override;

	virtual TSharedPtr<const Metasound::IOperatorData> GetOperatorData() const override;

private:
	mutable TSharedPtr<Metasound::MidiNoteToTimePrivate::FMidiNoteToTimeOperatorData> OperatorData;
};
