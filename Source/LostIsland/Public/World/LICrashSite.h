// Copyright LOST ISLAND Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/LIInteractable.h"
#include "LICrashSite.generated.h"

class UStaticMeshComponent;

/** One weighted entry of the wreck's loot table. */
USTRUCT(BlueprintType)
struct LOSTISLAND_API FLILootEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	FName ItemId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "1"))
	int32 MinQuantity = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "1"))
	int32 MaxQuantity = 1;

	/** Relative chance of being picked. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "0.0"))
	float Weight = 1.f;

	FLILootEntry() = default;
	FLILootEntry(FName InItemId, int32 InMin, int32 InMax, float InWeight)
		: ItemId(InItemId), MinQuantity(InMin), MaxQuantity(InMax), Weight(InWeight) {}
};

/**
 * ALICrashSite
 *
 * The wreck of a small plane: the island's landmark and the story's starting point.
 * Placed once per island by ALIIslandGenerator at a seed-determined, flat spot on
 * the beach or a meadow (same place for every player and after loading a save).
 *
 * Gameplay: "Search Wreckage" (E) a limited number of times (shared by all players).
 * Each search rolls the loot table RollsPerSearch times and costs stamina.
 *
 * Visuals: assembled from engine basic shapes as a placeholder (fuselage, nose,
 * one intact wing, a torn-off wing on the ground, tail, scattered debris), tilted
 * nose-down and partly buried. Replace with a low-poly mesh in a Blueprint subclass.
 * Actor origin = ground level at the centre of the fuselage.
 */
UCLASS(Blueprintable)
class LOSTISLAND_API ALICrashSite : public AActor, public ILIInteractable
{
	GENERATED_BODY()

public:
	ALICrashSite();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ILIInteractable
	virtual bool CanInteract_Implementation(APawn* InstigatorPawn) const override;
	virtual void Interact_Implementation(APawn* InstigatorPawn) override;
	virtual FText GetInteractionPrompt_Implementation(APawn* InstigatorPawn) const override;
	virtual void OnFocusBegin_Implementation(APawn* InstigatorPawn) override;
	virtual void OnFocusEnd_Implementation(APawn* InstigatorPawn) override;

	UFUNCTION(BlueprintPure, Category = "LostIsland|CrashSite")
	int32 GetSearchesLeft() const { return SearchesLeft; }

protected:
	virtual void BeginPlay() override;

	/** Server-side hook after a search (play sound, spawn particles...). */
	UFUNCTION(BlueprintImplementableEvent, Category = "LostIsland|CrashSite", meta = (DisplayName = "On Searched"))
	void BP_OnSearched(APawn* Searcher, int32 RemainingSearches);

	// ---- Placeholder geometry ----
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<USceneComponent> SceneRoot;
	/** Everything that belongs to the plane body; tilted as one piece. */
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<USceneComponent> WreckPivot;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> Fuselage;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> Nose;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> LeftWing;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> RightWingStub;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> TailFin;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> TailPlane;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> TornWing;
	UPROPERTY(VisibleAnywhere, Category = "Components") TArray<TObjectPtr<UStaticMeshComponent>> Debris;

	// ---- Loot ----
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LostIsland|CrashSite")
	TArray<FLILootEntry> LootTable;

	/** Total searches available (shared by all players). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LostIsland|CrashSite", meta = (ClampMin = "1"))
	int32 MaxSearches = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LostIsland|CrashSite", meta = (ClampMin = "1"))
	int32 RollsPerSearch = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LostIsland|CrashSite", meta = (ClampMin = "0.0"))
	float StaminaCostPerSearch = 3.f;

	UPROPERTY(EditAnywhere, Category = "LostIsland|CrashSite") FLinearColor HullColor  = FLinearColor(0.78f, 0.77f, 0.72f);
	UPROPERTY(EditAnywhere, Category = "LostIsland|CrashSite") FLinearColor StripeColor = FLinearColor(0.70f, 0.18f, 0.12f);
	UPROPERTY(EditAnywhere, Category = "LostIsland|CrashSite") FLinearColor BurntColor = FLinearColor(0.08f, 0.07f, 0.065f);

private:
	UStaticMeshComponent* MakePart(const TCHAR* Name, USceneComponent* Parent, class UStaticMesh* Mesh,
		const FVector& Location, const FRotator& Rotation, const FVector& Scale);
	const FLILootEntry* PickLoot(FRandomStream& Random) const;

	UPROPERTY(Replicated)
	int32 SearchesLeft = 0;
};
