// Copyright LOST ISLAND Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/LIInteractable.h"
#include "LIWaterSource.generated.h"

class UStaticMeshComponent;

/**
 * ALIWaterSource
 *
 * Fresh-water spring. Interacting drinks directly (no container needed yet):
 * restores ThirstPerDrink. Infinite, but the interaction component's server-side
 * MinInteractInterval paces it. Only offered when the player is actually thirsty.
 *
 * Origin = ground level. The island generator places a handful inland.
 */
UCLASS(Blueprintable)
class LOSTISLAND_API ALIWaterSource : public AActor, public ILIInteractable
{
	GENERATED_BODY()

public:
	ALIWaterSource();

	// ILIInteractable
	virtual bool CanInteract_Implementation(APawn* InstigatorPawn) const override;
	virtual void Interact_Implementation(APawn* InstigatorPawn) override;
	virtual FText GetInteractionPrompt_Implementation(APawn* InstigatorPawn) const override;
	virtual void OnFocusBegin_Implementation(APawn* InstigatorPawn) override;
	virtual void OnFocusEnd_Implementation(APawn* InstigatorPawn) override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LostIsland|Water", meta = (ClampMin = "0.0"))
	float ThirstPerDrink = 15.f;

	/** Minimum thirst deficit before drinking is offered. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LostIsland|Water", meta = (ClampMin = "0.0"))
	float MinDeficitToDrink = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LostIsland|Water")
	FText PromptText;

	UPROPERTY(EditAnywhere, Category = "LostIsland|Water")
	FLinearColor WaterColor = FLinearColor(0.1f, 0.5f, 0.8f);
};
