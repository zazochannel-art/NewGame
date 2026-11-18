// Copyright LOST ISLAND Team. All Rights Reserved.

#include "World/LIIslandGenerator.h"

#include "LostIsland.h"
#include "Resources/LIResourceNode.h"
#include "World/LIWaterSource.h"
#include "World/LICrashSite.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/DirectionalLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/PlatformTime.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "ProceduralMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

ALIIslandGenerator::ALIIslandGenerator()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true; // every client needs the seed to build the terrain

	TerrainMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("TerrainMesh"));
	SetRootComponent(TerrainMesh);
	TerrainMesh->bUseAsyncCooking = false; // collision must exist before the first pawn lands on it
	TerrainMesh->bUseComplexAsSimpleCollision = true;
	TerrainMesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	TerrainMesh->SetMobility(EComponentMobility::Movable); // runtime-built, no baked lighting

	WaterMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WaterMesh"));
	WaterMesh->SetupAttachment(TerrainMesh);
	WaterMesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	WaterMesh->SetCastShadow(false);
	WaterMesh->SetMobility(EComponentMobility::Movable);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneMesh.Succeeded())
	{
		WaterMesh->SetStaticMesh(PlaneMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> VertexColorMat(TEXT("/Engine/EngineDebugMaterials/VertexColorMaterial.VertexColorMaterial"));
	if (VertexColorMat.Succeeded())
	{
		TerrainMaterial = VertexColorMat.Object;
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicShapeMat(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (BasicShapeMat.Succeeded())
	{
		WaterMaterial = BasicShapeMat.Object;
	}

	// --- Self-contained environment (disabled at runtime if the level already has a sun) ---
	Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
	Sun->SetupAttachment(TerrainMesh);
	Sun->SetMobility(EComponentMobility::Movable);
	Sun->SetRelativeRotation(FRotator(-42.f, 35.f, 0.f));
	Sun->SetAtmosphereSunLight(true);

	SkyAtmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("SkyAtmosphere"));
	SkyAtmosphere->SetupAttachment(TerrainMesh);

	SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
	SkyLight->SetupAttachment(TerrainMesh);
	SkyLight->SetMobility(EComponentMobility::Movable);
	SkyLight->bRealTimeCapture = true;

	HeightFog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("HeightFog"));
	HeightFog->SetupAttachment(TerrainMesh);

	// --- Default resource rules (hard difficulty: resources are spread out) ---
	FLIResourceSpawnRule Trees;
	Trees.NodeClass = ALITreeNode::StaticClass();
	Trees.AllowedBiomes = { ELIIslandBiome::Forest };
	Trees.Count = 90;
	Trees.MinSpacing = 650.f;
	Trees.MinNormalZ = 0.85f;

	FLIResourceSpawnRule Rocks;
	Rocks.NodeClass = ALIRockNode::StaticClass();
	Rocks.AllowedBiomes = { ELIIslandBiome::Hills, ELIIslandBiome::Meadow };
	Rocks.Count = 45;
	Rocks.MinSpacing = 800.f;
	Rocks.MinNormalZ = 0.f;

	FLIResourceSpawnRule Bushes;
	Bushes.NodeClass = ALIFiberBushNode::StaticClass();
	Bushes.AllowedBiomes = { ELIIslandBiome::Forest, ELIIslandBiome::Meadow, ELIIslandBiome::Beach };
	Bushes.Count = 60;
	Bushes.MinSpacing = 450.f;
	Bushes.MinNormalZ = 0.8f;

	FLIResourceSpawnRule Berries;
	Berries.NodeClass = ALIBerryBushNode::StaticClass();
	Berries.AllowedBiomes = { ELIIslandBiome::Forest, ELIIslandBiome::Meadow };
	Berries.Count = 18;
	Berries.MinSpacing = 900.f;
	Berries.MinNormalZ = 0.85f;

	// Few, far-apart springs: water is the hardest need on this island.
	FLIResourceSpawnRule Springs;
	Springs.NodeClass = ALIWaterSource::StaticClass();
	Springs.AllowedBiomes = { ELIIslandBiome::Meadow, ELIIslandBiome::Forest, ELIIslandBiome::Hills };
	Springs.Count = 5;
	Springs.MinSpacing = 6000.f;
	Springs.MinNormalZ = 0.93f;

	CrashSiteClass = ALICrashSite::StaticClass();
	CrashSiteBiomes = { ELIIslandBiome::Beach, ELIIslandBiome::Meadow };

	// Order matters: springs first so their wide spacing is guaranteed, then sparse food, then the rest.
	SpawnRules = { Springs, Berries, Trees, Rocks, Bushes };
}

void ALIIslandGenerator::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ALIIslandGenerator, Seed);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void ALIIslandGenerator::GenerateIsland(int32 InSeed)
{
	if (!HasAuthority())
	{
		return;
	}

	const double StartTime = FPlatformTime::Seconds();

	ClearIsland();
	Seed = InSeed;
	BuildTerrain();
	BuildSpawnPoints();
	SpawnResourceNodes();
	ForceNetUpdate();

	UE_LOG(LogLostIsland, Log, TEXT("Island generated: seed %d, %d actors, crash site %s, %.0f ms."),
		Seed, SpawnedNodes.Num(), bHasCrashSite ? *GetCrashSiteTransform().GetLocation().ToCompactString() : TEXT("none"),
		(FPlatformTime::Seconds() - StartTime) * 1000.0);
}

void ALIIslandGenerator::ClearIsland()
{
	if (!HasAuthority())
	{
		return;
	}

	for (AActor* Node : SpawnedNodes)
	{
		if (IsValid(Node))
		{
			Node->Destroy();
		}
	}
	SpawnedNodes.Reset();
	SpawnPoints.Reset();
	bHasCrashSite = false;

	TerrainMesh->ClearAllMeshSections();
	bHasBuiltTerrain = false;
}

void ALIIslandGenerator::OnRep_Seed()
{
	// Clients rebuild the identical terrain locally; resource nodes arrive via replication.
	BuildTerrain();
}

// ---------------------------------------------------------------------------
// Height / biome functions (deterministic for a given Seed)
// ---------------------------------------------------------------------------

float ALIIslandGenerator::Fbm(const FVector2D& P, int32 Octaves) const
{
	float Sum = 0.f;
	float Amplitude = 1.f;
	float Norm = 0.f;
	FVector2D Q = P;
	for (int32 Octave = 0; Octave < Octaves; ++Octave)
	{
		Sum += Amplitude * FMath::PerlinNoise2D(Q);
		Norm += Amplitude;
		Amplitude *= 0.5f;
		Q *= 2.03f;
	}
	return Norm > 0.f ? Sum / Norm : 0.f; // roughly -1..1
}

float ALIIslandGenerator::GetHeightAt(const FVector2D& LocalXY) const
{
	const FVector2D N = LocalXY / GetIslandRadius();

	// Distance from centre, distorted by noise for an irregular coastline.
	const float Warp = Fbm(N * CoastNoiseFrequency + CoastOffset, 3) * CoastNoiseStrength;
	const float D = N.Size() + Warp;

	const float LandMask = 1.f - FMath::SmoothStep(0.7f, 1.0f, D); // 1 inland, 0 at sea
	const float Inland = 1.f - FMath::SmoothStep(0.1f, 0.8f, D);   // hills grow toward the centre

	// Tuned offline (Docs/Systems/Island.md): ~40% land, ~5% beach, hills peaking ~25-50 m.
	const float Hill = FMath::Clamp(Fbm(N * HillNoiseFrequency + HillOffset, 4) * HillContrast + 0.5f, 0.f, 1.f);
	const float Detail = Fbm(N * DetailNoiseFrequency + DetailOffset, 3);

	const float LandHeight = BaseLandHeight
		+ FMath::Pow(Hill, 1.5f) * HillHeight * (0.2f + 0.8f * Inland)
		+ Detail * DetailHeight;

	return FMath::Lerp(SeabedHeight, LandHeight, LandMask);
}

FVector ALIIslandGenerator::GetNormalAt(const FVector2D& LocalXY) const
{
	const float E = CellSize * 0.5f;
	const float Dx = GetHeightAt(LocalXY + FVector2D(E, 0.f)) - GetHeightAt(LocalXY - FVector2D(E, 0.f));
	const float Dy = GetHeightAt(LocalXY + FVector2D(0.f, E)) - GetHeightAt(LocalXY - FVector2D(0.f, E));
	return FVector(-Dx, -Dy, 2.f * E).GetSafeNormal();
}

ELIIslandBiome ALIIslandGenerator::GetBiomeAt(const FVector2D& LocalXY) const
{
	const float Height = GetHeightAt(LocalXY);
	if (Height < 0.f)
	{
		return ELIIslandBiome::Ocean;
	}
	if (Height < BeachMaxHeight)
	{
		return ELIIslandBiome::Beach;
	}
	if (Height > HillsMinHeight)
	{
		return ELIIslandBiome::Hills;
	}

	const FVector2D N = LocalXY / GetIslandRadius();
	const float ForestNoise = Fbm(N * ForestNoiseFrequency + ForestOffset, 3);
	return ForestNoise > ForestThreshold ? ELIIslandBiome::Forest : ELIIslandBiome::Meadow;
}

FLinearColor ALIIslandGenerator::GetSurfaceColor(const FVector2D& LocalXY, float NormalZ) const
{
	const float Height = GetHeightAt(LocalXY);
	const ELIIslandBiome Biome = GetBiomeAt(LocalXY);

	FLinearColor Color;
	if (NormalZ < RockSlopeNormalZ && Height > BeachMaxHeight)
	{
		Color = RockColor;
	}
	else
	{
		switch (Biome)
		{
		case ELIIslandBiome::Ocean:  Color = SeabedColor; break;
		case ELIIslandBiome::Beach:  Color = SandColor;   break;
		case ELIIslandBiome::Forest: Color = ForestColor; break;
		case ELIIslandBiome::Hills:
			Color = FLinearColor::LerpUsingHSV(HillsColor, RockColor,
				FMath::Clamp((Height - HillsMinHeight) / FMath::Max(1.f, HillHeight * 0.5f), 0.f, 1.f));
			break;
		case ELIIslandBiome::Meadow:
		default:                     Color = MeadowColor; break;
		}
	}

	// Small per-face brightness variation for the faceted low-poly look.
	const float Variation = 1.f + 0.06f * FMath::PerlinNoise2D(LocalXY / (CellSize * 1.7f) + DetailOffset);
	return FLinearColor(Color.R * Variation, Color.G * Variation, Color.B * Variation, 1.f);
}

// ---------------------------------------------------------------------------
// Terrain mesh (all machines)
// ---------------------------------------------------------------------------

void ALIIslandGenerator::BuildTerrain()
{
	if (bHasBuiltTerrain && BuiltSeed == Seed)
	{
		return;
	}

	// Seed -> noise offsets. FMath::PerlinNoise2D repeats every 256 units, so offsets stay in that range.
	FRandomStream Random(Seed);
	HillOffset   = FVector2D(Random.FRandRange(0.f, 256.f), Random.FRandRange(0.f, 256.f));
	DetailOffset = FVector2D(Random.FRandRange(0.f, 256.f), Random.FRandRange(0.f, 256.f));
	CoastOffset  = FVector2D(Random.FRandRange(0.f, 256.f), Random.FRandRange(0.f, 256.f));
	ForestOffset = FVector2D(Random.FRandRange(0.f, 256.f), Random.FRandRange(0.f, 256.f));

	SetupEnvironment();

	const int32 N = GridResolution;
	const int32 Stride = N + 1;
	const float Half = GetHalfExtent();

	// Heights at grid vertices.
	TArray<float> Heights;
	Heights.SetNumUninitialized(Stride * Stride);
	for (int32 Y = 0; Y <= N; ++Y)
	{
		for (int32 X = 0; X <= N; ++X)
		{
			Heights[Y * Stride + X] = GetHeightAt(FVector2D(-Half + X * CellSize, -Half + Y * CellSize));
		}
	}

	auto GridPoint = [&](int32 X, int32 Y)
	{
		return FVector(-Half + X * CellSize, -Half + Y * CellSize, Heights[Y * Stride + X]);
	};

	// Flat shading: every triangle has its own 3 vertices and a face normal.
	const int32 NumTriangles = N * N * 2;
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FLinearColor> Colors;
	TArray<FProcMeshTangent> Tangents;
	Vertices.Reserve(NumTriangles * 3);
	Triangles.Reserve(NumTriangles * 3);
	Normals.Reserve(NumTriangles * 3);
	UVs.Reserve(NumTriangles * 3);
	Colors.Reserve(NumTriangles * 3);

	auto AddTriangle = [&](FVector A, FVector B, FVector C)
	{
		// Unreal treats a triangle as front-facing when CrossProduct(B - A, C - A) points AWAY from
		// the viewer (same convention as UKismetProceduralMeshLibrary::GenerateBoxMesh). The terrain
		// must face up, so order the vertices so that cross product points down.
		FVector Cross = FVector::CrossProduct(B - A, C - A);
		if (Cross.Z > 0.f)
		{
			Swap(B, C);
			Cross = -Cross;
		}
		const FVector FaceNormal = (-Cross).GetSafeNormal(); // upward face normal for lighting
		if (bFlipWinding)
		{
			Swap(B, C);
		}

		const FVector Centroid = (A + B + C) / 3.f;
		const FLinearColor Color = GetSurfaceColor(FVector2D(Centroid.X, Centroid.Y), FaceNormal.Z);

		for (const FVector& V : { A, B, C })
		{
			Triangles.Add(Vertices.Num());
			Vertices.Add(V);
			Normals.Add(FaceNormal);
			UVs.Add(FVector2D(V.X, V.Y) / 1000.f);
			Colors.Add(Color);
		}
	};

	for (int32 Y = 0; Y < N; ++Y)
	{
		for (int32 X = 0; X < N; ++X)
		{
			const FVector P00 = GridPoint(X, Y);
			const FVector P10 = GridPoint(X + 1, Y);
			const FVector P01 = GridPoint(X, Y + 1);
			const FVector P11 = GridPoint(X + 1, Y + 1);

			// Alternate the diagonal so the facets don't line up into stripes.
			if (((X + Y) & 1) == 0)
			{
				AddTriangle(P00, P10, P11);
				AddTriangle(P00, P11, P01);
			}
			else
			{
				AddTriangle(P00, P10, P01);
				AddTriangle(P10, P11, P01);
			}
		}
	}

	TerrainMesh->ClearAllMeshSections();
	TerrainMesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, Colors, Tangents,
		/*bCreateCollision*/ true, /*bSRGBConversion*/ true);
	if (TerrainMaterial)
	{
		TerrainMesh->SetMaterial(0, TerrainMaterial);
	}

	// Water: engine plane is 100x100 cm; cover well past the island so the horizon reads as ocean.
	const float WaterSize = Half * 2.f * 4.f;
	WaterMesh->SetRelativeLocation(FVector::ZeroVector);
	WaterMesh->SetRelativeScale3D(FVector(WaterSize / 100.f, WaterSize / 100.f, 1.f));
	if (WaterMaterial)
	{
		UMaterialInstanceDynamic* WaterMID = UMaterialInstanceDynamic::Create(WaterMaterial, this);
		WaterMID->SetVectorParameterValue(TEXT("Color"), WaterColor); // BasicShapeMaterial exposes "Color"
		WaterMesh->SetMaterial(0, WaterMID);
	}

	BuiltSeed = Seed;
	bHasBuiltTerrain = true;
}

void ALIIslandGenerator::SetupEnvironment()
{
	bool bUseOwnEnvironment = bCreateEnvironmentIfMissing;
	if (bUseOwnEnvironment && GetWorld())
	{
		for (TActorIterator<ADirectionalLight> It(GetWorld()); It; ++It)
		{
			bUseOwnEnvironment = false; // the level provides its own lighting
			break;
		}
	}

	bOwnEnvironmentActive = bUseOwnEnvironment;
	Sun->SetVisibility(bUseOwnEnvironment);
	SkyAtmosphere->SetVisibility(bUseOwnEnvironment);
	SkyLight->SetVisibility(bUseOwnEnvironment);
	HeightFog->SetVisibility(bUseOwnEnvironment);
}

// ---------------------------------------------------------------------------
// Resources (server)
// ---------------------------------------------------------------------------

void ALIIslandGenerator::ApplyTimeOfDay(float Hours, float SunriseHours, float SunsetHours)
{
	if (!bOwnEnvironmentActive || !Sun)
	{
		return;
	}

	// The sun crosses the sky (0 -> 180 deg) between sunrise and sunset; the moon crosses it during
	// the rest of the day. Sunrise/sunset come from the game state's night window, so it gets dark
	// exactly when gameplay "night" (cold) begins.
	const float DayLength = FMath::Clamp(FMath::Fmod(SunsetHours - SunriseHours + 24.f, 24.f), 1.f, 23.f);
	const float SinceSunrise = FMath::Fmod(Hours - SunriseHours + 48.f, 24.f);
	const bool bSunUp = SinceSunrise < DayLength;

	float Elevation;
	float Intensity;
	FLinearColor Color;
	if (bSunUp)
	{
		Elevation = FMath::Clamp(SinceSunrise / DayLength * 180.f, 2.f, 178.f);
		const float Height01 = FMath::Sin(FMath::DegreesToRadians(Elevation)); // 0 at horizon, 1 at noon
		Intensity = SunIntensity * FMath::Clamp(Height01 * 1.5f, 0.05f, 1.f);
		Color = FLinearColor::LerpUsingHSV(SunsetColor, NoonColor, FMath::Clamp(Height01 * 2.f, 0.f, 1.f));
	}
	else
	{
		const float MoonElevation = (SinceSunrise - DayLength) / (24.f - DayLength) * 180.f;
		Elevation = FMath::Clamp(MoonElevation, 10.f, 170.f);
		Intensity = MoonIntensity;
		Color = MoonColor;
	}

	// A directional light shines along its forward vector: pitch down by the elevation.
	// Past 90 degrees the light comes from the opposite side of the sky.
	const float Yaw = Elevation <= 90.f ? SunYaw : SunYaw + 180.f;
	const float Pitch = Elevation <= 90.f ? -Elevation : -(180.f - Elevation);
	Sun->SetWorldRotation(FRotator(Pitch, Yaw, 0.f));
	Sun->SetIntensity(Intensity);
	Sun->SetLightColor(Color);
}

FTransform ALIIslandGenerator::GetCrashSiteTransform() const
{
	return CrashSiteLocalTransform * GetActorTransform();
}

void ALIIslandGenerator::ChooseCrashSite()
{
	bHasCrashSite = false;
	if (!CrashSiteClass)
	{
		return;
	}

	FRandomStream Random(static_cast<int32>(HashCombine(GetTypeHash(Seed), 0xC4A5Eu)));
	const float Radius = GetIslandRadius();
	const FVector2D SpawnXY = [this]()
	{
		const FVector Local = GetActorTransform().InverseTransformPosition(FindPlayerSpawnTransform().GetLocation());
		return FVector2D(Local.X, Local.Y);
	}();

	// Relax the distance-from-spawn requirement if a small island has no valid spot far enough away.
	for (const float DistanceScale : { 1.f, 0.5f, 0.f })
	{
		const float MinSpawnDistSq = FMath::Square(CrashSiteMinDistanceFromSpawn * DistanceScale);
		for (int32 Attempt = 0; Attempt < 600; ++Attempt)
		{
			const FVector2D XY(Random.FRandRange(-Radius, Radius), Random.FRandRange(-Radius, Radius));
			const float Yaw = Random.FRandRange(0.f, 360.f);

			if (!CrashSiteBiomes.Contains(GetBiomeAt(XY)) || FVector2D::DistSquared(XY, SpawnXY) < MinSpawnDistSq)
			{
				continue;
			}

			// The whole footprint must be dry and reasonably flat.
			const FVector2D Forward(FMath::Cos(FMath::DegreesToRadians(Yaw)), FMath::Sin(FMath::DegreesToRadians(Yaw)));
			const FVector2D Right(-Forward.Y, Forward.X);
			const FVector2D Samples[] =
			{
				XY, XY + Forward * CrashSiteFootprint, XY - Forward * CrashSiteFootprint * 2.f, // debris trail behind
				XY + Right * CrashSiteFootprint, XY - Right * CrashSiteFootprint,
			};

			bool bValid = true;
			float MinHeight = TNumericLimits<float>::Max();
			for (const FVector2D& Sample : Samples)
			{
				const float Height = GetHeightAt(Sample);
				MinHeight = FMath::Min(MinHeight, Height);
				if (Height < 60.f || GetNormalAt(Sample).Z < 0.88f)
				{
					bValid = false;
					break;
				}
			}
			if (!bValid)
			{
				continue;
			}

			// Sit on the lowest sampled point so nothing floats; the wreck is "dug in" anyway.
			CrashSiteLocalTransform = FTransform(FRotator(0.f, Yaw, 0.f), FVector(XY.X, XY.Y, FMath::Min(GetHeightAt(XY), MinHeight)));
			bHasCrashSite = true;
			return;
		}
	}

	UE_LOG(LogLostIsland, Warning, TEXT("Island seed %d: no valid crash site location found."), Seed);
}

void ALIIslandGenerator::BuildSpawnPoints()
{
	SpawnPoints.Reset();
	ChooseCrashSite();
	const FVector2D CrashXY(CrashSiteLocalTransform.GetLocation().X, CrashSiteLocalTransform.GetLocation().Y);
	const float CrashClearanceSq = FMath::Square(CrashSiteClearance);

	// Different stream from the terrain offsets, still derived from the seed.
	FRandomStream Random(static_cast<int32>(HashCombine(GetTypeHash(Seed), 0x2C1B3C6Du)));
	const float Radius = GetIslandRadius();

	for (const FLIResourceSpawnRule& Rule : SpawnRules)
	{
		if (!Rule.NodeClass || Rule.Count <= 0)
		{
			continue;
		}

		const float MinSpacingSq = FMath::Square(Rule.MinSpacing);
		const int32 MaxAttempts = Rule.Count * 40;
		int32 Placed = 0;

		for (int32 Attempt = 0; Attempt < MaxAttempts && Placed < Rule.Count; ++Attempt)
		{
			const FVector2D XY(Random.FRandRange(-Radius, Radius), Random.FRandRange(-Radius, Radius));
			const float Yaw = Random.FRandRange(0.f, 360.f); // always consume, keeps the sequence stable

			const ELIIslandBiome Biome = GetBiomeAt(XY);
			if (!Rule.AllowedBiomes.Contains(Biome))
			{
				continue;
			}
			if (GetNormalAt(XY).Z < Rule.MinNormalZ)
			{
				continue;
			}
			if (bHasCrashSite && FVector2D::DistSquared(XY, CrashXY) < CrashClearanceSq)
			{
				continue;
			}

			// Same-type points use the rule's spacing; other types only need a small clearance.
			bool bTooClose = false;
			for (const FLIResourceSpawnPoint& Existing : SpawnPoints)
			{
				const float RequiredSq = Existing.NodeClass == Rule.NodeClass ? MinSpacingSq : FMath::Square(OtherTypeClearance);
				if (FVector2D::DistSquared(FVector2D(Existing.Location.X, Existing.Location.Y), XY) < RequiredSq)
				{
					bTooClose = true;
					break;
				}
			}
			if (bTooClose)
			{
				continue;
			}

			FLIResourceSpawnPoint& Point = SpawnPoints.AddDefaulted_GetRef();
			Point.Location = FVector(XY.X, XY.Y, GetHeightAt(XY) - 10.f); // sink slightly so slopes don't show gaps
			Point.Rotation = FRotator(0.f, Yaw, 0.f);
			Point.Biome = Biome;
			Point.NodeClass = Rule.NodeClass;
			++Placed;
		}

		if (Placed < Rule.Count)
		{
			UE_LOG(LogLostIsland, Verbose, TEXT("Island: placed %d/%d %s (not enough valid space)."),
				Placed, Rule.Count, *GetNameSafe(Rule.NodeClass));
		}
	}
}

void ALIIslandGenerator::SpawnResourceNodes()
{
	UWorld* World = GetWorld();
	if (!World || !HasAuthority())
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	const FTransform& ActorTransform = GetActorTransform();
	SpawnedNodes.Reserve(SpawnPoints.Num() + 1);

	if (bHasCrashSite && CrashSiteClass)
	{
		if (AActor* Wreck = World->SpawnActor<AActor>(CrashSiteClass, GetCrashSiteTransform(), Params))
		{
			SpawnedNodes.Add(Wreck);
		}
	}

	for (const FLIResourceSpawnPoint& Point : SpawnPoints)
	{
		const FVector WorldLocation = ActorTransform.TransformPosition(Point.Location);
		const FRotator WorldRotation = ActorTransform.TransformRotation(Point.Rotation.Quaternion()).Rotator();
		if (AActor* Node = World->SpawnActor<AActor>(Point.NodeClass, WorldLocation, WorldRotation, Params))
		{
			SpawnedNodes.Add(Node);
		}
	}
}

FTransform ALIIslandGenerator::FindPlayerSpawnTransform() const
{
	FRandomStream Random(static_cast<int32>(HashCombine(GetTypeHash(Seed), 0x51A7u)));
	const float Radius = GetIslandRadius();

	for (int32 Attempt = 0; Attempt < 48; ++Attempt)
	{
		const float Angle = Random.FRandRange(0.f, 2.f * PI);
		const FVector2D Dir(FMath::Cos(Angle), FMath::Sin(Angle));

		// Walk outward from inland until we hit dry, flat beach.
		for (float T = 0.4f; T <= 1.3f; T += 0.01f)
		{
			const FVector2D XY = Dir * (T * Radius);
			const float Height = GetHeightAt(XY);
			if (Height > 50.f && Height < BeachMaxHeight && GetNormalAt(XY).Z > 0.9f)
			{
				const FVector Local(XY.X, XY.Y, Height + 120.f);
				const FRotator FaceInland(0.f, FMath::RadiansToDegrees(FMath::Atan2(-Dir.Y, -Dir.X)), 0.f);
				return FTransform(FaceInland, GetActorTransform().TransformPosition(Local));
			}
		}
	}

	// Fallback: island centre.
	return FTransform(FRotator::ZeroRotator, GetActorTransform().TransformPosition(FVector(0.f, 0.f, GetHeightAt(FVector2D::ZeroVector) + 200.f)));
}
