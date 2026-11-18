// Copyright LOST ISLAND Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "LIAnimInstance.generated.h"

class ALICharacter;
class UCharacterMovementComponent;

/**
 * ULIAnimInstance
 *
 * C++ parent for the character Anim Blueprint. Computes locomotion variables once
 * per animation update so the AnimBP graph stays simple (idle/walk/run blendspace
 * driven by GroundSpeed, jump/fall states driven by bIsFalling).
 *
 * Usage: create an Anim Blueprint for your skeleton, set its parent class to
 * LIAnimInstance, and read these variables in the AnimGraph.
 */
UCLASS(Transient, Blueprintable)
class LOSTISLAND_API ULIAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
	UPROPERTY(BlueprintReadOnly, Category = "LostIsland|Locomotion")
	TObjectPtr<ALICharacter> OwningCharacter;

	UPROPERTY(BlueprintReadOnly, Category = "LostIsland|Locomotion")
	TObjectPtr<UCharacterMovementComponent> MovementComponent;

	UPROPERTY(BlueprintReadOnly, Category = "LostIsland|Locomotion")
	FVector Velocity = FVector::ZeroVector;

	/** Horizontal speed in cm/s. */
	UPROPERTY(BlueprintReadOnly, Category = "LostIsland|Locomotion")
	float GroundSpeed = 0.f;

	/** Movement direction relative to actor facing, -180..180 deg (for strafing blendspaces). */
	UPROPERTY(BlueprintReadOnly, Category = "LostIsland|Locomotion")
	float Direction = 0.f;

	/** True when moving with input and above the threshold speed. */
	UPROPERTY(BlueprintReadOnly, Category = "LostIsland|Locomotion")
	bool bShouldMove = false;

	UPROPERTY(BlueprintReadOnly, Category = "LostIsland|Locomotion")
	bool bIsFalling = false;

	UPROPERTY(BlueprintReadOnly, Category = "LostIsland|Locomotion")
	bool bIsSprinting = false;

	/** Minimum ground speed (cm/s) that counts as moving. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Locomotion")
	float MoveSpeedThreshold = 3.f;
};
