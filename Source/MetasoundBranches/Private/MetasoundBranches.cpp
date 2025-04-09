// Copyright Charles Matthews 2025. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundBranches.h"
#include "Modules/ModuleManager.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Interfaces/IPluginManager.h"

TSharedPtr<FSlateStyleSet> FMetasoundBranchesModule::StyleSet = nullptr;

void FMetasoundBranchesModule::StartupModule()
{
    if (const ISlateStyle* ExistingStyle = FSlateStyleRegistry::FindSlateStyle("MetaSoundStyle"))
    {
        FSlateStyleSet* MutableStyle = const_cast<FSlateStyleSet*>(static_cast<const FSlateStyleSet*>(ExistingStyle));

        if (TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("MetasoundBranches")))
        {
            const FString PluginContentDir = Plugin->GetBaseDir();
            const FString FullIconPath = PluginContentDir / TEXT("Resources/Icons");

            MutableStyle->SetContentRoot(FullIconPath);

            MutableStyle->Set(
                TEXT("MetasoundEditor.Graph.Node.Custom.GreaterThan"),
                new FSlateImageBrush(
                    MutableStyle->RootToContentDir(TEXT("node_math_greaterthan_40x.png")),
                    FVector2D(40.f, 40.f)
                )
            );
            MutableStyle->Set(
                TEXT("MetasoundEditor.Graph.Node.Custom.GreaterThanEqual"),
                new FSlateImageBrush(
                    MutableStyle->RootToContentDir(TEXT("node_math_greaterthanequal_40x.png")),
                    FVector2D(40.f, 40.f)
                )
            );

        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("MetaSoundStyle not found in style registry!"));
    }

    using namespace Metasound;
    FMetasoundFrontendRegistryContainer::Get()->RegisterPendingNodes();
}

void FMetasoundBranchesModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FMetasoundBranchesModule, MetasoundBranches);