// Copyright LOST ISLAND Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/LIInteractable.h"
#include "LIConstruction.generated.h"

class UBoxComponent;
class UProceduralMeshComponent;
class ALIDumpTruck;
struct FLILowPolyMesh;

/** What a dump truck load can be. */
UENUM(BlueprintType)
enum class ELIBuildMaterial : uint8
{
	None,
	Gravel,
	Lumber,
	Bricks,
};

UENUM(BlueprintType)
enum class ELIBuildingType : uint8
{
	Barn,
	House,
};

namespace LIConstruction
{
	LOSTISLAND_API FText GetMaterialName(ELIBuildMaterial Material);

	/** Coins for one truck load. */
	LOSTISLAND_API int32 GetMaterialPrice(ELIBuildMaterial Material);
}

/**
 * ALIMaterialDepot
 *
 * A gravel pit, timber yard or brick yard in the farm yard. Park the empty dump truck next to it, walk up and press
 * F: one load is paid for in coins and lands in the truck's bed.
 */
UCLASS()
class LOSTISLAND_API ALIMaterialDepot : public AActor, public ILIInteractable
{
	GENERATED_BODY()

public:
	ALIMaterialDepot();

	/** Call before FinishSpawning. */
	void Setup(ELIBuildMaterial InMaterial) { Material = InMaterial; }

	ELIBuildMaterial GetMaterial() const { return Material; }

	virtual bool CanInteract_Implementation(APawn* InstigatorPawn) const override;
	virtual void Interact_Implementation(APawn* InstigatorPawn) override;
	virtual FText GetInteractionPrompt_Implementation(APawn* InstigatorPawn) const override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

	/** The dump truck must be parked this close to be loaded. */
	UPROPERTY(EditDefaultsOnly, Category = "LostIsland|Build", meta = (Units = "cm"))
	float LoadRadius = 1600.f;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UBoxComponent> Collision;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UProceduralMeshComponent> Visual;

private:
	UFUNCTION()
	void OnRep_Material() { BuildVisual(); }

	void BuildVisual();
	ALIDumpTruck* FindTruck() const;

	UPROPERTY(ReplicatedUsing = OnRep_Material)
	ELIBuildMaterial Material = ELIBuildMaterial::Gravel;
};

/** One step of a building: so many truck loads of one material. */
struct FLIBuildStage
{
	ELIBuildMaterial Material = ELIBuildMaterial::Gravel;
	int32 Loads = 1;
	FText Name;
};

/**
 * ALIConstructionSite
 *
 * A staked-out plot that becomes a building as loads are tipped onto it: foundation (gravel), walls (lumber or
 * bricks), roof (lumber). Back the dump truck up to the plot and tip the bed (Q) to deliver. The model grows with
 * every load. A finished barn lets the farm store its grain and sell it for more (GrainPriceBonus).
 * The sign by the plot (E) tells what the site needs next.
 */
UCLASS()
class LOSTISLAND_API ALIConstructionSite : public AActor, public ILIInteractable
{
	GENERATED_BODY()

public:
	ALIConstructionSite();

	/** Call before FinishSpawning. */
	void Setup(ELIBuildingType InType, int32 InSiteIndex) { BuildingType = InType; SiteIndex = InSiteIndex; }

	int32 GetSiteIndex() const { return SiteIndex; }
	ELIBuildingType GetBuildingType() const { return BuildingType; }
	int32 GetDeliveredLoads() const { return DeliveredLoads; }
	bool IsComplete() const;

	/** Material the next load must be, None once complete. */
	ELIBuildMaterial GetNeededMaterial() const;

	/** Whether a truck tipping at WorldPoint is close enough to unload here. */
	bool IsInDeliveryRange(const FVector& WorldPoint) const;

	/** Server: one truck load tipped here. False if the site does not need that material now. */
	bool DeliverLoad(ELIBuildMaterial Material);

	/** Server: restore from a save. */
	void RestoreProgress(int32 Loads);

	FText GetDisplayName() const;
	FText GetStatusText() const;

	virtual void Interact_Implementation(APawn* InstigatorPawn) override;
	virtual FText GetInteractionPrompt_Implementation(APawn* InstigatorPawn) const override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Grain price multiplier a finished barn gives the farm. */
	UPROPERTY(EditDefaultsOnly, Category = "LostIsland|Build", meta = (ClampMin = "1.0"))
	float GrainPriceBonus = 1.25f;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UProceduralMeshComponent> Visual;

	/** The site board: what the player looks at to read the site's needs. */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UBoxComponent> SignCollision;

	/** Walls of the finished shell (front wall in two parts around the door). */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TArray<TObjectPtr<UBoxComponent>> WallCollision;

private:
	UFUNCTION()
	void OnRep_Progress();

	const TArray<FLIBuildStage>& GetStages() const;
	int32 GetTotalLoads() const;
	/** Current stage index (Stages.Num() when complete) and loads delivered within it. */
	void GetStageProgress(int32& OutStage, int32& OutLoadsInStage) const;
	/** 0..1 completion of a stage. */
	float GetStageFraction(int32 Stage) const;

	FVector GetHalfSize() const;
	float GetWallHeight() const;
	float GetRoofHeight() const;
	float GetDoorWidth() const;

	void RebuildVisual();
	void BuildModel(FLILowPolyMesh& Mesh) const;
	void UpdateCollision();
	void ApplyCompletionBonus();

	UPROPERTY(Replicated)
	ELIBuildingType BuildingType = ELIBuildingType::Barn;

	UPROPERTY(Replicated)
	int32 SiteIndex = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Progress)
	int32 DeliveredLoads = 0;
};
