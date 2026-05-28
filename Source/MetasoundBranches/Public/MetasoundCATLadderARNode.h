// Copyright 2026 Charles Matthews. All Rights Reserved.

#pragma once

#include "MetasoundFrontendDocument.h"
#include "MetasoundOperatorData.h"
#include "UObject/PropertyText.h"

#include "MetasoundCATLadderARNode.generated.h"

UENUM()
enum class EMetaSoundCATLadderARControlMode : uint8
{
	CAT UMETA(DisplayName = "CAT"),
	MonoAudio UMETA(DisplayName = "Mono Audio"),
	Float UMETA(DisplayName = "Float"),
	FloatArray UMETA(DisplayName = "Float Array")
};

UCLASS()
class UMetaSoundCATLadderARNodeOptionsHelper : public UObject
{
	GENERATED_BODY()

	UFUNCTION()
	static TArray<FPropertyTextFName> GetSoundFileFormatChannelOptions();
};

namespace Metasound::CATLadderARPrivate
{
	class FCATLadderAROperatorData;
}

USTRUCT()
struct FMetaSoundCATLadderARNodeConfiguration : public FMetaSoundFrontendNodeConfiguration
{
	GENERATED_BODY()

	FMetaSoundCATLadderARNodeConfiguration();

	UPROPERTY(EditAnywhere, Category = General, meta = (GetOptions = "MetaSoundCATLadderARNodeOptionsHelper.GetSoundFileFormatChannelOptions"))
	FName CatAudioTypeName = TEXT("Cat:Stereo2Dot0");

	UPROPERTY(EditAnywhere, Category = General)
	EMetaSoundCATLadderARControlMode CutoffMode = EMetaSoundCATLadderARControlMode::Float;

	UPROPERTY(EditAnywhere, Category = General)
	EMetaSoundCATLadderARControlMode ResonanceMode = EMetaSoundCATLadderARControlMode::Float;

	virtual TInstancedStruct<FMetasoundFrontendClassInterface> OverrideDefaultInterface(const FMetasoundFrontendClass& InClass) const override;

	virtual TSharedPtr<const Metasound::IOperatorData> GetOperatorData() const override;

private:
	mutable TSharedPtr<Metasound::CATLadderARPrivate::FCATLadderAROperatorData> OperatorData;
};
