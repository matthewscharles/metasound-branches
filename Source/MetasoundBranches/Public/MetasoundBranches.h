// Copyright 2025 Charles Matthews. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

class FMetasoundBranchesModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
    
private:
    static TSharedPtr<class FSlateStyleSet> StyleSet;
};