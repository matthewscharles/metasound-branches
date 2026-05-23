// Copyright 2026 Charles Matthews. All Rights Reserved.

#pragma once

#include "Math/UnrealMathUtility.h"

namespace Metasound
{
    
    inline float PerformWrap(float InSample, float Low, float High)
    {
        // Ensure Low <= High
        float NewLow  = FMath::Min(Low, High);
        float NewHigh = FMath::Max(Low, High);

        float Range = NewHigh - NewLow;
        if (Range <= 0.0f)
        {
            // Invalid or zero range, do nothing
            return InSample;
        }

        // Shift input so that 'NewLow' is zero
        float Shifted = InSample - NewLow;

        // Wrap into [0, Range)
        float Wrapped = fmodf(Shifted, Range);

        // fmodf can yield negative values if Shifted < 0
        if (Wrapped < 0.0f)
        {
            Wrapped += Range;
        }

        // Shift back into [NewLow, NewHigh]
        return Wrapped + NewLow;
    }
}