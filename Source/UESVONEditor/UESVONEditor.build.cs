using UnrealBuildTool;
using System.IO;

public class UESVONEditor : ModuleRules
{
	public UESVONEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

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
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "InputCore",

                "UESVON"
            });

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "BlueprintGraph",
                "EditorStyle",
                "GraphEditor",
                "PropertyEditor",
                "Slate",
                "SlateCore",
                "UnrealEd"
            });
    }
};
