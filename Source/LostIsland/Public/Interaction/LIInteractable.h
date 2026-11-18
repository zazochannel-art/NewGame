// Copyright LOST ISLAND Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "LIInteractable.generated.h"

class APawn;

UINTERFACE(MinimalAPI, BlueprintType)
class ULIInteractable : public UInterface
{
	GENERATED_BODY()
};

/**
 * ILIInteractable
 *
 * Contract for anything the player can interact with (resource nodes, pickups,
 * doors, crafting stations...). Implementable in C++ or Blueprint.
 *
 * Call sites MUST use the generated Execute_ wrappers so Blueprint
 * implementations are honoured, e.g.:
 *     if (Actor->Implements<ULIInteractable>())
 *         ILIInteractable::Execute_Interact(Actor, Pawn);
 *
 * Authority:
 *  - CanInteract / GetInteractionPrompt / OnFocusBegin / OnFocusEnd run on the
 *    local (owning) client for UI and highlighting.
 *  - Interact runs on the SERVER only (ULIInteractionComponent routes it there),
 *    so gameplay state changes are authoritative and co-op ready.
 */
class LOSTISLAND_API ILIInteractable
{
	GENERATED_BODY()

public:
	/** Whether InstigatorPawn may interact right now (e.g. node depleted -> false). */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "LostIsland|Interaction")
	bool CanInteract(APawn* InstigatorPawn) const;
	virtual bool CanInteract_Implementation(APawn* InstigatorPawn) const { return true; }

	/** Perform the interaction. Server-authoritative. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "LostIsland|Interaction")
	void Interact(APawn* InstigatorPawn);
	virtual void Interact_Implementation(APawn* InstigatorPawn) {}

	/** Short action text shown in the HUD, e.g. "Chop Tree". */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "LostIsland|Interaction")
	FText GetInteractionPrompt(APawn* InstigatorPawn) const;
	virtual FText GetInteractionPrompt_Implementation(APawn* InstigatorPawn) const;

	/** Local-only: the player started looking at this object (highlight it). */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "LostIsland|Interaction")
	void OnFocusBegin(APawn* InstigatorPawn);
	virtual void OnFocusBegin_Implementation(APawn* InstigatorPawn) {}

	/** Local-only: the player stopped looking at this object. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "LostIsland|Interaction")
	void OnFocusEnd(APawn* InstigatorPawn);
	virtual void OnFocusEnd_Implementation(APawn* InstigatorPawn) {}
};
