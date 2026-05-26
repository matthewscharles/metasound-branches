// Copyright 2026 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundArrayMultiplyNode.h"
#include "MetasoundTime.h"
#include "MetasoundNodeRegistrationMacro.h"

namespace Metasound
{
    template class TArrayMultiplyOperator<TArray<float>>;
    template class TArrayMultiplyNode<TArray<float>>;

    template class TArrayMultiplyOperator<TArray<int32>>;
    template class TArrayMultiplyNode<TArray<int32>>;

    template class TArrayMultiplyOperator<TArray<FTime>>;
    template class TArrayMultiplyNode<TArray<FTime>>;
}

using FMultiplyFloatNodeAlias = Metasound::TArrayMultiplyNode<TArray<float>>;
using FMultiplyIntNodeAlias   = Metasound::TArrayMultiplyNode<TArray<int32>>;
using FMultiplyTimeNodeAlias  = Metasound::TArrayMultiplyNode<TArray<Metasound::FTime>>;

METASOUND_REGISTER_NODE(FMultiplyFloatNodeAlias);
METASOUND_REGISTER_NODE(FMultiplyIntNodeAlias);
METASOUND_REGISTER_NODE(FMultiplyTimeNodeAlias);
