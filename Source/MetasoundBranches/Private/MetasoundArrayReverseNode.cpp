// Copyright 2025 Charles Matthews. All Rights Reserved.

#pragma once

#include "MetasoundBranches/Public/MetasoundArrayReverseNode.h"
// #include "MetasoundTime.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundDataTypeRegistrationMacro.h"


namespace Metasound
{
    template class TArrayReverseOperator<TArray<float>>;
    template class TArrayReverseNode<TArray<float>>;
    
    template class TArrayReverseOperator<TArray<int32>>;
    template class TArrayReverseNode<TArray<int32>>;
    
    template class TArrayReverseOperator<TArray<bool>>;
    template class TArrayReverseNode<TArray<bool>>;
    
    // template class TArrayReverseOperator<TArray<FTime>>;
    // template class TArrayReverseNode<TArray<FTime>>;
    
    
}

// alias workaround (because I can't use the template class directly in the METASOUND_REGISTER_NODE macro)
using FReverseFloatNodeAlias  = Metasound::TArrayReverseNode<TArray<float>>;
using FReverseIntNodeAlias    = Metasound::TArrayReverseNode<TArray<int32>>;
using FReverseBoolNodeAlias   = Metasound::TArrayReverseNode<TArray<bool>>;
// using FReverseTimeNodeAlias   = Metasound::TArrayReverseNode<TArray<FTime>>;

METASOUND_REGISTER_NODE(FReverseFloatNodeAlias);
METASOUND_REGISTER_NODE(FReverseIntNodeAlias);
METASOUND_REGISTER_NODE(FReverseBoolNodeAlias);
// METASOUND_REGISTER_NODE(FReverseTimeNodeAlias);