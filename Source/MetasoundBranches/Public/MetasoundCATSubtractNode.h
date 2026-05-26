// Copyright 2026 Charles Matthews. All Rights Reserved.

#pragma once

#include "MetasoundFrontendDocument.h"
#include "MetasoundOperatorData.h"
#include "UObject/PropertyText.h"

#include "MetasoundCATSubtractNode.generated.h"

UENUM()
enum class EMetaSoundCATSubtractInputBMode : uint8
{
	CAT UMETA(DisplayName = "CAT"),
	MonoAudio UMETA(DisplayName = "Mono Audio"),
	Float UMETA(DisplayName = "Float")
};

UCLASS()
class UMetaSoundCATSubtractNodeOptionsHelper : public UObject
{
	GENERATED_BODY()

	UFUNCTION()
	static TArray<FPropertyTextFName> GetSoundFileFormatChannelOptions();
};

namespace Metasound::CATSubtractPrivate
{
	class FCATSubtractOperatorData;
}

USTRUCT()
struct FMetaSoundCATSubtractNodeConfiguration : public FMetaSoundFrontendNodeConfiguration
{
	GENERATED_BODY()

	FMetaSoundCATSubtractNodeConfiguration();

	UPROPERTY(EditAnywhere, Category = General, meta = (GetOptions = "MetaSoundCATSubtractNodeOptionsHelper.GetSoundFileFormatChannelOptions"))
	FName CatAudioTypeName = TEXT("Cat:Stereo2Dot0");

	UPROPERTY(EditAnywhere, Category = General)
	EMetaSoundCATSubtractInputBMode InputBMode = EMetaSoundCATSubtractInputBMode::MonoAudio;

	virtual TInstancedStruct<FMetasoundFrontendClassInterface> OverrideDefaultInterface(const FMetasoundFrontendClass& InClass) const override;

	virtual TSharedPtr<const Metasound::IOperatorData> GetOperatorData() const override;

private:
	mutable TSharedPtr<Metasound::CATSubtractPrivate::FCATSubtractOperatorData> OperatorData;
};
