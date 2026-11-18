// Copyright LOST ISLAND Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/LIInteractable.h"
#include "Items/LIItemTypes.h"
#include "LIStorageChest.generated.h"

class UStaticMeshComponent;
class ULIInventoryComponent;

/**
 * ALIStorageChest
 *
 * A placeable wooden chest with its own stacks (Capacity slots, replicated to everyone).
 *  - E (interact): store every resource and food stack you carry. Tools and placeables stay with you.
 *  - R (while looking at it): take everything back that fits.
 * Persisted in the save game with its contents.
 */
UCLASS()
class LOSTISLAND_API ALIStorageChest : public AActor, public ILIInteractable
{
	GENERATED_BODY()

public:
	ALIStorageChest();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual bool CanInteract_Implementation(APawn* InstigatorPawn) const override;
	virtual void Interact_Implementation(APawn* InstigatorPawn) override;
	virtual FText GetInteractionPrompt_Implementation(APawn* InstigatorPawn) const override;

	/** Server: move resource and food stacks from Inventory into the chest. Returns items moved. */
	int32 StoreFrom(ULIInventoryComponent* Inventory);

	/** Server: move everything that fits from the chest into Inventory. Returns items moved. */
	int32 TakeAllInto(ULIInventoryComponent* Inventory);

	const TArray<FLIInventorySlot>& GetSlots() const { return Slots; }

	/** Server: restore contents (save loading). */
	void SetSlots(const TArray<FLIInventorySlot>& InSlots);

	int32 GetUsedSlots() const;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> Box;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> Lid;

	UPROPERTY(EditDefaultsOnly, Category = "LostIsland|Chest", meta = (ClampMin = "1", ClampMax = "100"))
	int32 Capacity = 30;

private:
	/** Adds Quantity of ItemId into existing stacks, then free stacks. Returns how many fit. */
	int32 AddToChest(FName ItemId, int32 Quantity, int32 Durability);

	UPROPERTY(Replicated)
	TArray<FLIInventorySlot> Slots;
};
