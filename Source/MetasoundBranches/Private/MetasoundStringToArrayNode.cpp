// Copyright 2025 Charles Matthews. All Rights Reserved.



#include "MetasoundBranches/Public/MetasoundStringToArrayNode.h"
#include "MetasoundNodeRegistrationMacro.h"

namespace Metasound
{
    template class TArraySplitOperator<int32>;
    template class TArraySplitNode<int32>;

    template class TArraySplitOperator<float>;
    template class TArraySplitNode<float>;

    template class TArraySplitOperator<bool>;
    template class TArraySplitNode<bool>;

    // template class TArraySplitOperator<FString>;
    // template class TArraySplitNode<FString>;
}

using FStringToArrayNodeInt32Alias  = Metasound::TArraySplitNode<int32>;
using FStringToArrayNodeFloatAlias  = Metasound::TArraySplitNode<float>;
using FStringToArrayNodeBoolAlias   = Metasound::TArraySplitNode<bool>;
// using FStringToArrayNodeStringAlias = Metasound::TArraySplitNode<FString>;

METASOUND_REGISTER_NODE(FStringToArrayNodeInt32Alias);
METASOUND_REGISTER_NODE(FStringToArrayNodeFloatAlias);
METASOUND_REGISTER_NODE(FStringToArrayNodeBoolAlias);
// METASOUND_REGISTER_NODE(FStringToArrayNodeStringAlias);