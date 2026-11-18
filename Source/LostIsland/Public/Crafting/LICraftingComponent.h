// Copyright LOST ISLAND Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LICraftingComponent.generated.h"

class ULIInventoryComponent;
class ULIRecipeDefinition;

/**
 * ULICraftingComponent
 *
 * Instant crafting from the owner's ULIInventoryComponent using recipes from
 * ULIItemRegistrySubsystem.
 *
 * Authority: RequestCraft() can be called on the owning client; the server
 * re-validates ingredients and inventory space, then removes ingredients and
 * adds the output atomically (all or nothing).
 *
 * Future: craft times, crafting stations (workbench/campfire requirement per recipe).
 */
UCLASS(ClassGroup = (LostIsland), meta = (BlueprintSpawnableComponent))
class LOSTISLAND_API ULICraftingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULICraftingComponent();

	/** True if the owner has every ingredient and room for the output. Server and owning client. */
	UFUNCTION(BlueprintPure, Category = "LostIsland|Crafting")
	bool CanCraft(FName RecipeId) const;

	/** Owning client or server. */
	UFUNCTION(BlueprintCallable, Category = "LostIsland|Crafting")
	void RequestCraft(FName RecipeId);

	/** Crafts the Nth recipe in the registry's sorted list (number keys). */
	void RequestCraftByIndex(int32 Index);

	/** Server: craft once. Returns true on success. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "LostIsland|Crafting")
	bool Craft(FName RecipeId);

private:
	ULIInventoryComponent* GetInventory() const;
	const ULIRecipeDefinition* GetRecipe(FName RecipeId) const;

	UFUNCTION(Server, Reliable)
	void ServerCraft(FName RecipeId);

	UFUNCTION(Client, Unreliable)
	void ClientNotifyCrafted(FName OutputItemId, int32 Quantity);
};
