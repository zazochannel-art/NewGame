// Copyright LOST ISLAND Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/SaveGame.h"
#include "Items/LIItemTypes.h"
#include "LISaveGame.generated.h"

/** Persistent state of one player. Keyed so co-op hosts can save every player. */
USTRUCT(BlueprintType)
struct LOSTISLAND_API FLIPlayerSaveData
{
	GENERATED_BODY()

	/** "Host" for the local/listen-server player, otherwise the player's unique net ID. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Save")
	FString PlayerKey;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Save")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Save")
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Save")
	float Health = 100.f;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Save")
	float Hunger = 100.f;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Save")
	float Thirst = 100.f;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Save")
	float Stamina = 100.f;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Save")
	TArray<FLIInventorySlot> InventorySlots;
};

/** A player-built structure (v2: campfires). */
USTRUCT(BlueprintType)
struct LOSTISLAND_API FLIStructureSaveData
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Save")
	TSoftClassPtr<AActor> ActorClass;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Save")
	FTransform Transform;

	/** Campfire fuel in seconds. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Save")
	float Fuel = 0.f;

	/** Sleeping bag owner (save subsystem player key). Empty for other structures. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Save")
	FString OwnerKey;

	/** Storage chest contents. Empty for other structures. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Save")
	TArray<FLIInventorySlot> Slots;
};

/** A resource node that is not in its fresh state (v3). Nodes at full hits are not stored. */
USTRUCT(BlueprintType)
struct LOSTISLAND_API FLIResourceNodeSaveData
{
	GENERATED_BODY()

	/** Index into ALIIslandGenerator::GetSpawnedNodes() (deterministic for the world seed). */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Save")
	int32 Index = INDEX_NONE;

	/** Class name, to detect a mismatch if the generator's spawn rules changed between versions. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Save")
	FName ClassName;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Save")
	int32 RemainingHits = 0;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Save")
	bool bDepleted = false;

	/** Seconds until regrowth for a depleted node; -1 = none pending. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Save")
	float RespawnSecondsRemaining = -1.f;
};

/**
 * ULISaveGame
 *
 * v1: world seed + per-player position, survival stats and inventory.
 * v2: + world clock (day, hour) and placed structures (campfires). v1 saves load with
 *     default time (day 1, 08:00) and no structures.
 * v3: + crash-site searches left and gathered/depleted resource nodes, so saving and
 *     loading no longer refills the world (no loot duplication). v1/v2 saves load with
 *     a fresh wreck and fresh nodes.
 * Bump CurrentVersion and handle older SaveVersion values in ULISaveSubsystem when
 * the format changes.
 */
/** One farm field: a state byte and growth per cell (see ALIFarmField). */
USTRUCT(BlueprintType)
struct FLIFieldSaveData
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	int32 FieldIndex = 0;

	UPROPERTY(SaveGame)
	TArray<uint8> Cells;

	UPROPERTY(SaveGame)
	TArray<float> Growth;
};

/** One construction site: loads delivered so far (see ALIConstructionSite). */
USTRUCT(BlueprintType)
struct FLISiteSaveData
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	int32 SiteIndex = 0;

	UPROPERTY(SaveGame)
	int32 DeliveredLoads = 0;
};

UCLASS()
class LOSTISLAND_API ULISaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	static constexpr int32 CurrentVersion = 3;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Save")
	int32 SaveVersion = CurrentVersion;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Save")
	int32 WorldSeed = 0;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Save")
	FDateTime SavedAt;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Save")
	TArray<FLIPlayerSaveData> Players;

	// ---- v2 ----
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Save")
	int32 Day = 1;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Save")
	float Hours = 8.f;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Save")
	TArray<FLIStructureSaveData> Structures;

	// ---- v3 ----
	/** Searches left at the wreck; -1 = not stored (fresh wreck). */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Save")
	int32 CrashSiteSearchesLeft = -1;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Save")
	TArray<FLIResourceNodeSaveData> ResourceNodes;

	// ---- Farm (valley); Money -1 = not stored ----
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Save")
	int32 Money = -1;

	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Save")
	int32 GrainKg = 0;

	UPROPERTY(SaveGame)
	TArray<FLIFieldSaveData> Fields;

	UPROPERTY(SaveGame)
	TArray<FLISiteSaveData> Sites;

	const FLIPlayerSaveData* FindPlayer(const FString& Key) const
	{
		return Players.FindByPredicate([&Key](const FLIPlayerSaveData& Data) { return Data.PlayerKey == Key; });
	}
};
