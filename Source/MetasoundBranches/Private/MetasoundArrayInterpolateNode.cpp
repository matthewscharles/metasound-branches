// Copyright 2026 Charles Matthews. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundArrayInterpolateNode.h"
#include "MetasoundTime.h"
#include "MetasoundNodeRegistrationMacro.h"

namespace Metasound
{
    template class TArrayInterpolateOperator<TArray<float>>;
    template class TArrayInterpolateNode<TArray<float>>;

    template class TArrayInterpolateOperator<TArray<int32>>;
    template class TArrayInterpolateNode<TArray<int32>>;

    template class TArrayInterpolateOperator<TArray<FTime>>;
    template class TArrayInterpolateNode<TArray<FTime>>;
}

using FInterpolateFloatNodeAlias = Metasound::TArrayInterpolateNode<TArray<float>>;
using FInterpolateIntNodeAlias   = Metasound::TArrayInterpolateNode<TArray<int32>>;
using FInterpolateTimeNodeAlias  = Metasound::TArrayInterpolateNode<TArray<Metasound::FTime>>;

METASOUND_REGISTER_NODE(FInterpolateFloatNodeAlias);
METASOUND_REGISTER_NODE(FInterpolateIntNodeAlias);
METASOUND_REGISTER_NODE(FInterpolateTimeNodeAlias);
