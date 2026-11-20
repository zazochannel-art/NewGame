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
			"AnimGraphRuntime",        // UKismetAnimationLibrary (ULIAnimInstance)
			"ProceduralMeshComponent", // island terrain (ALIIslandGenerator)
			"CoreOnline"               // FUniqueNetIdWrapper::ToString (ULISaveSubsystem player keys)
		});

		// Editor builds only: the realistic materials (vertex colour, water) are built at runtime with the
		// material editing API, since no .uasset materials exist yet. Packaged games fall back to the tinted
		// BasicShapeMaterial look (see Visuals/LIMaterials.cpp).
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd", "MaterialEditor", "RHI" });
		}
	}
}
