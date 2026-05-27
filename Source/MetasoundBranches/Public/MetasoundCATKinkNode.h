// Copyright 2026 Charles Matthews. All Rights Reserved.

#pragma once

#include "MetasoundFrontendDocument.h"
#include "MetasoundOperatorData.h"
#include "UObject/PropertyText.h"

#include "MetasoundCATKinkNode.generated.h"

UENUM()
enum class EMetaSoundCATKinkControlMode : uint8
{
	CAT UMETA(DisplayName = "CAT"),
	MonoAudio UMETA(DisplayName = "Mono Audio"),
	Float UMETA(DisplayName = "Float"),
	FloatArray UMETA(DisplayName = "Float Array")
};

UCLASS()
class UMetaSoundCATKinkNodeOptionsHelper : public UObject
{
	GENERATED_BODY()

	UFUNCTION()
	static TArray<FPropertyTextFName> GetSoundFileFormatChannelOptions();
};

namespace Metasound::CATKinkPrivate
{
	class FCATKinkOperatorData;
}

USTRUCT()
struct FMetaSoundCATKinkNodeConfiguration : public FMetaSoundFrontendNodeConfiguration
{
	GENERATED_BODY()

	FMetaSoundCATKinkNodeConfiguration();

	UPROPERTY(EditAnywhere, Category = General, meta = (GetOptions = "MetaSoundCATKinkNodeOptionsHelper.GetSoundFileFormatChannelOptions"))
	FName CatAudioTypeName = TEXT("Cat:Stereo2Dot0");

	UPROPERTY(EditAnywhere, Category = General)
	EMetaSoundCATKinkControlMode SlopeMode = EMetaSoundCATKinkControlMode::Float;

	virtual TInstancedStruct<FMetasoundFrontendClassInterface> OverrideDefaultInterface(const FMetasoundFrontendClass& InClass) const override;

	virtual TSharedPtr<const Metasound::IOperatorData> GetOperatorData() const override;

private:
	mutable TSharedPtr<Metasound::CATKinkPrivate::FCATKinkOperatorData> OperatorData;
};
