// Copyright 2025 Charles Matthews. All Rights Reserved.

#pragma once

#include <cstdint>

namespace Metasound
{
    constexpr int PitchIndex(char NoteChar)
    {
        char c = (NoteChar | 0x20);
        switch (c)
        {
            case 'c': return 0;
            case 'd': return 1;
            case 'e': return 2;
            case 'f': return 3;
            case 'g': return 4;
            case 'a': return 5;
            case 'b': return 6;
            default:  return 0;
        }
    }

    constexpr float kPitchOffsets[7] = {
        0.f, 2.f, 4.f, 5.f, 7.f, 9.f, 11.f
    };

    constexpr float kAccidentalOffsets[256] = {
        ['#'] =  1.0f,
        ['b'] = -1.0f,
        ['^'] =  0.5f,
        ['_'] = -0.5f
    };

    constexpr float ParseMidiNote(const char* Note)
    {
        int pIndex = PitchIndex(Note[0]);

        // Start reading accidentals from Note[1].
        int i = 1;

        // Unrolled reading of up to 4 accidentals.
        float a0 = kAccidentalOffsets[(unsigned char)Note[i]];
        i += (a0 != 0.f) ? 1 : 0;

        float a1 = kAccidentalOffsets[(unsigned char)Note[i]];
        i += (a1 != 0.f) ? 1 : 0;

        float a2 = kAccidentalOffsets[(unsigned char)Note[i]];
        i += (a2 != 0.f) ? 1 : 0;

        float a3 = kAccidentalOffsets[(unsigned char)Note[i]];
        i += (a3 != 0.f) ? 1 : 0;

        float accidentalSum = a0 + a1 + a2 + a3;

        bool isNegative = (Note[i] == '-');
        i += isNegative ? 1 : 0;

        // Read the single-digit octave
        int octaveDigit = Note[i] - '0';
        i++;

        int octave = isNegative ? -octaveDigit : octaveDigit;

        // C-2 = 0
        octave += 2;

        float base = kPitchOffsets[pIndex];
        return octave * 12.f + base + accidentalSum;
    }
}