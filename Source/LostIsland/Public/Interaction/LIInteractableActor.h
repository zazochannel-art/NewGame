// Copyright LOST ISLAND Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/LIInteractable.h"
#include "LIInteractableActor.generated.h"

class UStaticMeshComponent;

/**
 * ALIInteractableActor
 *
 * Minimal, reusable interactable: a static mesh with a prompt and an optional
 * use limit. Serves as the test target for Milestone 1 and as a base class for
 * simple world objects. Resource nodes (Milestone 3) will build on the same
 * ILIInteractable contract rather than on this class.
 *
 * Blueprint: subclass it, set the mesh/prompt, and implement OnInteracted.
 */
UCLASS(Blueprintable)
class LOSTISLAND_API ALIInteractableActor : public AActor, public ILIInteractable
{
	GENERATED_BODY()

public:
	ALIInteractableActor();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ILIInteractable
	virtual bool CanInteract_Implementation(APawn* InstigatorPawn) const override;
	virtual void Interact_Implementation(APawn* InstigatorPawn) override;
	virtual FText GetInteractionPrompt_Implementation(APawn* InstigatorPawn) const override;
	virtual void OnFocusBegin_Implementation(APawn* InstigatorPawn) override;
	virtual void OnFocusEnd_Implementation(APawn* InstigatorPawn) override;

	UFUNCTION(BlueprintPure, Category = "LostIsland|Interaction")
	int32 GetUseCount() const { return UseCount; }

	UFUNCTION(BlueprintCallable, Category = "LostIsland|Interaction")
	void SetPromptText(const FText& InPrompt) { PromptText = InPrompt; }

	/** 0 = unlimited. Set before or right after spawning (server). */
	UFUNCTION(BlueprintCallable, Category = "LostIsland|Interaction")
	void SetMaxUses(int32 InMaxUses) { MaxUses = FMath::Max(0, InMaxUses); }

protected:
	/** Server-side Blueprint hook, fired after a successful interaction. */
	UFUNCTION(BlueprintImplementableEvent, Category = "LostIsland|Interaction", meta = (DisplayName = "On Interacted"))
	void BP_OnInteracted(APawn* InstigatorPawn, int32 NewUseCount);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LostIsland|Interaction")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Text shown in the HUD prompt. */
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category = "LostIsland|Interaction")
	FText PromptText;

	/** 0 = unlimited uses. */
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category = "LostIsland|Interaction", meta = (ClampMin = "0"))
	int32 MaxUses = 0;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "LostIsland|Interaction")
	int32 UseCount = 0;
};
