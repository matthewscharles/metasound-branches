#include "MetasoundBranches/Public/MetasoundParseMidiNoteNode.h"
#include "MetasoundNodeRegistrationMacro.h"

namespace Metasound::MetasoundBranches
{
    template class TParseMidiNoteOperator<int32>;
    template class TParseMidiNoteNode<int32>;

    template class TParseMidiNoteOperator<float>;
    template class TParseMidiNoteNode<float>;
}

using FParseMidiNoteNodeInt32Alias = Metasound::TParseMidiNoteNode<int32>;
using FParseMidiNoteNodeFloatAlias = Metasound::TParseMidiNoteNode<float>;

METASOUND_REGISTER_NODE(FParseMidiNoteNodeInt32Alias);
METASOUND_REGISTER_NODE(FParseMidiNoteNodeFloatAlias);