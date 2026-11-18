// Copyright LOST ISLAND Team. All Rights Reserved.

#include "Core/LIGameMode.h"

#include "Character/LICharacter.h"
#include "Core/LIPlayerController.h"
#include "Interaction/LIInteractableActor.h"
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
}

void ALIGameMode::StartPlay()
{
	Super::StartPlay();

	if (bSpawnPrototypeTestActors)
	{
		SpawnPrototypeTestActors();
	}
}

void ALIGameMode::SpawnPrototypeTestActors()
{
	UWorld* World = GetWorld();
	if (!World || !PrototypeTestActorClass)
	{
		return;
	}

	FTransform Origin = FTransform::Identity;
	if (const AActor* Start = UGameplayStatics::GetActorOfClass(this, APlayerStart::StaticClass()))
	{
		Origin = FTransform(FRotator(0.f, Start->GetActorRotation().Yaw, 0.f), Start->GetActorLocation());
	}

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

	for (const FTestSpec& Spec : Specs)
	{
		FVector Location = Origin.TransformPosition(Spec.LocalOffset);

		// Drop onto the ground so the cube rests on whatever floor the map has.
		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, Location + FVector(0.f, 0.f, 500.f), Location - FVector(0.f, 0.f, 2000.f), ECC_WorldStatic))
		{
			Location.Z = Hit.ImpactPoint.Z + 50.f; // Engine cube is 100 cm with a centred pivot.
		}

		// Deferred spawn so per-instance settings are in place before BeginPlay.
		const FTransform SpawnTransform(Origin.Rotator(), Location);
		ALIInteractableActor* Actor = World->SpawnActorDeferred<ALIInteractableActor>(PrototypeTestActorClass,
			SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
		if (Actor)
		{
			Actor->SetPromptText(Spec.Prompt);
			Actor->SetMaxUses(Spec.MaxUses);
			Actor->FinishSpawning(SpawnTransform);
		}
	}

	UE_LOG(LogLostIsland, Log, TEXT("Spawned %d prototype interactables."), static_cast<int32>(UE_ARRAY_COUNT(Specs)));
}

#undef LOCTEXT_NAMESPACE
