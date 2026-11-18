// Copyright LOST ISLAND Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/LIInteractable.h"
#include "LICampfire.generated.h"

class UPointLightComponent;
class UStaticMeshComponent;

/**
 * ALICampfire
 *
 * A player-placed fire. While it has fuel it is lit: it lights the area and keeps
 * survivors within WarmthRadius warm at night (see ULISurvivalComponent cold rules).
 *
 * Fuel: burns 1 second of fuel per second (server timer, 1 Hz). Interact (E) to add
 * one Wood = FuelPerWood seconds, up to MaxFuel. Fuel and lit state replicate.
 * Placed from the "Campfire" item (G). Persisted in the save game.
 */
UCLASS(Blueprintable)
class LOSTISLAND_API ALICampfire : public AActor, public ILIInteractable
{
	GENERATED_BODY()

public:
	ALICampfire();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ILIInteractable
	virtual bool CanInteract_Implementation(APawn* InstigatorPawn) const override;
	virtual void Interact_Implementation(APawn* InstigatorPawn) override;
	virtual FText GetInteractionPrompt_Implementation(APawn* InstigatorPawn) const override;
	virtual void OnFocusBegin_Implementation(APawn* InstigatorPawn) override;
	virtual void OnFocusEnd_Implementation(APawn* InstigatorPawn) override;

	UFUNCTION(BlueprintPure, Category = "LostIsland|Campfire")
	bool IsLit() const { return Fuel > 0.f; }

	UFUNCTION(BlueprintPure, Category = "LostIsland|Campfire")
	float GetFuel() const { return Fuel; }

	/** Server: set fuel (save loading). */
	void SetFuel(float NewFuel);

	/** True if any lit campfire in World warms Location. Cheap: campfires are few. */
	static bool IsWarmAt(const UWorld* World, const FVector& Location);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<USceneComponent> SceneRoot;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> StoneRing;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> Logs;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> Flame;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UPointLightComponent> FireLight;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LostIsland|Campfire", meta = (ClampMin = "0.0", Units = "cm"))
	float WarmthRadius = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LostIsland|Campfire", meta = (ClampMin = "0.0", Units = "s"))
	float InitialFuel = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LostIsland|Campfire", meta = (ClampMin = "1.0", Units = "s"))
	float FuelPerWood = 90.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LostIsland|Campfire", meta = (ClampMin = "1.0", Units = "s"))
	float MaxFuel = 600.f;

private:
	void BurnFuel();
	void ApplyLitState();

	UFUNCTION()
	void OnRep_Fuel();

	UPROPERTY(ReplicatedUsing = OnRep_Fuel)
	float Fuel = 0.f;

	bool bWasLit = false;
	bool bFuelInitialized = false;
	FTimerHandle BurnTimerHandle;
};
