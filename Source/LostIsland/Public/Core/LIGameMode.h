// Copyright LOST ISLAND Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "LIGameMode.generated.h"

class ALIInteractableActor;

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

	virtual void StartPlay() override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "LostIsland|Prototype")
	bool bSpawnPrototypeTestActors = true;

	UPROPERTY(EditDefaultsOnly, Category = "LostIsland|Prototype", meta = (EditCondition = "bSpawnPrototypeTestActors"))
	TSubclassOf<ALIInteractableActor> PrototypeTestActorClass;

private:
	void SpawnPrototypeTestActors();
};
