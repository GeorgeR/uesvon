using UnrealBuildTool;

public class UESVON : ModuleRules
{
	public UESVON(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        if (Target.Version.MinorVersion <= 19)
        {
            PublicIncludePaths.AddRange(
                new string[] {
                     "UESVON/Public"
                });

            PrivateIncludePaths.AddRange(
                new string[] {
                     "UESVON/Private"
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
