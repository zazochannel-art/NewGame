// Copyright LOST ISLAND Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LIIslandGenerator.generated.h"

class ALIResourceNode;
class UDirectionalLightComponent;
class UExponentialHeightFogComponent;
class UMaterialInterface;
class UProceduralMeshComponent;
class USkyAtmosphereComponent;
class USkyLightComponent;
class UPostProcessComponent;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class ELIIslandBiome : uint8
{
	Ocean,
	Beach,
	Meadow,
	Forest,
	Hills
};

/** Where a resource node should be placed. Computed deterministically from the seed. */
USTRUCT(BlueprintType)
struct LOSTISLAND_API FLIResourceSpawnPoint
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Island")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Island")
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "Island")
	ELIIslandBiome Biome = ELIIslandBiome::Meadow;

	UPROPERTY(BlueprintReadOnly, Category = "Island")
	TSubclassOf<AActor> NodeClass;
};

/** Placement rule for one spawnable type (resource nodes, water sources...). Origin of the spawned actor = ground. */
USTRUCT(BlueprintType)
struct LOSTISLAND_API FLIResourceSpawnRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Island")
	TSubclassOf<AActor> NodeClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Island")
	TArray<ELIIslandBiome> AllowedBiomes;

	/** Target number of nodes (fewer if there is not enough valid space). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Island", meta = (ClampMin = "0"))
	int32 Count = 50;

	/** Minimum distance to any other spawn point. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Island", meta = (ClampMin = "0.0", Units = "cm"))
	float MinSpacing = 500.f;

	/** Minimum surface normal Z (1 = flat ground only, 0 = any slope). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Island", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinNormalZ = 0.8f;
};

/**
 * ALIIslandGenerator
 *
 * Builds a small procedural island from a single int32 seed:
 *  - Terrain: flat-shaded low-poly grid (UProceduralMeshComponent, with collision).
 *    Height = radial island mask (with noisy coastline) x fractal Perlin hills.
 *  - Biomes by height/noise: Ocean, Beach, Meadow, Forest, Hills (vertex colours).
 *  - Water plane at sea level (Z = 0 in actor space).
 *  - Resource spawn points per FLIResourceSpawnRule (trees in forest, rocks on
 *    hills/meadow, bushes anywhere green), with spacing.
 *  - Optional environment (sun, sky atmosphere, sky light, fog) when the map has none.
 *
 * Networking: only Seed replicates. Every machine rebuilds the identical terrain
 * locally (deterministic noise), and only the SERVER spawns the resource nodes,
 * which replicate as normal actors. This keeps bandwidth tiny for co-op.
 *
 * Cost: generation runs once (at game start, or on explicit Regenerate). No tick.
 */
UCLASS()
class LOSTISLAND_API ALIIslandGenerator : public AActor
{
	GENERATED_BODY()

public:
	ALIIslandGenerator();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server: build terrain for InSeed and spawn resource nodes. Replaces any previous island. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "LostIsland|Island")
	void GenerateIsland(int32 InSeed);

	/** Server: destroy spawned resource nodes and clear the terrain. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "LostIsland|Island")
	void ClearIsland();

	UFUNCTION(BlueprintPure, Category = "LostIsland|Island")
	int32 GetSeed() const { return Seed; }

	/** Terrain height (actor-local cm) at an actor-local XY position. Matches the mesh vertices exactly at grid points. */
	UFUNCTION(BlueprintPure, Category = "LostIsland|Island")
	float GetHeightAt(const FVector2D& LocalXY) const;

	UFUNCTION(BlueprintPure, Category = "LostIsland|Island")
	ELIIslandBiome GetBiomeAt(const FVector2D& LocalXY) const;

	/** World-space spawn transform on a beach, facing the island centre. */
	UFUNCTION(BlueprintPure, Category = "LostIsland|Island")
	FTransform FindPlayerSpawnTransform() const;

	const TArray<FLIResourceSpawnPoint>& GetSpawnPoints() const { return SpawnPoints; }

	/**
	 * Server: actors spawned for this seed (crash site first, then resource nodes in spawn-point order).
	 * The order is deterministic for a seed, so saves can refer to nodes by index.
	 */
	const TArray<TObjectPtr<AActor>>& GetSpawnedNodes() const { return SpawnedNodes; }

	UFUNCTION(BlueprintPure, Category = "LostIsland|Island")
	bool HasCrashSite() const { return bHasCrashSite; }

	/** World transform of the wreck (origin on the ground). Only valid if HasCrashSite(). */
	UFUNCTION(BlueprintPure, Category = "LostIsland|Island")
	FTransform GetCrashSiteTransform() const;

	/**
	 * Local (all machines): place the sun or moon for this hour (0..24). Called by ALIGameState
	 * with its night window, so the sun sets when night starts and rises when it ends.
	 * No effect when the level provides its own lighting.
	 */
	void ApplyTimeOfDay(float Hours, float SunriseHours = 6.f, float SunsetHours = 18.f);

	/** Half the width of the generated grid (cm). */
	UFUNCTION(BlueprintPure, Category = "LostIsland|Island")
	float GetHalfExtent() const { return GridResolution * CellSize * 0.5f; }

protected:
	// ---------------- Components ----------------
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UProceduralMeshComponent> TerrainMesh;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> WaterMesh;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UDirectionalLightComponent> Sun;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USkyAtmosphereComponent> SkyAtmosphere;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USkyLightComponent> SkyLight;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UExponentialHeightFogComponent> HeightFog;

	/** Unbound post process: fixed manual exposure so the scene is not blown out at noon (see ExposureCompensation). */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UPostProcessComponent> PostProcess;

	// ---------------- Shape ----------------
	/** Cells per side. 128 x 128 cells -> ~98k flat-shaded vertices. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LostIsland|Island|Shape", meta = (ClampMin = "16", ClampMax = "256"))
	int32 GridResolution = 128;

	/** Size of one cell (cm). Larger = chunkier low-poly look and a bigger island. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LostIsland|Island|Shape", meta = (ClampMin = "50.0", Units = "cm"))
	float CellSize = 300.f;

	/** Fraction of the half-extent covered by land (the rest is ocean). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LostIsland|Island|Shape", meta = (ClampMin = "0.3", ClampMax = "0.95"))
	float IslandSizeFraction = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LostIsland|Island|Shape", meta = (Units = "cm"))
	float SeabedHeight = -1000.f;

	/** Height of flat land near the coast. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LostIsland|Island|Shape", meta = (Units = "cm"))
	float BaseLandHeight = 150.f;

	/** Peak extra height of the hills. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LostIsland|Island|Shape", meta = (ClampMin = "0.0", Units = "cm"))
	float HillHeight = 5500.f;

	/** Contrast of the hill noise. Higher = more distinct peaks and flatter lowlands. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LostIsland|Island|Shape", meta = (ClampMin = "0.1", ClampMax = "3.0"))
	float HillContrast = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LostIsland|Island|Shape", meta = (ClampMin = "0.0", Units = "cm"))
	float DetailHeight = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LostIsland|Island|Shape", meta = (ClampMin = "0.1"))
	float HillNoiseFrequency = 1.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LostIsland|Island|Shape", meta = (ClampMin = "0.1"))
	float DetailNoiseFrequency = 6.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LostIsland|Island|Shape", meta = (ClampMin = "0.1"))
	float CoastNoiseFrequency = 2.5f;

	/** How irregular the coastline is (0 = perfect circle). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LostIsland|Island|Shape", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float CoastNoiseStrength = 0.2f;

	/** Debug: flip triangle winding. Leave off; the generator already emits Unreal's front-face order. */
	UPROPERTY(EditAnywhere, Category = "LostIsland|Island|Shape")
	bool bFlipWinding = false;

	// ---------------- Biomes ----------------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LostIsland|Island|Biomes", meta = (Units = "cm"))
	float BeachMaxHeight = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LostIsland|Island|Biomes", meta = (Units = "cm"))
	float HillsMinHeight = 1500.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LostIsland|Island|Biomes", meta = (ClampMin = "0.1"))
	float ForestNoiseFrequency = 3.f;

	/** Lower = more forest, higher = more meadow. Range roughly -1..1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LostIsland|Island|Biomes", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float ForestThreshold = -0.05f;

	UPROPERTY(EditAnywhere, Category = "LostIsland|Island|Biomes") FLinearColor SeabedColor = FLinearColor(0.55f, 0.48f, 0.32f);
	UPROPERTY(EditAnywhere, Category = "LostIsland|Island|Biomes") FLinearColor SandColor   = FLinearColor(0.93f, 0.82f, 0.55f);
	UPROPERTY(EditAnywhere, Category = "LostIsland|Island|Biomes") FLinearColor MeadowColor = FLinearColor(0.45f, 0.70f, 0.28f);
	UPROPERTY(EditAnywhere, Category = "LostIsland|Island|Biomes") FLinearColor ForestColor = FLinearColor(0.20f, 0.45f, 0.18f);
	UPROPERTY(EditAnywhere, Category = "LostIsland|Island|Biomes") FLinearColor HillsColor  = FLinearColor(0.50f, 0.55f, 0.35f);
	UPROPERTY(EditAnywhere, Category = "LostIsland|Island|Biomes") FLinearColor RockColor   = FLinearColor(0.45f, 0.43f, 0.40f);

	/** Surfaces steeper than this (normal Z below it) are coloured as rock. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LostIsland|Island|Biomes", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RockSlopeNormalZ = 0.75f;

	// ---------------- Resources ----------------
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LostIsland|Island|Resources")
	TArray<FLIResourceSpawnRule> SpawnRules;

	// ---------------- Crash site ----------------
	/** The plane wreck landmark. Null = no crash site. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LostIsland|Island|CrashSite")
	TSubclassOf<AActor> CrashSiteClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LostIsland|Island|CrashSite")
	TArray<ELIIslandBiome> CrashSiteBiomes;

	/** No resources spawn within this distance of the wreck (keeps the landmark readable). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LostIsland|Island|CrashSite", meta = (ClampMin = "0.0", Units = "cm"))
	float CrashSiteClearance = 1800.f;

	/** Half-length of the area that must be flat and dry for the wreck. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LostIsland|Island|CrashSite", meta = (ClampMin = "100.0", Units = "cm"))
	float CrashSiteFootprint = 700.f;

	/** Minimum distance from the player's beach spawn, so finding the wreck is a small expedition. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LostIsland|Island|CrashSite", meta = (ClampMin = "0.0", Units = "cm"))
	float CrashSiteMinDistanceFromSpawn = 6000.f;

	/** Minimum distance between spawn points of different types. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LostIsland|Island|Resources", meta = (ClampMin = "0.0", Units = "cm"))
	float OtherTypeClearance = 350.f;

	// ---------------- Rendering / environment ----------------
	/**
	 * Lit material for the terrain. The terrain is split into one mesh section per surface colour, and each section
	 * gets a dynamic instance of this material with its "Color" parameter set (defaults to BasicShapeMaterial).
	 * Vertex colours are still written, so a custom material may use Vertex Color instead and ignore "Color".
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LostIsland|Island|Rendering")
	TObjectPtr<UMaterialInterface> TerrainMaterial;

	/** Faces whose colours differ by less than this share a mesh section (fewer sections = fewer draw calls). */
	UPROPERTY(EditAnywhere, Category = "LostIsland|Island|Rendering", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float TerrainColorTolerance = 0.035f;

	/** Upper bound on terrain mesh sections; past it, faces join the closest existing colour. */
	UPROPERTY(EditAnywhere, Category = "LostIsland|Island|Rendering", meta = (ClampMin = "1", ClampMax = "64"))
	int32 MaxTerrainSections = 24;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LostIsland|Island|Rendering")
	TObjectPtr<UMaterialInterface> WaterMaterial;

	UPROPERTY(EditAnywhere, Category = "LostIsland|Island|Rendering")
	FLinearColor WaterColor = FLinearColor(0.05f, 0.35f, 0.55f);

	/** Use the built-in sun/sky/fog only if the level has no directional light of its own. */
	UPROPERTY(EditAnywhere, Category = "LostIsland|Island|Rendering")
	bool bCreateEnvironmentIfMissing = true;

	UPROPERTY(EditAnywhere, Category = "LostIsland|Island|Rendering|DayNight", meta = (Units = "lux"))
	float SunIntensity = 10.f;

	/** Moonlight is a dim, cool directional light so nights are dark but playable. */
	UPROPERTY(EditAnywhere, Category = "LostIsland|Island|Rendering|DayNight", meta = (Units = "lux"))
	float MoonIntensity = 2.5f;

	/** Manual exposure compensation (EV) for the generated environment. Lower = darker. Tuned for SunIntensity 10 lux. */
	UPROPERTY(EditAnywhere, Category = "LostIsland|Island|Rendering|DayNight")
	float ExposureCompensation = -3.f;

	UPROPERTY(EditAnywhere, Category = "LostIsland|Island|Rendering|DayNight")
	FLinearColor NoonColor = FLinearColor(1.f, 0.96f, 0.88f);

	UPROPERTY(EditAnywhere, Category = "LostIsland|Island|Rendering|DayNight")
	FLinearColor SunsetColor = FLinearColor(1.f, 0.55f, 0.3f);

	UPROPERTY(EditAnywhere, Category = "LostIsland|Island|Rendering|DayNight")
	FLinearColor MoonColor = FLinearColor(0.55f, 0.65f, 1.f);

	/** Compass direction the sun travels along (degrees). */
	UPROPERTY(EditAnywhere, Category = "LostIsland|Island|Rendering|DayNight")
	float SunYaw = 35.f;

private:
	UFUNCTION()
	void OnRep_Seed();

	/** Builds terrain + water + environment for the current Seed (all machines). */
	void BuildTerrain();
	void BuildSpawnPoints();
	void ChooseCrashSite();
	void SpawnResourceNodes();
	void SetupEnvironment();

	float Fbm(const FVector2D& P, int32 Octaves) const;
	FVector GetNormalAt(const FVector2D& LocalXY) const;
	float GetIslandRadius() const { return GetHalfExtent() * IslandSizeFraction; }
	FLinearColor GetSurfaceColor(const FVector2D& LocalXY, float NormalZ) const;

	UPROPERTY(ReplicatedUsing = OnRep_Seed)
	int32 Seed = 0;

	/** Seed the current terrain was built for (prevents duplicate rebuilds). */
	int32 BuiltSeed = 0;
	bool bHasBuiltTerrain = false;

	FVector2D HillOffset = FVector2D::ZeroVector;
	FVector2D DetailOffset = FVector2D::ZeroVector;
	FVector2D CoastOffset = FVector2D::ZeroVector;
	FVector2D ForestOffset = FVector2D::ZeroVector;

	TArray<FLIResourceSpawnPoint> SpawnPoints;

	bool bHasCrashSite = false;
	bool bOwnEnvironmentActive = false;
	FTransform CrashSiteLocalTransform;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> SpawnedNodes;
};
