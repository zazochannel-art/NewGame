// Copyright LOST ISLAND Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/LIInteractable.h"
#include "Items/LIItemTypes.h"
#include "LIResourceNode.generated.h"

class UStaticMeshComponent;

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

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
};

/** Rock -> Stone. */
UCLASS()
class LOSTISLAND_API ALIRockNode : public ALIResourceNode
{
	GENERATED_BODY()
public:
	ALIRockNode();
};

/** Berry bush -> Berries (food). */
UCLASS()
class LOSTISLAND_API ALIBerryBushNode : public ALIResourceNode
{
	GENERATED_BODY()
public:
	ALIBerryBushNode();
};

/** Bush -> Fiber. */
UCLASS()
class LOSTISLAND_API ALIFiberBushNode : public ALIResourceNode
{
	GENERATED_BODY()
public:
	ALIFiberBushNode();
};
