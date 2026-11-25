// Copyright LOST ISLAND Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/LIInteractable.h"
#include "LIFarmField.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UProceduralMeshComponent;

/** What one patch of a field looks like right now. Stored as uint8 (replicated, saved). */
UENUM(BlueprintType)
enum class ELIFieldCell : uint8
{
	Grass,    // untouched meadow
	Plowed,   // dark furrows, ready to sow
	Sown,     // seed in the ground, nothing visible yet
	Sprout,   // short green shoots
	Growing,  // tall green wheat
	Ripe,     // golden, ready to harvest
	Stubble   // harvested: short dry stalks (plough again to start over)
};

/** Implement on the tractor's hitch. */
UENUM(BlueprintType)
enum class ELIFarmTool : uint8
{
	Plough,
	Seeder,
	Harvester,
	Count UMETA(Hidden)
};

/**
 * ALIFarmField
 *
 * A rectangular field split into square patches (cells). Each cell goes Grass -> Plowed (plough) -> Sown (seeder)
 * -> Sprout -> Growing -> Ripe (time) -> Stubble (harvester, gives grain) -> Plowed again.
 *
 * The server owns the state: tools call ApplyTool, growth runs on a 1 s timer. The per-cell states replicate as one
 * byte array; every machine rebuilds the look (soil patches as a procedural mesh, plants as instanced cones) when
 * it changes, at most a few times per second.
 *
 * The actor sits on the flat ground in the field's centre, axis-aligned (the valley generator flattens the area).
 */
UCLASS()
class LOSTISLAND_API ALIFarmField : public AActor
{
	GENERATED_BODY()

public:
	ALIFarmField();

	/** Before FinishSpawning (server): size in cells and which field of the valley this is (saves use the index). */
	void Setup(int32 InFieldIndex, int32 InCellsX, int32 InCellsY, float InCellSize);

	/** Server: work the cell under WorldPoint with Tool. Returns kilograms of grain harvested (0 for other tools). */
	int32 ApplyTool(const FVector& WorldPoint, ELIFarmTool Tool);

	/** Whether a world position is over this field (ignores height). */
	bool ContainsPoint(const FVector& WorldPoint) const;

	UFUNCTION(BlueprintPure, Category = "LostIsland|Farm")
	int32 GetFieldIndex() const { return FieldIndex; }

	/** Number of cells in a state (HUD: "Field 2: 40/121 ploughed"). */
	int32 CountCells(ELIFieldCell State) const;
	int32 GetNumCells() const { return Cells.Num(); }

	/** "Field 2: 30% ploughed, 50% growing..." (short, for the HUD). */
	FText GetStatusText() const;

	// ---- Save ----
	const TArray<uint8>& GetCellStates() const { return Cells; }
	const TArray<float>& GetCellGrowth() const { return Growth; }
	/** Server: restore saved state (arrays of the wrong size are ignored). */
	void RestoreState(const TArray<uint8>& InCells, const TArray<float>& InGrowth);

	/** Real seconds from sowing to ripe. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LostIsland|Farm", meta = (ClampMin = "10.0", Units = "s"))
	float GrowthSeconds = 480.f;

	/** Grain per harvested cell. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LostIsland|Farm", meta = (ClampMin = "1"))
	int32 YieldPerCell = 25;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UProceduralMeshComponent> SoilMesh;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> SproutPlants;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> GrowingPlants;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> RipePlants;

private:
	UFUNCTION()
	void OnRep_Cells() { bVisualDirty = true; }

	void UpdateGrowth();
	void RebuildVisuals();
	int32 CellIndexAt(const FVector& WorldPoint) const;
	FVector CellCenterLocal(int32 X, int32 Y) const;
	void SetCell(int32 Index, ELIFieldCell State);

	UPROPERTY(Replicated)
	int32 FieldIndex = 0;

	UPROPERTY(Replicated)
	int32 CellsX = 11;

	UPROPERTY(Replicated)
	int32 CellsY = 11;

	UPROPERTY(Replicated)
	float CellSize = 200.f;

	UPROPERTY(ReplicatedUsing = OnRep_Cells)
	TArray<uint8> Cells;

	/** Server only: 0..1 from sowing to ripe, per cell. */
	TArray<float> Growth;

	bool bVisualDirty = true;
	double LastRebuildTime = -1.0;
	FTimerHandle GrowthTimer;
};

/**
 * ALIGrainSilo
 *
 * The farm's buyer, in the yard: E sells all harvested grain for coins (see ALIGameState::SellGrain).
 */
UCLASS()
class LOSTISLAND_API ALIGrainSilo : public AActor, public ILIInteractable
{
	GENERATED_BODY()

public:
	ALIGrainSilo();

	virtual bool CanInteract_Implementation(APawn* InstigatorPawn) const override;
	virtual void Interact_Implementation(APawn* InstigatorPawn) override;
	virtual FText GetInteractionPrompt_Implementation(APawn* InstigatorPawn) const override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<class UCapsuleComponent> Collision;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UProceduralMeshComponent> Visual;
};
