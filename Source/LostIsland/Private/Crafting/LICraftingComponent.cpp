// Copyright LOST ISLAND Team. All Rights Reserved.

#include "Crafting/LICraftingComponent.h"

#include "Crafting/LIRecipeDefinition.h"
#include "Inventory/LIInventoryComponent.h"
#include "Items/LIItemDefinition.h"
#include "Items/LIItemRegistrySubsystem.h"
#include "Save/LISaveSubsystem.h"
#include "World/LISleepingBag.h"
#include "World/LIBuildingPiece.h"
#include "World/LIRaft.h"
#include "World/LICampfire.h"
#include "World/LIWaterSource.h"
#include "GameFramework/Pawn.h"
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
	if (Recipe->bRequiresFire && (!GetOwner() || !ALICampfire::IsWarmAt(GetWorld(), GetOwner()->GetActorLocation())))
	{
		return false; // cooking needs a lit campfire within its warmth radius
	}
	if (Recipe->bRequiresWater && (!GetOwner() || !ALIWaterSource::IsNearWater(GetWorld(), GetOwner()->GetActorLocation(), 350.f)))
	{
		return false; // filling needs a spring
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
	RememberPlaceable(Recipe->Output.ItemId);
	return Added > 0;
}

void ULICraftingComponent::RememberPlaceable(FName ItemId)
{
	const ULIItemRegistrySubsystem* Registry = ULIItemRegistrySubsystem::Get(this);
	const ULIItemDefinition* Def = Registry ? Registry->FindItem(ItemId) : nullptr;
	if (Def && Def->PlaceableActorClass)
	{
		PreferredPlaceable = ItemId; // G places what was crafted last (walls after crafting walls, ...)
	}
}

FName ULICraftingComponent::FindPlaceableItem() const
{
	const ULIItemRegistrySubsystem* Registry = ULIItemRegistrySubsystem::Get(this);
	const ULIInventoryComponent* Inventory = GetInventory();
	if (!Registry || !Inventory)
	{
		return NAME_None;
	}
	if (!PreferredPlaceable.IsNone() && Inventory->HasItem(PreferredPlaceable, 1))
	{
		return PreferredPlaceable;
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

	// The raft goes in shallow water at the shore.
	if (Def->PlaceableActorClass->IsChildOf(ALIRaft::StaticClass()))
	{
		const APawn* OwnerPawn = Cast<APawn>(Owner);
		const FVector LookForward = OwnerPawn ? OwnerPawn->GetControlRotation().Vector() : Owner->GetActorForwardVector();
		FTransform RaftTransform;
		if (!ALIRaft::ComputePlacement(World, Owner->GetActorLocation(), LookForward, Owner, RaftTransform))
		{
			return false;
		}
		FActorSpawnParameters RaftParams;
		RaftParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (!World->SpawnActor<AActor>(Def->PlaceableActorClass, RaftTransform, RaftParams))
		{
			return false;
		}
		Inventory->RemoveItem(ItemId, 1);
		UE_LOG(LogLostIsland, Log, TEXT("%s launched the raft at %s."), *GetNameSafe(Owner), *RaftTransform.GetLocation().ToCompactString());
		return true;
	}

	// Building pieces snap to their own grid instead of the free ground placement below.
	if (Def->PlaceableActorClass->IsChildOf(ALIBuildingPiece::StaticClass()))
	{
		const APawn* OwnerPawn = Cast<APawn>(Owner);
		const FVector LookForward = OwnerPawn ? OwnerPawn->GetControlRotation().Vector() : Owner->GetActorForwardVector();
		FTransform PieceTransform;
		if (!ALIBuildingPiece::ComputePlacement(World, TSubclassOf<ALIBuildingPiece>(Def->PlaceableActorClass.Get()),
			Owner->GetActorLocation(), LookForward, Owner, PieceTransform))
		{
			return false;
		}
		FActorSpawnParameters PieceParams;
		PieceParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (!World->SpawnActor<AActor>(Def->PlaceableActorClass, PieceTransform, PieceParams))
		{
			return false;
		}
		Inventory->RemoveItem(ItemId, 1);
		UE_LOG(LogLostIsland, Log, TEXT("%s built %s at %s."), *GetNameSafe(Owner), *ItemId.ToString(), *PieceTransform.GetLocation().ToCompactString());
		return true;
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

	// Free-space check above the ground. The engine's DontSpawnIfColliding would also count the
	// terrain itself (the stone ring sits on, or on slopes slightly in, the ground) and refuse
	// almost every spot, so test against everything EXCEPT the ground actor that was hit.
	FCollisionQueryParams ClearanceParams(SCENE_QUERY_STAT(LIPlaceClearance), false);
	if (AActor* Ground = Hit.GetActor())
	{
		ClearanceParams.AddIgnoredActor(Ground);
	}
	FCollisionObjectQueryParams Blockers;
	Blockers.AddObjectTypesToQuery(ECC_WorldStatic);   // resource nodes, wreck, other campfires
	Blockers.AddObjectTypesToQuery(ECC_WorldDynamic);
	Blockers.AddObjectTypesToQuery(ECC_Pawn);          // never place a fire inside a player
	Blockers.AddObjectTypesToQuery(ECC_PhysicsBody);
	const FVector ClearanceCenter = Hit.ImpactPoint + FVector(0.f, 0.f, PlaceClearanceRadius + 5.f);
	if (World->OverlapAnyTestByObjectType(ClearanceCenter, FQuat::Identity, Blockers,
		FCollisionShape::MakeSphere(PlaceClearanceRadius), ClearanceParams))
	{
		return false; // blocked by something
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FRotator Rotation(0.f, Owner->GetActorRotation().Yaw, 0.f);
	AActor* Placed = World->SpawnActor<AActor>(Def->PlaceableActorClass, Hit.ImpactPoint, Rotation, SpawnParams);
	if (!Placed)
	{
		return false;
	}

	// A placed sleeping bag becomes the placer's respawn point straight away.
	if (ALISleepingBag* Bag = Cast<ALISleepingBag>(Placed))
	{
		const APawn* OwnerPawn = Cast<APawn>(Owner);
		Bag->ClaimFor(ULISaveSubsystem::GetPlayerKey(OwnerPawn ? OwnerPawn->GetController() : nullptr));
	}

	Inventory->RemoveItem(ItemId, 1);
	UE_LOG(LogLostIsland, Log, TEXT("%s placed %s."), *GetNameSafe(Owner), *ItemId.ToString());
	return true;
}

void ULICraftingComponent::ClientNotifyCrafted_Implementation(FName OutputItemId, int32 Quantity)
{
	UE_LOG(LogLostIsland, Log, TEXT("Crafted %d x %s."), Quantity, *OutputItemId.ToString());
	RememberPlaceable(OutputItemId); // client side, so the [G] hint matches what the server will place
}

void ULICraftingComponent::RequestPlaceItem(FName ItemId)
{
	if (GetOwnerRole() == ROLE_Authority)
	{
		PlaceItem(ItemId);
	}
	else
	{
		ServerPlaceItem(ItemId);
	}
}

void ULICraftingComponent::ServerPlaceItem_Implementation(FName ItemId)
{
	PlaceItem(ItemId); // validates that the item is carried and placeable
}
