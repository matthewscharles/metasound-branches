// Copyright 2025 Charles Matthews. All Rights Reserved.

#pragma once

#include "Metasound.h"
#include "MetasoundFacade.h"

namespace Metasound::MetasoundBranches
{
    class FZeroCrossingNode : public FNodeFacade
    {
    public:
        static FNodeClassMetadata CreateNodeClassMetadata();

        FZeroCrossingNode(FNodeData InNodeData,
                          TSharedRef<const FNodeClassMetadata> InClassMetadata);
    };
}