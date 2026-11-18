// Copyright LOST ISLAND Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "LIItemTypes.generated.h"

/** Well-known item IDs. Item IDs are plain FNames so data assets can add new ones without code. */
namespace LIItems
{
	inline const FName Wood(TEXT("Wood"));
	inline const FName Stone(TEXT("Stone"));
	inline const FName Fiber(TEXT("Fiber"));
	inline const FName Berries(TEXT("Berries"));
	inline const FName StoneAxe(TEXT("StoneAxe"));
	inline const FName StonePickaxe(TEXT("StonePickaxe"));
	inline const FName FiberBandage(TEXT("FiberBandage"));
	inline const FName ScrapMetal(TEXT("ScrapMetal"));
	inline const FName Campfire(TEXT("Campfire"));
	inline const FName SleepingBag(TEXT("SleepingBag"));
	inline const FName WoodenFloor(TEXT("WoodenFloor"));
	inline const FName WoodenWall(TEXT("WoodenWall"));
	inline const FName WoodenDoorway(TEXT("WoodenDoorway"));
	inline const FName WoodenRoof(TEXT("WoodenRoof"));
}

/** What a tool is good at. Resource nodes declare which tool type helps them. */
UENUM(BlueprintType)
enum class ELIToolType : uint8
{
	None,
	Axe,
	Pickaxe
};

UENUM(BlueprintType)
enum class ELIItemCategory : uint8
{
	Resource,
	Food,
	Tool,
	Misc
};

/** One inventory slot: an item ID and how many of it are stacked there. */
USTRUCT(BlueprintType)
struct LOSTISLAND_API FLIInventorySlot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	FName ItemId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory", meta = (ClampMin = "0"))
	int32 Quantity = 0;

	/** Remaining uses for items with MaxDurability > 0 (tools). 0 for everything else. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory", meta = (ClampMin = "0"))
	int32 Durability = 0;

	bool IsEmpty() const { return ItemId.IsNone() || Quantity <= 0; }
	void Clear() { ItemId = NAME_None; Quantity = 0; Durability = 0; }

	bool operator==(const FLIInventorySlot& Other) const
	{
		return ItemId == Other.ItemId && Quantity == Other.Quantity && Durability == Other.Durability;
	}
};

/** An item ID with a count (recipe ingredients and outputs). */
USTRUCT(BlueprintType)
struct LOSTISLAND_API FLIItemQuantity
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FName ItemId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", meta = (ClampMin = "1"))
	int32 Quantity = 1;

	FLIItemQuantity() = default;
	FLIItemQuantity(FName InItemId, int32 InQuantity) : ItemId(InItemId), Quantity(InQuantity) {}
};
