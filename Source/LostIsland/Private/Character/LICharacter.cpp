// Copyright LOST ISLAND Team. All Rights Reserved.

#include "Character/LICharacter.h"

#include "Interaction/LIInteractionComponent.h"
#include "Survival/LISurvivalComponent.h"
#include "Inventory/LIInventoryComponent.h"
#include "Crafting/LICraftingComponent.h"
#include "Core/LIPlayerController.h"
#include "Core/LIGameMode.h"
#include "World/LIIslandGenerator.h"
#include "World/LIBuildingPiece.h"
#include "World/LIStorageChest.h"
#include "Crafting/LICraftingComponent.h"
#include "Items/LIItemRegistrySubsystem.h"
#include "Items/LIItemDefinition.h"
#include "LostIsland.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/DamageEvents.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
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
	// Ticks only for swimming (water depth check); everything else is event/input driven.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.TickInterval = 0.f;

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
	InventoryComponent = CreateDefaultSubobject<ULIInventoryComponent>(TEXT("Inventory"));
	CraftingComponent = CreateDefaultSubobject<ULICraftingComponent>(TEXT("Crafting"));

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
	if (InventoryAction)
	{
		EIC->BindAction(InventoryAction, ETriggerEvent::Started, this, &ALICharacter::ToggleInventory);
	}
	if (ConsumeAction)
	{
		EIC->BindAction(ConsumeAction, ETriggerEvent::Started, this, &ALICharacter::Consume);
	}
	if (PlaceAction)
	{
		EIC->BindAction(PlaceAction, ETriggerEvent::Started, this, &ALICharacter::Place);
	}
	for (UInputAction* CraftAction : CraftSlotActions)
	{
		if (CraftAction)
		{
			EIC->BindAction(CraftAction, ETriggerEvent::Started, this, &ALICharacter::CraftSlot);
		}
	}
	if (QuickSaveAction)
	{
		EIC->BindAction(QuickSaveAction, ETriggerEvent::Started, this, &ALICharacter::QuickSave);
	}
	if (QuickLoadAction)
	{
		EIC->BindAction(QuickLoadAction, ETriggerEvent::Started, this, &ALICharacter::QuickLoad);
	}
	if (BuildModeAction)
	{
		EIC->BindAction(BuildModeAction, ETriggerEvent::Started, this, &ALICharacter::ToggleBuildMode);
	}
	if (CraftPageAction)
	{
		EIC->BindAction(CraftPageAction, ETriggerEvent::Started, this, &ALICharacter::NextCraftPage);
	}
	if (TakeAllAction)
	{
		EIC->BindAction(TakeAllAction, ETriggerEvent::Started, this, &ALICharacter::TakeAllFromFocused);
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
	if (!InventoryAction) { InventoryAction = MakeAction(TEXT("IA_Inventory_Runtime"), EInputActionValueType::Boolean); }
	if (!ConsumeAction)   { ConsumeAction   = MakeAction(TEXT("IA_Consume_Runtime"),   EInputActionValueType::Boolean); }
	if (!PlaceAction)     { PlaceAction     = MakeAction(TEXT("IA_Place_Runtime"),     EInputActionValueType::Boolean); }

	const bool bGenerateCraftActions = CraftSlotActions.Num() == 0;
	if (bGenerateCraftActions)
	{
		for (int32 Index = 0; Index < 10; ++Index) // keys 1-9, then 0
		{
			const FString Name = FString::Printf(TEXT("IA_Craft%d_Runtime"), Index + 1);
			CraftSlotActions.Add(MakeAction(*Name, EInputActionValueType::Boolean));
		}
	}
	if (!QuickSaveAction) { QuickSaveAction = MakeAction(TEXT("IA_QuickSave_Runtime"), EInputActionValueType::Boolean); }
	if (!QuickLoadAction) { QuickLoadAction = MakeAction(TEXT("IA_QuickLoad_Runtime"), EInputActionValueType::Boolean); }
	if (!BuildModeAction) { BuildModeAction = MakeAction(TEXT("IA_BuildMode_Runtime"), EInputActionValueType::Boolean); }
	if (!CraftPageAction) { CraftPageAction = MakeAction(TEXT("IA_CraftPage_Runtime"), EInputActionValueType::Boolean); }
	if (!TakeAllAction)   { TakeAllAction   = MakeAction(TEXT("IA_TakeAll_Runtime"),   EInputActionValueType::Boolean); }

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

	IMC->MapKey(InventoryAction, EKeys::Tab);
	IMC->MapKey(InventoryAction, EKeys::Gamepad_Special_Right);

	const FKey NumberKeys[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine, EKeys::Zero };
	for (int32 Index = 0; Index < CraftSlotActions.Num() && Index < static_cast<int32>(UE_ARRAY_COUNT(NumberKeys)); ++Index)
	{
		if (CraftSlotActions[Index])
		{
			IMC->MapKey(CraftSlotActions[Index], NumberKeys[Index]);
		}
	}

	IMC->MapKey(PlaceAction, EKeys::G);
	IMC->MapKey(PlaceAction, EKeys::Gamepad_DPad_Down);

	IMC->MapKey(ConsumeAction, EKeys::F);
	IMC->MapKey(ConsumeAction, EKeys::Gamepad_DPad_Up);

	IMC->MapKey(QuickSaveAction, EKeys::F5);
	IMC->MapKey(QuickLoadAction, EKeys::F9);
	IMC->MapKey(BuildModeAction, EKeys::B);
	IMC->MapKey(CraftPageAction, EKeys::Q);
	IMC->MapKey(TakeAllAction, EKeys::R);
	IMC->MapKey(TakeAllAction, EKeys::Gamepad_FaceButton_Top);
	IMC->MapKey(BuildModeAction, EKeys::Gamepad_DPad_Left);

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

void ALICharacter::ToggleInventory()
{
	if (ALIPlayerController* PC = Cast<ALIPlayerController>(Controller))
	{
		PC->ToggleInventoryUI();
	}
}

void ALICharacter::Consume()
{
	if (!IsDead())
	{
		InventoryComponent->RequestConsumeBest();
	}
}

void ALICharacter::Place()
{
	if (!IsDead())
	{
		CraftingComponent->RequestPlace();
	}
}

void ALICharacter::CraftSlot(const FInputActionInstance& Instance)
{
	// Crafting only from the inventory screen, so number keys stay free for a future hotbar.
	const ALIPlayerController* PC = Cast<ALIPlayerController>(Controller);
	if (!PC || !PC->IsInventoryOpen() || IsDead())
	{
		return;
	}
	const UInputAction* Source = Instance.GetSourceAction();
	for (int32 Index = 0; Index < CraftSlotActions.Num(); ++Index)
	{
		if (CraftSlotActions[Index].Get() == Source)
		{
			CraftingComponent->RequestCraftByIndex(CraftPage * CraftKeysPerPage + Index);
			return;
		}
	}
}

void ALICharacter::QuickSave()
{
	if (ALIPlayerController* PC = Cast<ALIPlayerController>(Controller))
	{
		PC->LISave();
	}
}

void ALICharacter::QuickLoad()
{
	if (ALIPlayerController* PC = Cast<ALIPlayerController>(Controller))
	{
		PC->LILoad();
	}
}

bool ALICharacter::CanSprint_Implementation() const
{
	const UCharacterMovementComponent* Movement = GetCharacterMovement();
	return Movement && !Movement->IsCrouching() && !bIsSwimmingLocal && SurvivalComponent && SurvivalComponent->CanSprint();
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
	if (HasAuthority())
	{
		if (ALIGameMode* GameMode = GetWorld()->GetAuthGameMode<ALIGameMode>())
		{
			GameMode->HandlePlayerDeath(this);
		}
	}

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
	const bool bAccepted = bNewSprinting && CanSprint();
	SetSprinting(bAccepted);

	// The client already applied bNewSprinting. If the server ended up with something else, tell it
	// explicitly: bIsSprinting may not have changed on the server, so replication would stay silent.
	if (bAccepted != bNewSprinting)
	{
		ClientCorrectSprinting(bIsSprinting);
	}
}

void ALICharacter::ClientCorrectSprinting_Implementation(bool bServerSprinting)
{
	if (!bServerSprinting)
	{
		bWantsToSprint = false;
	}
	ApplySprintStateLocal(bServerSprinting);
}

void ALICharacter::OnRep_IsSprinting()
{
	ApplyMovementSpeed();
	SurvivalComponent->SetSprinting(bIsSprinting);
	OnSprintStateChanged.Broadcast(bIsSprinting);
}

void ALICharacter::ApplySprintStateLocal(bool bNewSprinting)
{
	if (bIsSprinting == bNewSprinting)
	{
		return;
	}
	bIsSprinting = bNewSprinting;
	OnRep_IsSprinting();
}

void ALICharacter::ApplyMovementSpeed()
{
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->MaxWalkSpeed = bIsSprinting ? SprintSpeed : WalkSpeed;
	}
}

// ---------------------------------------------------------------------------
// Swimming
// ---------------------------------------------------------------------------

void ALICharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Server and owning client run the same depth test; simulated proxies get the movement mode replicated.
	if (HasAuthority() || IsLocallyControlled())
	{
		UpdateSwimming(DeltaSeconds);
	}
	if (IsLocallyControlled())
	{
		UpdateBuildPreview();
	}
}

float ALICharacter::GetSeaLevelZ()
{
	if (!bSearchedIsland || (!CachedIsland.IsValid() && GetWorld()))
	{
		bSearchedIsland = true;
		for (TActorIterator<ALIIslandGenerator> It(GetWorld()); It; ++It)
		{
			CachedIsland = *It;
			break;
		}
	}
	// The island's water plane sits at the generator's origin.
	return CachedIsland.IsValid() ? CachedIsland->GetActorLocation().Z : DefaultSeaLevelZ;
}

void ALICharacter::UpdateSwimming(float DeltaSeconds)
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (!Movement || IsDead())
	{
		return;
	}

	const float SeaLevel = GetSeaLevelZ();
	const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Location = GetActorLocation();
	const float FeetDepth = SeaLevel - (Location.Z - HalfHeight);

	const bool bWasSwimming = bIsSwimmingLocal;
	if (!bIsSwimmingLocal && FeetDepth > SwimStartDepth)
	{
		bIsSwimmingLocal = true;
	}
	else if (bIsSwimmingLocal && FeetDepth < SwimStartDepth - 30.f)
	{
		bIsSwimmingLocal = false;
	}

	if (bIsSwimmingLocal != bWasSwimming)
	{
		UE_LOG(LogLostIsland, Log, TEXT("%s: %s swimming (depth %.0f cm)."), *GetName(), bIsSwimmingLocal ? TEXT("started") : TEXT("stopped"), FeetDepth);
	}
	if (bIsSwimmingLocal != bWasSwimming)
	{
		if (bIsSwimmingLocal)
		{
			if (bIsSprinting)
			{
				ForceStopSprinting();
			}
			// Flying = free 3D movement without gravity; Z is driven below (no water volume exists).
			Movement->MaxFlySpeed = SwimSpeed;
			Movement->BrakingDecelerationFlying = 600.f;
			Movement->SetMovementMode(MOVE_Flying);
		}
		else if (Movement->MovementMode == MOVE_Flying)
		{
			Movement->SetMovementMode(MOVE_Falling); // lands and switches to walking by itself
		}
	}

	const bool bExhausted = SurvivalComponent && SurvivalComponent->IsExhausted();
	if (bIsSwimmingLocal && Movement->MovementMode == MOVE_Flying)
	{
		// Float with the head above water; an exhausted swimmer slowly sinks.
		const float TargetZ = bExhausted ? SeaLevel - HalfHeight * 2.f - 100.f : SeaLevel - FloatDepth;
		const float MaxVertical = bExhausted ? 60.f : 150.f;
		Movement->Velocity.Z = FMath::Clamp((TargetZ - Location.Z) * 2.f, -MaxVertical, MaxVertical);
	}

	// Breath: the head (top of the capsule, minus a little) is under the surface.
	const bool bSubmerged = (Location.Z + HalfHeight * 0.8f) < SeaLevel;
	if (SurvivalComponent && HasAuthority())
	{
		SurvivalComponent->SetWaterState(bIsSwimmingLocal, bSubmerged);
	}
}

// ---------------------------------------------------------------------------
// Build mode (local preview only; placement itself stays server-side via G)
// ---------------------------------------------------------------------------

void ALICharacter::ToggleBuildMode()
{
	bBuildMode = !bBuildMode;
	if (!bBuildMode)
	{
		DestroyBuildPreview();
	}
}

void ALICharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DestroyBuildPreview();
	Super::EndPlay(EndPlayReason);
}

void ALICharacter::DestroyBuildPreview()
{
	if (IsValid(BuildPreview))
	{
		BuildPreview->Destroy();
		BuildPreview = nullptr;
	}
}

void ALICharacter::UpdateBuildPreview()
{
	// The piece G would place right now, if it is a building piece.
	TSubclassOf<ALIBuildingPiece> PieceClass;
	if (bBuildMode && !IsDead() && CraftingComponent)
	{
		const ULIItemRegistrySubsystem* Registry = ULIItemRegistrySubsystem::Get(this);
		const ULIItemDefinition* Def = Registry ? Registry->FindItem(CraftingComponent->FindPlaceableItem()) : nullptr;
		if (Def && Def->PlaceableActorClass && Def->PlaceableActorClass->IsChildOf(ALIBuildingPiece::StaticClass()))
		{
			PieceClass = Def->PlaceableActorClass.Get();
		}
	}
	if (!PieceClass)
	{
		DestroyBuildPreview();
		return;
	}

	if (!IsValid(BuildPreview) || BuildPreview->GetClass() != PieceClass)
	{
		DestroyBuildPreview();
		BuildPreview = ALIBuildingPiece::SpawnPreview(GetWorld(), PieceClass);
		if (!BuildPreview)
		{
			return;
		}
	}

	FTransform Transform;
	const bool bValid = ALIBuildingPiece::ComputePlacement(GetWorld(), PieceClass, GetActorLocation(),
		GetControlRotation().Vector(), this, Transform);
	BuildPreview->SetActorHiddenInGame(!bValid);
	if (bValid)
	{
		BuildPreview->SetActorTransform(Transform);
	}
}

void ALICharacter::NextCraftPage()
{
	const ALIPlayerController* PC = Cast<ALIPlayerController>(Controller);
	const ULIItemRegistrySubsystem* Registry = ULIItemRegistrySubsystem::Get(this);
	if (!PC || !PC->IsInventoryOpen() || !Registry)
	{
		return;
	}
	const int32 NumPages = FMath::Max(1, FMath::DivideAndRoundUp(Registry->GetRecipes().Num(), CraftKeysPerPage));
	CraftPage = (CraftPage + 1) % NumPages;
}

// ---------------------------------------------------------------------------
// Storage chest: R takes everything from the chest in focus
// ---------------------------------------------------------------------------

void ALICharacter::TakeAllFromFocused()
{
	if (IsDead() || !InteractionComponent)
	{
		return;
	}
	if (ALIStorageChest* Chest = Cast<ALIStorageChest>(InteractionComponent->GetFocusedActor()))
	{
		ServerTakeAllFromChest(Chest);
	}
}

void ALICharacter::ServerTakeAllFromChest_Implementation(ALIStorageChest* Chest)
{
	// Same reach rule as interaction, with some slack for latency.
	if (Chest && !IsDead() && FVector::DistSquared(Chest->GetActorLocation(), GetActorLocation()) < FMath::Square(450.f))
	{
		Chest->TakeAllInto(InventoryComponent);
	}
}
