// Copyright LOST ISLAND Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LIRainEffect.generated.h"

class UInstancedStaticMeshComponent;

/**
 * ALIRainEffect
 *
 * Local only (never replicated, never on a dedicated server): thin falling streaks in a
 * cylinder around this machine's camera. How many fall follows ALIGameState::GetRainAmount,
 * so every player sees the same shower. Spawned by the game state the first time it rains.
 */
UCLASS(NotBlueprintable)
class LOSTISLAND_API ALIRainEffect : public AActor
{
	GENERATED_BODY()

public:
	ALIRainEffect();

	virtual void Tick(float DeltaSeconds) override;

	/** Streaks at full rain. */
	UPROPERTY(EditDefaultsOnly, Category = "LostIsland|Weather", meta = (ClampMin = "16", ClampMax = "4000"))
	int32 MaxDrops = 900;

	/** Radius of the rain cylinder around the camera (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "LostIsland|Weather", meta = (ClampMin = "200.0", Units = "cm"))
	float Radius = 1800.f;

	UPROPERTY(EditDefaultsOnly, Category = "LostIsland|Weather", meta = (ClampMin = "100.0", Units = "cm/s"))
	float FallSpeed = 1300.f;

	/** Sideways drift of the drops (cm/s). */
	UPROPERTY(EditDefaultsOnly, Category = "LostIsland|Weather")
	FVector Wind = FVector(160.f, 90.f, 0.f);

	UPROPERTY(EditDefaultsOnly, Category = "LostIsland|Weather")
	FLinearColor DropColor = FLinearColor(0.62f, 0.7f, 0.8f);

protected:
	virtual void BeginPlay() override;

private:
	FVector RandomDropPoint(const FVector& Camera, bool bAnyHeight) const;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> Drops;

	TArray<FVector> DropLocations;
	TArray<FTransform> Transforms;
};
