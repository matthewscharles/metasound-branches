// Copyright 2025 Charles Matthews. All Rights Reserved.



#include "MetasoundBranches/Public/MetasoundArrayRouteNode.h"
#include "MetasoundTime.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundDataTypeRegistrationMacro.h"


namespace Metasound
{
    template class TArrayRouteOperator<TArray<float>>;
    template class TArrayRouteNode<TArray<float>>;
    
    template class TArrayRouteOperator<TArray<int32>>;
    template class TArrayRouteNode<TArray<int32>>;
    
    template class TArrayRouteOperator<TArray<bool>>;
    template class TArrayRouteNode<TArray<bool>>;
    
    template class TArrayRouteOperator<TArray<FTime>>;
    template class TArrayRouteNode<TArray<FTime>>;
    
    
}

// alias workaround (because I can't use the template class directly in the METASOUND_REGISTER_NODE macro)
using FRouteFloatNodeAlias  = Metasound::TArrayRouteNode<TArray<float>>;
using FRouteIntNodeAlias    = Metasound::TArrayRouteNode<TArray<int32>>;
using FRouteBoolNodeAlias   = Metasound::TArrayRouteNode<TArray<bool>>;
using FRouteTimeNodeAlias = Metasound::TArrayRouteNode<TArray<Metasound::FTime>>;

METASOUND_REGISTER_NODE(FRouteFloatNodeAlias);
METASOUND_REGISTER_NODE(FRouteIntNodeAlias);
METASOUND_REGISTER_NODE(FRouteBoolNodeAlias);
METASOUND_REGISTER_NODE(FRouteTimeNodeAlias);