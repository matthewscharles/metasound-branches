// Copyright 2026 Charles Matthews. All Rights Reserved.

#pragma once

#include "MetasoundFrontendDocument.h"
#include "MetasoundNode.h"
#include "MetasoundOperatorData.h"
#include "MetasoundPrimitives.h"
#include "MetasoundTrigger.h"
#include "MetasoundDustNode.generated.h"

USTRUCT()
struct FMetaSoundDustNodeConfiguration : public FMetaSoundFrontendNodeConfiguration
{
    GENERATED_BODY()

    FMetaSoundDustNodeConfiguration();

    UPROPERTY(EditAnywhere, Category = "General", meta = (ClampMin = "1", ClampMax = "32"))
    int32 NumChannels;

    UPROPERTY(EditAnywhere, Category = "Outputs")
    bool bCreateTriggerPinsPerChannel;

    UPROPERTY(EditAnywhere, Category = "Outputs")
    bool bCreateAudioPinsPerChannel;

    virtual TInstancedStruct<FMetasoundFrontendClassInterface>
    OverrideDefaultInterface(const FMetasoundFrontendClass& InClass) const override;

    virtual TSharedPtr<const Metasound::IOperatorData> GetOperatorData() const override;
};