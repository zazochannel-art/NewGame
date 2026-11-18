// Copyright LOST ISLAND Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "LIInteractionComponent.generated.h"

class APawn;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FLIOnInteractableFocusChanged, AActor*, FocusedActor, const FText&, Prompt);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLIOnInteracted, AActor*, TargetActor);

/**
 * ULIInteractionComponent
 *
 * Finds ILIInteractable actors in front of the player's camera and triggers
 * interactions on them.
 *
 * Design:
 *  - NO TICK. Focus detection runs on a looping timer (ScanInterval) and only
 *    on the locally controlled pawn. Remote proxies and the server-side copy
 *    of a client's pawn never trace.
 *  - TryInteract() is called from input on the owning client. It forwards to the
 *    server via a reliable RPC; the server re-validates distance and CanInteract
 *    before calling ILIInteractable::Interact. Safe for 2-4 player co-op.
 *  - The owner must be an APawn (the component reads its controller's view point).
 */
UCLASS(ClassGroup = (LostIsland), meta = (BlueprintSpawnableComponent))
class LOSTISLAND_API ULIInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULIInteractionComponent();

	/** Start/stop the focus scan timer. The owning character calls this when its controller changes. */
	UFUNCTION(BlueprintCallable, Category = "LostIsland|Interaction")
	void SetScanningEnabled(bool bEnabled);

	/** Interact with the currently focused actor (if any). Call on the owning client or server. */
	UFUNCTION(BlueprintCallable, Category = "LostIsland|Interaction")
	void TryInteract();

	UFUNCTION(BlueprintPure, Category = "LostIsland|Interaction")
	AActor* GetFocusedActor() const { return FocusedActor.Get(); }

	UFUNCTION(BlueprintPure, Category = "LostIsland|Interaction")
	FText GetFocusedPrompt() const { return FocusedPrompt; }

	UFUNCTION(BlueprintPure, Category = "LostIsland|Interaction")
	float GetInteractionDistance() const { return InteractionDistance; }

	/** Local-only: fired when the focused interactable or its prompt changes. Null actor = nothing focused. */
	UPROPERTY(BlueprintAssignable, Category = "LostIsland|Interaction")
	FLIOnInteractableFocusChanged OnFocusChanged;

	/** Server-only: fired after a successful, validated interaction. */
	UPROPERTY(BlueprintAssignable, Category = "LostIsland|Interaction")
	FLIOnInteracted OnInteracted;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Max reach, measured from the pawn's centre to the target's bounds (cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Interaction", meta = (ClampMin = "50.0", Units = "cm"))
	float InteractionDistance = 250.f;

	/** Sphere sweep radius. Larger values make small objects easier to target. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Interaction", meta = (ClampMin = "0.0", Units = "cm"))
	float TraceRadius = 20.f;

	/** Seconds between focus scans. 0.1 s is responsive and very cheap. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Interaction", meta = (ClampMin = "0.02", Units = "s"))
	float ScanInterval = 0.1f;

	/** Collision channel used for the focus sweep. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Interaction")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	/** Extra reach the server allows on top of InteractionDistance to absorb network latency. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Interaction", meta = (ClampMin = "0.0", Units = "cm"))
	float ServerDistanceTolerance = 100.f;

	/** Minimum seconds between interactions, enforced on the server (anti-spam / gather pacing). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Interaction", meta = (ClampMin = "0.0", Units = "s"))
	float MinInteractInterval = 0.4f;

	/** Draw the focus sweep in the world (development builds only). */
	UPROPERTY(EditAnywhere, Category = "LostIsland|Interaction|Debug")
	bool bDrawDebugTrace = false;

private:
	void ScanForInteractable();
	void SetFocusedActor(AActor* NewFocus);
	bool IsWithinReach(const AActor* Target, float ExtraTolerance) const;
	bool CanInteractWith(AActor* Target, float ExtraTolerance) const;

	UFUNCTION(Server, Reliable)
	void ServerInteract(AActor* Target);

	/** Server-side: validate and execute. */
	void PerformInteract(AActor* Target);

	APawn* GetOwnerPawn() const;

	FTimerHandle ScanTimerHandle;
	double LastServerInteractTime = -1000.0;
	TWeakObjectPtr<AActor> FocusedActor;
	FText FocusedPrompt;
};
