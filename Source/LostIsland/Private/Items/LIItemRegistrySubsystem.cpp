// Copyright LOST ISLAND Team. All Rights Reserved.

#include "Items/LIItemRegistrySubsystem.h"

#include "Items/LIItemDefinition.h"
#include "Crafting/LIRecipeDefinition.h"
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
		{ LIItems::FiberBandage, LOCTEXT("FiberBandage", "Fiber Bandage"), LOCTEXT("FiberBandageDesc", "Crude plant-fibre wrap. Stops some bleeding."), 10, ELIItemCategory::Misc, 0.f, 0.f },
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
}

void ULIItemRegistrySubsystem::RegisterBuiltInRecipes()
{
	struct FBuiltInRecipe
	{
		FName Id;
		TArray<FLIItemQuantity> Ingredients;
		FLIItemQuantity Output;
		int32 SortOrder;
	};

	const FBuiltInRecipe BuiltIns[] =
	{
		{ LIItems::StoneAxe,     { { LIItems::Wood, 3 }, { LIItems::Stone, 2 }, { LIItems::Fiber, 4 } }, { LIItems::StoneAxe, 1 },     10 },
		{ LIItems::StonePickaxe, { { LIItems::Wood, 3 }, { LIItems::Stone, 4 }, { LIItems::Fiber, 4 } }, { LIItems::StonePickaxe, 1 }, 20 },
		{ LIItems::FiberBandage, { { LIItems::Fiber, 6 } },                                              { LIItems::FiberBandage, 1 }, 30 },
	};

	for (const FBuiltInRecipe& BuiltIn : BuiltIns)
	{
		ULIRecipeDefinition* Recipe = NewObject<ULIRecipeDefinition>(this, NAME_None, RF_Transient);
		Recipe->RecipeId = BuiltIn.Id;
		Recipe->Ingredients = BuiltIn.Ingredients;
		Recipe->Output = BuiltIn.Output;
		Recipe->SortOrder = BuiltIn.SortOrder;
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
