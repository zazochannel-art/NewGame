// Copyright LOST ISLAND Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/LIInteractable.h"
#include "LIVehicleDealer.generated.h"

class ALIVehicle;
class UBoxComponent;
class UProceduralMeshComponent;

/**
 * ALIVehicleDealer
 *
 * One pad of the vehicle dealer on the west side of the farmyard: a painted parking bay with a price board.
 * E at the board buys that vehicle for coins; it is delivered onto the bay, facing the yard, once the bay is clear.
 */
UCLASS()
class LOSTISLAND_API ALIVehicleDealer : public AActor, public ILIInteractable
{
	GENERATED_BODY()

public:
	ALIVehicleDealer();

	/** Call before FinishSpawning. */
	void Setup(TSubclassOf<ALIVehicle> InVehicleClass, int32 InPrice);

	virtual bool CanInteract_Implementation(APawn* InstigatorPawn) const override;
	virtual void Interact_Implementation(APawn* InstigatorPawn) override;
	virtual FText GetInteractionPrompt_Implementation(APawn* InstigatorPawn) const override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** No more vehicles than this in the world. */
	UPROPERTY(EditDefaultsOnly, Category = "LostIsland|Vehicle", meta = (ClampMin = "1"))
	int32 MaxVehicles = 12;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UBoxComponent> Board;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UProceduralMeshComponent> Visual;

	/** Centre of the bay, in actor space (the board stands at the origin, on the yard side). */
	UPROPERTY(EditDefaultsOnly, Category = "LostIsland|Vehicle")
	FVector BayOffset = FVector(-470.f, 0.f, 0.f);

	UPROPERTY(EditDefaultsOnly, Category = "LostIsland|Vehicle")
	FVector BayHalfSize = FVector(390.f, 150.f, 0.f);

private:
	UFUNCTION()
	void OnRep_Offer() { BuildVisual(); }

	void BuildVisual();
	FText GetVehicleName() const;
	bool IsBayClear() const;

	UPROPERTY(ReplicatedUsing = OnRep_Offer)
	TSubclassOf<ALIVehicle> VehicleClass;

	UPROPERTY(ReplicatedUsing = OnRep_Offer)
	int32 Price = 0;
};
