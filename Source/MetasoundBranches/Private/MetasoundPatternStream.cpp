// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundPatternStream.h"
#include "MetasoundFrontendRegistries.h"

DEFINE_LOG_CATEGORY(LogPatternStream);

namespace MetasoundPattern
{
    void FPatternStream::AddEvent(const FPatternEvent& Event)
    {
        EventsInBlock.Add(Event);
    }

    void FPatternStream::InsertEvent(const FPatternEvent& Event)
    {
        EventsInBlock.Insert(Event, 0);
    }

    FPatternEvent FPatternStream::GetLatestEvent() const
    {
        return EventsInBlock.Num() > 0 ? EventsInBlock.Last() : FPatternEvent();
    }

    TArray<FPatternEvent> FPatternStream::GetEventsUpToFrame(int32 FrameIndex) const
    {
        TArray<FPatternEvent> FilteredEvents;
        for (const FPatternEvent& Event : EventsInBlock)
        {
            if (Event.BlockSampleFrameIndex <= FrameIndex)
            {
                FilteredEvents.Add(Event);
            }
        }
        return FilteredEvents;
    }
}

REGISTER_METASOUND_DATATYPE(MetasoundPattern::FPatternStream);