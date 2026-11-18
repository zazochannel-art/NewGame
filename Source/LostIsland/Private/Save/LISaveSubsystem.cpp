// Copyright LOST ISLAND Team. All Rights Reserved.

#include "Save/LISaveSubsystem.h"

#include "Save/LISaveGame.h"
#include "Character/LICharacter.h"
#include "Core/LIGameMode.h"
#include "Core/LIPlayerController.h"
#include "Inventory/LIInventoryComponent.h"
#include "LostIsland.h"
#include "Survival/LISurvivalComponent.h"
#include "World/LIIslandGenerator.h"
#include "World/LICampfire.h"
#include "World/LISleepingBag.h"
#include "World/LIBuildingPiece.h"
#include "World/LIRaft.h"
#include "World/LIStorageChest.h"
#include "World/LICrashSite.h"
#include "Resources/LIResourceNode.h"
#include "Core/LIGameState.h"
#include "EngineUtils.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"

#define LOCTEXT_NAMESPACE "LISave"

ULISaveSubsystem* ULISaveSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<ULISaveSubsystem>() : nullptr;
}

bool ULISaveSubsystem::HasSaveGame() const
{
	return UGameplayStatics::DoesSaveGameExist(SlotName, 0);
}

ULISaveGame* ULISaveSubsystem::LoadFromDisk() const
{
	if (!HasSaveGame())
	{
		return nullptr;
	}

	ULISaveGame* Save = Cast<ULISaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
	if (!Save)
	{
		UE_LOG(LogLostIsland, Warning, TEXT("Save slot '%s' exists but could not be read as ULISaveGame."), *SlotName);
		return nullptr;
	}

	if (Save->SaveVersion > ULISaveGame::CurrentVersion)
	{
		UE_LOG(LogLostIsland, Warning, TEXT("Save '%s' is from a newer game version (%d > %d); ignoring."),
			*SlotName, Save->SaveVersion, ULISaveGame::CurrentVersion);
		return nullptr;
	}
	if (Save->SaveVersion < 2)
	{
		// v1 -> v2: no clock or structures stored; the defaults (day 1, 08:00, none) apply.
		Save->Day = 1;
		Save->Hours = 8.f;
		Save->Structures.Reset();
	}
	if (Save->SaveVersion < 3)
	{
		// v2 -> v3: world resources were not stored; the wreck and nodes start fresh.
		Save->CrashSiteSearchesLeft = -1;
		Save->ResourceNodes.Reset();
	}

	return Save;
}

// ---------------------------------------------------------------------------
// Map start
// ---------------------------------------------------------------------------

bool ULISaveSubsystem::PrepareForNewMap(const FString& Options)
{
	AppliedPlayerKeys.Reset();
	const bool bHonourUrlOptions = PendingIntent == EPendingIntent::None;

	const bool bUrlNewGame = UGameplayStatics::HasOption(Options, TEXT("NewGame"))
		|| !UGameplayStatics::ParseOption(Options, TEXT("Seed")).IsEmpty();

	switch (PendingIntent)
	{
	case EPendingIntent::Load:
		// PendingSave was already loaded by LoadAndRestart.
		break;
	case EPendingIntent::NewGame:
		PendingSave = nullptr;
		break;
	case EPendingIntent::None:
	default:
		PendingSave = (!bUrlNewGame && bLoadSaveOnStart) ? LoadFromDisk() : nullptr;
		break;
	}
	PendingIntent = EPendingIntent::None;

	if (PendingSave)
	{
		UE_LOG(LogLostIsland, Log, TEXT("Continuing save '%s' (seed %d, %d player(s), saved %s)."),
			*SlotName, PendingSave->WorldSeed, PendingSave->Players.Num(), *PendingSave->SavedAt.ToString());
	}
	return bHonourUrlOptions;
}

// ---------------------------------------------------------------------------
// Save
// ---------------------------------------------------------------------------

FString ULISaveSubsystem::GetPlayerKey(const AController* Controller)
{
	if (!Controller)
	{
		return FString();
	}

	if (Controller->IsLocalController())
	{
		return TEXT("Host");
	}

	const APlayerState* PlayerState = Controller->GetPlayerState<APlayerState>();
	if (PlayerState && PlayerState->GetUniqueId().IsValid())
	{
		return PlayerState->GetUniqueId().ToString();
	}
	return PlayerState ? PlayerState->GetPlayerName() : Controller->GetName();
}

void ULISaveSubsystem::CapturePlayer(const ALICharacter* Character, FLIPlayerSaveData& OutData)
{
	OutData.Location = Character->GetActorLocation();
	OutData.Rotation = Character->GetController() ? Character->GetController()->GetControlRotation() : Character->GetActorRotation();

	if (const ULISurvivalComponent* Survival = Character->GetSurvivalComponent())
	{
		OutData.Health  = Survival->GetStatValue(ELISurvivalStat::Health);
		OutData.Hunger  = Survival->GetStatValue(ELISurvivalStat::Hunger);
		OutData.Thirst  = Survival->GetStatValue(ELISurvivalStat::Thirst);
		OutData.Stamina = Survival->GetStatValue(ELISurvivalStat::Stamina);
	}

	if (const ULIInventoryComponent* Inventory = Character->GetInventoryComponent())
	{
		OutData.InventorySlots = Inventory->GetSlots();
	}
}

bool ULISaveSubsystem::SaveCurrentGame()
{
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	const ALIGameMode* GameMode = World ? World->GetAuthGameMode<ALIGameMode>() : nullptr;
	if (!GameMode)
	{
		UE_LOG(LogLostIsland, Warning, TEXT("SaveCurrentGame: only the server can save."));
		return false;
	}

	ULISaveGame* Save = Cast<ULISaveGame>(UGameplayStatics::CreateSaveGameObject(ULISaveGame::StaticClass()));
	Save->SaveVersion = ULISaveGame::CurrentVersion;
	Save->SavedAt = FDateTime::Now();
	Save->WorldSeed = GameMode->GetIsland() ? GameMode->GetIsland()->GetSeed() : 0;

	if (const ALIGameState* GameState = World->GetGameState<ALIGameState>())
	{
		Save->Day = GameState->GetDayNumber();
		Save->Hours = GameState->GetTimeOfDayHours();
	}
	for (TActorIterator<ALICampfire> It(World); It; ++It)
	{
		FLIStructureSaveData& Structure = Save->Structures.AddDefaulted_GetRef();
		Structure.ActorClass = It->GetClass();
		Structure.Transform = It->GetActorTransform();
		Structure.Fuel = It->GetFuel();
	}
	for (TActorIterator<ALISleepingBag> It(World); It; ++It)
	{
		FLIStructureSaveData& Structure = Save->Structures.AddDefaulted_GetRef();
		Structure.ActorClass = It->GetClass();
		Structure.Transform = It->GetActorTransform();
		Structure.OwnerKey = It->GetOwnerKey();
	}
	for (TActorIterator<ALIStorageChest> It(World); It; ++It)
	{
		FLIStructureSaveData& Structure = Save->Structures.AddDefaulted_GetRef();
		Structure.ActorClass = It->GetClass();
		Structure.Transform = It->GetActorTransform();
		Structure.Slots = It->GetSlots();
	}
	for (TActorIterator<ALIRaft> It(World); It; ++It)
	{
		FLIStructureSaveData& Structure = Save->Structures.AddDefaulted_GetRef();
		Structure.ActorClass = It->GetClass();
		Structure.Transform = It->GetActorTransform();
	}
	for (TActorIterator<ALIBuildingPiece> It(World); It; ++It)
	{
		if (It->IsPreview())
		{
			continue; // the host's build-mode ghost
		}
		FLIStructureSaveData& Structure = Save->Structures.AddDefaulted_GetRef();
		Structure.ActorClass = It->GetClass();
		Structure.Transform = It->GetActorTransform();
	}

	// Wreck and resource nodes, by their deterministic spawn index.
	if (const ALIIslandGenerator* Island = GameMode->GetIsland())
	{
		const TArray<TObjectPtr<AActor>>& Spawned = Island->GetSpawnedNodes();
		for (int32 Index = 0; Index < Spawned.Num(); ++Index)
		{
			const AActor* Actor = Spawned[Index].Get();
			if (const ALICrashSite* Wreck = Cast<ALICrashSite>(Actor))
			{
				Save->CrashSiteSearchesLeft = Wreck->GetSearchesLeft();
			}
			else if (const ALIResourceNode* Node = Cast<ALIResourceNode>(Actor))
			{
				if (Node->IsDepleted() || Node->GetRemainingHits() < Node->GetHitsToDeplete())
				{
					FLIResourceNodeSaveData& Data = Save->ResourceNodes.AddDefaulted_GetRef();
					Data.Index = Index;
					Data.ClassName = Node->GetClass()->GetFName();
					Data.RemainingHits = Node->GetRemainingHits();
					Data.bDepleted = Node->IsDepleted();
					Data.RespawnSecondsRemaining = Node->GetRespawnTimeRemaining();
				}
			}
		}
	}

	// Keep entries of players who are offline right now (co-op), replace the rest.
	if (PendingSave)
	{
		Save->Players = PendingSave->Players;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		const ALICharacter* Character = PC ? Cast<ALICharacter>(PC->GetPawn()) : nullptr;
		if (!Character || Character->IsDead())
		{
			continue; // dead players keep their previous saved state
		}

		const FString Key = GetPlayerKey(PC);
		FLIPlayerSaveData* Data = Save->Players.FindByPredicate([&Key](const FLIPlayerSaveData& D) { return D.PlayerKey == Key; });
		if (!Data)
		{
			Data = &Save->Players.AddDefaulted_GetRef();
			Data->PlayerKey = Key;
		}
		CapturePlayer(Character, *Data);
	}

	// Keep this object as the session's reference save, so later saves merge with it.
	PendingSave = Save;

	const FString Slot = SlotName;
	FAsyncSaveGameToSlotDelegate OnSaved;
	OnSaved.BindLambda([Slot](const FString&, int32, bool bSuccess)
	{
		UE_LOG(LogLostIsland, Log, TEXT("Save '%s' %s."), *Slot, bSuccess ? TEXT("written") : TEXT("FAILED"));
	});
	UGameplayStatics::AsyncSaveGameToSlot(Save, SlotName, 0, OnSaved);

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (ALIPlayerController* PC = Cast<ALIPlayerController>(It->Get()))
		{
			PC->ClientShowMessage(LOCTEXT("Saved", "Game saved"));
		}
	}
	return true;
}

// ---------------------------------------------------------------------------
// Load / new game
// ---------------------------------------------------------------------------

void ULISaveSubsystem::ApplyToPlayer(AController* Controller)
{
	ALICharacter* Character = Controller ? Cast<ALICharacter>(Controller->GetPawn()) : nullptr;
	if (!Character)
	{
		return;
	}

	// Only a player's FIRST spawn in this map session may restore saved state. Mark every first
	// spawn (even in a new game without a save), so a respawn after death (LIRevive) always starts
	// fresh, even if a save or autosave was written in the meantime.
	const FString Key = GetPlayerKey(Controller);
	bool bAlreadySpawned = false;
	AppliedPlayerKeys.Add(Key, &bAlreadySpawned);
	if (bAlreadySpawned || !PendingSave)
	{
		return;
	}

	const FLIPlayerSaveData* Data = PendingSave->FindPlayer(Key);
	if (!Data)
	{
		return; // new co-op player joining a saved world
	}

	Character->TeleportTo(Data->Location, FRotator(0.f, Data->Rotation.Yaw, 0.f), false, true);
	Controller->SetControlRotation(Data->Rotation);

	if (ULISurvivalComponent* Survival = Character->GetSurvivalComponent())
	{
		// A save never stores a dead player, but guard against corrupt data.
		Survival->SetStatValue(ELISurvivalStat::Health,  FMath::Max(Data->Health, 1.f));
		Survival->SetStatValue(ELISurvivalStat::Hunger,  Data->Hunger);
		Survival->SetStatValue(ELISurvivalStat::Thirst,  Data->Thirst);
		Survival->SetStatValue(ELISurvivalStat::Stamina, Data->Stamina);
	}

	if (ULIInventoryComponent* Inventory = Character->GetInventoryComponent())
	{
		Inventory->SetSlots(Data->InventorySlots);
	}

	UE_LOG(LogLostIsland, Log, TEXT("Restored saved state for player '%s'."), *Key);
}

void ULISaveSubsystem::ApplyWorldState()
{
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!PendingSave || !World || World->GetNetMode() == NM_Client)
	{
		return;
	}

	if (ALIGameState* GameState = World->GetGameState<ALIGameState>())
	{
		GameState->SetTime(PendingSave->Day, PendingSave->Hours);
	}

	for (const FLIStructureSaveData& Structure : PendingSave->Structures)
	{
		UClass* Class = Structure.ActorClass.LoadSynchronous();
		if (!Class)
		{
			continue;
		}
		AActor* Actor = World->SpawnActorDeferred<AActor>(Class, Structure.Transform, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (ALICampfire* Campfire = Cast<ALICampfire>(Actor))
		{
			Campfire->SetFuel(Structure.Fuel);
		}
		if (Actor)
		{
			Actor->FinishSpawning(Structure.Transform);
			if (ALIStorageChest* Chest = Cast<ALIStorageChest>(Actor))
			{
				Chest->SetSlots(Structure.Slots);
			}
			if (ALISleepingBag* Bag = Cast<ALISleepingBag>(Actor))
			{
				Bag->ClaimFor(Structure.OwnerKey);
			}
		}
	}

	// Wreck and resource nodes. The island was rebuilt from the same seed, so indices match.
	int32 RestoredNodes = 0;
	const ALIGameMode* GameMode = World->GetAuthGameMode<ALIGameMode>();
	if (const ALIIslandGenerator* Island = GameMode ? GameMode->GetIsland() : nullptr)
	{
		const TArray<TObjectPtr<AActor>>& Spawned = Island->GetSpawnedNodes();
		if (PendingSave->CrashSiteSearchesLeft >= 0)
		{
			for (const TObjectPtr<AActor>& Actor : Spawned)
			{
				if (ALICrashSite* Wreck = Cast<ALICrashSite>(Actor.Get()))
				{
					Wreck->SetSearchesLeft(PendingSave->CrashSiteSearchesLeft);
					break;
				}
			}
		}
		for (const FLIResourceNodeSaveData& Data : PendingSave->ResourceNodes)
		{
			ALIResourceNode* Node = Spawned.IsValidIndex(Data.Index) ? Cast<ALIResourceNode>(Spawned[Data.Index].Get()) : nullptr;
			if (!Node || Node->GetClass()->GetFName() != Data.ClassName)
			{
				continue; // spawn rules changed since the save: leave that node fresh
			}
			Node->RestoreSavedState(Data.RemainingHits, Data.bDepleted, Data.RespawnSecondsRemaining);
			++RestoredNodes;
		}
	}

	UE_LOG(LogLostIsland, Log, TEXT("Restored world: day %d %.1fh, %d structure(s), %d gathered node(s), wreck searches %d."),
		PendingSave->Day, PendingSave->Hours, PendingSave->Structures.Num(), RestoredNodes, PendingSave->CrashSiteSearchesLeft);
}

bool ULISaveSubsystem::LoadAndRestart()
{
	ULISaveGame* Save = LoadFromDisk();
	if (!Save)
	{
		UE_LOG(LogLostIsland, Warning, TEXT("No valid save in slot '%s'."), *SlotName);
		return false;
	}

	PendingSave = Save;
	PendingIntent = EPendingIntent::Load;
	RestartLevel();
	return true;
}

void ULISaveSubsystem::NewGameAndRestart()
{
	if (HasSaveGame())
	{
		UGameplayStatics::DeleteGameInSlot(SlotName, 0);
	}
	PendingSave = nullptr;
	PendingIntent = EPendingIntent::NewGame;
	RestartLevel();
}

void ULISaveSubsystem::RestartLevel()
{
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	if (World->GetNetMode() == NM_Standalone)
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			PC->RestartLevel();
			return;
		}
	}

	// Listen server: bring connected clients along.
	World->ServerTravel(TEXT("?Restart"));
}

#undef LOCTEXT_NAMESPACE
