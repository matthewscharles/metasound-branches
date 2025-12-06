// Copyright 2025 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundMakeArrayNode.h"
#include "MetasoundNodeRegistrationMacro.h"

namespace Metasound::MetasoundBranches
{
    template class TArrayMakeOperator<int32>;
    template class TArrayMakeNode<int32>;

    template class TArrayMakeOperator<float>;
    template class TArrayMakeNode<float>;

    template class TArrayMakeOperator<bool>;
    template class TArrayMakeNode<bool>;

    template class TArrayMakeOperator<FString>;
    template class TArrayMakeNode<FString>;
}

using FArrayMakeNodeInt32Alias = Metasound::TArrayMakeNode<int32>;
using FArrayMakeNodeFloatAlias = Metasound::TArrayMakeNode<float>;
using FArrayMakeNodeBoolAlias  = Metasound::TArrayMakeNode<bool>;
using FArrayMakeNodeStringAlias = Metasound::TArrayMakeNode<FString>;

METASOUND_REGISTER_NODE(FArrayMakeNodeInt32Alias);
METASOUND_REGISTER_NODE(FArrayMakeNodeFloatAlias);
METASOUND_REGISTER_NODE(FArrayMakeNodeBoolAlias);
METASOUND_REGISTER_NODE(FArrayMakeNodeStringAlias);