#pragma once

#include "CoreMinimal.h"
namespace Metasound
{
    struct FVoiceState
    {
        bool Active = false;
        int32 NoteEndSample = 0;
        float Pitch = 0.0f;
    };
}