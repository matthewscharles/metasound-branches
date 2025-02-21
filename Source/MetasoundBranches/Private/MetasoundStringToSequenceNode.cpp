// Copyright 2025 Charles Matthews. All Rights Reserved.

#pragma once

#include "MetasoundBranches/Public/MetasoundStringToSequenceNode.h"
#include "MetasoundNodeRegistrationMacro.h"

namespace Metasound
{
    template class TStringToSequenceOperator<int32>;
    template class TStringToSequenceNode<int32>;

    template class TStringToSequenceOperator<float>;
    template class TStringToSequenceNode<float>;

    template class TStringToSequenceOperator<bool>;
    template class TStringToSequenceNode<bool>;
}

// Register each type with Metasound
using FStringToSequenceNodeInt32Alias = Metasound::TStringToSequenceNode<int32>;
METASOUND_REGISTER_NODE(FStringToSequenceNodeInt32Alias);

using FStringToSequenceNodeFloatAlias = Metasound::TStringToSequenceNode<float>;
METASOUND_REGISTER_NODE(FStringToSequenceNodeFloatAlias);

using FStringToSequenceNodeBoolAlias  = Metasound::TStringToSequenceNode<bool>;
METASOUND_REGISTER_NODE(FStringToSequenceNodeBoolAlias);