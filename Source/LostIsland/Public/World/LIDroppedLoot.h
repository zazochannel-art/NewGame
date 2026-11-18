// Copyright LOST ISLAND Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/LIInteractable.h"
#include "Items/LIItemTypes.h"
#include "LIDroppedLoot.generated.h"

class UStaticMeshComponent;

/**
 * ALIDroppedLoot
 *
 * A bundle left where a player died, holding their whole inventory. Interact (E) takes everything
 * that fits; the bundle disappears once empty, or after Lifetime seconds. Not saved: loading a save
 * restores the inventories from the save itself.
 */
UCLASS()
class LOSTISLAND_API ALIDroppedLoot : public AActor, public ILIInteractable
{
	GENERATED_BODY()

public:
	ALIDroppedLoot();

	/** Server, before or right after spawning: the items to hold (empty slots are skipped). */
	void SetContents(const TArray<FLIInventorySlot>& InSlots);

	virtual bool CanInteract_Implementation(APawn* InstigatorPawn) const override;
	virtual void Interact_Implementation(APawn* InstigatorPawn) override;
	virtual FText GetInteractionPrompt_Implementation(APawn* InstigatorPawn) const override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> Mesh;

	/** Seconds before an untouched bundle disappears (0 = never). */
	UPROPERTY(EditAnywhere, Category = "LostIsland|Loot", meta = (ClampMin = "0.0", Units = "s"))
	float Lifetime = 1800.f;

private:
	UPROPERTY(Replicated)
	TArray<FLIInventorySlot> Slots;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
