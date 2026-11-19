// Copyright LOST ISLAND Team. All Rights Reserved.

#include "World/LIRaft.h"

#include "Math/RandomStream.h"
#include "ProceduralMeshComponent.h"
#include "Visuals/LILowPolyMesh.h"
#include "Visuals/LIVisualAssets.h"
#include "Materials/MaterialInterface.h"
#include "LostIsland.h"
#include "Core/LIGameState.h"
#include "Survival/LISurvivalComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "LIRaft"

ALIRaft::ALIRaft()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// 400 x 250 cm log deck floating at the root (sea level), a mast and a fibre sail.
	Deck = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Deck"));
	Deck->SetupAttachment(Root);
	Deck->SetStaticMesh(CubeMesh.Object);
	Deck->SetRelativeScale3D(FVector(4.f, 2.5f, 0.3f));
	Deck->SetRelativeLocation(FVector(0.f, 0.f, 5.f));
	Deck->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

	Mast = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mast"));
	Mast->SetupAttachment(Root);
	Mast->SetStaticMesh(CylinderMesh.Object);
	Mast->SetRelativeScale3D(FVector(0.15f, 0.15f, 3.5f));
	Mast->SetRelativeLocation(FVector(30.f, 0.f, 195.f));
	Mast->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

	Sail = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Sail"));
	Sail->SetupAttachment(Root);
	Sail->SetStaticMesh(CubeMesh.Object);
	Sail->SetRelativeScale3D(FVector(0.05f, 2.f, 2.2f));
	Sail->SetRelativeLocation(FVector(40.f, 0.f, 230.f));
	Sail->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
}

void ALIRaft::BeginPlay()
{
	Super::BeginPlay();
	LITintPlaceholder(Deck, FLinearColor(0.38f, 0.25f, 0.12f));
	LITintPlaceholder(Mast, FLinearColor(0.30f, 0.20f, 0.10f));
	LITintPlaceholder(Sail, FLinearColor(0.80f, 0.75f, 0.60f));

	// Detailed model (cosmetic, every machine): lashed logs, cross beams, mast, yard and a billowing sail.
	const FLinearColor LogColor(0.46f, 0.32f, 0.17f);
	const FLinearColor BeamColor(0.30f, 0.20f, 0.10f);
	const FLinearColor Rope(0.62f, 0.55f, 0.38f);
	const FLinearColor Canvas(0.82f, 0.76f, 0.62f);
	FRandomStream Random(LIVisualSeed(GetActorLocation(), 0x4AF7));
	FLILowPolyMesh Model;

	const int32 Logs = 7;
	for (int32 Index = 0; Index < Logs; ++Index)
	{
		const float Y = -107.f + Index * (214.f / (Logs - 1));
		const float R = Random.FRandRange(15.f, 18.f);
		const float Length = Random.FRandRange(385.f, 410.f);
		Model.AddFrustum(FTransform(FRotator(-90.f, 0.f, 0.f), FVector(-Length * 0.5f, Y, 5.f)), 9, R, R * 0.9f, Length, LogColor, 0.05f, &Random);
	}
	for (const float X : { -150.f, 150.f })
	{
		Model.AddBox(FTransform(FVector(X, 0.f, 25.f)), FVector(8.f, 128.f, 5.f), BeamColor);
		Model.AddBox(FTransform(FVector(X + 22.f, 0.f, 5.f)), FVector(3.f, 126.f, 19.f), Rope); // lashing
	}
	Model.AddBox(FTransform(FVector(-120.f, 55.f, 47.f)), FVector(20.f, 20.f, 18.f), BeamColor); // supply crate

	// Mast and yard.
	Model.AddFrustum(FTransform(FVector(30.f, 0.f, 20.f)), 8, 9.f, 6.f, 350.f, BeamColor, 0.03f, &Random);
	Model.AddFrustum(FTransform(FRotator(0.f, 0.f, -90.f), FVector(36.f, -110.f, 338.f)), 6, 5.f, 4.f, 220.f, BeamColor);

	// Sail: a curved sheet (both sides visible), bellied toward +X.
	const int32 Columns = 6;
	const int32 Rows = 6;
	auto SailPoint = [](float U, float V)
	{
		const float Belly = 18.f * FMath::Sin(U * PI) * FMath::Sin(V * PI);
		return FVector(44.f + Belly, -100.f + U * 200.f, 130.f + V * 205.f);
	};
	for (int32 Column = 0; Column < Columns; ++Column)
	{
		for (int32 Row = 0; Row < Rows; ++Row)
		{
			const float U0 = static_cast<float>(Column) / Columns;
			const float U1 = static_cast<float>(Column + 1) / Columns;
			const float V0 = static_cast<float>(Row) / Rows;
			const float V1 = static_cast<float>(Row + 1) / Rows;
			const FVector A = SailPoint(U0, V0), B = SailPoint(U1, V0), C = SailPoint(U1, V1), D = SailPoint(U0, V1);
			const FVector Centre = (A + B + C + D) * 0.25f;
			for (const float Side : { -1.f, 1.f })
			{
				const FVector Interior = Centre + FVector(Side * 50.f, 0.f, 0.f);
				Model.AddTri(A, B, C, Canvas, Interior);
				Model.AddTri(A, C, D, Canvas, Interior);
			}
		}
	}

	const ULIVisualAssets& Assets = ULIVisualAssets::Get();
	Model.SetSurfaceMaterial(LogColor, ULIVisualAssets::LoadOptional<UMaterialInterface>(Assets.WoodMaterial));
	Model.SetSurfaceMaterial(BeamColor, ULIVisualAssets::LoadOptional<UMaterialInterface>(Assets.DarkWoodMaterial));
	if (Model.ApplyTo(LICreateVisualComponent(this)))
	{
		Deck->SetHiddenInGame(true); // keeps its collision
		Mast->SetHiddenInGame(true);
		Sail->SetHiddenInGame(true);
	}
}

bool ALIRaft::CanInteract_Implementation(APawn* InstigatorPawn) const
{
	const ULISurvivalComponent* Survival = InstigatorPawn ? InstigatorPawn->FindComponentByClass<ULISurvivalComponent>() : nullptr;
	const ALIGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ALIGameState>() : nullptr;
	return Survival && !Survival->IsDead() && GameState && !GameState->HasEscaped();
}

FText ALIRaft::GetInteractionPrompt_Implementation(APawn* InstigatorPawn) const
{
	return LOCTEXT("SailAway", "Sail away from the island");
}

void ALIRaft::Interact_Implementation(APawn* InstigatorPawn)
{
	if (!HasAuthority())
	{
		return;
	}
	if (ALIGameState* GameState = GetWorld()->GetGameState<ALIGameState>())
	{
		GameState->SetEscaped();
	}
}

bool ALIRaft::ComputePlacement(const UWorld* World, const FVector& Location, const FVector& Forward, const AActor* Builder, FTransform& OutTransform)
{
	if (!World)
	{
		return false;
	}
	const FVector Flat = FVector(Forward.X, Forward.Y, 0.f).GetSafeNormal();
	const FVector Probe = Location + Flat * 350.f;

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(LIRaftPlace), false, Builder);
	if (!World->LineTraceSingleByObjectType(Hit, FVector(Probe.X, Probe.Y, 500.f), FVector(Probe.X, Probe.Y, -1000.f),
		FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		return false;
	}
	// Sea level is Z = 0 (island generator origin). Shallow water only: you build it at the shore.
	const float Depth = -Hit.ImpactPoint.Z;
	if (Depth < 30.f || Depth > 250.f)
	{
		return false;
	}
	OutTransform = FTransform(FRotator(0.f, Flat.Rotation().Yaw, 0.f), FVector(Probe.X, Probe.Y, 0.f));
	return true;
}

#undef LOCTEXT_NAMESPACE
