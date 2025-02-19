// Copyright 2025 Charles Matthews. All Rights Reserved.

#pragma once

#include "MetasoundTime.h"

namespace Metasound
{
    namespace MetasoundArrayNodesPrivate
    {
        template<typename T>
        struct TArrayElementType;  // Forward declaration

        template<>
        struct TArrayElementType<TArray<FTime>>
        {
            using Type = FTime;
        };
    }
}