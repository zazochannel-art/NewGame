// Copyright LOST ISLAND Team. All Rights Reserved.

using UnrealBuildTool;

public class LostIsland : ModuleRules
{
	public LostIsland(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput"
		});

		// Added per milestone as systems need them (UMG for UI, etc.).
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"AnimGraphRuntime" // UKismetAnimationLibrary (ULIAnimInstance)
		});
	}
}
