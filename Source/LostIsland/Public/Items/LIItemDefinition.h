// Copyright LOST ISLAND Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Items/LIItemTypes.h"
#include "LIItemDefinition.generated.h"

class UTexture2D;

/**
 * ULIItemDefinition
 *
 * Static data for one item type. Create Data Assets of this class under
 * /Game/Items to add or override items (the Asset Manager scans that folder,
 * see DefaultGame.ini). Wood, Stone and Fiber also exist as built-in fallbacks
 * so the prototype runs with no assets.
 */
UCLASS(BlueprintType)
class LOSTISLAND_API ULIItemDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType PrimaryAssetType;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	/** Unique ID used by inventories and save games. Never rename once shipped. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", AssetRegistrySearchable)
	FName ItemId = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	ELIItemCategory Category = ELIItemCategory::Misc;

	/** Maximum quantity per inventory slot. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (ClampMin = "1"))
	int32 MaxStackSize = 50;

	// ---- Consumable effects (applied once per item consumed) ----

	/** Hunger restored. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Consumable", meta = (ClampMin = "0.0"))
	float FoodValue = 0.f;

	/** Thirst restored. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Consumable", meta = (ClampMin = "0.0"))
	float WaterValue = 0.f;

	/** Health restored (negative = harmful, e.g. raw meat later). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Consumable")
	float HealthValue = 0.f;

	UFUNCTION(BlueprintPure, Category = "Item")
	bool IsConsumable() const { return FoodValue > 0.f || WaterValue > 0.f || HealthValue != 0.f; }

	/** UI icon (loaded on demand by UI). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	TSoftObjectPtr<UTexture2D> Icon;
};
