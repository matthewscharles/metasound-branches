// Copyright 2026 Charles Matthews. All Rights Reserved.

#pragma once

#include "MetasoundFrontendDocument.h"
#include "MetasoundOperatorData.h"
#include "UObject/PropertyText.h"

#include "MetasoundCATBiquadARNode.generated.h"

UENUM()
enum class EMetaSoundCATBiquadARControlMode : uint8
{
	CAT UMETA(DisplayName = "CAT"),
	MonoAudio UMETA(DisplayName = "Mono Audio"),
	Float UMETA(DisplayName = "Float"),
	FloatArray UMETA(DisplayName = "Float Array")
};

UCLASS()
class UMetaSoundCATBiquadARNodeOptionsHelper : public UObject
{
	GENERATED_BODY()

	UFUNCTION()
	static TArray<FPropertyTextFName> GetSoundFileFormatChannelOptions();
};

namespace Metasound::CATBiquadARPrivate
{
	class FCATBiquadAROperatorData;
}

USTRUCT()
struct FMetaSoundCATBiquadARNodeConfiguration : public FMetaSoundFrontendNodeConfiguration
{
	GENERATED_BODY()

	FMetaSoundCATBiquadARNodeConfiguration();

	UPROPERTY(EditAnywhere, Category = General, meta = (GetOptions = "MetaSoundCATBiquadARNodeOptionsHelper.GetSoundFileFormatChannelOptions"))
	FName CatAudioTypeName = TEXT("Cat:Stereo2Dot0");

	UPROPERTY(EditAnywhere, Category = General)
	EMetaSoundCATBiquadARControlMode CutoffMode = EMetaSoundCATBiquadARControlMode::Float;

	UPROPERTY(EditAnywhere, Category = General)
	EMetaSoundCATBiquadARControlMode BandwidthMode = EMetaSoundCATBiquadARControlMode::Float;

	virtual TInstancedStruct<FMetasoundFrontendClassInterface> OverrideDefaultInterface(const FMetasoundFrontendClass& InClass) const override;

	virtual TSharedPtr<const Metasound::IOperatorData> GetOperatorData() const override;

private:
	mutable TSharedPtr<Metasound::CATBiquadARPrivate::FCATBiquadAROperatorData> OperatorData;
};
