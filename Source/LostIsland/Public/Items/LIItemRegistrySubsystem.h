// Copyright LOST ISLAND Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "LIItemRegistrySubsystem.generated.h"

class ULIItemDefinition;

/**
 * ULIItemRegistrySubsystem
 *
 * Lookup table ItemId -> ULIItemDefinition, available on server and clients.
 * On startup it registers the built-in items (Wood, Stone, Fiber), then loads
 * every "LIItem" primary asset found by the Asset Manager. Assets override
 * built-ins with the same ItemId.
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

private:
	void RegisterBuiltInItems();
	void LoadItemAssets();

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<ULIItemDefinition>> Items;
};
