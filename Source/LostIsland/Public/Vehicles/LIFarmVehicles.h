// Copyright LOST ISLAND Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Vehicles/LIVehicle.h"
#include "LIFarmVehicles.generated.h"

class UProceduralMeshComponent;

/**
 * ALITractor
 *
 * Slow, heavy and sure-footed: big rear wheels, small steering wheels in front, an open cab under a sun roof and a
 * hitch at the back. The plough and the seeder hook onto it in the farming step (Q will lower / raise the tool).
 */
UCLASS()
class LOSTISLAND_API ALITractor : public ALIVehicle
{
	GENERATED_BODY()

public:
	ALITractor();

	/** Where tools attach, in chassis space. */
	UFUNCTION(BlueprintPure, Category = "LostIsland|Vehicle")
	FVector GetHitchPoint() const { return HitchPoint; }

protected:
	virtual void BuildBody(FLILowPolyMesh& Mesh) const override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Vehicle")
	FVector HitchPoint = FVector(-190.f, 0.f, -40.f);
};

/**
 * ALIDumpTruck
 *
 * Six-wheeled site truck. Q tips the bed (hinged at the back) up to BedMaxAngle and back down; the bed only tips
 * while the truck is nearly stopped. Cargo for the bed comes with the building step.
 */
UCLASS()
class LOSTISLAND_API ALIDumpTruck : public ALIVehicle
{
	GENERATED_BODY()

public:
	ALIDumpTruck();

	virtual bool HasTool() const override { return true; }
	virtual FText GetToolHint() const override;

	/** Current bed angle in degrees (0 = down). */
	UFUNCTION(BlueprintPure, Category = "LostIsland|Vehicle")
	float GetBedAngle() const { return BedAngle; }

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void BuildBody(FLILowPolyMesh& Mesh) const override;
	virtual void ToggleTool() override;

	/** The tipping bed, pivoting on its rear hinge. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UProceduralMeshComponent> BedMesh;

	/** Hinge of the bed, in chassis space. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Vehicle|Bed")
	FVector BedHinge = FVector(-345.f, 0.f, 30.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Vehicle|Bed", meta = (ClampMin = "0.0", ClampMax = "70.0"))
	float BedMaxAngle = 48.f;

	/** Degrees per second while tipping. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Vehicle|Bed", meta = (ClampMin = "1.0"))
	float BedSpeed = 16.f;

	/** The bed only starts moving below this speed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Vehicle|Bed", meta = (Units = "km/h"))
	float MaxTipSpeedKmh = 5.f;

private:
	float BedAngle = 0.f;
};
