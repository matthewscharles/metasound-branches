// Copyright 2026 Charles Matthews. All Rights Reserved.

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
                "Projects"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "AudioExtensions",
                "SignalProcessing",
                "AudioChannelAgnosticCore"
            }
        );
    }
}