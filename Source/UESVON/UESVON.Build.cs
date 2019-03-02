// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class UESVON : ModuleRules
{
	public UESVON(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        if (Target.Version.MinorVersion <= 19)
        {
            PublicIncludePaths.AddRange(
                new string[] {
                    Path.Combine(ModuleDirectory, "Public")
                });

            PrivateIncludePaths.AddRange(
                new string[] {
                    Path.Combine(ModuleDirectory, "Private")
                });
        }
        
        PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
                "AIModule",
                "GameplayTasks",
                "NavigationSystem"
			});

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore"
			});
	}
}
