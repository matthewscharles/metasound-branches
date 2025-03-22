// Copyright 2025 Charles Matthews. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MetasoundDataReference.h"
#include "MetasoundDataReferenceMacro.h"
#include "MetasoundOperatorSettings.h"
#include "MetasoundFrontendRegistries.h"
#include "MetasoundVariable.h"
#include "Logging/LogMacros.h"
#include "MetasoundBranches.h"

DECLARE_LOG_CATEGORY_EXTERN(LogKbmData, Log, All);

namespace MetasoundKbm
{
    struct METASOUNDBRANCHES_API FKbmData
    {
        int32 MapSize = 0;
        int32 FirstNote = 0;
        int32 LastNote = 127;
        int32 MiddleNote = 60;
        int32 ReferenceNote = 69;
        float ReferenceFrequency = 440.0f;
        int32 OctaveDegree = 12;
        float PeriodRatio = 2.0f;

        TArray<int32> ScaleDegrees;
        TArray<float> CentValues;
    };
}

// Register the struct as a MetaSound data reference type
DECLARE_METASOUND_DATA_REFERENCE_TYPES(
	MetasoundKbm::FKbmData,
	METASOUNDBRANCHES_API,
	FKbmDataTypeInfo,
	FKbmDataReadRef,
	FKbmDataWriteRef
);

REGISTER_METASOUND_DATATYPE(MetasoundKbm::FKbmData, "KbmData");
