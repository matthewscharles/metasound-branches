#include "MetasoundBranches/Public/MetasoundBranches.h"
#include "Modules/ModuleManager.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Interfaces/IPluginManager.h"

TSharedPtr<FSlateStyleSet> FMetasoundBranchesModule::StyleSet = nullptr;

void FMetasoundBranchesModule::StartupModule()
{
    if (!StyleSet.IsValid())
    {
        StyleSet = MakeShareable(new FSlateStyleSet("MetasoundEditor"));

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

    UE_LOG(LogTemp, Log, TEXT("Brush registered: %s"), *StyleSet->GetBrush("MetasoundEditor.Graph.Node.Custom.GreaterThan")->GetResourceName().ToString());
    
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