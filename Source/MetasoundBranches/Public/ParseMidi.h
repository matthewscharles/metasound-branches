// Copyright 2025 Charles Matthews. All Rights Reserved.

#pragma once

#include <cstdint>

namespace Metasound
{
    constexpr uint8_t kNoteOffsets[7] = { 0, 2, 4, 5, 7, 9, 11 };

    // direct ascii indexing 
    constexpr int8_t kAccidentalOffsets[256] = {
        ['#'] = 1, ['b'] = -1,
        // avoid garbage values
        ['0'] = 0, ['1'] = 0, ['2'] = 0, ['3'] = 0, ['4'] = 0, ['5'] = 0, ['6'] = 0, ['7'] = 0, ['8'] = 0, ['9'] = 0,
        // null terminator
        ['\0'] = 0, 
    };

    constexpr int ParseMidi(const char* note)
    {
        uint8_t pitchIndex = (note[0] | 0x20) - 'a';

        int accidental = kAccidentalOffsets[note[1]] + kAccidentalOffsets[note[2]];

        int octave = note[1 + (accidental != 0) + (accidental == 2 || accidental == -2)] - '0';

        return 12 + (octave * 12) + kNoteOffsets[pitchIndex] + accidental;
    }
}