// Copyright 2026 Charles Matthews. All Rights Reserved.

#pragma once

#include "MetasoundFrontendDocument.h"
#include "MetasoundOperatorData.h"
#include "UObject/PropertyText.h"

#include "MetasoundCATSawtoothOscillatorNode.generated.h"

UENUM()
enum class EMetaSoundCATSawtoothControlMode : uint8
{
	CAT UMETA(DisplayName = "CAT"),
	MonoAudio UMETA(DisplayName = "Mono Audio"),
	Float UMETA(DisplayName = "Float"),
	FloatArray UMETA(DisplayName = "Float Array")
};

UCLASS()
class UMetaSoundCATSawtoothOscillatorNodeOptionsHelper : public UObject
{
	GENERATED_BODY()

	UFUNCTION()
	static TArray<FPropertyTextFName> GetSoundFileFormatChannelOptions();
};

namespace Metasound::CATSawtoothOscillatorPrivate
{
	class FCATSawtoothOscillatorOperatorData;
}

USTRUCT()
struct FMetaSoundCATSawtoothOscillatorNodeConfiguration : public FMetaSoundFrontendNodeConfiguration
{
	GENERATED_BODY()

	FMetaSoundCATSawtoothOscillatorNodeConfiguration();

	UPROPERTY(EditAnywhere, Category = General, meta = (GetOptions = "MetaSoundCATSawtoothOscillatorNodeOptionsHelper.GetSoundFileFormatChannelOptions"))
	FName CatAudioTypeName = TEXT("Cat:Stereo2Dot0");

	UPROPERTY(EditAnywhere, Category = General)
	EMetaSoundCATSawtoothControlMode FrequencyMode = EMetaSoundCATSawtoothControlMode::Float;

	UPROPERTY(EditAnywhere, Category = General)
	EMetaSoundCATSawtoothControlMode PhaseMode = EMetaSoundCATSawtoothControlMode::Float;

	virtual TInstancedStruct<FMetasoundFrontendClassInterface> OverrideDefaultInterface(const FMetasoundFrontendClass& InClass) const override;

	virtual TSharedPtr<const Metasound::IOperatorData> GetOperatorData() const override;

private:
	mutable TSharedPtr<Metasound::CATSawtoothOscillatorPrivate::FCATSawtoothOscillatorOperatorData> OperatorData;
};
