// Copyright 2026 Charles Matthews. All Rights Reserved.

#pragma once

#include "MetasoundFrontendDocument.h"
#include "MetasoundOperatorData.h"
#include "UObject/PropertyText.h"

#include "MetasoundCATFeedbackDelayNode.generated.h"

UENUM()
enum class EMetaSoundCATFeedbackDelayControlMode : uint8
{
	CAT UMETA(DisplayName = "CAT"),
	MonoAudio UMETA(DisplayName = "Mono Audio"),
	Float UMETA(DisplayName = "Float"),
	FloatArray UMETA(DisplayName = "Float Array")
};

UENUM()
enum class EMetaSoundCATDelayMultichannelBehaviour : uint8
{
	Linked UMETA(DisplayName = "Linked"),
	Unlinked UMETA(DisplayName = "Unlinked"),
	PingPong UMETA(DisplayName = "Ping Pong")
};

UCLASS()
class UMetaSoundCATFeedbackDelayNodeOptionsHelper : public UObject
{
	GENERATED_BODY()

	UFUNCTION()
	static TArray<FPropertyTextFName> GetSoundFileFormatChannelOptions();
};

namespace Metasound::CATFeedbackDelayPrivate
{
	class FCATFeedbackDelayOperatorData;
}

USTRUCT()
struct FMetaSoundCATFeedbackDelayNodeConfiguration : public FMetaSoundFrontendNodeConfiguration
{
	GENERATED_BODY()

	FMetaSoundCATFeedbackDelayNodeConfiguration();

	UPROPERTY(EditAnywhere, Category = General, meta = (GetOptions = "MetaSoundCATFeedbackDelayNodeOptionsHelper.GetSoundFileFormatChannelOptions"))
	FName CatAudioTypeName = TEXT("Cat:Stereo2Dot0");

	UPROPERTY(EditAnywhere, Category = General)
	EMetaSoundCATFeedbackDelayControlMode DelayMode = EMetaSoundCATFeedbackDelayControlMode::Float;

	UPROPERTY(EditAnywhere, Category = General)
	EMetaSoundCATFeedbackDelayControlMode FeedbackMode = EMetaSoundCATFeedbackDelayControlMode::Float;

	UPROPERTY(EditAnywhere, Category = General)
	EMetaSoundCATDelayMultichannelBehaviour MultichannelBehaviour = EMetaSoundCATDelayMultichannelBehaviour::Unlinked;

	UPROPERTY(EditAnywhere, Category = General)
	bool bAllowUnityFeedback = false;

	UPROPERTY(EditAnywhere, Category = General)
	bool bEnableDelaySlew = false;

	UPROPERTY(EditAnywhere, Category = General, meta = (ClampMin = "0.01", ClampMax = "10.0"))
	float MaxDelaySeconds = 2.0f;

	virtual TInstancedStruct<FMetasoundFrontendClassInterface> OverrideDefaultInterface(const FMetasoundFrontendClass& InClass) const override;

	virtual TSharedPtr<const Metasound::IOperatorData> GetOperatorData() const override;

private:
	mutable TSharedPtr<Metasound::CATFeedbackDelayPrivate::FCATFeedbackDelayOperatorData> OperatorData;
};
