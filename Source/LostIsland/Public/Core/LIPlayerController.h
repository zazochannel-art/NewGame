// Copyright LOST ISLAND Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "LIPlayerController.generated.h"

/**
 * ALIPlayerController
 *
 * Thin controller for now. Gameplay input lives on the pawn (ALICharacter) so it
 * travels with possession; this class will own UI (inventory, menus) and
 * input-mode switching in later milestones.
 */
UCLASS()
class LOSTISLAND_API ALIPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;
};
