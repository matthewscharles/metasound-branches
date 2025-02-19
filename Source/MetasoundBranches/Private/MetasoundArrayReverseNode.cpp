// Copyright 2025 Charles Matthews. All Rights Reserved.

#pragma once
#include "MetasoundBranches/Public/MetasoundArrayReverseNode.h"
#include "MetasoundNodeRegistrationMacro.h"


namespace Metasound
{
    template class TArrayReverseOperator<TArray<float>>;
    template class TArrayReverseNode<TArray<float>>;
    template class TArrayReverseOperator<TArray<int32>>;
    template class TArrayReverseNode<TArray<int32>>;
}

// alias workaround
using FReverseFloatNodeAlias = Metasound::TArrayReverseNode<TArray<float>>;
using FReverseIntNodeAlias   = Metasound::TArrayReverseNode<TArray<int32>>;

METASOUND_REGISTER_NODE(FReverseFloatNodeAlias);
METASOUND_REGISTER_NODE(FReverseIntNodeAlias);