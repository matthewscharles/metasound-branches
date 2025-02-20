// Copyright 2025 Charles Matthews. All Rights Reserved.

#pragma once

#include "MetasoundBranches/Public/MetasoundArrayJoinNode.h"
// #include "MetasoundTime.h"
#include "MetasoundNodeRegistrationMacro.h"

namespace Metasound
{
    template class TArrayJoinOperator<TArray<int32>>;
    template class TArrayJoinNode<TArray<int32>>;

    template class TArrayJoinOperator<TArray<float>>;
    template class TArrayJoinNode<TArray<float>>;

    template class TArrayJoinOperator<TArray<bool>>;
    template class TArrayJoinNode<TArray<bool>>;

    template class TArrayJoinOperator<TArray<FString>>;
    template class TArrayJoinNode<TArray<FString>>;
}

using FArrayJoinNodeInt32Alias = Metasound::TArrayJoinNode<TArray<int32>>;
using FArrayJoinNodeFloatAlias = Metasound::TArrayJoinNode<TArray<float>>;
using FArrayJoinNodeBoolAlias  = Metasound::TArrayJoinNode<TArray<bool>>;
using FArrayJoinNodeStringAlias = Metasound::TArrayJoinNode<TArray<FString>>;

METASOUND_REGISTER_NODE(FArrayJoinNodeInt32Alias);
METASOUND_REGISTER_NODE(FArrayJoinNodeFloatAlias);
METASOUND_REGISTER_NODE(FArrayJoinNodeBoolAlias);
METASOUND_REGISTER_NODE(FArrayJoinNodeStringAlias);