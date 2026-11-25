// Copyright LOST ISLAND Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/LIInteractable.h"
#include "LIFuelPump.generated.h"

class ALIVehicle;
class UBoxComponent;
class UProceduralMeshComponent;

/**
 * ALIFuelPump
 *
 * The farm's diesel pump by the yard road. Park a vehicle next to it, get out, look at the pump and press E:
 * the nearest vehicle with an engine is filled up for coins (as far as the coins go).
 */
UCLASS()
class LOSTISLAND_API ALIFuelPump : public AActor, public ILIInteractable
{
	GENERATED_BODY()

public:
	ALIFuelPump();

	virtual bool CanInteract_Implementation(APawn* InstigatorPawn) const override;
	virtual void Interact_Implementation(APawn* InstigatorPawn) override;
	virtual FText GetInteractionPrompt_Implementation(APawn* InstigatorPawn) const override;

	/** Coins per litre. */
	UPROPERTY(EditDefaultsOnly, Category = "LostIsland|Fuel", meta = (ClampMin = "0.0"))
	float PricePerLitre = 1.5f;

	/** How close a vehicle must be parked (cm, from the pump to the vehicle's centre). */
	UPROPERTY(EditDefaultsOnly, Category = "LostIsland|Fuel", meta = (ClampMin = "100.0", Units = "cm"))
	float Reach = 800.f;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UBoxComponent> Body;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UProceduralMeshComponent> Visual;

private:
	void BuildVisual();

	/** The nearest parked vehicle with an engine in reach, or null. */
	ALIVehicle* FindVehicle() const;

	int32 GetCost(float Litres) const { return FMath::CeilToInt(Litres * PricePerLitre); }
};
