// Copyright 2025 Charles Matthews. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"
#include "MetasoundDataReference.h"
#include "MetasoundOperatorSettings.h"
#include "MetasoundVariable.h"
#include "MetasoundBranches.h"

DECLARE_LOG_CATEGORY_EXTERN(LogPatternStream, Log, All);

namespace MetasoundPattern
{
    struct METASOUNDBRANCHES_API FPatternEvent
    {
        int32 BlockSampleFrameIndex = 0;
        float ControlValue = 0.0f;

        FPatternEvent() = default;

        FPatternEvent(int32 InFrameIndex, float InValue)
            : BlockSampleFrameIndex(InFrameIndex)
            , ControlValue(InValue)
        {}
    };

    class METASOUNDBRANCHES_API FPatternStream
    {
    public:
        FPatternStream() = default;
        FPatternStream(const FPatternStream&) = default;
        FPatternStream& operator=(const FPatternStream&) = default;
        FPatternStream(FPatternStream&&) = default;  // * Move constructor (?)
        FPatternStream& operator=(FPatternStream&&) = default;  // * Move assignment operator (?)

        void AddEvent(const FPatternEvent& Event);
        void InsertEvent(const FPatternEvent& Event);

        FPatternEvent GetLatestEvent() const;
        TArray<FPatternEvent> GetEventsUpToFrame(int32 FrameIndex) const;

        const TArray<FPatternEvent>& GetEventsInBlock() const
        { 
            return EventsInBlock; 
        }
        
    private:
        TArray<FPatternEvent> EventsInBlock;
    };
}

DECLARE_METASOUND_DATA_REFERENCE_TYPES(
    MetasoundPattern::FPatternStream, 
    METASOUNDBRANCHES_API, 
    FPatternStreamTypeInfo, 
    FPatternStreamReadRef, 
    FPatternStreamWriteRef
)