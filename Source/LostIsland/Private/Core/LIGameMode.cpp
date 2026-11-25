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
#include "Vehicles/LIVehicle.h"
#include "Vehicles/LIFarmVehicles.h"
#include "Vehicles/LIVehicleDealer.h"
#include "Farm/LIFarmField.h"
#include "Construction/LIConstruction.h"

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
	StartingVehicles = { ALIPickupTruck::StaticClass(), ALITractor::StaticClass(), ALIDumpTruck::StaticClass() };
}

void ALIGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	// World type first: it picks the save slot that PrepareForNewMap reads. The option stays on the URL after an
	// in-game Load / New Game restart, which is what we want (the same world type again).
	const FString WorldOption = UGameplayStatics::ParseOption(Options, TEXT("World"));
	if (WorldOption.Equals(TEXT("Island"), ESearchCase::IgnoreCase))
	{
		bValleyWorld = false;
	}
	else if (WorldOption.Equals(TEXT("Valley"), ESearchCase::IgnoreCase))
	{
		bValleyWorld = true;
	}

	// Decide now (before the island and the first pawn exist) whether this session continues a save.
	// After an in-game Load / New Game restart the original URL options are still on the travel URL;
	// they must not override the loaded save's seed or force the same "new" island again.
	bool bHonourUrlOptions = true;
	if (ULISaveSubsystem* SaveSubsystem = ULISaveSubsystem::Get(this))
	{
		SaveSubsystem->SlotName = bValleyWorld ? TEXT("Valley_Slot0") : TEXT("LostIsland_Slot0");
		bHonourUrlOptions = SaveSubsystem->PrepareForNewMap(Options);
	}

	const FString SeedOption = UGameplayStatics::ParseOption(Options, TEXT("Seed"));
	UrlSeed = (bHonourUrlOptions && !SeedOption.IsEmpty()) ? FCString::Atoi(*SeedOption) : 0;
}

void ALIGameMode::StartPlay()
{
	// Local players can be spawned before StartPlay, so ChoosePlayerStart may already have built the island.
	EnsureIslandGenerated();
	SpawnFarm(); // before the save is applied: it restores the fields
	SpawnConstruction(); // likewise for building progress

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

	SpawnDealers();
	SpawnStartingVehicles(/*bFromSave*/ true);

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

	Island->SetValleyMode(bValleyWorld);
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
	SpawnFarm();
	SpawnConstruction();
	SpawnDealers();
	SpawnStartingVehicles();

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

void ALIGameMode::SpawnStartingVehicles(bool bFromSave)
{
	UWorld* World = GetWorld();
	for (TActorIterator<ALIVehicle> It(World); It; ++It)
	{
		It->ExitVehicle(); // the driver gets their character back first
		It->Destroy();
	}
	if (!Island || !Island->IsValley())
	{
		return;
	}

	const ULISaveSubsystem* SaveSubsystem = bFromSave ? ULISaveSubsystem::Get(this) : nullptr;
	const ULISaveGame* Save = SaveSubsystem ? SaveSubsystem->GetPendingSave() : nullptr;
	if (Save && Save->bVehiclesSaved)
	{
		TArray<ALIGrainTrailer*> HitchedTrailers;
		for (const FLIVehicleSaveData& Data : Save->Vehicles)
		{
			UClass* Class = Data.VehicleClass.LoadSynchronous();
			if (!Class || !Class->IsChildOf(ALIVehicle::StaticClass()))
			{
				continue;
			}
			FTransform Transform = Data.Transform;
			Transform.AddToTranslation(FVector(0.f, 0.f, 30.f)); // settle back onto the wheels
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
			if (ALIVehicle* Vehicle = World->SpawnActor<ALIVehicle>(Class, Transform, Params))
			{
				Vehicle->ApplySaveState(Data.Mode, Data.Amount);
				if (Data.bHitched)
				{
					if (ALIGrainTrailer* Trailer = Cast<ALIGrainTrailer>(Vehicle))
					{
						HitchedTrailers.Add(Trailer);
					}
				}
			}
		}
		for (ALIGrainTrailer* Trailer : HitchedTrailers)
		{
			// The nearest free tractor (HitchTo lines the trailer up behind it again).
			ALITractor* Best = nullptr;
			double BestDistSq = FMath::Square(2000.0);
			for (TActorIterator<ALITractor> It(World); It; ++It)
			{
				const double DistSq = FVector::DistSquared(It->GetActorLocation(), Trailer->GetActorLocation());
				if (!It->GetTrailer() && DistSq < BestDistSq)
				{
					BestDistSq = DistSq;
					Best = *It;
				}
			}
			Trailer->HitchTo(Best);
		}
		UE_LOG(LogLostIsland, Log, TEXT("Restored %d vehicles from the save."), Save->Vehicles.Num());
		return;
	}

	const TArray<FTransform> Spots = Island->GetVehicleSpawnTransforms(StartingVehicles.Num());
	for (int32 Index = 0; Index < StartingVehicles.Num() && Index < Spots.Num(); ++Index)
	{
		if (StartingVehicles[Index])
		{
			SpawnOnGround(StartingVehicles[Index], Spots[Index].GetLocation(), Spots[Index].Rotator(), 130.f); // drops onto its wheels
		}
	}

	// The grain trailer starts hitched behind the tractor (HitchTo lines it up on the coupling).
	for (TActorIterator<ALITractor> It(World); It; ++It)
	{
		const FTransform TractorTransform = It->GetActorTransform();
		const FVector Behind = TractorTransform.TransformPosition(It->GetTowPoint() - FVector(470.f, 0.f, 0.f));
		if (ALIGrainTrailer* Trailer = Cast<ALIGrainTrailer>(SpawnOnGround(ALIGrainTrailer::StaticClass(), Behind, TractorTransform.Rotator(), 130.f)))
		{
			Trailer->HitchTo(*It);
		}
		break;
	}
}

void ALIGameMode::SpawnFarm()
{
	UWorld* World = GetWorld();
	for (TActorIterator<ALIFarmField> It(World); It; ++It)
	{
		It->Destroy();
	}
	for (TActorIterator<ALIGrainSilo> It(World); It; ++It)
	{
		It->Destroy();
	}
	const bool bValley = Island && Island->IsValley();
	if (ALIGameState* LIGameState = GetGameState<ALIGameState>())
	{
		LIGameState->SetFarmEconomyEnabled(bValley);
	}
	if (!bValley)
	{
		return;
	}

	constexpr float CellSize = 200.f; // matches the terrain grid, so cells line up with the flattened ground
	const TArray<FBox2D> Rects = Island->GetFieldRects();
	for (int32 Index = 0; Index < Rects.Num(); ++Index)
	{
		const FVector2D Size = Rects[Index].GetSize();
		const FTransform Transform(Island->GetFieldCenterWorld(Rects[Index]));
		if (ALIFarmField* Field = World->SpawnActorDeferred<ALIFarmField>(ALIFarmField::StaticClass(), Transform, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			Field->Setup(Index, FMath::RoundToInt(Size.X / CellSize), FMath::RoundToInt(Size.Y / CellSize), CellSize);
			Field->FinishSpawning(Transform);
		}
	}

	// The grain buyer in the north-west corner of the yard.
	const FVector SiloLocation = Island->GetActorTransform().TransformPosition(FVector(-1500.f, 1500.f, 0.f));
	SpawnOnGround(ALIGrainSilo::StaticClass(), SiloLocation, FRotator::ZeroRotator, 520.f);
}

void ALIGameMode::SpawnDealers()
{
	UWorld* World = GetWorld();
	for (TActorIterator<ALIVehicleDealer> It(World); It; ++It)
	{
		It->Destroy();
	}
	if (!Island || !Island->IsValley())
	{
		return;
	}

	// West side of the yard: boards on the yard side (x -1230), bays behind them toward the edge (x ~ -1700).
	struct FOffer { TSubclassOf<ALIVehicle> Class; int32 Price; float Y; };
	const FOffer Offers[] =
	{
		{ ALIPickupTruck::StaticClass(),  300,  900.f },
		{ ALITractor::StaticClass(),      700,  450.f },
		{ ALIDumpTruck::StaticClass(),    1000, 0.f },
		{ ALIGrainTrailer::StaticClass(), 250,  -450.f },
	};
	const FTransform IslandTransform = Island->GetActorTransform();
	const FRotator Facing(0.f, IslandTransform.Rotator().Yaw, 0.f); // bays face +X, into the yard
	for (const FOffer& Offer : Offers)
	{
		const FVector Location = IslandTransform.TransformPosition(FVector(-1230.f, Offer.Y, 0.f));
		if (ALIVehicleDealer* Dealer = Cast<ALIVehicleDealer>(SpawnOnGround(ALIVehicleDealer::StaticClass(), Location, Facing, 120.f, /*bDeferred*/ true)))
		{
			Dealer->Setup(Offer.Class, Offer.Price);
			Dealer->FinishSpawning(Dealer->GetActorTransform());
		}
	}
}

void ALIGameMode::SpawnConstruction()
{
	UWorld* World = GetWorld();
	for (TActorIterator<ALIMaterialDepot> It(World); It; ++It)
	{
		It->Destroy();
	}
	for (TActorIterator<ALIConstructionSite> It(World); It; ++It)
	{
		It->Destroy();
	}
	if (ALIGameState* LIGameState = GetGameState<ALIGameState>())
	{
		LIGameState->SetGrainPriceMultiplier(1.f); // a restored, finished barn sets it again
	}
	if (!Island || !Island->IsValley())
	{
		return;
	}

	// Yard layout (island space; the yard is the flat square of +-YardHalfSize around the centre, the main road
	// crosses it along Y ~ 0): depots line the east edge, the plots sit in the south half.
	const FTransform IslandTransform = Island->GetActorTransform();
	struct FDepotSpot { ELIBuildMaterial Material; FVector Local; };
	const FDepotSpot Depots[] =
	{
		{ ELIBuildMaterial::Gravel, FVector(1650.f, 1750.f, 0.f) },
		{ ELIBuildMaterial::Lumber, FVector(1650.f, 750.f, 0.f) },
		{ ELIBuildMaterial::Bricks, FVector(1650.f, -850.f, 0.f) },
	};
	for (const FDepotSpot& Spot : Depots)
	{
		const FRotator Yaw(0.f, IslandTransform.Rotator().Yaw + 180.f, 0.f); // sign and loading side face the yard (-X)
		if (ALIMaterialDepot* Depot = Cast<ALIMaterialDepot>(SpawnOnGround(ALIMaterialDepot::StaticClass(),
			IslandTransform.TransformPosition(Spot.Local), Yaw, 120.f, /*bDeferred*/ true)))
		{
			Depot->Setup(Spot.Material);
			Depot->FinishSpawning(Depot->GetActorTransform());
		}
	}

	struct FSiteSpot { ELIBuildingType Type; FVector Local; float Yaw; };
	const FSiteSpot Sites[] =
	{
		{ ELIBuildingType::Barn,  FVector(-1250.f, -1250.f, 0.f), 0.f },  // doors toward the yard centre (+X)
		{ ELIBuildingType::House, FVector(450.f, -1500.f, 0.f),   90.f }, // door toward the road (+Y)
	};
	for (int32 Index = 0; Index < int32(UE_ARRAY_COUNT(Sites)); ++Index)
	{
		const FRotator Rotation(0.f, IslandTransform.Rotator().Yaw + Sites[Index].Yaw, 0.f);
		if (ALIConstructionSite* Site = Cast<ALIConstructionSite>(SpawnOnGround(ALIConstructionSite::StaticClass(),
			IslandTransform.TransformPosition(Sites[Index].Local), Rotation, 0.f, /*bDeferred*/ true)))
		{
			Site->Setup(Sites[Index].Type, Index);
			Site->FinishSpawning(Site->GetActorTransform());
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
