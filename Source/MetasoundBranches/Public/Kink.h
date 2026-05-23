// Copyright 2026 Charles Matthews. All Rights Reserved.

#pragma once

namespace Metasound
{
    inline float KinkProcess(float x, float s)
    {
        float y = x * s;
        if (y < 0.5f) return y;
        float slope2 = 0.5f / (1.f - (0.5f / s));
        float b = 0.5f - slope2 * (0.5f / s);
        return slope2 * x + b;
    }
}