// Copyright LOST ISLAND Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPath.h"
#include "LIVisualAssets.generated.h"

/**
 * ULIVisualAssets
 *
 * Paths of real art the game uses when it exists in the project, with the code-built models as fallback.
 * Nothing here is required: every entry that doesn't resolve is skipped silently.
 *
 * The defaults point at content that ships with every Unreal Engine install and is added to the project
 * from the editor in one click (Content Drawer -> Add -> Add Feature or Content Pack):
 *  - Starter Content : photo textures (grass, moss, rock, sandstone, water normals), SM_Rock, SM_Bush.
 *  - Third Person    : the animated UE5 mannequin (SKM_Quinn_Simple + ABP_Unarmed).
 * Trees aren't in either pack: add free photoreal trees (e.g. Megascans on Fab) and list them in TreeMeshes.
 *
 * Override in Config/DefaultGame.ini, section [/Script/LostIsland.LIVisualAssets], e.g.
 *     +TreeMeshes=/Game/Megascans/3D_Plants/Pine/SM_Pine_01.SM_Pine_01
 */
UCLASS(config = Game, defaultconfig)
class LOSTISLAND_API ULIVisualAssets : public UObject
{
	GENERATED_BODY()

public:
	ULIVisualAssets();

	static const ULIVisualAssets& Get() { return *GetDefault<ULIVisualAssets>(); }

	/** Loads an asset if its package exists (no warnings for missing content); results are cached. */
	static UObject* LoadOptional(const FSoftObjectPath& Path);

	template <typename T>
	static T* LoadOptional(const FSoftObjectPath& Path) { return Cast<T>(LoadOptional(Path)); }

	/** Picks one of the paths that resolves (deterministic for Index), or null if none does. */
	template <typename T>
	static T* PickOptional(const TArray<FSoftObjectPath>& Paths, int32 Index)
	{
		TArray<T*, TInlineAllocator<8>> Valid;
		for (const FSoftObjectPath& Path : Paths)
		{
			if (T* Asset = LoadOptional<T>(Path))
			{
				Valid.Add(Asset);
			}
		}
		return Valid.Num() > 0 ? Valid[FMath::Abs(Index) % Valid.Num()] : nullptr;
	}

	// ---------------- Terrain textures (realistic style; all eight are needed for the textured terrain) ----------------
	UPROPERTY(config, EditAnywhere, Category = "Terrain") FSoftObjectPath GrassColorTexture;
	UPROPERTY(config, EditAnywhere, Category = "Terrain") FSoftObjectPath GrassNormalTexture;
	UPROPERTY(config, EditAnywhere, Category = "Terrain") FSoftObjectPath ForestFloorColorTexture;
	UPROPERTY(config, EditAnywhere, Category = "Terrain") FSoftObjectPath ForestFloorNormalTexture;
	UPROPERTY(config, EditAnywhere, Category = "Terrain") FSoftObjectPath RockColorTexture;
	UPROPERTY(config, EditAnywhere, Category = "Terrain") FSoftObjectPath RockNormalTexture;
	UPROPERTY(config, EditAnywhere, Category = "Terrain") FSoftObjectPath SandColorTexture;
	UPROPERTY(config, EditAnywhere, Category = "Terrain") FSoftObjectPath SandNormalTexture;

	/** Large-scale brightness variation that breaks up texture tiling (optional). */
	UPROPERTY(config, EditAnywhere, Category = "Terrain") FSoftObjectPath MacroVariationTexture;

	/** Metres covered by one repeat of the ground textures. */
	UPROPERTY(config, EditAnywhere, Category = "Terrain", meta = (ClampMin = "0.5"))
	float TerrainTextureTileMeters = 3.f;

	// ---------------- Water ----------------
	/** Normal map for moving waves on the sea (optional). */
	UPROPERTY(config, EditAnywhere, Category = "Water") FSoftObjectPath WaterNormalTexture;

	// ---------------- Surface materials (code-built models: buildings, raft, chest, wreck) ----------------
	/** Light wood: floors, walls, doorways, raft, chest planks. */
	UPROPERTY(config, EditAnywhere, Category = "Materials") FSoftObjectPath WoodMaterial;

	/** Dark wood: roofs, beams, chest lid. */
	UPROPERTY(config, EditAnywhere, Category = "Materials") FSoftObjectPath DarkWoodMaterial;

	/** Bare aluminium: the plane's hull. */
	UPROPERTY(config, EditAnywhere, Category = "Materials") FSoftObjectPath MetalMaterial;

	/** Rusty / burnt metal: wreck debris, chest bands. */
	UPROPERTY(config, EditAnywhere, Category = "Materials") FSoftObjectPath RustMaterial;

	// ---------------- Meshes ----------------
	/** Real tree meshes; one is picked per tree. Empty = code-built trees. */
	UPROPERTY(config, EditAnywhere, Category = "Meshes") TArray<FSoftObjectPath> TreeMeshes;

	UPROPERTY(config, EditAnywhere, Category = "Meshes") TArray<FSoftObjectPath> RockMeshes;

	/** Used for fiber bushes and (with code-built berries on top) berry bushes. */
	UPROPERTY(config, EditAnywhere, Category = "Meshes") TArray<FSoftObjectPath> BushMeshes;

	/** Ground-cover grass (instanced). Empty = thin code cones. */
	UPROPERTY(config, EditAnywhere, Category = "Meshes") TArray<FSoftObjectPath> GrassMeshes;

	// ---------------- Character ----------------
	/** Skeletal mesh for the survivor. Hides the code-built body when it loads. */
	UPROPERTY(config, EditAnywhere, Category = "Character") FSoftObjectPath CharacterMesh;

	/** Animation Blueprint class for CharacterMesh (path ending in _C). */
	UPROPERTY(config, EditAnywhere, Category = "Character") FSoftClassPath CharacterAnimClass;

	/**
	 * Optional MetaHuman (or any actor blueprint with skeletal meshes on the UE5 mannequin skeleton) worn as the
	 * survivor body. Empty = use the first blueprint named BP_* under MetaHumanSearchPath. Its meshes follow the
	 * mannequin animation (leader pose); the mannequin itself keeps animating but is not drawn.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Character") FSoftClassPath CharacterBlueprint;

	/** Content folder searched for a MetaHuman blueprint when CharacterBlueprint is empty. */
	UPROPERTY(config, EditAnywhere, Category = "Character") FString MetaHumanSearchPath = TEXT("/Game/MetaHumans");
};
