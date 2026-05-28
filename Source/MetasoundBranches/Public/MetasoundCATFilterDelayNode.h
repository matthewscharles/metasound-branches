// Copyright 2026 Charles Matthews. All Rights Reserved.

#pragma once

#include "MetasoundFrontendDocument.h"
#include "MetasoundOperatorData.h"
#include "UObject/PropertyText.h"

#include "MetasoundCATFilterDelayNode.generated.h"

UENUM()
enum class EMetaSoundCATFilterDelayControlMode : uint8
{
	CAT UMETA(DisplayName = "CAT"),
	MonoAudio UMETA(DisplayName = "Mono Audio"),
	Float UMETA(DisplayName = "Float"),
	FloatArray UMETA(DisplayName = "Float Array")
};

UENUM()
enum class EMetaSoundCATFilterDelayFilterControlMode : uint8
{
	Float UMETA(DisplayName = "Float"),
	FloatArray UMETA(DisplayName = "Float Array")
};

UENUM()
enum class EMetaSoundCATFilterDelayMultichannelBehaviour : uint8
{
	Linked UMETA(DisplayName = "Linked"),
	Unlinked UMETA(DisplayName = "Unlinked"),
	PingPong UMETA(DisplayName = "Ping Pong")
};

UCLASS()
class UMetaSoundCATFilterDelayNodeOptionsHelper : public UObject
{
	GENERATED_BODY()

	UFUNCTION()
	static TArray<FPropertyTextFName> GetSoundFileFormatChannelOptions();
};

namespace Metasound::CATFilterDelayPrivate
{
	class FCATFilterDelayOperatorData;
}

USTRUCT()
struct FMetaSoundCATFilterDelayNodeConfiguration : public FMetaSoundFrontendNodeConfiguration
{
	GENERATED_BODY()

	FMetaSoundCATFilterDelayNodeConfiguration();

	UPROPERTY(EditAnywhere, Category = General, meta = (GetOptions = "MetaSoundCATFilterDelayNodeOptionsHelper.GetSoundFileFormatChannelOptions"))
	FName CatAudioTypeName = TEXT("Cat:Stereo2Dot0");

	UPROPERTY(EditAnywhere, Category = General)
	EMetaSoundCATFilterDelayControlMode DelayMode = EMetaSoundCATFilterDelayControlMode::Float;

	UPROPERTY(EditAnywhere, Category = General)
	EMetaSoundCATFilterDelayControlMode FeedbackMode = EMetaSoundCATFilterDelayControlMode::Float;

	UPROPERTY(EditAnywhere, Category = General)
	EMetaSoundCATFilterDelayFilterControlMode CutoffMode = EMetaSoundCATFilterDelayFilterControlMode::Float;

	UPROPERTY(EditAnywhere, Category = General)
	EMetaSoundCATFilterDelayFilterControlMode BandwidthMode = EMetaSoundCATFilterDelayFilterControlMode::Float;

	UPROPERTY(EditAnywhere, Category = General)
	EMetaSoundCATFilterDelayFilterControlMode GainMode = EMetaSoundCATFilterDelayFilterControlMode::Float;

	UPROPERTY(EditAnywhere, Category = General)
	EMetaSoundCATFilterDelayMultichannelBehaviour MultichannelBehaviour = EMetaSoundCATFilterDelayMultichannelBehaviour::Unlinked;

	UPROPERTY(EditAnywhere, Category = General)
	bool bAllowUnityFeedback = false;

	UPROPERTY(EditAnywhere, Category = General)
	bool bEnableDelaySlew = false;

	UPROPERTY(EditAnywhere, Category = General, meta = (ClampMin = "0.01", ClampMax = "10.0"))
	float MaxDelaySeconds = 2.0f;

	virtual TInstancedStruct<FMetasoundFrontendClassInterface> OverrideDefaultInterface(const FMetasoundFrontendClass& InClass) const override;

	virtual TSharedPtr<const Metasound::IOperatorData> GetOperatorData() const override;

private:
	mutable TSharedPtr<Metasound::CATFilterDelayPrivate::FCATFilterDelayOperatorData> OperatorData;
};
