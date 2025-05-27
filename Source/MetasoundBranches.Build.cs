// Copyright 2025 Charles Matthews. All Rights Reserved.

using UnrealBuildTool;

public class MetasoundBranches : ModuleRules
{
    public MetasoundBranches(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
       
        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "MetasoundEngine",
                "MetasoundStandardNodes",
                "MetasoundFrontend", 
                "MetasoundGraphCore",
                "Slate",
                "SlateCore",
                "Projects",
                // NB: this would disqualify the plugin for FAB, hopefully the CAT material will make it into the 5.6 release
                "MetasoundExperimental",
                "MetasoundExperimentalRuntime"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "AudioExtensions",
                "SignalProcessing"
            }
        );
    }
}