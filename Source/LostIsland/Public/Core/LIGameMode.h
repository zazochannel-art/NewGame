// Copyright LOST ISLAND Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "LIGameMode.generated.h"

class ALIInteractableActor;
class ALIResourceNode;
class ALIIslandGenerator;
class APlayerStart;

/**
 * ALIGameMode
 *
 * Server-only rules object. Sets the default pawn / controller / HUD classes.
 * Blueprint subclasses can swap DefaultPawnClass for a BP character with a mesh.
 *
 * Prototype helper: when bSpawnPrototypeTestActors is enabled it spawns a few
 * ALIInteractableActor cubes in front of the PlayerStart (one-time, at StartPlay)
 * so the interaction foundation can be tested in any map without placing actors.
 * Turn it off once real content exists.
 */
UCLASS()
class LOSTISLAND_API ALIGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ALIGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void StartPlay() override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual void FinishRestartPlayer(AController* NewPlayer, const FRotator& StartRotation) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintPure, Category = "LostIsland|Island")
	ALIIslandGenerator* GetIsland() const { return Island; }

	/** Server: rebuild the island with a new seed (0 = random) and move all players to the new beach spawn. */
	UFUNCTION(BlueprintCallable, Category = "LostIsland|Island")
	void RegenerateIsland(int32 NewSeed);

	/** Server: a player character died. Drops its inventory as loot and respawns the player after RespawnDelay. */
	void HandlePlayerDeath(class ALICharacter* DeadCharacter);

	/** Server: replace the player's pawn with a fresh one at their sleeping bag (or the beach spawn). */
	void RespawnPlayer(AController* Controller);

protected:
	/** Generate the procedural island at game start. */
	UPROPERTY(EditDefaultsOnly, Category = "LostIsland|Island")
	bool bGenerateIsland = true;

	UPROPERTY(EditDefaultsOnly, Category = "LostIsland|Island", meta = (EditCondition = "bGenerateIsland"))
	TSubclassOf<ALIIslandGenerator> IslandGeneratorClass;

	/** Fixed world seed. 0 = random each new game. Overridden by the "?Seed=123" URL option. */
	UPROPERTY(EditDefaultsOnly, Category = "LostIsland|Island", meta = (EditCondition = "bGenerateIsland"))
	int32 IslandSeed = 0;

	/** Seconds between automatic saves (server). 0 disables autosave. */
	UPROPERTY(EditDefaultsOnly, Category = "LostIsland|Save", meta = (ClampMin = "0.0", Units = "s"))
	float AutosaveInterval = 300.f;

	/** Seconds between death and automatic respawn. */
	UPROPERTY(EditDefaultsOnly, Category = "LostIsland|Death", meta = (ClampMin = "0.0", Units = "s"))
	float RespawnDelay = 6.f;

	/** Spawn the test crates / resource ring. Always on when the island is disabled. */
	UPROPERTY(EditDefaultsOnly, Category = "LostIsland|Prototype")
	bool bSpawnPrototypeTestActors = false;

	UPROPERTY(EditDefaultsOnly, Category = "LostIsland|Prototype", meta = (EditCondition = "bSpawnPrototypeTestActors"))
	TSubclassOf<ALIInteractableActor> PrototypeTestActorClass;

	/** Resource nodes placed in a ring around the PlayerStart for gathering tests. */
	UPROPERTY(EditDefaultsOnly, Category = "LostIsland|Prototype", meta = (EditCondition = "bSpawnPrototypeTestActors"))
	TArray<TSubclassOf<ALIResourceNode>> PrototypeResourceClasses;

	UPROPERTY(EditDefaultsOnly, Category = "LostIsland|Prototype", meta = (EditCondition = "bSpawnPrototypeTestActors", ClampMin = "0"))
	int32 PrototypeNodesPerClass = 3;

	/** Seed to use for a new island. Save loading (Milestone 6) hooks in here. */
	virtual int32 ChooseIslandSeed() const;

	/** Creates and generates the island once. Safe to call repeatedly. */
	void EnsureIslandGenerated();

private:
	UPROPERTY(Transient)
	TObjectPtr<ALIIslandGenerator> Island;

	UPROPERTY(Transient)
	TObjectPtr<APlayerStart> IslandPlayerStart;

	/** Seed from the "?Seed=" URL option (0 = none). */
	int32 UrlSeed = 0;

	FTimerHandle AutosaveTimerHandle;
	void Autosave();

	void SpawnPrototypeTestActors();
	AActor* SpawnOnGround(TSubclassOf<AActor> Class, const FVector& Location, const FRotator& Rotation, float GroundOffset, bool bDeferred = false);
};
