// Copyright LOST ISLAND Team. All Rights Reserved.

#include "World/LIRainEffect.h"

#include "Core/LIGameState.h"
#include "Visuals/LILowPolyMesh.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "UObject/ConstructorHelpers.h"

ALIRainEffect::ALIRainEffect()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	bReplicates = false;
	SetCanBeDamaged(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> DropShape(TEXT("/Engine/BasicShapes/Cube.Cube"));
	Drops = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Drops"));
	RootComponent = Drops;
	Drops->SetMobility(EComponentMobility::Movable);
	Drops->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Drops->SetCastShadow(false);
	Drops->SetCanEverAffectNavigation(false);
	Drops->SetGenerateOverlapEvents(false);
	if (DropShape.Succeeded())
	{
		Drops->SetStaticMesh(DropShape.Object);
	}
}

void ALIRainEffect::BeginPlay()
{
	Super::BeginPlay();
	Drops->SetMaterial(0, LIGetTintedMaterial(DropColor));
}

FVector ALIRainEffect::RandomDropPoint(const FVector& Camera, bool bAnyHeight) const
{
	const float Angle = FMath::FRandRange(0.f, 2.f * PI);
	const float Distance = Radius * FMath::Sqrt(FMath::FRand()); // even spread over the disc
	const float Height = bAnyHeight ? FMath::FRandRange(-500.f, 1400.f) : FMath::FRandRange(900.f, 1400.f);
	return Camera + FVector(FMath::Cos(Angle) * Distance, FMath::Sin(Angle) * Distance, Height);
}

void ALIRainEffect::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const UWorld* World = GetWorld();
	const ALIGameState* GameState = World ? World->GetGameState<ALIGameState>() : nullptr;
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	const float Rain = GameState ? GameState->GetRainAmount() : 0.f;
	const int32 Wanted = FMath::RoundToInt(MaxDrops * FMath::Clamp(Rain, 0.f, 1.f));
	if (!PC || !PC->PlayerCameraManager || Wanted <= 0 || !Drops->GetStaticMesh())
	{
		if (Drops->GetInstanceCount() > 0)
		{
			Drops->ClearInstances();
			DropLocations.Reset();
		}
		return;
	}

	const FVector Camera = PC->PlayerCameraManager->GetCameraLocation();
	const FVector Velocity = FVector(Wind.X, Wind.Y, -FallSpeed);

	// Grow or shrink the pool with the rain; new drops start anywhere in the column.
	while (DropLocations.Num() < Wanted)
	{
		DropLocations.Add(RandomDropPoint(Camera, true));
	}
	if (DropLocations.Num() > Wanted)
	{
		DropLocations.SetNum(Wanted);
	}

	// Streaks lean along their motion: 1.2 cm thick, 45 cm long (the cube is 100 cm).
	const FQuat Lean = FQuat::FindBetweenNormals(FVector::UpVector, -Velocity.GetSafeNormal());
	const FVector Scale(0.012f, 0.012f, 0.45f);
	const float RadiusSq = FMath::Square(Radius);

	Transforms.Reset(DropLocations.Num());
	for (FVector& Drop : DropLocations)
	{
		Drop += Velocity * DeltaSeconds;
		// Below the camera, or left behind as the camera moves: start again at the top.
		if (Drop.Z < Camera.Z - 600.f || FVector::DistSquared2D(Drop, Camera) > RadiusSq)
		{
			Drop = RandomDropPoint(Camera, false);
		}
		Transforms.Add(FTransform(Lean, Drop, Scale));
	}

	if (Drops->GetInstanceCount() != Transforms.Num())
	{
		Drops->ClearInstances();
		Drops->AddInstances(Transforms, /*bShouldReturnIndices*/ false, /*bWorldSpace*/ true);
	}
	else
	{
		Drops->BatchUpdateInstancesTransforms(0, Transforms, /*bWorldSpace*/ true, /*bMarkRenderStateDirty*/ true, /*bTeleport*/ true);
	}
}
