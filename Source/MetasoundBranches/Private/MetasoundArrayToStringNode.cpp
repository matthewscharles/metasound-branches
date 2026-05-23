// Copyright 2026 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundArrayToStringNode.h"
#include "MetasoundNodeRegistrationMacro.h"

namespace Metasound
{
    template class TArrayToStringOperator<int32>;
    template class TArrayToStringNode<int32>;

    template class TArrayToStringOperator<float>;
    template class TArrayToStringNode<float>;

    template class TArrayToStringOperator<bool>;
    template class TArrayToStringNode<bool>;

    template class TArrayToStringOperator<FString>;
    template class TArrayToStringNode<FString>;
}

using FArrayToStringNodeInt32Alias = Metasound::TArrayToStringNode<int32>;
using FArrayToStringNodeFloatAlias = Metasound::TArrayToStringNode<float>;
using FArrayToStringNodeBoolAlias  = Metasound::TArrayToStringNode<bool>;
using FArrayToStringNodeStringAlias = Metasound::TArrayToStringNode<FString>;

METASOUND_REGISTER_NODE(FArrayToStringNodeInt32Alias);
METASOUND_REGISTER_NODE(FArrayToStringNodeFloatAlias);
METASOUND_REGISTER_NODE(FArrayToStringNodeBoolAlias);
METASOUND_REGISTER_NODE(FArrayToStringNodeStringAlias);