// Copyright 2025 Charles Matthews. All Rights Reserved.

#pragma once

#include "MetasoundBranches/Public/MetasoundArrayJoinNode.h"
#include "MetasoundNodeRegistrationMacro.h"

namespace Metasound
{
    template class TArrayJoinOperator<int32>;
    template class TArrayJoinNode<int32>;

    template class TArrayJoinOperator<float>;
    template class TArrayJoinNode<float>;

    template class TArrayJoinOperator<bool>;
    template class TArrayJoinNode<bool>;

    template class TArrayJoinOperator<FString>;
    template class TArrayJoinNode<FString>;
}

using FArrayJoinNodeInt32Alias = Metasound::TArrayJoinNode<int32>;
using FArrayJoinNodeFloatAlias = Metasound::TArrayJoinNode<float>;
using FArrayJoinNodeBoolAlias  = Metasound::TArrayJoinNode<bool>;
using FArrayJoinNodeStringAlias = Metasound::TArrayJoinNode<FString>;

METASOUND_REGISTER_NODE(FArrayJoinNodeInt32Alias);
METASOUND_REGISTER_NODE(FArrayJoinNodeFloatAlias);
METASOUND_REGISTER_NODE(FArrayJoinNodeBoolAlias);
METASOUND_REGISTER_NODE(FArrayJoinNodeStringAlias);