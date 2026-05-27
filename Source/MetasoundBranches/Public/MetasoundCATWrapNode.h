// Copyright 2026 Charles Matthews. All Rights Reserved.

#pragma once

#include "MetasoundFrontendDocument.h"
#include "MetasoundOperatorData.h"
#include "UObject/PropertyText.h"

#include "MetasoundCATWrapNode.generated.h"

UENUM()
enum class EMetaSoundCATWrapControlMode : uint8
{
	CAT UMETA(DisplayName = "CAT"),
	MonoAudio UMETA(DisplayName = "Mono Audio"),
	Float UMETA(DisplayName = "Float"),
	FloatArray UMETA(DisplayName = "Float Array")
};

UCLASS()
class UMetaSoundCATWrapNodeOptionsHelper : public UObject
{
	GENERATED_BODY()

	UFUNCTION()
	static TArray<FPropertyTextFName> GetSoundFileFormatChannelOptions();
};

namespace Metasound::CATWrapPrivate
{
	class FCATWrapOperatorData;
}

USTRUCT()
struct FMetaSoundCATWrapNodeConfiguration : public FMetaSoundFrontendNodeConfiguration
{
	GENERATED_BODY()

	FMetaSoundCATWrapNodeConfiguration();

	UPROPERTY(EditAnywhere, Category = General, meta = (GetOptions = "MetaSoundCATWrapNodeOptionsHelper.GetSoundFileFormatChannelOptions"))
	FName CatAudioTypeName = TEXT("Cat:Stereo2Dot0");

	UPROPERTY(EditAnywhere, Category = General)
	EMetaSoundCATWrapControlMode HighMode = EMetaSoundCATWrapControlMode::Float;

	UPROPERTY(EditAnywhere, Category = General)
	EMetaSoundCATWrapControlMode LowMode = EMetaSoundCATWrapControlMode::Float;

	virtual TInstancedStruct<FMetasoundFrontendClassInterface> OverrideDefaultInterface(const FMetasoundFrontendClass& InClass) const override;

	virtual TSharedPtr<const Metasound::IOperatorData> GetOperatorData() const override;

private:
	mutable TSharedPtr<Metasound::CATWrapPrivate::FCATWrapOperatorData> OperatorData;
};
