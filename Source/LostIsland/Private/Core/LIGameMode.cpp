// Copyright LOST ISLAND Team. All Rights Reserved.

#include "Core/LIGameMode.h"

#include "Character/LICharacter.h"
#include "Core/LIPlayerController.h"
#include "Core/LIGameState.h"
#include "World/LICampfire.h"
#include "World/LIDroppedLoot.h"
#include "World/LISleepingBag.h"
#include "World/LIBuildingPiece.h"
#include "World/LIRaft.h"
#include "World/LIStorageChest.h"
#include "Inventory/LIInventoryComponent.h"
#include "EngineUtils.h"
#include "Interaction/LIInteractableActor.h"
#include "Resources/LIResourceNode.h"
#include "World/LIIslandGenerator.h"
#include "Save/LISaveGame.h"
#include "Save/LISaveSubsystem.h"
#include "LostIsland.h"
#include "UI/LIHUD.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "LIGameMode"

ALIGameMode::ALIGameMode()
{
	DefaultPawnClass = ALICharacter::StaticClass();
	PlayerControllerClass = ALIPlayerController::StaticClass();
	HUDClass = ALIHUD::StaticClass();
	GameStateClass = ALIGameState::StaticClass();
	PrototypeTestActorClass = ALIInteractableActor::StaticClass();
	PrototypeResourceClasses = { ALITreeNode::StaticClass(), ALIRockNode::StaticClass(), ALIFiberBushNode::StaticClass() };
	IslandGeneratorClass = ALIIslandGenerator::StaticClass();
}

void ALIGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	// Decide now (before the island and the first pawn exist) whether this session continues a save.
	// After an in-game Load / New Game restart the original URL options are still on the travel URL;
	// they must not override the loaded save's seed or force the same "new" island again.
	bool bHonourUrlOptions = true;
	if (ULISaveSubsystem* SaveSubsystem = ULISaveSubsystem::Get(this))
	{
		bHonourUrlOptions = SaveSubsystem->PrepareForNewMap(Options);
	}

	const FString SeedOption = UGameplayStatics::ParseOption(Options, TEXT("Seed"));
	UrlSeed = (bHonourUrlOptions && !SeedOption.IsEmpty()) ? FCString::Atoi(*SeedOption) : 0;
}

void ALIGameMode::StartPlay()
{
	// Local players can be spawned before StartPlay, so ChoosePlayerStart may already have built the island.
	EnsureIslandGenerated();

	Super::StartPlay();

	// Clock and placed structures from a continued save (after GameState BeginPlay set its defaults).
	if (ULISaveSubsystem* SaveSubsystem = ULISaveSubsystem::Get(this))
	{
		SaveSubsystem->ApplyWorldState();
	}

	if (bSpawnPrototypeTestActors || !bGenerateIsland)
	{
		SpawnPrototypeTestActors();
	}

	if (AutosaveInterval > 0.f)
	{
		GetWorldTimerManager().SetTimer(AutosaveTimerHandle, this, &ALIGameMode::Autosave, AutosaveInterval, true);
	}
}

void ALIGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(AutosaveTimerHandle);
	Super::EndPlay(EndPlayReason);
}

void ALIGameMode::Autosave()
{
	if (ULISaveSubsystem* SaveSubsystem = ULISaveSubsystem::Get(this))
	{
		SaveSubsystem->SaveCurrentGame();
	}
}

void ALIGameMode::FinishRestartPlayer(AController* NewPlayer, const FRotator& StartRotation)
{
	Super::FinishRestartPlayer(NewPlayer, StartRotation);

	// First spawn of a player in a continued game: restore position, stats and inventory.
	if (ULISaveSubsystem* SaveSubsystem = ULISaveSubsystem::Get(this))
	{
		SaveSubsystem->ApplyToPlayer(NewPlayer);
	}
}

int32 ALIGameMode::ChooseIslandSeed() const
{
	if (UrlSeed != 0)
	{
		return UrlSeed;
	}
	if (const ULISaveSubsystem* SaveSubsystem = ULISaveSubsystem::Get(this))
	{
		const ULISaveGame* Save = SaveSubsystem->GetPendingSave();
		if (Save && Save->WorldSeed != 0)
		{
			return Save->WorldSeed;
		}
	}
	if (IslandSeed != 0)
	{
		return IslandSeed;
	}
	return FMath::RandRange(1, MAX_int32 - 1);
}

void ALIGameMode::EnsureIslandGenerated()
{
	if (!bGenerateIsland || Island || !IslandGeneratorClass)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Island = World->SpawnActor<ALIIslandGenerator>(IslandGeneratorClass, FTransform::Identity, Params);
	if (!Island)
	{
		UE_LOG(LogLostIsland, Error, TEXT("Failed to spawn the island generator."));
		return;
	}

	Island->GenerateIsland(ChooseIslandSeed());

	IslandPlayerStart = World->SpawnActor<APlayerStart>(APlayerStart::StaticClass(), Island->FindPlayerSpawnTransform(), Params);
}

AActor* ALIGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	EnsureIslandGenerated();
	if (IslandPlayerStart)
	{
		return IslandPlayerStart;
	}
	return Super::ChoosePlayerStart_Implementation(Player);
}

void ALIGameMode::RegenerateIsland(int32 NewSeed)
{
	if (!Island)
	{
		EnsureIslandGenerated();
		if (!Island)
		{
			return;
		}
	}

	// Player structures belong to the old terrain.
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->IsA<ALICampfire>() || It->IsA<ALISleepingBag>() || It->IsA<ALIDroppedLoot>() || It->IsA<ALIBuildingPiece>() || It->IsA<ALIRaft>() || It->IsA<ALIStorageChest>())
		{
			It->Destroy();
		}
	}

	Island->GenerateIsland(NewSeed != 0 ? NewSeed : FMath::RandRange(1, MAX_int32 - 1));

	// PlayerStart has a static root and cannot be moved at runtime: replace it.
	const FTransform Spawn = Island->FindPlayerSpawnTransform();
	if (IslandPlayerStart)
	{
		IslandPlayerStart->Destroy();
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	IslandPlayerStart = GetWorld()->SpawnActor<APlayerStart>(APlayerStart::StaticClass(), Spawn, Params);

	// Move everyone to the new beach, spaced out so capsules don't overlap.
	int32 Index = 0;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (APawn* Pawn = It->IsValid() ? (*It)->GetPawn() : nullptr)
		{
			const FVector Offset = Spawn.GetRotation().GetRightVector() * (Index++ * 150.f);
			Pawn->TeleportTo(Spawn.GetLocation() + Offset, Spawn.Rotator());
		}
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

// ---------------------------------------------------------------------------
// Death and respawn
// ---------------------------------------------------------------------------

void ALIGameMode::HandlePlayerDeath(ALICharacter* DeadCharacter)
{
	if (!DeadCharacter)
	{
		return;
	}

	// Everything the survivor carried stays where they fell.
	if (ULIInventoryComponent* Inventory = DeadCharacter->GetInventoryComponent())
	{
		bool bHasItems = false;
		for (const FLIInventorySlot& Slot : Inventory->GetSlots())
		{
			bHasItems |= !Slot.IsEmpty();
		}
		if (bHasItems)
		{
			const FVector FeetLocation = DeadCharacter->GetActorLocation()
				- FVector(0.f, 0.f, DeadCharacter->GetSimpleCollisionHalfHeight() - 25.f);
			if (ALIDroppedLoot* Loot = GetWorld()->SpawnActorDeferred<ALIDroppedLoot>(ALIDroppedLoot::StaticClass(),
				FTransform(FeetLocation), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
			{
				Loot->SetContents(Inventory->GetSlots());
				Loot->FinishSpawning(FTransform(FeetLocation));
				Inventory->ClearInventory();
				UE_LOG(LogLostIsland, Log, TEXT("%s dropped its inventory at %s."), *DeadCharacter->GetName(), *FeetLocation.ToCompactString());
			}
		}
	}

	AController* Controller = DeadCharacter->GetController();
	if (!Controller)
	{
		return;
	}
	// Always deferred: this runs inside the dying pawn's death handler, which must not destroy it.
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle,
		FTimerDelegate::CreateWeakLambda(this, [this, WeakController = TWeakObjectPtr<AController>(Controller)]()
		{
			AController* DeadController = WeakController.Get();
			const ALICharacter* Current = DeadController ? Cast<ALICharacter>(DeadController->GetPawn()) : nullptr;
			if (DeadController && (!Current || Current->IsDead())) // not already revived (LIRevive)
			{
				RespawnPlayer(DeadController);
			}
		}),
		FMath::Max(RespawnDelay, 0.1f), false);
}

void ALIGameMode::RespawnPlayer(AController* Controller)
{
	if (!Controller)
	{
		return;
	}
	if (APawn* OldPawn = Controller->GetPawn())
	{
		Controller->UnPossess();
		OldPawn->Destroy();
	}

	if (const ALISleepingBag* Bag = ALISleepingBag::FindBagFor(GetWorld(), ULISaveSubsystem::GetPlayerKey(Controller)))
	{
		RestartPlayerAtTransform(Controller, Bag->GetRespawnTransform());
		UE_LOG(LogLostIsland, Log, TEXT("%s respawned at sleeping bag %s."), *GetNameSafe(Controller), *Bag->GetName());
	}
	else
	{
		RestartPlayer(Controller);
		UE_LOG(LogLostIsland, Log, TEXT("%s respawned on the beach (no sleeping bag)."), *GetNameSafe(Controller));
	}
}
