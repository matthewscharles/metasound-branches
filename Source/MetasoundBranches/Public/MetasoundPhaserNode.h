// Copyright 2026 Charles Matthews. All Rights Reserved.

#pragma once

#include "MetasoundFrontendDocument.h"
#include "MetasoundOperatorData.h"

#include "MetasoundPhaserNode.generated.h"

namespace Metasound::PhaserPrivate
{
    class FPhaserOperatorData;
}

USTRUCT()
struct FMetaSoundPhaserNodeConfiguration : public FMetaSoundFrontendNodeConfiguration
{
    GENERATED_BODY()

    FMetaSoundPhaserNodeConfiguration();

    UPROPERTY(EditAnywhere, Category = General, meta = (ClampMin = "1", ClampMax = "32"))
    int32 NumPoles = 6;

    virtual TInstancedStruct<FMetasoundFrontendClassInterface> OverrideDefaultInterface(const FMetasoundFrontendClass& InClass) const override;

    virtual TSharedPtr<const Metasound::IOperatorData> GetOperatorData() const override;

private:
    mutable TSharedPtr<Metasound::PhaserPrivate::FPhaserOperatorData> OperatorData;
};
