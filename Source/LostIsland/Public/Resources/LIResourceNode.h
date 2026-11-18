// Copyright LOST ISLAND Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/LIInteractable.h"
#include "Items/LIItemTypes.h"
#include "UObject/SoftObjectPath.h"
#include "LIResourceNode.generated.h"

class UStaticMeshComponent;
class UProceduralMeshComponent;
struct FLILowPolyMesh;
struct FRandomStream;

/**
 * ALIResourceNode
 *
 * A gatherable world object (tree, rock, bush). Each interaction ("hit"):
 *  - costs the gatherer stamina (hard difficulty: gathering is tiring),
 *  - adds MinYield..MaxYield of ResourceItemId to the gatherer's inventory,
 *  - consumes one of HitsToDeplete.
 * When depleted the node hides and disables collision, then respawns after
 * RespawnTime seconds (0 = never). All state changes run on the server and
 * replicate (bDepleted / RemainingHits).
 *
 * Actor origin = base of the object (the mesh is offset upward), so placement
 * and procedural spawning can put the origin directly on the ground.
 *
 * Presets: ALITreeNode (Wood), ALIRockNode (Stone), ALIFiberBushNode (Fiber), ALIBerryBushNode (Berries).
 */
UCLASS(Blueprintable)
class LOSTISLAND_API ALIResourceNode : public AActor, public ILIInteractable
{
	GENERATED_BODY()

public:
	ALIResourceNode();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ILIInteractable
	virtual bool CanInteract_Implementation(APawn* InstigatorPawn) const override;
	virtual void Interact_Implementation(APawn* InstigatorPawn) override;
	virtual FText GetInteractionPrompt_Implementation(APawn* InstigatorPawn) const override;
	virtual void OnFocusBegin_Implementation(APawn* InstigatorPawn) override;
	virtual void OnFocusEnd_Implementation(APawn* InstigatorPawn) override;

	UFUNCTION(BlueprintPure, Category = "LostIsland|Resource")
	bool IsDepleted() const { return bDepleted; }

	UFUNCTION(BlueprintPure, Category = "LostIsland|Resource")
	FName GetResourceItemId() const { return ResourceItemId; }

	UFUNCTION(BlueprintPure, Category = "LostIsland|Resource")
	int32 GetRemainingHits() const { return RemainingHits; }

	UFUNCTION(BlueprintPure, Category = "LostIsland|Resource")
	int32 GetHitsToDeplete() const { return HitsToDeplete; }

	/** Server: seconds until a depleted node regrows, or -1 if no respawn is pending. */
	float GetRespawnTimeRemaining() const;

	/** Server (save loading): restore hits left, depletion and the pending respawn countdown. */
	void RestoreSavedState(int32 InRemainingHits, bool bInDepleted, float RespawnSecondsRemaining);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Server-side hook after a successful gather. */
	UFUNCTION(BlueprintImplementableEvent, Category = "LostIsland|Resource", meta = (DisplayName = "On Gathered"))
	void BP_OnGathered(APawn* Gatherer, int32 AmountGathered);

	/** All machines: depleted state changed (play VFX, swap to stump mesh...). */
	UFUNCTION(BlueprintImplementableEvent, Category = "LostIsland|Resource", meta = (DisplayName = "On Depleted Changed"))
	void BP_OnDepletedChanged(bool bIsDepleted);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Collision and interaction shape. Hidden in game once a low-poly model (Visual) has been built. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Cosmetic low-poly model, built at BeginPlay on every machine from BuildVisual(). No collision. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UProceduralMeshComponent> Visual;

	/**
	 * Fills the low-poly model in the actor's local space (origin = ground). Random is seeded from the spawn
	 * location, so every machine builds the same variant. Leave empty to keep the tinted placeholder shape.
	 */
	virtual void BuildVisual(FLILowPolyMesh& OutMesh, FRandomStream& Random) const {}

	/** Real art (e.g. Starter Content SM_Rock), scaled to fit; used instead of the code model when it loads. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> DetailMesh;

	/**
	 * Candidate real meshes (ULIVisualAssets) and the size to fit them to: OutTargetSize is the height when
	 * bOutFitHeight, otherwise the width. Null = always use the code model.
	 */
	virtual const TArray<FSoftObjectPath>* GetRealMeshPaths(float& OutTargetSize, bool& bOutFitHeight) const { return nullptr; }

	/** Code-built extras drawn on top of a real mesh (e.g. berries). FittedBox = the real mesh's local bounds. */
	virtual void BuildRealMeshOverlay(FLILowPolyMesh& OutMesh, FRandomStream& Random, const FBox& FittedBox) const {}

	/** Loads and fits a real mesh into DetailMesh; returns false if none is available. */
	bool TrySetupRealMesh(FRandomStream& Random);

	/** Tint for the placeholder shape (sets the "Color" parameter of the engine BasicShapeMaterial). Alpha 0 = don't tint. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LostIsland|Resource")
	FLinearColor PlaceholderColor = FLinearColor(0.f, 0.f, 0.f, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LostIsland|Resource")
	FName ResourceItemId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LostIsland|Resource", meta = (ClampMin = "1"))
	int32 MinYield = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LostIsland|Resource", meta = (ClampMin = "1"))
	int32 MaxYield = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LostIsland|Resource", meta = (ClampMin = "1"))
	int32 HitsToDeplete = 5;

	/** Seconds until a depleted node returns. 0 = never respawns. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LostIsland|Resource", meta = (ClampMin = "0.0", Units = "s"))
	float RespawnTime = 300.f;

	/** Stamina spent per hit. The gatherer needs at least this much to interact. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LostIsland|Resource", meta = (ClampMin = "0.0"))
	float StaminaCostPerHit = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LostIsland|Resource")
	FText PromptText;

	/** Tool that speeds up gathering here (yield x, stamina x, and it wears down). None = bare hands only. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LostIsland|Resource")
	ELIToolType EffectiveTool = ELIToolType::None;

	/** Best matching tool the gatherer carries (and its slot), or null. */
	const class ULIItemDefinition* FindToolFor(const APawn* Gatherer, int32* OutSlotIndex) const;

	/** Stamina this gatherer would spend per hit (reduced by a matching tool). */
	float GetStaminaCostFor(const APawn* Gatherer) const;

private:
	void Deplete();
	void Respawn();
	void ApplyDepletedState();

	UFUNCTION()
	void OnRep_Depleted();

	UPROPERTY(ReplicatedUsing = OnRep_Depleted)
	bool bDepleted = false;

	UPROPERTY(Replicated)
	int32 RemainingHits = 0;

	FTimerHandle RespawnTimerHandle;
};

/** Tree -> Wood. */
UCLASS()
class LOSTISLAND_API ALITreeNode : public ALIResourceNode
{
	GENERATED_BODY()
public:
	ALITreeNode();

protected:
	virtual void BuildVisual(FLILowPolyMesh& OutMesh, FRandomStream& Random) const override;
	virtual const TArray<FSoftObjectPath>* GetRealMeshPaths(float& OutTargetSize, bool& bOutFitHeight) const override;
};

/** Rock -> Stone. */
UCLASS()
class LOSTISLAND_API ALIRockNode : public ALIResourceNode
{
	GENERATED_BODY()
public:
	ALIRockNode();

protected:
	virtual void BuildVisual(FLILowPolyMesh& OutMesh, FRandomStream& Random) const override;
	virtual const TArray<FSoftObjectPath>* GetRealMeshPaths(float& OutTargetSize, bool& bOutFitHeight) const override;
};

/** Berry bush -> Berries (food). */
UCLASS()
class LOSTISLAND_API ALIBerryBushNode : public ALIResourceNode
{
	GENERATED_BODY()
public:
	ALIBerryBushNode();

protected:
	virtual void BuildVisual(FLILowPolyMesh& OutMesh, FRandomStream& Random) const override;
	virtual const TArray<FSoftObjectPath>* GetRealMeshPaths(float& OutTargetSize, bool& bOutFitHeight) const override;
	virtual void BuildRealMeshOverlay(FLILowPolyMesh& OutMesh, FRandomStream& Random, const FBox& FittedBox) const override;
};

/** Bush -> Fiber. */
UCLASS()
class LOSTISLAND_API ALIFiberBushNode : public ALIResourceNode
{
	GENERATED_BODY()
public:
	ALIFiberBushNode();

protected:
	virtual void BuildVisual(FLILowPolyMesh& OutMesh, FRandomStream& Random) const override;
	virtual const TArray<FSoftObjectPath>* GetRealMeshPaths(float& OutTargetSize, bool& bOutFitHeight) const override;
};


/**
 * A resource node that moves: it wanders around its spawn point, keeps out of the sea, and either
 * runs from players (timid) or charges and bites them (aggressive). Server-side movement, replicated.
 * When depleted it hides and later respawns at home like any resource node.
 */
UCLASS(Abstract)
class LOSTISLAND_API ALIAnimalNode : public ALIResourceNode
{
	GENERATED_BODY()
public:
	ALIAnimalNode();
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void BuildVisual(FLILowPolyMesh& OutMesh, FRandomStream& Random) const override;

	UPROPERTY(EditAnywhere, Category = "LostIsland|Animal", meta = (Units = "cm/s"))
	float WanderSpeed = 70.f;

	/** Speed when fleeing (timid) or charging (aggressive). */
	UPROPERTY(EditAnywhere, Category = "LostIsland|Animal", meta = (Units = "cm/s"))
	float RunSpeed = 200.f;

	/** Players closer than this make the animal react (flee or charge). */
	UPROPERTY(EditAnywhere, Category = "LostIsland|Animal", meta = (Units = "cm"))
	float ReactRadius = 400.f;

	UPROPERTY(EditAnywhere, Category = "LostIsland|Animal", meta = (Units = "cm"))
	float WanderRadius = 600.f;

	/** Aggressive animals charge instead of fleeing. */
	UPROPERTY(EditAnywhere, Category = "LostIsland|Animal")
	bool bAggressive = false;

	UPROPERTY(EditAnywhere, Category = "LostIsland|Animal", meta = (EditCondition = "bAggressive"))
	float AttackDamage = 8.f;

	UPROPERTY(EditAnywhere, Category = "LostIsland|Animal", meta = (EditCondition = "bAggressive", Units = "cm"))
	float AttackRange = 160.f;

	UPROPERTY(EditAnywhere, Category = "LostIsland|Animal", meta = (EditCondition = "bAggressive", Units = "s"))
	float AttackCooldown = 1.5f;

private:
	bool PickWanderTarget();
	/** Ground under XY, or false if it is under water, too steep, or missing. */
	bool FindGround(const FVector& XY, FVector& OutGround) const;

	FVector Home = FVector::ZeroVector;
	FVector WanderTarget = FVector::ZeroVector;
	float PauseTime = 0.f;
	float AttackTimer = 0.f;
	bool bWasDepleted = false;
};

/** Crab -> Raw Meat. Timid: scuttles around a beach spawn point and runs from players. */
UCLASS()
class LOSTISLAND_API ALICrabNode : public ALIAnimalNode
{
	GENERATED_BODY()
public:
	ALICrabNode();
};

/** Wild boar -> Raw Meat (lots). Aggressive: charges nearby players and bites. Takes many hits. */
UCLASS()
class LOSTISLAND_API ALIBoarNode : public ALIAnimalNode
{
	GENERATED_BODY()
public:
	ALIBoarNode();
};
