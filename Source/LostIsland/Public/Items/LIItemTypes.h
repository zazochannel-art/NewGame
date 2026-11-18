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
}

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

	bool IsEmpty() const { return ItemId.IsNone() || Quantity <= 0; }
	void Clear() { ItemId = NAME_None; Quantity = 0; }

	bool operator==(const FLIInventorySlot& Other) const { return ItemId == Other.ItemId && Quantity == Other.Quantity; }
};
