// Copyright LOST ISLAND Team. All Rights Reserved.

#include "Core/LIGameMode.h"

#include "Character/LICharacter.h"
#include "Core/LIPlayerController.h"
#include "Interaction/LIInteractableActor.h"
#include "Resources/LIResourceNode.h"
#include "LostIsland.h"
#include "UI/LIHUD.h"

#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"

#define LOCTEXT_NAMESPACE "LIGameMode"

ALIGameMode::ALIGameMode()
{
	DefaultPawnClass = ALICharacter::StaticClass();
	PlayerControllerClass = ALIPlayerController::StaticClass();
	HUDClass = ALIHUD::StaticClass();
	PrototypeTestActorClass = ALIInteractableActor::StaticClass();
	PrototypeResourceClasses = { ALITreeNode::StaticClass(), ALIRockNode::StaticClass(), ALIFiberBushNode::StaticClass() };
}

void ALIGameMode::StartPlay()
{
	Super::StartPlay();

	if (bSpawnPrototypeTestActors)
	{
		SpawnPrototypeTestActors();
	}
}

AActor* ALIGameMode::SpawnOnGround(TSubclassOf<AActor> Class, const FVector& Location, const FRotator& Rotation, float GroundOffset, bool bDeferred)
{
	UWorld* World = GetWorld();
	if (!World || !Class)
	{
		return nullptr;
	}

	FVector SpawnLocation = Location;
	FHitResult Hit;
	// Object-type query so pawns and other dynamic actors never count as "ground".
	const FCollisionObjectQueryParams GroundObjects(ECC_WorldStatic);
	if (World->LineTraceSingleByObjectType(Hit, Location + FVector(0.f, 0.f, 500.f), Location - FVector(0.f, 0.f, 2000.f), GroundObjects))
	{
		SpawnLocation.Z = Hit.ImpactPoint.Z + GroundOffset;
	}

	const FTransform SpawnTransform(Rotation, SpawnLocation);
	if (bDeferred)
	{
		return World->SpawnActorDeferred<AActor>(Class, SpawnTransform, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	return World->SpawnActor<AActor>(Class, SpawnTransform, Params);
}

void ALIGameMode::SpawnPrototypeTestActors()
{
	FTransform Origin = FTransform::Identity;
	if (const AActor* Start = UGameplayStatics::GetActorOfClass(this, APlayerStart::StaticClass()))
	{
		Origin = FTransform(FRotator(0.f, Start->GetActorRotation().Yaw, 0.f), Start->GetActorLocation());
	}

	// --- Interaction test crates in front of the player ---
	struct FTestSpec
	{
		FVector LocalOffset;
		FText Prompt;
		int32 MaxUses;
	};
	const FTestSpec Specs[] =
	{
		{ FVector(400.f, -200.f, 0.f), LOCTEXT("Inspect", "Inspect Crate"), 0 },
		{ FVector(400.f,    0.f, 0.f), LOCTEXT("Search",  "Search Crate (3 uses)"), 3 },
		{ FVector(400.f,  200.f, 0.f), LOCTEXT("Open",    "Open Crate (1 use)"), 1 },
	};

	int32 Spawned = 0;
	if (PrototypeTestActorClass)
	{
		for (const FTestSpec& Spec : Specs)
		{
			// Deferred so per-instance settings are in place before BeginPlay. Engine cube: 100 cm, centred pivot.
			AActor* Actor = SpawnOnGround(PrototypeTestActorClass, Origin.TransformPosition(Spec.LocalOffset), Origin.Rotator(), 50.f, /*bDeferred*/ true);
			if (ALIInteractableActor* Crate = Cast<ALIInteractableActor>(Actor))
			{
				Crate->SetPromptText(Spec.Prompt);
				Crate->SetMaxUses(Spec.MaxUses);
				Crate->FinishSpawning(Crate->GetActorTransform());
				++Spawned;
			}
		}
	}

	// --- Resource nodes in a ring behind and beside the player (deterministic layout) ---
	FRandomStream Random(1337);
	const int32 Total = PrototypeResourceClasses.Num() * PrototypeNodesPerClass;
	int32 NodeIndex = 0;
	for (const TSubclassOf<ALIResourceNode>& NodeClass : PrototypeResourceClasses)
	{
		for (int32 Copy = 0; Copy < PrototypeNodesPerClass; ++Copy, ++NodeIndex)
		{
			// Spread over 240 degrees, leaving the crate area (in front) free.
			const float Angle = FMath::DegreesToRadians(60.f + 240.f * (NodeIndex + 0.5f) / FMath::Max(1, Total));
			const float Radius = Random.FRandRange(700.f, 1100.f);
			const FVector Local(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.f);
			const FRotator Yaw(0.f, Random.FRandRange(0.f, 360.f), 0.f);

			// Resource node origin is its base, so no extra ground offset.
			if (SpawnOnGround(NodeClass, Origin.TransformPosition(Local), Yaw, 0.f))
			{
				++Spawned;
			}
		}
	}

	UE_LOG(LogLostIsland, Log, TEXT("Spawned %d prototype actors."), Spawned);
}

#undef LOCTEXT_NAMESPACE
