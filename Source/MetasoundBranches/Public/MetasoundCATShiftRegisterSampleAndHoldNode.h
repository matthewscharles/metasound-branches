// Copyright 2026 Charles Matthews. All Rights Reserved.

#pragma once

#include "MetasoundFrontendDocument.h"
#include "MetasoundOperatorData.h"
#include "UObject/PropertyText.h"

#include "MetasoundCATShiftRegisterSampleAndHoldNode.generated.h"

UCLASS()
class UMetaSoundCATShiftRegisterSampleAndHoldNodeOptionsHelper : public UObject
{
	GENERATED_BODY()

	UFUNCTION()
	static TArray<FPropertyTextFName> GetSoundFileFormatChannelOptions();
};

namespace Metasound::CATShiftRegisterSampleAndHoldPrivate
{
	class FCATShiftRegisterSampleAndHoldOperatorData;
}

USTRUCT()
struct FMetaSoundCATShiftRegisterSampleAndHoldNodeConfiguration : public FMetaSoundFrontendNodeConfiguration
{
	GENERATED_BODY()

	FMetaSoundCATShiftRegisterSampleAndHoldNodeConfiguration();

	UPROPERTY(EditAnywhere, Category = General, meta = (GetOptions = "MetaSoundCATShiftRegisterSampleAndHoldNodeOptionsHelper.GetSoundFileFormatChannelOptions"))
	FName CatAudioTypeName = TEXT("Cat:Stereo2Dot0");

	virtual TInstancedStruct<FMetasoundFrontendClassInterface> OverrideDefaultInterface(const FMetasoundFrontendClass& InClass) const override;

	virtual TSharedPtr<const Metasound::IOperatorData> GetOperatorData() const override;

private:
	mutable TSharedPtr<Metasound::CATShiftRegisterSampleAndHoldPrivate::FCATShiftRegisterSampleAndHoldOperatorData> OperatorData;
};
