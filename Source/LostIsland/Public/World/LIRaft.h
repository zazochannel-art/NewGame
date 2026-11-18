// Copyright LOST ISLAND Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/LIInteractable.h"
#include "LIRaft.generated.h"

class UStaticMeshComponent;

/**
 * ALIRaft
 *
 * The way off the island and the goal of a run. Crafted as a "Raft Kit" (a lot of wood, fibre and
 * some scrap) and placed with G in shallow water at the shore. Interact (E) to sail away: the game
 * state shows the ending for everyone, then a new island starts.
 */
UCLASS()
class LOSTISLAND_API ALIRaft : public AActor, public ILIInteractable
{
	GENERATED_BODY()

public:
	ALIRaft();

	virtual bool CanInteract_Implementation(APawn* InstigatorPawn) const override;
	virtual void Interact_Implementation(APawn* InstigatorPawn) override;
	virtual FText GetInteractionPrompt_Implementation(APawn* InstigatorPawn) const override;

	/** Where a raft can go for a builder at Location looking along Forward: shallow water (0.3-2.5 m deep), floating at sea level. */
	static bool ComputePlacement(const UWorld* World, const FVector& Location, const FVector& Forward, const AActor* Builder, FTransform& OutTransform);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> Deck;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> Mast;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> Sail;
};
