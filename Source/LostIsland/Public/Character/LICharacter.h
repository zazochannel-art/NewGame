// Copyright LOST ISLAND Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "LICharacter.generated.h"

class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class ULIInteractionComponent;
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

public:
	ALICharacter();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "LostIsland|Movement")
	bool IsSprinting() const { return bIsSprinting; }

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

	/** Fired on server and all clients whenever sprint state changes. */
	UPROPERTY(BlueprintAssignable, Category = "LostIsland|Movement")
	FLIOnSprintStateChanged OnSprintStateChanged;

protected:
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

	// ---- Components ----
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ULIInteractionComponent> InteractionComponent;

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

	// ---- Movement tuning ----
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Movement", meta = (ClampMin = "0.0", Units = "cm/s"))
	float WalkSpeed = 380.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Movement", meta = (ClampMin = "0.0", Units = "cm/s"))
	float SprintSpeed = 650.f;

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

	UFUNCTION(Server, Reliable)
	void ServerSetSprinting(bool bNewSprinting);

	UFUNCTION()
	void OnRep_IsSprinting();

	UPROPERTY(ReplicatedUsing = OnRep_IsSprinting)
	bool bIsSprinting = false;

	/** True while the sprint input is held on the owning client. */
	bool bWantsToSprint = false;
};
