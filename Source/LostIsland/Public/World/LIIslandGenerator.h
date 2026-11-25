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
class UHierarchicalInstancedStaticMeshComponent;
class UStaticMesh;
class UVolumetricCloudComponent;

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

	/**
	 * Server, before GenerateIsland: build the farm valley instead of the island. The valley is a wide, gently
	 * rolling floor ringed by mountains (the sea is only beyond them), with a flat farmyard in the middle and a
	 * dirt road: a loop around the valley plus a straight road through the yard. No wreck, crabs or boars.
	 */
	void SetValleyMode(bool bInValley) { bValley = bInValley; }

	UFUNCTION(BlueprintPure, Category = "LostIsland|Island")
	bool IsValley() const { return bValley; }

	/** Valley only: 0..1, how much a point is on the road (1 = on it, fading out at the verges). */
	float GetRoadWeight(const FVector2D& LocalXY) const;

	/** Valley only: 0..1, how much a point is in the flat farmyard. */
	float GetYardWeight(const FVector2D& LocalXY) const;

	/** Valley only: 0..1, how much a point is inside a field (flattened, kept clear of trees and grass). */
	float GetFieldWeight(const FVector2D& LocalXY) const;

	/** Valley only: the fields, as actor-local rectangles (on the terrain grid). */
	TArray<FBox2D> GetFieldRects() const;

	/** Valley only: world-space centre (on the ground) of a field rectangle. */
	FVector GetFieldCenterWorld(const FBox2D& Rect) const;

	/** Valley only: world transforms where vehicles are parked at the start (in the farmyard). */
	TArray<FTransform> GetVehicleSpawnTransforms(int32 Count) const;

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
	void ApplyTimeOfDay(float Hours, float SunriseHours = 6.f, float SunsetHours = 18.f, float Rain = 0.f);

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

	/** Flat overlay just above sea level: white foam at the shoreline, turquoise shallows, teal further out. */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UProceduralMeshComponent> ShallowWaterMesh;

	/** Low-poly clouds; CloudRoot turns slowly with the time of day so the sky drifts on every machine alike. */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> CloudRoot;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UProceduralMeshComponent> CloudMesh;

	/** Realistic style: engine volumetric clouds (replace the low-poly CloudMesh when available). */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UVolumetricCloudComponent> VolumetricCloud;

	/** Cosmetic ground cover (no collision): one instanced component per decoration kind (see ELIDecoration in the .cpp). */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> DecorationComponents;

	// ---------------- Valley ----------------
	/** Height of the valley floor (well above the beach height, so the floor reads as grass, not sand). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LostIsland|Island|Valley", meta = (Units = "cm"))
	float ValleyFloorHeight = 650.f;

	/** Long, gentle rises and dips across the floor (+-), small enough to drive and plough. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LostIsland|Island|Valley", meta = (Units = "cm"))
	float ValleyRollingHeight = 220.f;

	/** Where the mountains start, as a fraction of the radius. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LostIsland|Island|Valley", meta = (ClampMin = "0.2", ClampMax = "0.9"))
	float ValleyRimStart = 0.62f;

	/** Half size of the square farmyard. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LostIsland|Island|Valley", meta = (Units = "cm"))
	float YardHalfSize = 2200.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LostIsland|Island|Valley", meta = (Units = "cm"))
	float RoadHalfWidth = 320.f;

	/** Ring road radius as a fraction of the island radius. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LostIsland|Island|Valley", meta = (ClampMin = "0.1", ClampMax = "0.8"))
	float RingRoadRadius = 0.52f;

	/** Size of each of the four fields around the yard (a whole number of 2 m cells). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LostIsland|Island|Valley", meta = (Units = "cm"))
	float FieldSize = 2000.f;

	/** Distance from the valley centre to each field's centre, along X and along Y. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LostIsland|Island|Valley", meta = (Units = "cm"))
	float FieldOffset = 4000.f;

	UPROPERTY(EditAnywhere, Category = "LostIsland|Island|Valley") FLinearColor RoadColor = FLinearColor(0.47f, 0.35f, 0.22f);
	UPROPERTY(EditAnywhere, Category = "LostIsland|Island|Valley") FLinearColor YardColor = FLinearColor(0.56f, 0.50f, 0.40f);

	// ---------------- Shape ----------------
	/** Cells per side. 192 x 192 cells of 2 m = the same 384 m island as before, with finer detail (~74k triangles). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LostIsland|Island|Shape", meta = (ClampMin = "16", ClampMax = "384"))
	int32 GridResolution = 192;

	/** Size of one cell (cm). Larger = chunkier look and a bigger island. Keep GridResolution * CellSize ~38400. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LostIsland|Island|Shape", meta = (ClampMin = "50.0", Units = "cm"))
	float CellSize = 200.f;

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
	UPROPERTY(EditAnywhere, Category = "LostIsland|Island|Biomes") FLinearColor SandColor   = FLinearColor(1.00f, 0.86f, 0.58f);
	UPROPERTY(EditAnywhere, Category = "LostIsland|Island|Biomes") FLinearColor MeadowColor = FLinearColor(0.38f, 0.80f, 0.24f);
	UPROPERTY(EditAnywhere, Category = "LostIsland|Island|Biomes") FLinearColor ForestColor = FLinearColor(0.16f, 0.58f, 0.20f);
	UPROPERTY(EditAnywhere, Category = "LostIsland|Island|Biomes") FLinearColor HillsColor  = FLinearColor(0.55f, 0.68f, 0.32f);
	UPROPERTY(EditAnywhere, Category = "LostIsland|Island|Biomes") FLinearColor RockColor   = FLinearColor(0.58f, 0.55f, 0.52f);

	/** Realistic style: seabed colour at depth (wet sand at the waterline blends into this). */
	UPROPERTY(EditAnywhere, Category = "LostIsland|Island|Biomes") FLinearColor DeepSeabedColor = FLinearColor(0.16f, 0.22f, 0.20f);

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

	/** Deep ocean colour (the big water plane). */
	UPROPERTY(EditAnywhere, Category = "LostIsland|Island|Rendering")
	FLinearColor WaterColor = FLinearColor(0.03f, 0.24f, 0.42f);

	UPROPERTY(EditAnywhere, Category = "LostIsland|Island|Rendering|Water")
	FLinearColor FoamColor = FLinearColor(0.90f, 0.95f, 0.95f);

	UPROPERTY(EditAnywhere, Category = "LostIsland|Island|Rendering|Water")
	FLinearColor ShallowWaterColor = FLinearColor(0.10f, 0.85f, 0.78f);

	UPROPERTY(EditAnywhere, Category = "LostIsland|Island|Rendering|Water")
	FLinearColor MidWaterColor = FLinearColor(0.08f, 0.50f, 0.64f);

	/** Seabed depth (cm) down to which the water reads as turquoise shallows, then teal down to MidWaterDepth. */
	UPROPERTY(EditAnywhere, Category = "LostIsland|Island|Rendering|Water", meta = (ClampMin = "10.0", Units = "cm"))
	float ShallowWaterDepth = 160.f;

	UPROPERTY(EditAnywhere, Category = "LostIsland|Island|Rendering|Water", meta = (ClampMin = "10.0", Units = "cm"))
	float MidWaterDepth = 420.f;

	// ---------------- Decoration ----------------
	/** Grass, flowers, pebbles and shells (cosmetic, every machine, deterministic for the seed). */
	UPROPERTY(EditAnywhere, Category = "LostIsland|Island|Decoration")
	bool bGenerateDecorations = true;

	/** Multiplier on the amount of ground cover (1 = ~3 candidates per 3 m cell). Lower it on slow PCs. */
	UPROPERTY(EditAnywhere, Category = "LostIsland|Island|Decoration", meta = (ClampMin = "0.0", ClampMax = "4.0"))
	float DecorationDensity = 2.f;

	/** Ground cover fades out beyond this distance from the camera. */
	UPROPERTY(EditAnywhere, Category = "LostIsland|Island|Decoration", meta = (ClampMin = "1000.0", Units = "cm"))
	float DecorationCullDistance = 7000.f;

	UPROPERTY(EditAnywhere, Category = "LostIsland|Island|Decoration")
	bool bGenerateClouds = true;

	UPROPERTY(EditAnywhere, Category = "LostIsland|Island|Decoration", meta = (ClampMin = "0", ClampMax = "60"))
	int32 CloudCount = 16;

	/** Full turns of the cloud layer per in-game day (a whole number, so there is no jump at midnight). */
	UPROPERTY(EditAnywhere, Category = "LostIsland|Island|Decoration", meta = (ClampMin = "0", ClampMax = "8"))
	int32 CloudTurnsPerDay = 1;

	// ---------------- Look (post process) ----------------
	UPROPERTY(EditAnywhere, Category = "LostIsland|Island|Rendering|Look", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float LookSaturation = 1.3f;

	UPROPERTY(EditAnywhere, Category = "LostIsland|Island|Rendering|Look", meta = (ClampMin = "0.5", ClampMax = "2.0"))
	float LookContrast = 0.95f;

	UPROPERTY(EditAnywhere, Category = "LostIsland|Island|Rendering|Look", meta = (ClampMin = "0.0", ClampMax = "4.0"))
	float LookBloomIntensity = 0.8f;

	UPROPERTY(EditAnywhere, Category = "LostIsland|Island|Rendering|Look", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LookVignetteIntensity = 0.15f;

	UPROPERTY(EditAnywhere, Category = "LostIsland|Island|Rendering|Look", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LookAmbientOcclusion = 0.35f;

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

	/**
	 * Brightness of the visible sky and of the ambient light it casts (sky atmosphere luminance factor). The sun is
	 * a non-physical 10 lux, so the physically correct sky would be near black under the -3 EV exposure; the sky
	 * light captures this brighter sky, so shaded sides get the same fill as before.
	 */
	UPROPERTY(EditAnywhere, Category = "LostIsland|Island|Rendering|DayNight", meta = (ClampMin = "1.0"))
	float SkyBrightness = 8.f;

	UPROPERTY(EditAnywhere, Category = "LostIsland|Island|Rendering|DayNight")
	FLinearColor NoonColor = FLinearColor(1.f, 0.92f, 0.78f);

	UPROPERTY(EditAnywhere, Category = "LostIsland|Island|Rendering|DayNight")
	FLinearColor SunsetColor = FLinearColor(1.f, 0.45f, 0.18f);

	/** Extra white-balance temperature (K) at sunset: the whole picture turns golden, not only the sunlight. */
	UPROPERTY(EditAnywhere, Category = "LostIsland|Island|Rendering|DayNight", meta = (ClampMin = "0.0"))
	float GoldenHourWarmth = 2600.f;

	/** Sky brightness multiplier at night (1 = as bright as the day sky). */
	UPROPERTY(EditAnywhere, Category = "LostIsland|Island|Rendering|DayNight", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float NightSkyFactor = 0.3f;

	bool bSkyWasDay = true;

	/** Last sky luminance / rain amount pushed to the components (to skip redundant updates). */
	float AppliedSkyLuminance = -1.f;
	float AppliedFogRain = -1.f;

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

	/** Cosmetic layers built with the terrain on every machine. Heights = the terrain grid's vertex heights. */
	void BuildShallowWater(const TArray<float>& Heights);
	void BuildDecorations(const TArray<float>& Heights);
	void BuildClouds();

	/** Realistic style: one smooth, vertex-coloured terrain section. Returns false if the material is unavailable. */
	bool BuildSmoothTerrain(const TArray<float>& Heights);

	/** Textured terrain: per-vertex blend weights (R grass, G rock, B sand, A forest floor), summing to 1. */
	FLinearColor GetTerrainBlendWeights(const FVector2D& LocalXY, float Height, float NormalZ) const;

	/** Exact height of the triangulated terrain surface (matches the mesh, unlike the smooth GetHeightAt). */
	float GetMeshHeightAt(const TArray<float>& Heights, const FVector2D& LocalXY) const;

	float Fbm(const FVector2D& P, int32 Octaves) const;
	FVector GetNormalAt(const FVector2D& LocalXY) const;
	float GetIslandRadius() const { return GetHalfExtent() * IslandSizeFraction; }
	FLinearColor GetSurfaceColor(const FVector2D& LocalXY, float NormalZ) const;

	UPROPERTY(ReplicatedUsing = OnRep_Seed)
	int32 Seed = 0;

	/** Replicates with the seed (same bunch), so clients build the same world type. */
	UPROPERTY(Replicated)
	bool bValley = false;

	float GetValleyHeightAt(const FVector2D& LocalXY) const;
	/** Valley floor without detail noise: what roads and the yard are flattened to. */
	float GetValleyFloorAt(const FVector2D& LocalXY) const;
	float GetRingRoadRadiusAt(float Angle) const;

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
