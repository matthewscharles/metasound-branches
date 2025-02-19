// Copyright 2025 Charles Matthews. All Rights Reserved.

#pragma once

#include "MetasoundBranches/Public/MetasoundArraySumNode.h"
#include "MetasoundTime.h"
#include "MetasoundNodeRegistrationMacro.h"

namespace Metasound
{
    template class TArraySumOperator<TArray<float>>;
    template class TArraySumNode<TArray<float>>;

    template class TArraySumOperator<TArray<int32>>;
    template class TArraySumNode<TArray<int32>>;
    
    template class TArraySumOperator<TArray<FTime>>;
    template class TArraySumNode<TArray<FTime>>;
}

using FSumFloatNodeAlias = Metasound::TArraySumNode<TArray<float>>;
using FSumIntNodeAlias   = Metasound::TArraySumNode<TArray<int32>>;
using FSumTimeNodeAlias   = Metasound::TArraySumNode<TArray<Metasound::FTime>>;

METASOUND_REGISTER_NODE(FSumFloatNodeAlias);
METASOUND_REGISTER_NODE(FSumIntNodeAlias);
METASOUND_REGISTER_NODE(FSumTimeNodeAlias);