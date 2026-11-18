// Copyright LOST ISLAND Team. All Rights Reserved.

#include "Character/LIAnimInstance.h"

#include "Character/LICharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "KismetAnimationLibrary.h"

void ULIAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	OwningCharacter = Cast<ALICharacter>(TryGetPawnOwner());
	MovementComponent = OwningCharacter ? OwningCharacter->GetCharacterMovement() : nullptr;
}

void ULIAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!OwningCharacter || !MovementComponent)
	{
		return;
	}

	Velocity = MovementComponent->Velocity;
	GroundSpeed = Velocity.Size2D();
	Direction = UKismetAnimationLibrary::CalculateDirection(Velocity, OwningCharacter->GetActorRotation());
	bIsFalling = MovementComponent->IsFalling();
	bIsSprinting = OwningCharacter->IsSprinting();

	const bool bHasAcceleration = !MovementComponent->GetCurrentAcceleration().IsNearlyZero();
	bShouldMove = GroundSpeed > MoveSpeedThreshold && bHasAcceleration;
}
