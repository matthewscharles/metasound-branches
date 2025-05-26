// Copyright 2025 Charles Matthews. All Rights Reserved.

#pragma once

#include "MetasoundFrontendDocument.h"
#include "MetasoundOperatorData.h"
#include "MetasoundTime.h"
#include "MetasoundDataReference.h"
#include "MetasoundNode.h"

#include "MetasoundSlewConfigurableNode.generated.h"

UENUM()
enum class ESlewMode : uint8
{
    Control UMETA(DisplayName = "Float"),
    Audio   UMETA(DisplayName = "Audio")
};

USTRUCT()
struct FMetaSoundSlewNodeConfiguration : public FMetaSoundFrontendNodeConfiguration
{
    GENERATED_BODY()

    FMetaSoundSlewNodeConfiguration()
        : NumPins(1)
        , SlewMode(ESlewMode::Audio) 
    {}

    UPROPERTY(EditAnywhere, Category = "General", meta = (ClampMin = "1", ClampMax = "32"))
    int32 NumPins;

    UPROPERTY(EditAnywhere, Category = "General")
    ESlewMode SlewMode;

    virtual TInstancedStruct<FMetasoundFrontendClassInterface>
    OverrideDefaultInterface(const FMetasoundFrontendClass& InClass) const override;

    virtual TSharedPtr<const Metasound::IOperatorData>
    GetOperatorData() const override;
};


struct FSlewOperatorData : public Metasound::TOperatorData<FSlewOperatorData>
{
    static const FLazyName OperatorDataTypeName;
    ESlewMode Mode;
    int32     NumPins;

    FSlewOperatorData(ESlewMode InMode, int32 InNum)
        : Mode(InMode), NumPins(InNum) {}
};