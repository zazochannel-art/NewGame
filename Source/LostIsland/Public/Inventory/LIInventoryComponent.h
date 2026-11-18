// Copyright LOST ISLAND Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Items/LIItemTypes.h"
#include "LIInventoryComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FLIOnInventoryChanged);

/** Short-lived local message for the HUD ("+3 Wood", "Inventory full"). */
struct FLIInventoryNotice
{
	FText Text;
	bool bIsWarning = false;
	double ExpireTime = 0.0;
};

/**
 * ULIInventoryComponent
 *
 * Fixed-size slot inventory (SlotCount, default 20). Each slot holds one item ID
 * and a quantity up to that item's MaxStackSize (from ULIItemRegistrySubsystem).
 *
 * Authority:
 *  - AddItem / RemoveItem / SetSlots are SERVER ONLY.
 *  - Slots replicate to the owning client only (COND_OwnerOnly). OnInventoryChanged
 *    fires on server and owning client.
 *  - The server sends the owner a lightweight Client RPC for pickup / full
 *    messages, kept locally as notices for the HUD.
 *
 * Expansion: SetSlotCount() can grow the inventory later (backpacks), and a
 * FastArraySerializer can replace the plain array if inventories become large.
 */
UCLASS(ClassGroup = (LostIsland), meta = (BlueprintSpawnableComponent))
class LOSTISLAND_API ULIInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULIInventoryComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ---------------- Queries (server and owner) ----------------

	const TArray<FLIInventorySlot>& GetSlots() const { return Slots; }

	/** Blueprint copy of the slot array (C++ should use GetSlots()). */
	UFUNCTION(BlueprintPure, Category = "LostIsland|Inventory", meta = (DisplayName = "Get Slots"))
	TArray<FLIInventorySlot> K2_GetSlots() const { return Slots; }

	UFUNCTION(BlueprintPure, Category = "LostIsland|Inventory")
	int32 GetSlotCount() const { return Slots.Num(); }

	UFUNCTION(BlueprintPure, Category = "LostIsland|Inventory")
	int32 GetItemCount(FName ItemId) const;

	UFUNCTION(BlueprintPure, Category = "LostIsland|Inventory")
	bool HasItem(FName ItemId, int32 Quantity = 1) const { return GetItemCount(ItemId) >= Quantity; }

	/** How many of ItemId could still be added. 0 for unknown items. */
	UFUNCTION(BlueprintPure, Category = "LostIsland|Inventory")
	int32 GetRoomFor(FName ItemId) const;

	/**
	 * Picks the consumable that best covers the owner's current hunger/thirst deficit.
	 * Works on the owning client (for the HUD hint) and on the server. NAME_None if nothing useful.
	 */
	UFUNCTION(BlueprintPure, Category = "LostIsland|Inventory")
	FName FindBestConsumable() const;

	/** Owning client or server: eat/drink the best consumable (validated on the server). */
	UFUNCTION(BlueprintCallable, Category = "LostIsland|Inventory")
	void RequestConsumeBest();

	/** Server: consume one ItemId, applying its food/water/health values. Returns true on success. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "LostIsland|Inventory")
	bool ConsumeItem(FName ItemId);

	// ---------------- Mutation (server only) ----------------

	/** Adds up to Quantity. Returns how many were actually added (the rest did not fit). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "LostIsland|Inventory")
	int32 AddItem(FName ItemId, int32 Quantity);

	/** Removes exactly Quantity, or nothing if there are not enough. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "LostIsland|Inventory")
	bool RemoveItem(FName ItemId, int32 Quantity);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "LostIsland|Inventory")
	void ClearInventory();

	/** Replace the full contents (SaveGame load). Unknown items are dropped, stacks are clamped. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "LostIsland|Inventory")
	void SetSlots(const TArray<FLIInventorySlot>& NewSlots);

	/** Grow or shrink the slot count. Shrinking drops the contents of removed slots. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "LostIsland|Inventory")
	void SetSlotCount(int32 NewSlotCount);

	// ---------------- Events / UI ----------------

	UPROPERTY(BlueprintAssignable, Category = "LostIsland|Inventory")
	FLIOnInventoryChanged OnInventoryChanged;

	/** Local notices for the HUD (owning client). Expired entries are pruned lazily. */
	const TArray<FLIInventoryNotice>& GetActiveNotices();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Inventory", meta = (ClampMin = "1", ClampMax = "200"))
	int32 DefaultSlotCount = 20;

	UPROPERTY(EditDefaultsOnly, Category = "LostIsland|Inventory|UI", meta = (ClampMin = "0.5", Units = "s"))
	float NoticeDuration = 2.5f;

	UPROPERTY(EditDefaultsOnly, Category = "LostIsland|Inventory|UI", meta = (ClampMin = "1"))
	int32 MaxNotices = 5;

private:
	int32 GetMaxStack(FName ItemId) const;
	void NotifyChanged();
	void PushNotice(const FText& Text, bool bIsWarning);

	UFUNCTION(Server, Reliable)
	void ServerConsumeBest();

	UFUNCTION(Client, Unreliable)
	void ClientNotifyConsumed(FName ItemId);

	UFUNCTION(Client, Unreliable)
	void ClientNotifyItemAdded(FName ItemId, int32 Quantity);

	UFUNCTION(Client, Unreliable)
	void ClientNotifyInventoryFull(FName ItemId);

	UFUNCTION()
	void OnRep_Slots();

	UPROPERTY(ReplicatedUsing = OnRep_Slots)
	TArray<FLIInventorySlot> Slots;

	TArray<FLIInventoryNotice> Notices;
};
