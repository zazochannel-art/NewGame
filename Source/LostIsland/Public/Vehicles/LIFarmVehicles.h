// Copyright LOST ISLAND Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Vehicles/LIVehicle.h"
#include "Farm/LIFarmField.h"
#include "Construction/LIConstruction.h"
#include "LIFarmVehicles.generated.h"

class UProceduralMeshComponent;
class UPhysicsConstraintComponent;
class ALIGrainTrailer;

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

	/** Where a trailer's drawbar couples, behind the implement, in chassis space. */
	FVector GetTowPoint() const { return TowPoint; }

	ALIGrainTrailer* GetTrailer() const { return Trailer; }

	/** Server: called by the trailer when it is hitched (or unhitched, with null). */
	void SetTrailer(ALIGrainTrailer* InTrailer);

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

	/** Trailer coupling, behind the longest implement. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Vehicle")
	FVector TowPoint = FVector(-470.f, 0.f, -30.f);

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
	void TellDriver(const FText& Message);

	UPROPERTY(ReplicatedUsing = OnRep_Implement)
	ELIFarmTool Implement = ELIFarmTool::Plough;

	UPROPERTY(Replicated)
	TObjectPtr<ALIGrainTrailer> Trailer;

	/** Throttles "hitch the trailer" / "trailer full" messages while harvesting. */
	double LastHarvestWarning = -100.0;

	float RoadMaxSpeedKmh = 32.f;
	float ImplementDrop = 0.f;
	double LastFieldSearch = -100.0;
	TArray<TWeakObjectPtr<ALIFarmField>> Fields;
};

/**
 * ALIDumpTruck
 *
 * Six-wheeled site truck. Q tips the bed (hinged at the back) up to BedMaxAngle and back down; the bed only tips
 * while the truck is nearly stopped. A material depot (ALIMaterialDepot) fills the bed with one load; tipping it
 * with the tail at a construction site (ALIConstructionSite) that needs that material delivers it.
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

	ELIBuildMaterial GetCargo() const { return Cargo; }

	/** Whether a depot may load the truck now: empty bed, bed down, standing still. */
	bool CanLoadCargo() const;

	/** Server: a depot put one load in the bed. */
	void LoadCargo(ELIBuildMaterial Material);

	/** World position of the bed's tail, where the load comes out. */
	FVector GetTipPoint() const;

	virtual FText GetStatusText() const override;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void BuildBody(FLILowPolyMesh& Mesh) const override;
	virtual void ToggleTool() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** The load in the bed (child of the bed, so it tips with it). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UProceduralMeshComponent> CargoMesh;

	/** Bed angle at which the load slides out. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Vehicle|Bed")
	float UnloadAngle = 35.f;

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
	UFUNCTION()
	void OnRep_Cargo() { BuildCargo(); }

	void BuildCargo();
	void TryUnload();
	void TellDriver(const FText& Message) const;

	UPROPERTY(ReplicatedUsing = OnRep_Cargo)
	ELIBuildMaterial Cargo = ELIBuildMaterial::None;

	float BedAngle = 0.f;
	/** One "nobody needs this here" message per raise of the bed. */
	bool bUnloadTried = false;
};

/**
 * ALIGrainTrailer
 *
 * Single-axle grain trailer with a small jockey wheel at the front. Walk up to it and press E to hitch it to a
 * tractor whose coupling (behind the implement) is close to the drawbar, or to unhitch it. While hitched it rolls
 * freely behind the tractor (a physics constraint at the drawbar); unhitched it stands on its brakes.
 * The harvester fills it (up to Capacity); at the grain silo, E unloads and sells what it carries.
 */
UCLASS()
class LOSTISLAND_API ALIGrainTrailer : public ALIVehicle
{
	GENERATED_BODY()

public:
	ALIGrainTrailer();

	virtual bool CanInteract_Implementation(APawn* InstigatorPawn) const override;
	virtual void Interact_Implementation(APawn* InstigatorPawn) override;
	virtual FText GetInteractionPrompt_Implementation(APawn* InstigatorPawn) const override;

	ALITractor* GetTractor() const { return HitchedTractor; }
	int32 GetGrainKg() const { return GrainKg; }
	int32 GetCapacityKg() const { return CapacityKg; }
	bool IsFull() const { return GrainKg >= CapacityKg; }

	/** Server: harvested grain in. Returns how much fitted. */
	int32 AddGrain(int32 Kg);

	/** Server: everything out (to the silo). Returns the amount. */
	int32 EmptyGrain();

	/** Server: couple to Tractor (lines the trailer up behind it first). */
	bool HitchTo(ALITractor* Tractor);

	/** Server: uncouple. */
	void Unhitch();

	/** The closest tractor whose coupling is within reach of the drawbar, or null. */
	ALITractor* FindTractorInReach() const;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void BuildBody(FLILowPolyMesh& Mesh) const override;
	virtual bool IsParked() const override { return HitchedTractor == nullptr; }
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPhysicsConstraintComponent> Coupling;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UProceduralMeshComponent> GrainMesh;

	/** Front tip of the drawbar, in chassis space. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Vehicle")
	FVector DrawbarTip = FVector(470.f, 0.f, -30.f);

	/** How far the tractor's coupling may be from the drawbar for E to hitch. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Vehicle", meta = (Units = "cm"))
	float HitchReach = 400.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Farm", meta = (ClampMin = "1"))
	int32 CapacityKg = 2000;

private:
	UFUNCTION()
	void OnRep_Grain() { UpdateGrain(); }

	void UpdateGrain();

	UPROPERTY(Replicated)
	TObjectPtr<ALITractor> HitchedTractor;

	UPROPERTY(ReplicatedUsing = OnRep_Grain)
	int32 GrainKg = 0;

	float EmptyMass = 1400.f;
	/** Fill fraction the grain model was last built for (rebuilt in 5% steps). */
	int32 BuiltFillStep = -1;
};
