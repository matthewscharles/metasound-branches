// Copyright 2026 Charles Matthews. All Rights Reserved.

#pragma once


namespace Metasound
{
    inline float PerformFold(float InSample, float Low, float High)
    {
        // Swap Low and High without branching (if required)
        float NewLow  = FMath::Min(Low, High);
        float NewHigh = FMath::Max(Low, High);

        float Range = NewHigh - NewLow;
        if (Range <= 0.0f)
        {
            return InSample; // Invalid range, return input as-is
        }

        float Folded = InSample;

        if (Folded > NewHigh || Folded < NewLow)
        {
            float Diff = (Folded > NewHigh) ? (Folded - NewHigh) : (NewLow - Folded);
            int Mag = static_cast<int>(Diff / Range);

            float Modded = fmodf(Diff, Range);
            if (Modded < 0.0f)
            {
                Modded += Range;
            }

            if (Mag % 2 == 0)
            {
                Folded = (Folded > NewHigh) ? (NewHigh - Modded) : (NewLow + Modded);
            }
            else
            {
                Folded = (Folded > NewHigh) ? (NewLow + Modded) : (NewHigh - Modded);
            }
        }

        return Folded;
    }
}