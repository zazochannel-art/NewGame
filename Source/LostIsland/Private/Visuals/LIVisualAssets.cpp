// Copyright LOST ISLAND Team. All Rights Reserved.

#include "Visuals/LIVisualAssets.h"

#include "LostIsland.h"
#include "Misc/PackageName.h"

ULIVisualAssets::ULIVisualAssets()
{
	// Starter Content (Add Feature or Content Pack -> Content -> Starter Content).
	const TCHAR* Textures = TEXT("/Game/StarterContent/Textures/");
	auto Texture = [Textures](const TCHAR* Name)
	{
		return FSoftObjectPath(FString::Printf(TEXT("%s%s.%s"), Textures, Name, Name));
	};
	GrassColorTexture        = Texture(TEXT("T_Ground_Grass_D"));
	GrassNormalTexture       = Texture(TEXT("T_Ground_Grass_N"));
	ForestFloorColorTexture  = Texture(TEXT("T_Ground_Moss_D"));
	ForestFloorNormalTexture = Texture(TEXT("T_Ground_Moss_N"));
	RockColorTexture         = Texture(TEXT("T_Rock_Basalt_D"));
	RockNormalTexture        = Texture(TEXT("T_Rock_Basalt_N"));
	SandColorTexture         = Texture(TEXT("T_Rock_Sandstone_D"));
	SandNormalTexture        = Texture(TEXT("T_Rock_Sandstone_N"));
	MacroVariationTexture    = Texture(TEXT("T_MacroVariation"));
	WaterNormalTexture       = Texture(TEXT("T_Water_N"));

	const TCHAR* Materials = TEXT("/Game/StarterContent/Materials/");
	auto Material = [Materials](const TCHAR* Name)
	{
		return FSoftObjectPath(FString::Printf(TEXT("%s%s.%s"), Materials, Name, Name));
	};
	WoodMaterial     = Material(TEXT("M_Wood_Pine"));
	DarkWoodMaterial = Material(TEXT("M_Wood_Walnut"));
	MetalMaterial    = Material(TEXT("M_Metal_Brushed_Nickel"));
	RustMaterial     = Material(TEXT("M_Metal_Rust"));

	RockMeshes.Add(FSoftObjectPath(TEXT("/Game/StarterContent/Props/SM_Rock.SM_Rock")));
	BushMeshes.Add(FSoftObjectPath(TEXT("/Game/StarterContent/Props/SM_Bush.SM_Bush")));

	// Third Person (Add Feature or Content Pack -> Blueprint -> Third Person).
	CharacterMesh = FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple"));
	CharacterAnimClass = FSoftClassPath(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed.ABP_Unarmed_C"));
}

UObject* ULIVisualAssets::LoadOptional(const FSoftObjectPath& Path)
{
	if (Path.IsNull())
	{
		return nullptr;
	}

	static TMap<FSoftObjectPath, TWeakObjectPtr<UObject>> Loaded;
	static TSet<FSoftObjectPath> Missing;

	if (const TWeakObjectPtr<UObject>* Found = Loaded.Find(Path))
	{
		if (Found->IsValid())
		{
			return Found->Get();
		}
	}
	if (Missing.Contains(Path))
	{
		return nullptr;
	}

	// Check the package first so missing optional content doesn't spam "failed to load" warnings.
	UObject* Asset = nullptr;
	if (FPackageName::DoesPackageExist(Path.GetLongPackageName()))
	{
		Asset = Path.TryLoad();
	}
	if (!Asset)
	{
		Missing.Add(Path);
		UE_LOG(LogLostIsland, Log, TEXT("Optional visual asset not in the project: %s"), *Path.ToString());
		return nullptr;
	}

	Loaded.Add(Path, Asset);
	return Asset;
}
