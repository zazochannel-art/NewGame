// Copyright LOST ISLAND Team. All Rights Reserved.

#include "Items/LIItemRegistrySubsystem.h"

#include "Items/LIItemDefinition.h"
#include "LostIsland.h"
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

	UE_LOG(LogLostIsland, Log, TEXT("Item registry ready: %d item types."), Items.Num());
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
	};

	const FBuiltIn BuiltIns[] =
	{
		{ LIItems::Wood,  LOCTEXT("Wood",  "Wood"),  LOCTEXT("WoodDesc",  "Rough logs. The backbone of every shelter."), 50 },
		{ LIItems::Stone, LOCTEXT("Stone", "Stone"), LOCTEXT("StoneDesc", "Heavy chunks of island rock."),               40 },
		{ LIItems::Fiber, LOCTEXT("Fiber", "Fiber"), LOCTEXT("FiberDesc", "Plant fibres for rope and bindings."),        80 },
	};

	for (const FBuiltIn& BuiltIn : BuiltIns)
	{
		ULIItemDefinition* Def = NewObject<ULIItemDefinition>(this, NAME_None, RF_Transient);
		Def->ItemId = BuiltIn.Id;
		Def->DisplayName = BuiltIn.Name;
		Def->Description = BuiltIn.Description;
		Def->Category = ELIItemCategory::Resource;
		Def->MaxStackSize = BuiltIn.MaxStack;
		RegisterItem(Def);
	}
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
