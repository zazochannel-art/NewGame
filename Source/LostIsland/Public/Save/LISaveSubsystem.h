// Copyright LOST ISLAND Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "LISaveSubsystem.generated.h"

class AController;
class ALICharacter;
class ULISaveGame;
struct FLIPlayerSaveData;

/**
 * ULISaveSubsystem
 *
 * Owns saving and loading. Lives on the GameInstance, so a loaded save survives
 * the level restart that applies it.
 *
 * Flow:
 *  - Save (server): SaveCurrentGame() gathers the island seed and every player's
 *    pawn state, then writes asynchronously (no hitch).
 *  - Start of a map (ALIGameMode::InitGame -> PrepareForNewMap): unless the URL has
 *    ?NewGame or ?Seed=, the save slot is loaded into PendingSave.
 *  - The GameMode builds the island from PendingSave->WorldSeed and calls
 *    ApplyToPlayer() for each player as their pawn spawns (once per player).
 *  - LoadAndRestart() / NewGameAndRestart() reload the level to apply a save or start fresh.
 */
UCLASS()
class LOSTISLAND_API ULISaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static ULISaveSubsystem* Get(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "LostIsland|Save")
	bool HasSaveGame() const;

	/** Server: write the current world and players to disk (async). Returns false if nothing could be saved. */
	UFUNCTION(BlueprintCallable, Category = "LostIsland|Save")
	bool SaveCurrentGame();

	/** Server: load the save from disk and restart the level with it. */
	UFUNCTION(BlueprintCallable, Category = "LostIsland|Save")
	bool LoadAndRestart();

	/** Server: delete the save and restart the level on a fresh random island. */
	UFUNCTION(BlueprintCallable, Category = "LostIsland|Save")
	void NewGameAndRestart();

	/**
	 * Called by the GameMode at InitGame. Decides whether this map session uses the save.
	 * Returns false when this map start is a restart requested by LoadAndRestart / NewGameAndRestart:
	 * the URL options (?Seed=, ?NewGame) are left over from the original launch and must be ignored.
	 */
	bool PrepareForNewMap(const FString& Options);

	/** The save being applied in this map session, or null for a new game. */
	const ULISaveGame* GetPendingSave() const { return PendingSave; }

	/** Server, once per map session after the island exists: restore clock and structures. */
	void ApplyWorldState();

	/** Apply saved state to a freshly spawned player (first spawn only). */
	void ApplyToPlayer(AController* Controller);

	/** Stable key for a player: "Host" for the local listen-server/standalone player, else the unique net ID. */
	static FString GetPlayerKey(const AController* Controller);

	UPROPERTY(EditDefaultsOnly, Category = "LostIsland|Save")
	FString SlotName = TEXT("LostIsland_Slot0");

	/** Load the save automatically when a map starts (no main menu yet). */
	UPROPERTY(EditDefaultsOnly, Category = "LostIsland|Save")
	bool bLoadSaveOnStart = true;

private:
	ULISaveGame* LoadFromDisk() const;
	void RestartLevel();
	static void CapturePlayer(const ALICharacter* Character, FLIPlayerSaveData& OutData);

	UPROPERTY(Transient)
	TObjectPtr<ULISaveGame> PendingSave;

	/** Set by LoadAndRestart/NewGameAndRestart so PrepareForNewMap knows the intent. */
	enum class EPendingIntent : uint8 { None, Load, NewGame };
	EPendingIntent PendingIntent = EPendingIntent::None;

	/** Players that already received their saved state this map session (respawns start fresh). */
	TSet<FString> AppliedPlayerKeys;
};
