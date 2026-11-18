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
};

/**
 * ULISaveGame
 *
 * v1: world seed + per-player position, survival stats and inventory.
 * v2: + world clock (day, hour) and placed structures (campfires). v1 saves load with
 *     default time (day 1, 08:00) and no structures.
 * Bump CurrentVersion and handle older SaveVersion values in ULISaveSubsystem when
 * the format changes.
 */
UCLASS()
class LOSTISLAND_API ULISaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	static constexpr int32 CurrentVersion = 2;

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

	const FLIPlayerSaveData* FindPlayer(const FString& Key) const
	{
		return Players.FindByPredicate([&Key](const FLIPlayerSaveData& Data) { return Data.PlayerKey == Key; });
	}
};
