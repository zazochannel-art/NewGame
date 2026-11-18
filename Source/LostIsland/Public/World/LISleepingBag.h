// Copyright LOST ISLAND Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/LIInteractable.h"
#include "LISleepingBag.generated.h"

class UStaticMeshComponent;

/**
 * ALISleepingBag
 *
 * A player-placed bed roll. It is the respawn point of the player who placed it or last slept in it
 * (one bag per player: claiming a bag releases the player's previous one).
 *
 * Interact (E): claim the bag. At night in a single-player game it also skips to morning, which costs
 * some food and water. Placed from the "Sleeping Bag" item (G). Persisted in the save game (owner too).
 */
UCLASS(Blueprintable)
class LOSTISLAND_API ALISleepingBag : public AActor, public ILIInteractable
{
	GENERATED_BODY()

public:
	ALISleepingBag();

	// ILIInteractable
	virtual bool CanInteract_Implementation(APawn* InstigatorPawn) const override;
	virtual void Interact_Implementation(APawn* InstigatorPawn) override;
	virtual FText GetInteractionPrompt_Implementation(APawn* InstigatorPawn) const override;

	/** Server: make this the respawn bag of the given player key (releases that player's other bags). */
	void ClaimFor(const FString& PlayerKey);

	const FString& GetOwnerKey() const { return OwnerKey; }

	/** Server: the bag claimed by PlayerKey, or null. */
	static ALISleepingBag* FindBagFor(const UWorld* World, const FString& PlayerKey);

	/** Where a respawning player should stand: next to the bag, above the ground. */
	FTransform GetRespawnTransform() const;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<USceneComponent> SceneRoot;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> Mat;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> Pillow;

	/** Food and water lost by sleeping through the night. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LostIsland|SleepingBag", meta = (ClampMin = "0.0"))
	float SleepNeedsCost = 15.f;

private:
	/** Server only: SaveSubsystem player key of the owner. Empty = unclaimed. */
	FString OwnerKey;
};
