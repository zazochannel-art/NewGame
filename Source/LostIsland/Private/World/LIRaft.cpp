// Copyright LOST ISLAND Team. All Rights Reserved.

#include "World/LIRaft.h"

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
