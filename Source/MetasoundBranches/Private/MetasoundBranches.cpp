// Copyright Charles Matthews 2025. All Rights Reserved.

#include "MetasoundBranches/Public/MetasoundBranches.h"

#include "Modules/ModuleManager.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Interfaces/IPluginManager.h"

#include "MetasoundDataTypeRegistrationMacro.h"
#include "MetasoundFrontendRegistries.h"
#include "MetasoundFrontendModuleRegistrationMacros.h"

#define LOCTEXT_NAMESPACE "FMetasoundBranchesModule"

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

            MutableStyle->Set(TEXT("MetasoundEditor.Graph.Node.Custom.And"),
                new FSlateImageBrush(MutableStyle->RootToContentDir(TEXT("node_math_and_40x.png")), FVector2D(40.f, 40.f)));

            MutableStyle->Set(TEXT("MetasoundEditor.Graph.Node.Custom.EqualTo"),
                new FSlateImageBrush(MutableStyle->RootToContentDir(TEXT("node_math_equalto_40x.png")), FVector2D(40.f, 40.f)));

            MutableStyle->Set(TEXT("MetasoundEditor.Graph.Node.Custom.GreaterThan"),
                new FSlateImageBrush(MutableStyle->RootToContentDir(TEXT("node_math_greaterthan_40x.png")), FVector2D(40.f, 40.f)));

            MutableStyle->Set(TEXT("MetasoundEditor.Graph.Node.Custom.GreaterThanEqualTo"),
                new FSlateImageBrush(MutableStyle->RootToContentDir(TEXT("node_math_greaterthanorequalto_40x.png")), FVector2D(40.f, 40.f)));

            MutableStyle->Set(TEXT("MetasoundEditor.Graph.Node.Custom.LessThan"),
                new FSlateImageBrush(MutableStyle->RootToContentDir(TEXT("node_math_lessthan_40x.png")), FVector2D(40.f, 40.f)));

            MutableStyle->Set(TEXT("MetasoundEditor.Graph.Node.Custom.LessThanEqualTo"),
                new FSlateImageBrush(MutableStyle->RootToContentDir(TEXT("node_math_lessthanorequalto_40x.png")), FVector2D(40.f, 40.f)));

            MutableStyle->Set(TEXT("MetasoundEditor.Graph.Node.Custom.NotEqualTo"),
                new FSlateImageBrush(MutableStyle->RootToContentDir(TEXT("node_math_notequalto_40x.png")), FVector2D(40.f, 40.f)));

            MutableStyle->Set(TEXT("MetasoundEditor.Graph.Node.Custom.Or"),
                new FSlateImageBrush(MutableStyle->RootToContentDir(TEXT("node_math_or_40x.png")), FVector2D(40.f, 40.f)));
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("MetaSoundStyle not found in style registry, cannot register image brushes for Branches module."));
    }

    using namespace Metasound;

    METASOUND_REGISTER_ITEMS_IN_MODULE
}

void FMetasoundBranchesModule::ShutdownModule()
{
    METASOUND_UNREGISTER_ITEMS_IN_MODULE
}

#undef LOCTEXT_NAMESPACE

METASOUND_IMPLEMENT_MODULE_REGISTRATION_LIST
IMPLEMENT_MODULE(FMetasoundBranchesModule, MetasoundBranches);