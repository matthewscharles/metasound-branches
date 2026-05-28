// Copyright 2026 Charles Matthews. All Rights Reserved.

#include "MetasoundBranchesCAT/Public/MetasoundBranchesCAT.h"

#include "MetasoundFrontendModuleRegistrationMacros.h"

#define LOCTEXT_NAMESPACE "FMetasoundBranchesCATModule"

void FMetasoundBranchesCATModule::StartupModule()
{
    METASOUND_REGISTER_ITEMS_IN_MODULE
}

void FMetasoundBranchesCATModule::ShutdownModule()
{
    METASOUND_UNREGISTER_ITEMS_IN_MODULE
}

#undef LOCTEXT_NAMESPACE

METASOUND_IMPLEMENT_MODULE_REGISTRATION_LIST
IMPLEMENT_MODULE(FMetasoundBranchesCATModule, MetasoundBranchesCAT);
