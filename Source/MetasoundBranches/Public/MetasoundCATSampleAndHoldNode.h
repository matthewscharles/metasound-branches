// Copyright 2026 Charles Matthews. All Rights Reserved.

#pragma once

#include "MetasoundFrontendDocument.h"
#include "MetasoundOperatorData.h"
#include "UObject/PropertyText.h"

#include "MetasoundCATSampleAndHoldNode.generated.h"

UENUM()
enum class EMetaSoundCATSampleAndHoldTriggerMode : uint8
{
	Trigger UMETA(DisplayName = "Trigger Inputs"),
	AudioCAT UMETA(DisplayName = "Audio Trigger Over CAT")
};

UCLASS()
class UMetaSoundCATSampleAndHoldNodeOptionsHelper : public UObject
{
	GENERATED_BODY()

	UFUNCTION()
	static TArray<FPropertyTextFName> GetSoundFileFormatChannelOptions();
};

namespace Metasound::CATSampleAndHoldPrivate
{
	class FCATSampleAndHoldOperatorData;
}

USTRUCT()
struct FMetaSoundCATSampleAndHoldNodeConfiguration : public FMetaSoundFrontendNodeConfiguration
{
	GENERATED_BODY()

	FMetaSoundCATSampleAndHoldNodeConfiguration();

	UPROPERTY(EditAnywhere, Category = General, meta = (GetOptions = "MetaSoundCATSampleAndHoldNodeOptionsHelper.GetSoundFileFormatChannelOptions"))
	FName CatAudioTypeName = TEXT("Cat:Stereo2Dot0");

	UPROPERTY(EditAnywhere, Category = General)
	EMetaSoundCATSampleAndHoldTriggerMode TriggerMode = EMetaSoundCATSampleAndHoldTriggerMode::Trigger;

	virtual TInstancedStruct<FMetasoundFrontendClassInterface> OverrideDefaultInterface(const FMetasoundFrontendClass& InClass) const override;

	virtual TSharedPtr<const Metasound::IOperatorData> GetOperatorData() const override;

private:
	mutable TSharedPtr<Metasound::CATSampleAndHoldPrivate::FCATSampleAndHoldOperatorData> OperatorData;
};
