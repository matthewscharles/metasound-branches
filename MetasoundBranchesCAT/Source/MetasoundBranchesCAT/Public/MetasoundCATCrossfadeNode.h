// Copyright 2026 Charles Matthews. All Rights Reserved.

#pragma once

#include "MetasoundFrontendDocument.h"
#include "MetasoundOperatorData.h"
#include "UObject/PropertyText.h"

#include "MetasoundCATCrossfadeNode.generated.h"

UENUM()
enum class EMetaSoundCATCrossfadeInputMode : uint8
{
	CAT UMETA(DisplayName = "CAT"),
	MonoAudio UMETA(DisplayName = "Mono Audio"),
	Float UMETA(DisplayName = "Float"),
	FloatArray UMETA(DisplayName = "Float Array")
};

UENUM()
enum class EMetaSoundCATCrossfadePanningLaw : uint8
{
	EqualPower UMETA(DisplayName = "Equal Power"),
	Linear UMETA(DisplayName = "Linear")
};

UCLASS()
class UMetaSoundCATCrossfadeNodeOptionsHelper : public UObject
{
	GENERATED_BODY()

	UFUNCTION()
	static TArray<FPropertyTextFName> GetSoundFileFormatChannelOptions();
};

namespace Metasound::CATCrossfadePrivate
{
	class FCATCrossfadeOperatorData;
}

USTRUCT()
struct FMetaSoundCATCrossfadeNodeConfiguration : public FMetaSoundFrontendNodeConfiguration
{
	GENERATED_BODY()

	FMetaSoundCATCrossfadeNodeConfiguration();

	UPROPERTY(EditAnywhere, Category = General, meta = (GetOptions = "MetaSoundCATCrossfadeNodeOptionsHelper.GetSoundFileFormatChannelOptions"))
	FName CatAudioTypeName = TEXT("Cat:Stereo2Dot0");

	UPROPERTY(EditAnywhere, Category = General)
	EMetaSoundCATCrossfadeInputMode CrossfadeMode = EMetaSoundCATCrossfadeInputMode::Float;

	UPROPERTY(EditAnywhere, Category = General)
	float CrossfadeMin = -1.0f;

	UPROPERTY(EditAnywhere, Category = General)
	float CrossfadeMax = 1.0f;

	UPROPERTY(EditAnywhere, Category = General)
	EMetaSoundCATCrossfadePanningLaw PanningLaw = EMetaSoundCATCrossfadePanningLaw::EqualPower;

	virtual TInstancedStruct<FMetasoundFrontendClassInterface> OverrideDefaultInterface(const FMetasoundFrontendClass& InClass) const override;

	virtual TSharedPtr<const Metasound::IOperatorData> GetOperatorData() const override;

private:
	mutable TSharedPtr<Metasound::CATCrossfadePrivate::FCATCrossfadeOperatorData> OperatorData;
};
