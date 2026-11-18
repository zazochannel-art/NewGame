// Copyright LOST ISLAND Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Items/LIItemTypes.h"
#include "LIRecipeDefinition.generated.h"

/**
 * ULIRecipeDefinition
 *
 * A crafting recipe: ingredients -> output. Create Data Assets of this class under
 * /Game/Recipes to add recipes without code (Asset Manager type "LIRecipe").
 * Built-in recipes (stone axe, stone pickaxe, fiber bandage) are registered by
 * ULIItemRegistrySubsystem so the prototype works without assets.
 */
UCLASS(BlueprintType)
class LOSTISLAND_API ULIRecipeDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType PrimaryAssetType;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recipe", AssetRegistrySearchable)
	FName RecipeId = NAME_None;

	/** Shown in the crafting list. Falls back to the output item's name. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recipe")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recipe")
	TArray<FLIItemQuantity> Ingredients;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recipe")
	FLIItemQuantity Output;

	/** Lower values are listed first. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recipe")
	int32 SortOrder = 100;

	/** Only craftable next to a lit campfire (cooking). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recipe")
	bool bRequiresFire = false;

	/** Only craftable next to a fresh-water spring (filling flasks). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recipe")
	bool bRequiresWater = false;
};
