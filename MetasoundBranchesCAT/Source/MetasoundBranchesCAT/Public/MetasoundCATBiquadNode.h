// Copyright 2026 Charles Matthews. All Rights Reserved.

#pragma once

#include "MetasoundFrontendDocument.h"
#include "MetasoundOperatorData.h"
#include "UObject/PropertyText.h"

#include "MetasoundCATBiquadNode.generated.h"

UENUM()
enum class EMetaSoundCATBiquadControlMode : uint8
{
	Float UMETA(DisplayName = "Float"),
	FloatArray UMETA(DisplayName = "Float Array")
};

UCLASS()
class UMetaSoundCATBiquadNodeOptionsHelper : public UObject
{
	GENERATED_BODY()

	UFUNCTION()
	static TArray<FPropertyTextFName> GetSoundFileFormatChannelOptions();
};

namespace Metasound::CATBiquadPrivate
{
	class FCATBiquadOperatorData;
}

USTRUCT()
struct FMetaSoundCATBiquadNodeConfiguration : public FMetaSoundFrontendNodeConfiguration
{
	GENERATED_BODY()

	FMetaSoundCATBiquadNodeConfiguration();

	UPROPERTY(EditAnywhere, Category = General, meta = (GetOptions = "MetaSoundCATBiquadNodeOptionsHelper.GetSoundFileFormatChannelOptions"))
	FName CatAudioTypeName = TEXT("Cat:Stereo2Dot0");

	UPROPERTY(EditAnywhere, Category = General)
	EMetaSoundCATBiquadControlMode CutoffMode = EMetaSoundCATBiquadControlMode::Float;

	UPROPERTY(EditAnywhere, Category = General)
	EMetaSoundCATBiquadControlMode BandwidthMode = EMetaSoundCATBiquadControlMode::Float;

	UPROPERTY(EditAnywhere, Category = General)
	EMetaSoundCATBiquadControlMode GainMode = EMetaSoundCATBiquadControlMode::Float;

	UPROPERTY(EditAnywhere, Category = General)
	bool bEnableCutoffSlew = false;

	UPROPERTY(EditAnywhere, Category = General)
	bool bEnableBandwidthSlew = false;

	UPROPERTY(EditAnywhere, Category = General)
	bool bEnableGainSlew = false;

	virtual TInstancedStruct<FMetasoundFrontendClassInterface> OverrideDefaultInterface(const FMetasoundFrontendClass& InClass) const override;

	virtual TSharedPtr<const Metasound::IOperatorData> GetOperatorData() const override;

private:
	mutable TSharedPtr<Metasound::CATBiquadPrivate::FCATBiquadOperatorData> OperatorData;
};
