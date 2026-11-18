// Copyright LOST ISLAND Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "LIItemRegistrySubsystem.generated.h"

class ULIItemDefinition;
class ULIRecipeDefinition;

/**
 * ULIItemRegistrySubsystem
 *
 * Lookup table ItemId -> ULIItemDefinition, available on server and clients.
 * On startup it registers the built-in items (Wood, Stone, Fiber), then loads
 * every "LIItem" primary asset found by the Asset Manager. Assets override
 * built-ins with the same ItemId. Recipes ("LIRecipe") are handled the same way.
 */
UCLASS()
class LOSTISLAND_API ULIItemRegistrySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Convenience accessor from any world context. May return null very early or very late in the game's lifetime. */
	static ULIItemRegistrySubsystem* Get(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "LostIsland|Items")
	ULIItemDefinition* FindItem(FName ItemId) const;

	UFUNCTION(BlueprintPure, Category = "LostIsland|Items")
	bool IsValidItem(FName ItemId) const { return Items.Contains(ItemId); }

	/** 0 if the item is unknown. */
	UFUNCTION(BlueprintPure, Category = "LostIsland|Items")
	int32 GetMaxStackSize(FName ItemId) const;

	/** Falls back to the raw ID when the item is unknown. */
	UFUNCTION(BlueprintPure, Category = "LostIsland|Items")
	FText GetDisplayName(FName ItemId) const;

	UFUNCTION(BlueprintCallable, Category = "LostIsland|Items")
	void RegisterItem(ULIItemDefinition* Definition);

	// ---- Recipes ----

	UFUNCTION(BlueprintPure, Category = "LostIsland|Crafting")
	ULIRecipeDefinition* FindRecipe(FName RecipeId) const;

	/** All recipes, sorted by SortOrder then RecipeId (stable order for UI and number keys). */
	const TArray<TObjectPtr<ULIRecipeDefinition>>& GetRecipes() const { return SortedRecipes; }

	UFUNCTION(BlueprintCallable, Category = "LostIsland|Crafting")
	void RegisterRecipe(ULIRecipeDefinition* Recipe);

private:
	void RegisterBuiltInItems();
	void RegisterBuiltInRecipes();
	void LoadItemAssets();
	void LoadRecipeAssets();
	void RebuildSortedRecipes();

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<ULIItemDefinition>> Items;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<ULIRecipeDefinition>> Recipes;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ULIRecipeDefinition>> SortedRecipes;
};
