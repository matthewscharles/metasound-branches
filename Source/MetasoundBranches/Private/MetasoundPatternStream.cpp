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
}