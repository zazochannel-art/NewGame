// Copyright LOST ISLAND Team. All Rights Reserved.

#include "Crafting/LICraftingComponent.h"

#include "Crafting/LIRecipeDefinition.h"
#include "Inventory/LIInventoryComponent.h"
#include "Items/LIItemDefinition.h"
#include "Items/LIItemRegistrySubsystem.h"
#include "Engine/World.h"
#include "LostIsland.h"
#include "GameFramework/Actor.h"

ULICraftingComponent::ULICraftingComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetIsReplicatedByDefault(true); // for ServerCraft / ClientNotifyCrafted
}

ULIInventoryComponent* ULICraftingComponent::GetInventory() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<ULIInventoryComponent>() : nullptr;
}

const ULIRecipeDefinition* ULICraftingComponent::GetRecipe(FName RecipeId) const
{
	const ULIItemRegistrySubsystem* Registry = ULIItemRegistrySubsystem::Get(this);
	return Registry ? Registry->FindRecipe(RecipeId) : nullptr;
}

bool ULICraftingComponent::CanCraft(FName RecipeId) const
{
	const ULIRecipeDefinition* Recipe = GetRecipe(RecipeId);
	const ULIInventoryComponent* Inventory = GetInventory();
	if (!Recipe || !Inventory)
	{
		return false;
	}

	for (const FLIItemQuantity& Ingredient : Recipe->Ingredients)
	{
		if (!Inventory->HasItem(Ingredient.ItemId, Ingredient.Quantity))
		{
			return false;
		}
	}

	// Room check is conservative: it ignores slots the ingredients would free up.
	// Good enough for small recipes; refine when recipes produce large stacks.
	return Inventory->GetRoomFor(Recipe->Output.ItemId) >= Recipe->Output.Quantity;
}

void ULICraftingComponent::RequestCraft(FName RecipeId)
{
	if (GetOwnerRole() == ROLE_Authority)
	{
		Craft(RecipeId);
	}
	else
	{
		ServerCraft(RecipeId);
	}
}

void ULICraftingComponent::RequestCraftByIndex(int32 Index)
{
	const ULIItemRegistrySubsystem* Registry = ULIItemRegistrySubsystem::Get(this);
	if (Registry && Registry->GetRecipes().IsValidIndex(Index))
	{
		RequestCraft(Registry->GetRecipes()[Index]->RecipeId);
	}
}

void ULICraftingComponent::ServerCraft_Implementation(FName RecipeId)
{
	Craft(RecipeId);
}

bool ULICraftingComponent::Craft(FName RecipeId)
{
	if (GetOwnerRole() != ROLE_Authority || !CanCraft(RecipeId))
	{
		return false;
	}

	const ULIRecipeDefinition* Recipe = GetRecipe(RecipeId);
	ULIInventoryComponent* Inventory = GetInventory();

	// CanCraft verified every ingredient, so these removals cannot partially fail.
	for (const FLIItemQuantity& Ingredient : Recipe->Ingredients)
	{
		Inventory->RemoveItem(Ingredient.ItemId, Ingredient.Quantity);
	}

	const int32 Added = Inventory->AddItem(Recipe->Output.ItemId, Recipe->Output.Quantity);
	if (Added < Recipe->Output.Quantity)
	{
		// Should not happen after CanCraft; log loudly so it is noticed in testing.
		UE_LOG(LogLostIsland, Error, TEXT("Craft %s: only %d/%d output fit."), *RecipeId.ToString(), Added, Recipe->Output.Quantity);
	}

	ClientNotifyCrafted(Recipe->Output.ItemId, Added);
	return Added > 0;
}

FName ULICraftingComponent::FindPlaceableItem() const
{
	const ULIItemRegistrySubsystem* Registry = ULIItemRegistrySubsystem::Get(this);
	const ULIInventoryComponent* Inventory = GetInventory();
	if (!Registry || !Inventory)
	{
		return NAME_None;
	}
	for (const FLIInventorySlot& Slot : Inventory->GetSlots())
	{
		const ULIItemDefinition* Def = Slot.IsEmpty() ? nullptr : Registry->FindItem(Slot.ItemId);
		if (Def && Def->PlaceableActorClass)
		{
			return Slot.ItemId;
		}
	}
	return NAME_None;
}

void ULICraftingComponent::RequestPlace()
{
	if (GetOwnerRole() == ROLE_Authority)
	{
		PlaceItem(FindPlaceableItem());
	}
	else
	{
		ServerPlace();
	}
}

void ULICraftingComponent::ServerPlace_Implementation()
{
	PlaceItem(FindPlaceableItem());
}

bool ULICraftingComponent::PlaceItem(FName ItemId)
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	const ULIItemRegistrySubsystem* Registry = ULIItemRegistrySubsystem::Get(this);
	const ULIItemDefinition* Def = Registry ? Registry->FindItem(ItemId) : nullptr;
	ULIInventoryComponent* Inventory = GetInventory();
	if (GetOwnerRole() != ROLE_Authority || !Owner || !World || !Def || !Def->PlaceableActorClass || !Inventory || !Inventory->HasItem(ItemId, 1))
	{
		return false;
	}

	// Find the ground in front of the owner (static geometry only).
	const FVector Forward = FVector(Owner->GetActorForwardVector().X, Owner->GetActorForwardVector().Y, 0.f).GetSafeNormal();
	const FVector Probe = Owner->GetActorLocation() + Forward * PlaceDistance;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(LIPlaceItem), false, Owner);
	if (!World->LineTraceSingleByObjectType(Hit, Probe + FVector(0.f, 0.f, 200.f), Probe - FVector(0.f, 0.f, 600.f),
		FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		return false;
	}
	if (Hit.ImpactPoint.Z < 0.f || Hit.ImpactNormal.Z < 0.8f)
	{
		return false; // under the sea level (island at Z = 0) or too steep
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
	const FRotator Rotation(0.f, Owner->GetActorRotation().Yaw, 0.f);
	AActor* Placed = World->SpawnActor<AActor>(Def->PlaceableActorClass, Hit.ImpactPoint, Rotation, SpawnParams);
	if (!Placed)
	{
		return false; // blocked by something
	}

	Inventory->RemoveItem(ItemId, 1);
	UE_LOG(LogLostIsland, Log, TEXT("%s placed %s."), *GetNameSafe(Owner), *ItemId.ToString());
	return true;
}

void ULICraftingComponent::ClientNotifyCrafted_Implementation(FName OutputItemId, int32 Quantity)
{
	UE_LOG(LogLostIsland, Log, TEXT("Crafted %d x %s."), Quantity, *OutputItemId.ToString());
}
