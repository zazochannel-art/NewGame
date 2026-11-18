// Copyright LOST ISLAND Team. All Rights Reserved.

#include "Items/LIItemRegistrySubsystem.h"

#include "Items/LIItemDefinition.h"
#include "Crafting/LIRecipeDefinition.h"
#include "World/LICampfire.h"
#include "World/LISleepingBag.h"
#include "World/LIBuildingPiece.h"
#include "World/LIRaft.h"
#include "LostIsland.h"
#include "Algo/Sort.h"
#include "Engine/AssetManager.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "LIItemRegistry"

void ULIItemRegistrySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	RegisterBuiltInItems();
	LoadItemAssets();
	RegisterBuiltInRecipes();
	LoadRecipeAssets();
	RebuildSortedRecipes();

	UE_LOG(LogLostIsland, Log, TEXT("Item registry ready: %d item types, %d recipes."), Items.Num(), Recipes.Num());
}

ULIItemRegistrySubsystem* ULIItemRegistrySubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<ULIItemRegistrySubsystem>() : nullptr;
}

void ULIItemRegistrySubsystem::RegisterBuiltInItems()
{
	struct FBuiltIn
	{
		FName Id;
		FText Name;
		FText Description;
		int32 MaxStack;
		ELIItemCategory Category;
		float Food;
		float Water;
	};

	const FBuiltIn BuiltIns[] =
	{
		{ LIItems::Wood,    LOCTEXT("Wood",    "Wood"),    LOCTEXT("WoodDesc",    "Rough logs. The backbone of every shelter."), 50, ELIItemCategory::Resource, 0.f,  0.f },
		{ LIItems::Stone,   LOCTEXT("Stone",   "Stone"),   LOCTEXT("StoneDesc",   "Heavy chunks of island rock."),               40, ELIItemCategory::Resource, 0.f,  0.f },
		{ LIItems::Fiber,   LOCTEXT("Fiber",   "Fiber"),   LOCTEXT("FiberDesc",   "Plant fibres for rope and bindings."),        80, ELIItemCategory::Resource, 0.f,  0.f },
		{ LIItems::Berries, LOCTEXT("Berries", "Berries"), LOCTEXT("BerriesDesc", "Tart wild berries. A little food, a little water."), 20, ELIItemCategory::Food, 8.f, 2.f },
		{ LIItems::RawMeat,    LOCTEXT("RawMeat", "Raw Crab Meat"),    LOCTEXT("RawMeatDesc", "Stringy and cold. Better cooked at a fire."), 10, ELIItemCategory::Food, 3.f, 0.f },
		{ LIItems::CookedMeat, LOCTEXT("CookedMeat", "Cooked Crab Meat"), LOCTEXT("CookedMeatDesc", "Hot, salty and filling."), 10, ELIItemCategory::Food, 22.f, -2.f },
		{ LIItems::EmptyFlask, LOCTEXT("EmptyFlask", "Empty Flask"), LOCTEXT("EmptyFlaskDesc", "A dented aluminium flask. Fill it at a spring."), 5, ELIItemCategory::Misc, 0.f, 0.f },
		{ LIItems::WaterFlask, LOCTEXT("WaterFlask", "Water Flask"), LOCTEXT("WaterFlaskDesc", "Fresh spring water to carry. Drink with F."), 5, ELIItemCategory::Food, 0.f, 30.f },
		{ LIItems::MetalAxe, LOCTEXT("MetalAxe", "Scrap Axe"), LOCTEXT("MetalAxeDesc", "A sharpened aluminium blade on a haft. Fells trees fast."), 1, ELIItemCategory::Tool, 0.f, 0.f },
		{ LIItems::MetalPickaxe, LOCTEXT("MetalPickaxe", "Scrap Pickaxe"), LOCTEXT("MetalPickaxeDesc", "A bent metal spike on a haft. Breaks rock fast."), 1, ELIItemCategory::Tool, 0.f, 0.f },
		{ LIItems::RaftKit, LOCTEXT("RaftKit", "Raft Kit"), LOCTEXT("RaftKitDesc", "Lashed logs, a mast and a fibre sail. Place it (G) in shallow water at the shore and sail away."), 1, ELIItemCategory::Misc, 0.f, 0.f },
		{ LIItems::FiberBandage, LOCTEXT("FiberBandage", "Fiber Bandage"), LOCTEXT("FiberBandageDesc", "Crude plant-fibre wrap. Stops some bleeding."), 10, ELIItemCategory::Misc, 0.f, 0.f },
		{ LIItems::Campfire, LOCTEXT("Campfire", "Campfire"), LOCTEXT("CampfireDesc", "Stones and dry wood. Place it (G) to keep warm through the night."), 5, ELIItemCategory::Misc, 0.f, 0.f },
		{ LIItems::SleepingBag, LOCTEXT("SleepingBag", "Sleeping Bag"), LOCTEXT("SleepingBagDesc", "A fibre bed roll. Place it (G): you respawn here, and can sleep through the night."), 1, ELIItemCategory::Misc, 0.f, 0.f },
		{ LIItems::WoodenFloor, LOCTEXT("WoodenFloor", "Wooden Floor"), LOCTEXT("WoodenFloorDesc", "A 3 x 3 m plank floor. Place it (G); more floors snap next to it."), 10, ELIItemCategory::Misc, 0.f, 0.f },
		{ LIItems::WoodenWall,  LOCTEXT("WoodenWall", "Wooden Wall"),   LOCTEXT("WoodenWallDesc", "A plank wall. Snaps to the floor edge you look at."), 10, ELIItemCategory::Misc, 0.f, 0.f },
		{ LIItems::WoodenDoorway, LOCTEXT("WoodenDoorway", "Wooden Doorway"), LOCTEXT("WoodenDoorwayDesc", "A plank wall with a way through. Snaps like a wall."), 10, ELIItemCategory::Misc, 0.f, 0.f },
		{ LIItems::WoodenRoof,  LOCTEXT("WoodenRoof", "Wooden Roof"),   LOCTEXT("WoodenRoofDesc", "Roof boards over a floor. Shelter from the night cold."), 10, ELIItemCategory::Misc, 0.f, 0.f },
		{ LIItems::ScrapMetal, LOCTEXT("ScrapMetal", "Scrap Metal"), LOCTEXT("ScrapMetalDesc", "Twisted aluminium from the wreck. Better tools will need it."), 30, ELIItemCategory::Resource, 0.f, 0.f },
		{ LIItems::StoneAxe, LOCTEXT("StoneAxe", "Stone Axe"), LOCTEXT("StoneAxeDesc", "Sharp stone lashed to a branch. Fells trees faster."), 1, ELIItemCategory::Tool, 0.f, 0.f },
		{ LIItems::StonePickaxe, LOCTEXT("StonePickaxe", "Stone Pickaxe"), LOCTEXT("StonePickaxeDesc", "Heavy pointed stone on a haft. Breaks rock faster."), 1, ELIItemCategory::Tool, 0.f, 0.f },
	};

	for (const FBuiltIn& BuiltIn : BuiltIns)
	{
		ULIItemDefinition* Def = NewObject<ULIItemDefinition>(this, NAME_None, RF_Transient);
		Def->ItemId = BuiltIn.Id;
		Def->DisplayName = BuiltIn.Name;
		Def->Description = BuiltIn.Description;
		Def->Category = BuiltIn.Category;
		Def->MaxStackSize = BuiltIn.MaxStack;
		Def->FoodValue = BuiltIn.Food;
		Def->WaterValue = BuiltIn.Water;
		RegisterItem(Def);
	}

	// Extra properties that don't fit the table above.
	if (ULIItemDefinition* Bandage = FindItem(LIItems::FiberBandage))
	{
		Bandage->HealthValue = 15.f;
	}
	if (ULIItemDefinition* CampfireItem = FindItem(LIItems::Campfire))
	{
		CampfireItem->PlaceableActorClass = ALICampfire::StaticClass();
	}
	if (ULIItemDefinition* BagItem = FindItem(LIItems::SleepingBag))
	{
		BagItem->PlaceableActorClass = ALISleepingBag::StaticClass();
	}
	if (ULIItemDefinition* Item = FindItem(LIItems::WoodenFloor)) { Item->PlaceableActorClass = ALIWoodenFloor::StaticClass(); }
	if (ULIItemDefinition* Item = FindItem(LIItems::WoodenWall))  { Item->PlaceableActorClass = ALIWoodenWall::StaticClass(); }
	if (ULIItemDefinition* Item = FindItem(LIItems::WoodenDoorway)) { Item->PlaceableActorClass = ALIWoodenDoorway::StaticClass(); }
	if (ULIItemDefinition* Item = FindItem(LIItems::RaftKit)) { Item->PlaceableActorClass = ALIRaft::StaticClass(); }
	if (ULIItemDefinition* Item = FindItem(LIItems::WoodenRoof))  { Item->PlaceableActorClass = ALIWoodenRoof::StaticClass(); }
	if (ULIItemDefinition* Axe = FindItem(LIItems::StoneAxe))
	{
		Axe->ToolType = ELIToolType::Axe;
		Axe->MaxDurability = 40;
	}
	if (ULIItemDefinition* Pickaxe = FindItem(LIItems::StonePickaxe))
	{
		Pickaxe->ToolType = ELIToolType::Pickaxe;
		Pickaxe->MaxDurability = 40;
	}
	if (ULIItemDefinition* Flask = FindItem(LIItems::WaterFlask))
	{
		Flask->ConsumeReturnsItem = LIItems::EmptyFlask;
	}
	// Metal tools: more per hit, cheaper on stamina, last three times longer than stone.
	if (ULIItemDefinition* Axe = FindItem(LIItems::MetalAxe))
	{
		Axe->ToolType = ELIToolType::Axe;
		Axe->MaxDurability = 120;
		Axe->ToolYieldMultiplier = 3.f;
		Axe->ToolStaminaMultiplier = 0.45f;
	}
	if (ULIItemDefinition* Pickaxe = FindItem(LIItems::MetalPickaxe))
	{
		Pickaxe->ToolType = ELIToolType::Pickaxe;
		Pickaxe->MaxDurability = 120;
		Pickaxe->ToolYieldMultiplier = 3.f;
		Pickaxe->ToolStaminaMultiplier = 0.45f;
	}
}

void ULIItemRegistrySubsystem::RegisterBuiltInRecipes()
{
	struct FBuiltInRecipe
	{
		FName Id;
		TArray<FLIItemQuantity> Ingredients;
		FLIItemQuantity Output;
		int32 SortOrder;
		bool bRequiresFire = false;
		bool bRequiresWater = false;
	};

	const FBuiltInRecipe BuiltIns[] =
	{
		{ LIItems::StoneAxe,     { { LIItems::Wood, 3 }, { LIItems::Stone, 2 }, { LIItems::Fiber, 4 } }, { LIItems::StoneAxe, 1 },     10 },
		{ LIItems::StonePickaxe, { { LIItems::Wood, 3 }, { LIItems::Stone, 4 }, { LIItems::Fiber, 4 } }, { LIItems::StonePickaxe, 1 }, 20 },
		{ LIItems::FiberBandage, { { LIItems::Fiber, 6 } },                                              { LIItems::FiberBandage, 1 }, 30 },
		{ LIItems::CookedMeat,   { { LIItems::RawMeat, 1 } },                                            { LIItems::CookedMeat, 1 },   35, true },
		{ LIItems::EmptyFlask,   { { LIItems::ScrapMetal, 2 }, { LIItems::Fiber, 2 } },                  { LIItems::EmptyFlask, 1 },   32 },
		{ LIItems::WaterFlask,   { { LIItems::EmptyFlask, 1 } },                                         { LIItems::WaterFlask, 1 },   33, false, true },
		{ LIItems::MetalAxe,     { { LIItems::Wood, 2 }, { LIItems::ScrapMetal, 4 }, { LIItems::Fiber, 2 } }, { LIItems::MetalAxe, 1 },   12 },
		{ LIItems::MetalPickaxe, { { LIItems::Wood, 2 }, { LIItems::ScrapMetal, 5 }, { LIItems::Fiber, 2 } }, { LIItems::MetalPickaxe, 1 }, 22 },
		{ LIItems::Campfire,     { { LIItems::Wood, 5 }, { LIItems::Stone, 6 } },                        { LIItems::Campfire, 1 },     5 },
		{ LIItems::SleepingBag,  { { LIItems::Fiber, 20 }, { LIItems::Wood, 4 } },                       { LIItems::SleepingBag, 1 },  6 },
		{ LIItems::WoodenFloor,  { { LIItems::Wood, 8 } },                                              { LIItems::WoodenFloor, 1 },  40 },
		{ LIItems::WoodenWall,   { { LIItems::Wood, 6 }, { LIItems::Fiber, 2 } },                        { LIItems::WoodenWall, 1 },   41 },
		{ LIItems::WoodenDoorway, { { LIItems::Wood, 5 }, { LIItems::Fiber, 2 } },                       { LIItems::WoodenDoorway, 1 }, 43 },
		{ LIItems::RaftKit,      { { LIItems::Wood, 60 }, { LIItems::Fiber, 40 }, { LIItems::ScrapMetal, 6 } }, { LIItems::RaftKit, 1 }, 90 },
		{ LIItems::WoodenRoof,   { { LIItems::Wood, 6 }, { LIItems::Fiber, 4 } },                        { LIItems::WoodenRoof, 1 },   42 },
	};

	for (const FBuiltInRecipe& BuiltIn : BuiltIns)
	{
		ULIRecipeDefinition* Recipe = NewObject<ULIRecipeDefinition>(this, NAME_None, RF_Transient);
		Recipe->RecipeId = BuiltIn.Id;
		Recipe->Ingredients = BuiltIn.Ingredients;
		Recipe->Output = BuiltIn.Output;
		Recipe->SortOrder = BuiltIn.SortOrder;
		Recipe->bRequiresFire = BuiltIn.bRequiresFire;
		Recipe->bRequiresWater = BuiltIn.bRequiresWater;
		RegisterRecipe(Recipe);
	}
}

void ULIItemRegistrySubsystem::LoadRecipeAssets()
{
	UAssetManager* AssetManager = UAssetManager::GetIfInitialized();
	if (!AssetManager)
	{
		return;
	}

	TArray<FPrimaryAssetId> AssetIds;
	AssetManager->GetPrimaryAssetIdList(ULIRecipeDefinition::PrimaryAssetType, AssetIds);
	for (const FPrimaryAssetId& AssetId : AssetIds)
	{
		if (ULIRecipeDefinition* Recipe = Cast<ULIRecipeDefinition>(AssetManager->GetPrimaryAssetPath(AssetId).TryLoad()))
		{
			RegisterRecipe(Recipe);
		}
	}
}

void ULIItemRegistrySubsystem::RegisterRecipe(ULIRecipeDefinition* Recipe)
{
	if (!Recipe || Recipe->RecipeId.IsNone() || Recipe->Output.ItemId.IsNone())
	{
		UE_LOG(LogLostIsland, Warning, TEXT("Ignoring recipe with no RecipeId or output: %s"), *GetNameSafe(Recipe));
		return;
	}
	Recipes.Add(Recipe->RecipeId, Recipe);
	RebuildSortedRecipes();
}

void ULIItemRegistrySubsystem::RebuildSortedRecipes()
{
	SortedRecipes.Reset();
	for (const TPair<FName, TObjectPtr<ULIRecipeDefinition>>& Pair : Recipes)
	{
		SortedRecipes.Add(Pair.Value);
	}
	Algo::Sort(SortedRecipes, [](const TObjectPtr<ULIRecipeDefinition>& A, const TObjectPtr<ULIRecipeDefinition>& B)
	{
		return A->SortOrder != B->SortOrder ? A->SortOrder < B->SortOrder : A->RecipeId.LexicalLess(B->RecipeId);
	});
}

ULIRecipeDefinition* ULIItemRegistrySubsystem::FindRecipe(FName RecipeId) const
{
	const TObjectPtr<ULIRecipeDefinition>* Found = Recipes.Find(RecipeId);
	return Found ? Found->Get() : nullptr;
}

void ULIItemRegistrySubsystem::LoadItemAssets()
{
	UAssetManager* AssetManager = UAssetManager::GetIfInitialized();
	if (!AssetManager)
	{
		return;
	}

	TArray<FPrimaryAssetId> AssetIds;
	AssetManager->GetPrimaryAssetIdList(ULIItemDefinition::PrimaryAssetType, AssetIds);

	// Item definitions are tiny; a synchronous load at startup is acceptable.
	for (const FPrimaryAssetId& AssetId : AssetIds)
	{
		const FSoftObjectPath Path = AssetManager->GetPrimaryAssetPath(AssetId);
		if (ULIItemDefinition* Def = Cast<ULIItemDefinition>(Path.TryLoad()))
		{
			RegisterItem(Def);
		}
	}
}

void ULIItemRegistrySubsystem::RegisterItem(ULIItemDefinition* Definition)
{
	if (!Definition || Definition->ItemId.IsNone())
	{
		UE_LOG(LogLostIsland, Warning, TEXT("Ignoring item definition with no ItemId: %s"), *GetNameSafe(Definition));
		return;
	}

	Items.Add(Definition->ItemId, Definition);
}

ULIItemDefinition* ULIItemRegistrySubsystem::FindItem(FName ItemId) const
{
	const TObjectPtr<ULIItemDefinition>* Found = Items.Find(ItemId);
	return Found ? Found->Get() : nullptr;
}

int32 ULIItemRegistrySubsystem::GetMaxStackSize(FName ItemId) const
{
	const ULIItemDefinition* Def = FindItem(ItemId);
	return Def ? FMath::Max(1, Def->MaxStackSize) : 0;
}

FText ULIItemRegistrySubsystem::GetDisplayName(FName ItemId) const
{
	const ULIItemDefinition* Def = FindItem(ItemId);
	return Def && !Def->DisplayName.IsEmpty() ? Def->DisplayName : FText::FromName(ItemId);
}

#undef LOCTEXT_NAMESPACE
