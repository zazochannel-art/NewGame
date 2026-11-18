// Copyright LOST ISLAND Team. All Rights Reserved.

#include "Character/LICharacter.h"

#include "Interaction/LIInteractionComponent.h"
#include "Survival/LISurvivalComponent.h"
#include "LostIsland.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/DamageEvents.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

ALICharacter::ALICharacter()
{
	// Character logic is fully event/input driven.
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);

	// Camera drives look; the body turns toward movement direction.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = true;
	Movement->RotationRate = FRotator(0.f, 540.f, 0.f);
	Movement->JumpZVelocity = 480.f;
	Movement->AirControl = 0.3f;
	Movement->MaxWalkSpeed = WalkSpeed;
	Movement->MinAnalogWalkSpeed = 20.f;
	Movement->BrakingDecelerationWalking = 2000.f;
	Movement->BrakingDecelerationFalling = 1500.f;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = DefaultArmLength;
	CameraBoom->SocketOffset = FVector(0.f, 0.f, 40.f);
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 12.f;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	InteractionComponent = CreateDefaultSubobject<ULIInteractionComponent>(TEXT("Interaction"));
	SurvivalComponent = CreateDefaultSubobject<ULISurvivalComponent>(TEXT("Survival"));

	// Standard UE mannequin alignment for when a skeletal mesh is assigned.
	GetMesh()->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -96.f), FRotator(0.f, -90.f, 0.f));

	PlaceholderBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderBody"));
	PlaceholderBody->SetupAttachment(GetCapsuleComponent());
	PlaceholderBody->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	PlaceholderBody->SetGenerateOverlapEvents(false);
	PlaceholderBody->SetCanEverAffectNavigation(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		// Engine cylinder is 100x100x100 cm with a centred pivot -> scale to the capsule.
		PlaceholderBody->SetStaticMesh(CylinderMesh.Object);
		PlaceholderBody->SetRelativeScale3D(FVector(0.8f, 0.8f, 1.9f));
	}
}

void ALICharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ALICharacter, bIsSprinting);
}

void ALICharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	SurvivalComponent->OnExhaustedChanged.AddDynamic(this, &ALICharacter::HandleExhaustedChanged);
	SurvivalComponent->OnDeath.AddDynamic(this, &ALICharacter::HandleDeath);
}

void ALICharacter::BeginPlay()
{
	Super::BeginPlay();

	// Blueprint subclasses may have changed tuning after the constructor ran.
	CameraBoom->TargetArmLength = FMath::Clamp(DefaultArmLength, MinArmLength, MaxArmLength);
	ApplyMovementSpeed();

	if (GetMesh() && GetMesh()->GetSkeletalMeshAsset())
	{
		PlaceholderBody->SetVisibility(false);
		PlaceholderBody->SetHiddenInGame(true);
	}
}

void ALICharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		EnsureDefaultInput();

		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (DefaultMappingContext)
			{
				Subsystem->AddMappingContext(DefaultMappingContext, 0);
			}
		}
	}

	// Only the locally controlled player needs focus tracing.
	InteractionComponent->SetScanningEnabled(IsLocallyControlled() && IsPlayerControlled());

	// Lost control (unpossessed, menu, death...) -> stop sprinting.
	if (!Controller)
	{
		bWantsToSprint = false;
		if (HasAuthority())
		{
			SetSprinting(false);
		}
	}
}

void ALICharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	EnsureDefaultInput();

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EIC)
	{
		UE_LOG(LogLostIsland, Error,
			TEXT("%s requires the Enhanced Input component. Check DefaultInput.ini (DefaultInputComponentClass)."), *GetName());
		return;
	}

	if (JumpAction)
	{
		EIC->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EIC->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
	}
	if (MoveAction)
	{
		EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ALICharacter::Move);
	}
	if (LookAction)
	{
		EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &ALICharacter::Look);
	}
	if (ZoomAction)
	{
		EIC->BindAction(ZoomAction, ETriggerEvent::Triggered, this, &ALICharacter::Zoom);
	}
	if (SprintAction)
	{
		EIC->BindAction(SprintAction, ETriggerEvent::Started, this, &ALICharacter::StartSprint);
		EIC->BindAction(SprintAction, ETriggerEvent::Completed, this, &ALICharacter::StopSprint);
		EIC->BindAction(SprintAction, ETriggerEvent::Canceled, this, &ALICharacter::StopSprint);
	}
	if (InteractAction)
	{
		EIC->BindAction(InteractAction, ETriggerEvent::Started, this, &ALICharacter::Interact);
	}
}

void ALICharacter::EnsureDefaultInput()
{
	auto MakeAction = [this](const TCHAR* Name, EInputActionValueType ValueType) -> UInputAction*
	{
		UInputAction* Action = NewObject<UInputAction>(this, FName(Name), RF_Transient);
		Action->ValueType = ValueType;
		return Action;
	};

	if (!MoveAction)     { MoveAction     = MakeAction(TEXT("IA_Move_Runtime"),     EInputActionValueType::Axis2D); }
	if (!LookAction)     { LookAction     = MakeAction(TEXT("IA_Look_Runtime"),     EInputActionValueType::Axis2D); }
	if (!JumpAction)     { JumpAction     = MakeAction(TEXT("IA_Jump_Runtime"),     EInputActionValueType::Boolean); }
	if (!SprintAction)   { SprintAction   = MakeAction(TEXT("IA_Sprint_Runtime"),   EInputActionValueType::Boolean); }
	if (!InteractAction) { InteractAction = MakeAction(TEXT("IA_Interact_Runtime"), EInputActionValueType::Boolean); }
	if (!ZoomAction)     { ZoomAction     = MakeAction(TEXT("IA_Zoom_Runtime"),     EInputActionValueType::Axis1D); }

	if (DefaultMappingContext)
	{
		return; // Designer-authored mapping context takes precedence.
	}

	UInputMappingContext* IMC = NewObject<UInputMappingContext>(this, TEXT("IMC_Default_Runtime"), RF_Transient);

	auto Swizzle = [IMC]()
	{
		return NewObject<UInputModifierSwizzleAxis>(IMC); // Default order YXZ: maps a 1D key onto the Y axis.
	};
	auto Negate = [IMC](bool bX, bool bY, bool bZ)
	{
		UInputModifierNegate* Mod = NewObject<UInputModifierNegate>(IMC);
		Mod->bX = bX;
		Mod->bY = bY;
		Mod->bZ = bZ;
		return Mod;
	};
	auto DeadZone = [IMC]()
	{
		return NewObject<UInputModifierDeadZone>(IMC);
	};

	// Move: X = right, Y = forward.
	IMC->MapKey(MoveAction, EKeys::W).Modifiers.Add(Swizzle());
	{
		FEnhancedActionKeyMapping& S = IMC->MapKey(MoveAction, EKeys::S);
		S.Modifiers.Add(Swizzle());
		S.Modifiers.Add(Negate(true, true, true));
	}
	IMC->MapKey(MoveAction, EKeys::A).Modifiers.Add(Negate(true, true, true));
	IMC->MapKey(MoveAction, EKeys::D);
	IMC->MapKey(MoveAction, EKeys::Gamepad_Left2D).Modifiers.Add(DeadZone());

	// Look: invert Y so moving the mouse up looks up.
	IMC->MapKey(LookAction, EKeys::Mouse2D).Modifiers.Add(Negate(false, true, false));
	{
		FEnhancedActionKeyMapping& Stick = IMC->MapKey(LookAction, EKeys::Gamepad_Right2D);
		Stick.Modifiers.Add(DeadZone());
		Stick.Modifiers.Add(Negate(false, true, false));
	}

	IMC->MapKey(JumpAction, EKeys::SpaceBar);
	IMC->MapKey(JumpAction, EKeys::Gamepad_FaceButton_Bottom);

	IMC->MapKey(SprintAction, EKeys::LeftShift);
	IMC->MapKey(SprintAction, EKeys::Gamepad_LeftThumbstick);

	IMC->MapKey(InteractAction, EKeys::E);
	IMC->MapKey(InteractAction, EKeys::Gamepad_FaceButton_Left);

	IMC->MapKey(ZoomAction, EKeys::MouseWheelAxis);

	DefaultMappingContext = IMC;

	UE_LOG(LogLostIsland, Log, TEXT("%s: no InputMappingContext assigned, using generated default bindings."), *GetName());
}

void ALICharacter::Move(const FInputActionValue& Value)
{
	const FVector2D Input = Value.Get<FVector2D>();
	if (!Controller || Input.IsNearlyZero())
	{
		return;
	}

	const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
	const FRotationMatrix YawMatrix(YawRotation);
	AddMovementInput(YawMatrix.GetUnitAxis(EAxis::X), Input.Y);
	AddMovementInput(YawMatrix.GetUnitAxis(EAxis::Y), Input.X);
}

void ALICharacter::Look(const FInputActionValue& Value)
{
	const FVector2D Input = Value.Get<FVector2D>();
	if (!Controller)
	{
		return;
	}

	AddControllerYawInput(Input.X);
	AddControllerPitchInput(Input.Y);
}

void ALICharacter::Zoom(const FInputActionValue& Value)
{
	const float Axis = Value.Get<float>();
	if (FMath::IsNearlyZero(Axis))
	{
		return;
	}

	// Scroll up (positive) zooms in.
	CameraBoom->TargetArmLength = FMath::Clamp(CameraBoom->TargetArmLength - Axis * ZoomStep, MinArmLength, MaxArmLength);
}

void ALICharacter::StartSprint()
{
	bWantsToSprint = true;
	if (CanSprint())
	{
		SetSprinting(true);
	}
}

void ALICharacter::StopSprint()
{
	bWantsToSprint = false;
	SetSprinting(false);
}

void ALICharacter::ForceStopSprinting()
{
	bWantsToSprint = false;
	SetSprinting(false);
}

void ALICharacter::Interact()
{
	if (IsDead())
	{
		return;
	}

	InteractionComponent->TryInteract();
}

bool ALICharacter::CanSprint_Implementation() const
{
	const UCharacterMovementComponent* Movement = GetCharacterMovement();
	return Movement && !Movement->IsCrouching() && SurvivalComponent && SurvivalComponent->CanSprint();
}

bool ALICharacter::IsDead() const
{
	return SurvivalComponent && SurvivalComponent->IsDead();
}

float ALICharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	const float Incoming = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	return SurvivalComponent ? SurvivalComponent->ApplyDamage(Incoming, EventInstigator) : 0.f;
}

void ALICharacter::HandleExhaustedChanged(bool bIsExhausted)
{
	if (bIsExhausted)
	{
		ForceStopSprinting();
	}
}

void ALICharacter::HandleDeath(AController* Killer)
{
	ForceStopSprinting();
	InteractionComponent->SetScanningEnabled(false);
	GetCharacterMovement()->DisableMovement();

	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		DisableInput(PC);
	}

	BP_OnDied();
}

void ALICharacter::SetSprinting(bool bNewSprinting)
{
	if (bIsSprinting == bNewSprinting)
	{
		return;
	}

	// Apply locally right away for responsiveness, then let the server confirm.
	bIsSprinting = bNewSprinting;
	ApplyMovementSpeed();
	SurvivalComponent->SetSprinting(bIsSprinting);
	OnSprintStateChanged.Broadcast(bIsSprinting);

	if (!HasAuthority())
	{
		ServerSetSprinting(bNewSprinting);
	}
}

void ALICharacter::ServerSetSprinting_Implementation(bool bNewSprinting)
{
	SetSprinting(bNewSprinting && CanSprint());
}

void ALICharacter::OnRep_IsSprinting()
{
	ApplyMovementSpeed();
	SurvivalComponent->SetSprinting(bIsSprinting);
	OnSprintStateChanged.Broadcast(bIsSprinting);
}

void ALICharacter::ApplyMovementSpeed()
{
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->MaxWalkSpeed = bIsSprinting ? SprintSpeed : WalkSpeed;
	}
}
