#include "MetasoundBranches/Public/MetasoundBranches.h"
#include "Modules/ModuleManager.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Interfaces/IPluginManager.h"

// If you use something like FPaths::Combine, you also need #include "Misc/Paths.h"

TSharedPtr<FSlateStyleSet> FMetasoundBranchesModule::StyleSet = nullptr;

void FMetasoundBranchesModule::StartupModule()
{
    // Create it only once
    if (!StyleSet.IsValid())
    {
        StyleSet = MakeShareable(new FSlateStyleSet("MetasoundBranchesStyle"));

        if (TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("MetasoundBranches")))
        {
            const FString PluginContentDir = Plugin->GetBaseDir();
            StyleSet->SetContentRoot(PluginContentDir / TEXT("Resources/Icons"));
        }
        else
        {
        }

        StyleSet->Set(
            TEXT("MetasoundEditor.Graph.Node.Custom.GreaterThan"),
            new FSlateImageBrush(
                StyleSet->RootToContentDir(TEXT("node_math_greaterthan_40x.png")),
                FVector2D(40.f, 40.f)
            )
        );

        FSlateStyleRegistry::RegisterSlateStyle(*StyleSet);
    }

    using namespace Metasound;
    FMetasoundFrontendRegistryContainer::Get()->RegisterPendingNodes();
}

void FMetasoundBranchesModule::ShutdownModule()
{
    if (StyleSet.IsValid())
    {
        FSlateStyleRegistry::UnRegisterSlateStyle(StyleSet->GetStyleSetName());
        StyleSet.Reset();
    }
}

IMPLEMENT_MODULE(FMetasoundBranchesModule, MetasoundBranches);