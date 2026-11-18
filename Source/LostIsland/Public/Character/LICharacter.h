// Copyright LOST ISLAND Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "LICharacter.generated.h"

class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class ULIInteractionComponent;
class ULISurvivalComponent;
class ULIInventoryComponent;
class ULICraftingComponent;
class USpringArmComponent;
class UStaticMeshComponent;
struct FInputActionValue;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLIOnSprintStateChanged, bool, bIsSprinting);

/**
 * ALICharacter
 *
 * Third-person survivor. Milestone 1 scope:
 *  - Camera-relative movement, mouse/gamepad look, scroll-wheel camera zoom.
 *  - Jump (ACharacter), hold-to-sprint (replicated, server-validated).
 *  - Interaction via ULIInteractionComponent.
 *
 * Input: uses Enhanced Input. If no InputMappingContext / InputActions are assigned
 * (e.g. in a Blueprint subclass), sensible defaults are created at runtime so the
 * project is playable without any .uasset content:
 *     WASD / Left stick     Move           Mouse / Right stick   Look
 *     Space / Face bottom   Jump           Left Shift / L3       Sprint (hold)
 *     E / Face left         Interact       Mouse wheel           Camera zoom
 *     Tab / Special right   Inventory      F5 / F9               Quick save / load
 *     F / D-pad up          Eat / drink / heal (best consumable)
 *     1..9 (inventory open) Craft recipe N
 *     G / D-pad down        Place campfire (placeable item)
 *
 * Visuals: assign a Skeletal Mesh + Anim Blueprint (parented to ULIAnimInstance)
 * in a Blueprint subclass. Until then a placeholder capsule-sized cylinder is shown.
 *
 * Extension points for later milestones:
 *  - CanSprint() is a BlueprintNativeEvent; the stamina system will gate it.
 *  - OnSprintStateChanged lets the survival component start/stop stamina drain
 *    without polling.
 */
UCLASS(Blueprintable)
class LOSTISLAND_API ALICharacter : public ACharacter
{
	GENERATED_BODY()
	friend class ALIPlayerController; // debug commands (LIBuildMode)

public:
	ALICharacter();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "LostIsland|Movement")
	bool IsSprinting() const { return bIsSprinting; }

	/** True while in deep water (local state, computed the same way on server and owning client). */
	UFUNCTION(BlueprintPure, Category = "LostIsland|Movement")
	bool IsSwimming() const { return bIsSwimmingLocal; }

	/** Local: build mode is on (preview of the next building piece). */
	UFUNCTION(BlueprintPure, Category = "LostIsland|Building")
	bool IsInBuildMode() const { return bBuildMode; }

	/** Whether the character is currently allowed to sprint. Stamina will override this. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "LostIsland|Movement")
	bool CanSprint() const;

	/** Force sprinting to stop (e.g. stamina depleted). Safe to call on server or owning client. */
	UFUNCTION(BlueprintCallable, Category = "LostIsland|Movement")
	void ForceStopSprinting();

	UFUNCTION(BlueprintPure, Category = "LostIsland|Movement")
	float GetWalkSpeed() const { return WalkSpeed; }

	UFUNCTION(BlueprintPure, Category = "LostIsland|Movement")
	float GetSprintSpeed() const { return SprintSpeed; }

	USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	UCameraComponent* GetFollowCamera() const { return FollowCamera; }
	ULIInteractionComponent* GetInteractionComponent() const { return InteractionComponent; }

	UFUNCTION(BlueprintPure, Category = "LostIsland|Survival")
	ULISurvivalComponent* GetSurvivalComponent() const { return SurvivalComponent; }

	UFUNCTION(BlueprintPure, Category = "LostIsland|Inventory")
	ULIInventoryComponent* GetInventoryComponent() const { return InventoryComponent; }

	UFUNCTION(BlueprintPure, Category = "LostIsland|Crafting")
	ULICraftingComponent* GetCraftingComponent() const { return CraftingComponent; }

	UFUNCTION(BlueprintPure, Category = "LostIsland|Survival")
	bool IsDead() const;

	/** Routes engine damage (ApplyDamage, damage volumes...) into the survival component. */
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	/** Fired on server and all clients whenever sprint state changes. */
	UPROPERTY(BlueprintAssignable, Category = "LostIsland|Movement")
	FLIOnSprintStateChanged OnSprintStateChanged;

protected:
	virtual void PostInitializeComponents() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void BeginPlay() override;
	virtual void NotifyControllerChanged() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	// ---- Input handlers ----
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void Zoom(const FInputActionValue& Value);
	void StartSprint();
	void StopSprint();
	void Interact();
	void ToggleInventory();
	void Consume();
	void Place();
	void CraftSlot(const struct FInputActionInstance& Instance);
	void QuickSave();
	void QuickLoad();
	void ToggleBuildMode();

	// ---- Components ----
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ULIInteractionComponent> InteractionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ULISurvivalComponent> SurvivalComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ULIInventoryComponent> InventoryComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ULICraftingComponent> CraftingComponent;

	/** Blueprint hook for death presentation (ragdoll, camera, UI). Runs on all machines. */
	UFUNCTION(BlueprintImplementableEvent, Category = "LostIsland|Survival", meta = (DisplayName = "On Died"))
	void BP_OnDied();

	/** Visible stand-in body until a skeletal mesh is assigned. Hidden automatically at BeginPlay if one is. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> PlaceholderBody;

	// ---- Input assets (optional; runtime defaults are generated when left empty) ----
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Input")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Input")
	TObjectPtr<UInputAction> SprintAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Input")
	TObjectPtr<UInputAction> InteractAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Input")
	TObjectPtr<UInputAction> ZoomAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Input")
	TObjectPtr<UInputAction> InventoryAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Input")
	TObjectPtr<UInputAction> ConsumeAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Input")
	TObjectPtr<UInputAction> PlaceAction;

	/** Number keys 1..9 while the inventory is open: craft recipe N. One action per key (index = array position). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Input")
	TArray<TObjectPtr<UInputAction>> CraftSlotActions;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Input")
	TObjectPtr<UInputAction> QuickSaveAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Input")
	TObjectPtr<UInputAction> QuickLoadAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Input")
	TObjectPtr<UInputAction> BuildModeAction;

	// ---- Movement tuning ----
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Movement", meta = (ClampMin = "0.0", Units = "cm/s"))
	float WalkSpeed = 380.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Movement", meta = (ClampMin = "0.0", Units = "cm/s"))
	float SprintSpeed = 650.f;

	// ---- Swimming (no water volume: the island sea level is a plane, so depth is computed directly) ----
	/** Horizontal swim speed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Swimming", meta = (ClampMin = "0.0", Units = "cm/s"))
	float SwimSpeed = 260.f;

	/** Water depth at the feet (cm) above which the character starts swimming. Leaves with 30 cm of hysteresis. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Swimming", meta = (ClampMin = "0.0", Units = "cm"))
	float SwimStartDepth = 130.f;

	/** While swimming, the capsule centre floats this far below the surface (head stays above). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Swimming", meta = (Units = "cm"))
	float FloatDepth = 55.f;

	/** Sea level used when no island generator exists. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Swimming", meta = (Units = "cm"))
	float DefaultSeaLevelZ = 0.f;

	// ---- Camera tuning ----
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Camera", meta = (ClampMin = "50.0", Units = "cm"))
	float DefaultArmLength = 400.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Camera", meta = (ClampMin = "50.0", Units = "cm"))
	float MinArmLength = 150.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Camera", meta = (ClampMin = "50.0", Units = "cm"))
	float MaxArmLength = 700.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Camera", meta = (ClampMin = "0.0", Units = "cm"))
	float ZoomStep = 50.f;

private:
	/** Creates runtime input actions / mapping context for anything not assigned. Idempotent. */
	void EnsureDefaultInput();

	void SetSprinting(bool bNewSprinting);
	void ApplyMovementSpeed();

	/** Enters/leaves swimming from the water depth, keeps the body floating (or sinking when exhausted). */
	void UpdateSwimming(float DeltaSeconds);
	float GetSeaLevelZ();

	TWeakObjectPtr<AActor> CachedIsland;
	bool bSearchedIsland = false;
	bool bIsSwimmingLocal = false;

	/** Local only: build mode shows a preview of the building piece G would place. */
	void UpdateBuildPreview();
	void DestroyBuildPreview();
	bool bBuildMode = false;

	UPROPERTY(Transient)
	TObjectPtr<class ALIBuildingPiece> BuildPreview;

	UFUNCTION(Server, Reliable)
	void ServerSetSprinting(bool bNewSprinting);

	/**
	 * Server -> owning client: the server rejected a predicted sprint change. Needed because the
	 * replicated bIsSprinting does not change (and so never replicates) when the server simply
	 * keeps its old value, which would leave the client running at the wrong speed.
	 */
	UFUNCTION(Client, Reliable)
	void ClientCorrectSprinting(bool bServerSprinting);

	/** Applies a sprint state locally without sending an RPC (replication / correction paths). */
	void ApplySprintStateLocal(bool bNewSprinting);

	UFUNCTION()
	void OnRep_IsSprinting();

	UFUNCTION()
	void HandleExhaustedChanged(bool bIsExhausted);

	UFUNCTION()
	void HandleDeath(AController* Killer);

	UPROPERTY(ReplicatedUsing = OnRep_IsSprinting)
	bool bIsSprinting = false;

	/** True while the sprint input is held on the owning client. */
	bool bWantsToSprint = false;
};
