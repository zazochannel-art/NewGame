// Copyright LOST ISLAND Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Vehicles/LIVehicle.h"
#include "Farm/LIFarmField.h"
#include "LIFarmVehicles.generated.h"

class UProceduralMeshComponent;

/**
 * ALITractor
 *
 * Slow, heavy and sure-footed: big rear wheels, small steering wheels in front, an open cab under a sun roof and a
 * hitch at the back carrying one implement: plough, seeder or harvester (T switches while it is raised).
 * Q lowers the implement; while lowered and moving, every patch of field under it is worked (ALIFarmField::ApplyTool)
 * and the tractor is limited to WorkingMaxSpeedKmh. Harvested grain goes to the farm store (ALIGameState).
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

	UFUNCTION(BlueprintPure, Category = "LostIsland|Vehicle")
	ELIFarmTool GetImplement() const { return Implement; }

	virtual bool HasTool() const override { return true; }
	virtual FText GetToolHint() const override;
	virtual bool HasToolModes() const override { return true; }
	virtual FText GetToolModeName() const override;
	virtual FText GetStatusText() const override;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void BuildBody(FLILowPolyMesh& Mesh) const override;
	virtual void CycleTool() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Vehicle")
	FVector HitchPoint = FVector(-190.f, 0.f, -40.f);

	/** Working width of every implement. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Farm", meta = (Units = "cm"))
	float ImplementWidth = 320.f;

	/** Top speed with the implement lowered. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Farm", meta = (Units = "km/h"))
	float WorkingMaxSpeedKmh = 14.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UProceduralMeshComponent> ImplementMesh;

private:
	UFUNCTION()
	void OnRep_Implement() { BuildImplement(); }

	void BuildImplement();
	void WorkFields();
	ALIFarmField* FindFieldUnder(const FVector& WorldPoint);
	void RefreshFields();

	UPROPERTY(ReplicatedUsing = OnRep_Implement)
	ELIFarmTool Implement = ELIFarmTool::Plough;

	float RoadMaxSpeedKmh = 32.f;
	float ImplementDrop = 0.f;
	double LastFieldSearch = -100.0;
	TArray<TWeakObjectPtr<ALIFarmField>> Fields;
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
