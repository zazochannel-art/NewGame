// Copyright LOST ISLAND Team. All Rights Reserved.

#include "Vehicles/LIVehicle.h"

#include "Character/LICharacter.h"
#include "Core/LIGameMode.h"
#include "Core/LIGameState.h"
#include "LostIsland.h"
#include "Visuals/LILowPolyMesh.h"

#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/CollisionProfile.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Net/UnrealNetwork.h"
#include "ProceduralMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "LIVehicle"

namespace
{
	constexpr float CmPerSecPerKmh = 27.7778f;
}

ALIVehicle::ALIVehicle()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	bReplicates = true;
	SetReplicatingMovement(true);
	AutoPossessAI = EAutoPossessAI::Disabled;
	AutoPossessPlayer = EAutoReceiveInput::Disabled;

	Chassis = CreateDefaultSubobject<UBoxComponent>(TEXT("Chassis"));
	RootComponent = Chassis;
	Chassis->SetBoxExtent(BoxExtent);
	Chassis->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	Chassis->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block); // the interaction trace finds the vehicle
	Chassis->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Chassis->SetLinearDamping(0.05f);
	Chassis->SetAngularDamping(0.8f);
	Chassis->SetHiddenInGame(true);
	Chassis->SetCanEverAffectNavigation(false);

	BodyMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(Chassis);
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyMesh->SetCanEverAffectNavigation(false);

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(Chassis);
	CameraBoom->SetUsingAbsoluteRotation(true); // UpdateCamera steers it so it follows the heading without rolling
	CameraBoom->TargetArmLength = CameraDistance;
	CameraBoom->bUsePawnControlRotation = false;
	CameraBoom->bDoCollisionTest = true;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 10.f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;

	Headlight = CreateDefaultSubobject<USpotLightComponent>(TEXT("Headlight"));
	Headlight->SetupAttachment(Chassis);
	Headlight->SetIntensityUnits(ELightUnits::Lumens);
	Headlight->SetIntensity(9000.f);
	Headlight->SetAttenuationRadius(4500.f);
	Headlight->SetInnerConeAngle(18.f);
	Headlight->SetOuterConeAngle(38.f);
	Headlight->SetLightColor(FLinearColor(1.f, 0.93f, 0.78f));
	Headlight->SetCastShadows(false); // a dozen vehicles: keep them cheap
	Headlight->SetRelativeRotation(FRotator(-6.f, 0.f, 0.f));
	Headlight->SetVisibility(false);

	TailLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("TailLight"));
	TailLight->SetupAttachment(Chassis);
	TailLight->SetIntensityUnits(ELightUnits::Lumens);
	TailLight->SetIntensity(0.f);
	TailLight->SetAttenuationRadius(350.f);
	TailLight->SetLightColor(FLinearColor(1.f, 0.08f, 0.04f));
	TailLight->SetCastShadows(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PuffShape(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	DustPuffs = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("DustPuffs"));
	DustPuffs->SetupAttachment(Chassis);
	DustPuffs->SetUsingAbsoluteLocation(true); // instances are placed in world space
	DustPuffs->SetUsingAbsoluteRotation(true);
	DustPuffs->SetUsingAbsoluteScale(true);
	DustPuffs->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DustPuffs->SetCastShadow(false);
	DustPuffs->SetCanEverAffectNavigation(false);
	if (PuffShape.Succeeded())
	{
		DustPuffs->SetStaticMesh(PuffShape.Object);
	}

	DisplayName = LOCTEXT("Vehicle", "Vehicle");
}

void ALIVehicle::SetChassisExtent(const FVector& InExtent)
{
	BoxExtent = InExtent;
	Chassis->SetBoxExtent(BoxExtent);
}

void ALIVehicle::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ALIVehicle, Driver);
	DOREPLIFETIME(ALIVehicle, bToolActive);
	DOREPLIFETIME_CONDITION(ALIVehicle, ThrottleInput, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(ALIVehicle, SteerInput, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(ALIVehicle, bHandbrake, COND_SkipOwner);
}

void ALIVehicle::BeginPlay()
{
	Super::BeginPlay();

	CameraBoom->SetRelativeLocation(FVector(0.f, 0.f, CameraHeight));
	CameraBoom->TargetArmLength = CameraDistance;
	CameraBoom->SetWorldRotation(FRotator(-12.f, GetActorRotation().Yaw, 0.f));

	// Mass and balance on every machine: clients simulate too (replicated physics corrects them).
	Chassis->SetMassOverrideInKg(NAME_None, Mass, true);
	Chassis->SetCenterOfMass(CenterOfMassOffset);
	if (HasAuthority())
	{
		Chassis->SetSimulatePhysics(true);
	}

	for (FLIVehicleWheel& Wheel : Wheels)
	{
		Wheel.Compression = RestCompression;
	}
	BuildVisuals();

	const FVector Front = HeadlightOffset.IsNearlyZero() ? FVector(BoxExtent.X + 5.f, 0.f, 0.f) : HeadlightOffset;
	Headlight->SetRelativeLocation(Front);
	TailLight->SetRelativeLocation(FVector(-BoxExtent.X - 25.f, 0.f, 0.f));
	DustPuffs->SetWorldLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
	DustPuffs->SetMaterial(0, LIGetTintedMaterial(DustColor));
	LightCheckTimer = FMath::FRand(); // spread the checks over frames
}

void ALIVehicle::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority() && Driver)
	{
		// Destroyed with someone inside (player left, world reset): leave the character on foot, not hidden.
		Driver->SetSeatedVehicle(nullptr);
		Driver = nullptr;
	}
	if (APlayerController* PC = InputOwner.Get())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Subsystem->RemoveMappingContext(MappingContext);
		}
	}
	Super::EndPlay(EndPlayReason);
}

// ---------------------------------------------------------------------------
// Getting in and out
// ---------------------------------------------------------------------------

bool ALIVehicle::CanInteract_Implementation(APawn* InstigatorPawn) const
{
	const ALICharacter* Character = Cast<ALICharacter>(InstigatorPawn);
	return Character && !Character->IsDead() && !Driver;
}

FText ALIVehicle::GetInteractionPrompt_Implementation(APawn* InstigatorPawn) const
{
	return FText::Format(LOCTEXT("Drive", "Drive {0}"), DisplayName);
}

void ALIVehicle::Interact_Implementation(APawn* InstigatorPawn)
{
	EnterVehicle(Cast<ALICharacter>(InstigatorPawn));
}

bool ALIVehicle::EnterVehicle(ALICharacter* Character)
{
	if (!HasAuthority() || Driver || !Character || Character->IsDead() || Character->GetSeatedVehicle())
	{
		return false;
	}
	AController* CharacterController = Character->GetController();
	if (!CharacterController)
	{
		return false;
	}

	Driver = Character;
	Character->SetSeatedVehicle(this);
	Character->SetActorRelativeLocation(SeatOffset);
	Character->SetActorRelativeRotation(FRotator::ZeroRotator);

	CharacterController->Possess(this);
	CharacterController->SetControlRotation(FRotator(0.f, GetActorRotation().Yaw, 0.f));
	ForceNetUpdate();
	UE_LOG(LogLostIsland, Log, TEXT("%s gets into %s."), *Character->GetName(), *GetName());
	return true;
}

bool ALIVehicle::ExitVehicle()
{
	if (!HasAuthority() || !Driver)
	{
		return false;
	}

	ALICharacter* Character = Driver;
	AController* DriverController = Controller;
	UWorld* World = GetWorld();

	// Driver's side, the other side, behind, then on the roof as a last resort.
	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const FTransform Transform = GetActorTransform();
	const FVector Candidates[] =
	{
		ExitOffset,
		FVector(ExitOffset.X, -ExitOffset.Y, ExitOffset.Z),
		FVector(-(BoxExtent.X + Radius + 60.f), 0.f, 0.f),
		FVector(BoxExtent.X + Radius + 60.f, 0.f, 0.f),
	};

	FCollisionQueryParams Params(SCENE_QUERY_STAT(LIVehicleExit), false, this);
	Params.AddIgnoredActor(Character);
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(Radius, HalfHeight);

	FVector ExitLocation = Transform.GetLocation() + FVector(0.f, 0.f, BoxExtent.Z + HalfHeight + 120.f);
	for (const FVector& Local : Candidates)
	{
		FVector Location = Transform.TransformPosition(Local);
		FHitResult Ground;
		if (World->LineTraceSingleByChannel(Ground, Location + FVector(0.f, 0.f, 250.f), Location - FVector(0.f, 0.f, 600.f), ECC_Visibility, Params))
		{
			Location.Z = Ground.ImpactPoint.Z + HalfHeight + 5.f;
		}
		if (!World->OverlapBlockingTestByChannel(Location, FQuat::Identity, ECC_Pawn, Shape, Params))
		{
			ExitLocation = Location;
			break;
		}
	}

	Driver = nullptr;
	ThrottleInput = 0.f;
	SteerInput = 0.f;
	bHandbrake = false;

	Character->SetSeatedVehicle(nullptr);
	Character->TeleportTo(ExitLocation, FRotator(0.f, GetActorRotation().Yaw, 0.f), false, true);
	if (DriverController)
	{
		DriverController->Possess(Character);
		DriverController->SetControlRotation(FRotator(-10.f, GetActorRotation().Yaw, 0.f));
	}
	ForceNetUpdate();
	UE_LOG(LogLostIsland, Log, TEXT("%s gets out of %s."), *Character->GetName(), *GetName());
	return true;
}

ALICharacter* ALIVehicle::GetCharacterFromPawn(APawn* Pawn)
{
	if (ALICharacter* Character = Cast<ALICharacter>(Pawn))
	{
		return Character;
	}
	const ALIVehicle* Vehicle = Cast<ALIVehicle>(Pawn);
	return Vehicle ? Vehicle->GetDriver() : nullptr;
}

bool ALIVehicle::IsTippedOver() const
{
	return GetActorUpVector().Z < 0.35f;
}

void ALIVehicle::ResetUpright()
{
	if (!HasAuthority())
	{
		return;
	}
	const FVector Location = GetActorLocation() + FVector(0.f, 0.f, 150.f);
	SetActorLocationAndRotation(Location, FRotator(0.f, GetActorRotation().Yaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
	Chassis->SetPhysicsLinearVelocity(FVector::ZeroVector);
	Chassis->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
}

float ALIVehicle::GetSpeedKmh() const
{
	return FVector::DotProduct(GetVelocity(), GetActorForwardVector()) / CmPerSecPerKmh;
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

void ALIVehicle::EnsureDefaultInput()
{
	if (MappingContext)
	{
		return;
	}

	auto MakeAction = [this](const TCHAR* Name, EInputActionValueType ValueType) -> UInputAction*
	{
		UInputAction* Action = NewObject<UInputAction>(this, FName(Name), RF_Transient);
		Action->ValueType = ValueType;
		return Action;
	};
	ThrottleAction  = MakeAction(TEXT("IA_VehicleThrottle"),  EInputActionValueType::Axis1D);
	BrakeAction     = MakeAction(TEXT("IA_VehicleBrake"),     EInputActionValueType::Axis1D);
	SteerAction     = MakeAction(TEXT("IA_VehicleSteer"),     EInputActionValueType::Axis1D);
	HandbrakeAction = MakeAction(TEXT("IA_VehicleHandbrake"), EInputActionValueType::Boolean);
	LookAction      = MakeAction(TEXT("IA_VehicleLook"),      EInputActionValueType::Axis2D);
	ZoomAction      = MakeAction(TEXT("IA_VehicleZoom"),      EInputActionValueType::Axis1D);
	ExitAction      = MakeAction(TEXT("IA_VehicleExit"),      EInputActionValueType::Boolean);
	ResetAction     = MakeAction(TEXT("IA_VehicleReset"),     EInputActionValueType::Boolean);
	ToolAction      = MakeAction(TEXT("IA_VehicleTool"),      EInputActionValueType::Boolean);
	CycleToolAction = MakeAction(TEXT("IA_VehicleCycleTool"), EInputActionValueType::Boolean);

	MappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Vehicle_Runtime"), RF_Transient);
	MappingContext->MapKey(ThrottleAction, EKeys::W);
	MappingContext->MapKey(ThrottleAction, EKeys::Up);
	MappingContext->MapKey(ThrottleAction, EKeys::Gamepad_RightTriggerAxis);
	MappingContext->MapKey(BrakeAction, EKeys::S);
	MappingContext->MapKey(BrakeAction, EKeys::Down);
	MappingContext->MapKey(BrakeAction, EKeys::Gamepad_LeftTriggerAxis);

	MappingContext->MapKey(SteerAction, EKeys::D);
	MappingContext->MapKey(SteerAction, EKeys::Right);
	MappingContext->MapKey(SteerAction, EKeys::A).Modifiers.Add(NewObject<UInputModifierNegate>(MappingContext));
	MappingContext->MapKey(SteerAction, EKeys::Left).Modifiers.Add(NewObject<UInputModifierNegate>(MappingContext));
	MappingContext->MapKey(SteerAction, EKeys::Gamepad_LeftX).Modifiers.Add(NewObject<UInputModifierDeadZone>(MappingContext));

	MappingContext->MapKey(HandbrakeAction, EKeys::SpaceBar);
	MappingContext->MapKey(HandbrakeAction, EKeys::Gamepad_FaceButton_Right);
	MappingContext->MapKey(LookAction, EKeys::Mouse2D);
	{
		FEnhancedActionKeyMapping& Stick = MappingContext->MapKey(LookAction, EKeys::Gamepad_Right2D);
		Stick.Modifiers.Add(NewObject<UInputModifierDeadZone>(MappingContext));
		UInputModifierScalar* Scale = NewObject<UInputModifierScalar>(MappingContext);
		Scale->Scalar = FVector(4.f, 4.f, 1.f);
		Stick.Modifiers.Add(Scale);
	}
	MappingContext->MapKey(ZoomAction, EKeys::MouseWheelAxis);
	MappingContext->MapKey(ExitAction, EKeys::E);
	MappingContext->MapKey(ExitAction, EKeys::Gamepad_FaceButton_Left);
	MappingContext->MapKey(ResetAction, EKeys::R);
	MappingContext->MapKey(ResetAction, EKeys::Gamepad_FaceButton_Top);
	MappingContext->MapKey(ToolAction, EKeys::Q);
	MappingContext->MapKey(ToolAction, EKeys::Gamepad_RightShoulder);
	MappingContext->MapKey(CycleToolAction, EKeys::T);
	MappingContext->MapKey(CycleToolAction, EKeys::Gamepad_LeftShoulder);
}

void ALIVehicle::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	APlayerController* PC = Cast<APlayerController>(Controller);
	if (APlayerController* OldPC = InputOwner.Get(); OldPC && OldPC != PC)
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(OldPC->GetLocalPlayer()))
		{
			Subsystem->RemoveMappingContext(MappingContext);
		}
		InputOwner.Reset();
	}

	if (PC && PC->IsLocalController())
	{
		EnsureDefaultInput();
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			// Above the character's context, so W/A/S/D/E/Space drive the vehicle instead.
			Subsystem->AddMappingContext(MappingContext, 1);
			InputOwner = PC;
		}
		LookYawOffset = 0.f;
		LookPitchOffset = 0.f;
		CameraBoom->SetWorldRotation(FRotator(-12.f, GetActorRotation().Yaw, 0.f));
	}

	LocalThrottle = LocalBrake = LocalSteer = 0.f;
	bLocalHandbrake = false;
	if (!Controller && HasAuthority())
	{
		ThrottleInput = SteerInput = 0.f;
		bHandbrake = false;
	}
}

void ALIVehicle::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	EnsureDefaultInput();

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EIC)
	{
		return;
	}
	for (ETriggerEvent Event : { ETriggerEvent::Triggered, ETriggerEvent::Completed })
	{
		EIC->BindAction(ThrottleAction, Event, this, &ALIVehicle::OnThrottle);
		EIC->BindAction(BrakeAction, Event, this, &ALIVehicle::OnBrake);
		EIC->BindAction(SteerAction, Event, this, &ALIVehicle::OnSteer);
	}
	EIC->BindAction(HandbrakeAction, ETriggerEvent::Started, this, &ALIVehicle::OnHandbrake);
	EIC->BindAction(HandbrakeAction, ETriggerEvent::Completed, this, &ALIVehicle::OnHandbrake);
	EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &ALIVehicle::OnLook);
	EIC->BindAction(ZoomAction, ETriggerEvent::Triggered, this, &ALIVehicle::OnZoom);
	EIC->BindAction(ExitAction, ETriggerEvent::Started, this, &ALIVehicle::OnExitPressed);
	EIC->BindAction(ResetAction, ETriggerEvent::Started, this, &ALIVehicle::OnResetPressed);
	EIC->BindAction(ToolAction, ETriggerEvent::Started, this, &ALIVehicle::OnToolPressed);
	EIC->BindAction(CycleToolAction, ETriggerEvent::Started, this, &ALIVehicle::OnCycleToolPressed);
}

void ALIVehicle::OnThrottle(const FInputActionValue& Value)
{
	LocalThrottle = FMath::Clamp(Value.Get<float>(), 0.f, 1.f);
	SendInputIfChanged();
}

void ALIVehicle::OnBrake(const FInputActionValue& Value)
{
	LocalBrake = FMath::Clamp(Value.Get<float>(), 0.f, 1.f);
	SendInputIfChanged();
}

void ALIVehicle::OnSteer(const FInputActionValue& Value)
{
	LocalSteer = FMath::Clamp(Value.Get<float>(), -1.f, 1.f);
	SendInputIfChanged();
}

void ALIVehicle::OnHandbrake(const FInputActionValue& Value)
{
	bLocalHandbrake = Value.Get<bool>();
	SendInputIfChanged();
}

void ALIVehicle::OnLook(const FInputActionValue& Value)
{
	const FVector2D Look = Value.Get<FVector2D>();
	LookYawOffset = FMath::Clamp(LookYawOffset + static_cast<float>(Look.X) * 1.5f, -170.f, 170.f);
	LookPitchOffset = FMath::Clamp(LookPitchOffset + static_cast<float>(Look.Y) * 1.5f, -35.f, 30.f);
	LastLookTime = GetWorld()->GetTimeSeconds();
}

void ALIVehicle::OnZoom(const FInputActionValue& Value)
{
	CameraZoom = FMath::Clamp(CameraZoom - Value.Get<float>() * 0.1f, 0.5f, 2.f);
}

void ALIVehicle::OnExitPressed()
{
	if (HasAuthority())
	{
		ExitVehicle();
	}
	else
	{
		ServerExit();
	}
}

void ALIVehicle::OnResetPressed()
{
	if (HasAuthority())
	{
		ResetUpright();
	}
	else
	{
		ServerReset();
	}
}

void ALIVehicle::OnToolPressed()
{
	if (!HasTool())
	{
		return;
	}
	if (HasAuthority())
	{
		ToggleTool();
	}
	else
	{
		ServerToggleTool();
	}
}

void ALIVehicle::OnCycleToolPressed()
{
	if (!HasToolModes())
	{
		return;
	}
	if (HasAuthority())
	{
		CycleTool();
	}
	else
	{
		ServerCycleTool();
	}
}

void ALIVehicle::ServerCycleTool_Implementation()
{
	if (HasToolModes())
	{
		CycleTool();
	}
}

void ALIVehicle::ServerToggleTool_Implementation()
{
	if (HasTool())
	{
		ToggleTool();
	}
}

void ALIVehicle::ToggleTool()
{
	bToolActive = !bToolActive;
	ForceNetUpdate();
}

void ALIVehicle::SendInputIfChanged()
{
	// Combined so a key and a trigger can be used together; reverse is "brake" while stopped (see SimulatePhysics).
	ThrottleInput = FMath::Clamp(LocalThrottle - LocalBrake, -1.f, 1.f);
	SteerInput = LocalSteer;
	bHandbrake = bLocalHandbrake;

	if (HasAuthority())
	{
		return;
	}
	if (!FMath::IsNearlyEqual(ThrottleInput, SentThrottle, 0.02f) || !FMath::IsNearlyEqual(SteerInput, SentSteer, 0.02f) || bHandbrake != bSentHandbrake)
	{
		SentThrottle = ThrottleInput;
		SentSteer = SteerInput;
		bSentHandbrake = bHandbrake;
		LastInputSendTime = GetWorld()->GetTimeSeconds();
		ServerSetInput(ThrottleInput, SteerInput, bHandbrake);
	}
}

void ALIVehicle::ServerSetInput_Implementation(float InThrottle, float InSteer, bool bInHandbrake)
{
	ThrottleInput = FMath::Clamp(InThrottle, -1.f, 1.f);
	SteerInput = FMath::Clamp(InSteer, -1.f, 1.f);
	bHandbrake = bInHandbrake;
}

void ALIVehicle::ServerExit_Implementation()
{
	ExitVehicle();
}

void ALIVehicle::ServerReset_Implementation()
{
	ResetUpright();
}

// ---------------------------------------------------------------------------
// Simulation
// ---------------------------------------------------------------------------

void ALIVehicle::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (HasAuthority() && Driver && Driver->IsDead())
	{
		// The driver starved or froze at the wheel: put the body outside and let the normal death flow respawn them.
		ALICharacter* DeadDriver = Driver;
		ExitVehicle();
		if (ALIGameMode* GameMode = GetWorld()->GetAuthGameMode<ALIGameMode>())
		{
			GameMode->HandlePlayerDeath(DeadDriver);
		}
	}

	// Unreliable input: repeat it now and then, so a lost "key released" never leaves the throttle stuck.
	if (!HasAuthority() && IsLocallyControlled() && GetWorld()->GetTimeSeconds() - LastInputSendTime > 0.25)
	{
		LastInputSendTime = GetWorld()->GetTimeSeconds();
		ServerSetInput(ThrottleInput, SteerInput, bHandbrake);
	}

	SimulatePhysics(DeltaSeconds);
	UpdateWheelVisuals(DeltaSeconds);
	if (GetNetMode() != NM_DedicatedServer)
	{
		UpdateEffects(DeltaSeconds);
		UpdateLights(DeltaSeconds);
	}
	if (IsLocallyControlled())
	{
		UpdateCamera(DeltaSeconds);
	}
}

void ALIVehicle::SimulatePhysics(float DeltaSeconds)
{
	if (!Chassis->IsSimulatingPhysics() || Wheels.Num() == 0 || DeltaSeconds <= 0.f)
	{
		return;
	}
	UWorld* World = GetWorld();
	const float Dt = FMath::Clamp(DeltaSeconds, 1.f / 240.f, 1.f / 20.f);

	const FTransform Transform = Chassis->GetComponentTransform();
	const FVector Up = Transform.GetUnitAxis(EAxis::Z);
	const FVector Forward = Transform.GetUnitAxis(EAxis::X);
	const FVector Right = Transform.GetUnitAxis(EAxis::Y);

	const int32 NumWheels = Wheels.Num();
	const float Gravity = FMath::Abs(World->GetGravityZ());
	const float WheelMass = Mass / NumWheels;
	const float Stiffness = WheelMass * Gravity / FMath::Max(1.f, RestCompression * SuspensionLength); // per cm
	const float Damping = DampingRatio * 2.f * FMath::Sqrt(Stiffness * WheelMass);

	const FVector Velocity = Chassis->GetPhysicsLinearVelocity();
	const float ForwardSpeed = FVector::DotProduct(Velocity, Forward);
	const float MaxSpeed = MaxSpeedKmh * CmPerSecPerKmh;
	const float MaxReverse = MaxReverseKmh * CmPerSecPerKmh;
	const float SpeedAlpha = FMath::Clamp(FMath::Abs(ForwardSpeed) / FMath::Max(1.f, MaxSpeed), 0.f, 1.f);

	const float SteerTarget = SteerInput * MaxSteerAngle * FMath::Lerp(1.f, HighSpeedSteerFactor, SpeedAlpha);
	SmoothedSteer = FMath::FInterpTo(SmoothedSteer, SteerTarget, Dt, 7.f);

	// W while rolling backwards brakes first, S while rolling forwards brakes first; otherwise they drive.
	float Drive = 0.f;
	float Brake = 0.f;
	if (IsParked())
	{
		Brake = 1.f; // parked
	}
	else if (ThrottleInput > 0.05f)
	{
		(ForwardSpeed < -60.f ? Brake : Drive) = ThrottleInput;
	}
	else if (ThrottleInput < -0.05f)
	{
		if (ForwardSpeed > 60.f)
		{
			Brake = -ThrottleInput;
		}
		else
		{
			Drive = ThrottleInput;
		}
	}

	int32 DrivenWheels = 0;
	for (const FLIVehicleWheel& Wheel : Wheels)
	{
		DrivenWheels += Wheel.bDriven ? 1 : 0;
	}
	DrivenWheels = FMath::Max(DrivenWheels, 1);

	// Engine force falls off toward the top speed (forward and reverse have their own limit).
	float EngineForce = 0.f;
	if (Drive > 0.f && ForwardSpeed < MaxSpeed)
	{
		EngineForce = Drive * Mass * Acceleration * 100.f * (1.f - FMath::Square(FMath::Max(ForwardSpeed, 0.f) / MaxSpeed));
	}
	else if (Drive < 0.f && ForwardSpeed > -MaxReverse)
	{
		EngineForce = Drive * Mass * Acceleration * 100.f * 0.6f * (1.f - FMath::Square(FMath::Max(-ForwardSpeed, 0.f) / MaxReverse));
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(LIVehicleWheel), false, this);
	if (Driver)
	{
		Params.AddIgnoredActor(Driver);
	}
	FCollisionObjectQueryParams GroundObjects;
	GroundObjects.AddObjectTypesToQuery(ECC_WorldStatic);
	GroundObjects.AddObjectTypesToQuery(ECC_WorldDynamic);
	GroundObjects.AddObjectTypesToQuery(ECC_PhysicsBody);

	for (FLIVehicleWheel& Wheel : Wheels)
	{
		const FVector Mount = Transform.TransformPosition(Wheel.MountPoint);
		const float RayLength = SuspensionLength + Wheel.Radius;
		FHitResult Hit;
		Wheel.bGrounded = World->LineTraceSingleByObjectType(Hit, Mount, Mount - Up * RayLength, GroundObjects, Params);
		if (!Wheel.bGrounded)
		{
			Wheel.Compression = FMath::FInterpTo(Wheel.Compression, 0.f, Dt, 10.f);
			Wheel.NormalForce = 0.f;
			continue;
		}

		Wheel.Compression = FMath::Clamp(1.f - (Hit.Distance - Wheel.Radius) / SuspensionLength, 0.f, 1.f);
		Wheel.ContactPoint = Hit.ImpactPoint;
		Wheel.ContactNormal = Hit.ImpactNormal;

		const FVector PointVelocity = Chassis->GetPhysicsLinearVelocityAtPoint(Hit.ImpactPoint);
		const float SpringSpeed = FVector::DotProduct(PointVelocity, Up);
		float SpringForce = Stiffness * Wheel.Compression * SuspensionLength - Damping * SpringSpeed;
		if (Wheel.Compression >= 0.98f)
		{
			SpringForce += Stiffness * SuspensionLength * 2.f; // bump stop
		}
		SpringForce = FMath::Max(SpringForce, 0.f);
		Wheel.NormalForce = SpringForce;
		Chassis->AddForceAtLocation(Up * SpringForce, Mount);

		// Tyre frame on the ground, turned by the steering.
		const FQuat SteerRotation(Up, FMath::DegreesToRadians(Wheel.bSteers ? SmoothedSteer : 0.f));
		const FVector WheelForward = FVector::VectorPlaneProject(SteerRotation.RotateVector(Forward), Hit.ImpactNormal).GetSafeNormal();
		const FVector WheelRight = FVector::VectorPlaneProject(SteerRotation.RotateVector(Right), Hit.ImpactNormal).GetSafeNormal();
		const float LateralSpeed = FVector::DotProduct(PointVelocity, WheelRight);
		const float LongSpeed = FVector::DotProduct(PointVelocity, WheelForward);

		const bool bRear = !Wheel.bSteers;
		const float Grip = TyreGrip * ((bRear && bHandbrake) ? HandbrakeRearGrip : 1.f);
		const float MaxFriction = Grip * Wheel.GripScale * SpringForce;

		// Sideways: cancel most of the slide this step, as far as the grip allows.
		const float LateralForce = -LateralSpeed * WheelMass / Dt * 0.6f;

		// Along the wheel: engine, brakes and rolling resistance. Braking never pushes the wheel backwards.
		float LongForce = Wheel.bDriven ? EngineForce / DrivenWheels : 0.f;
		const float StopForce = FMath::Abs(LongSpeed) * WheelMass / Dt;
		float Resist = 0.f;
		if (Brake > 0.f)
		{
			Resist = Brake * Mass * BrakeDeceleration * 100.f / NumWheels;
		}
		if (bRear && bHandbrake)
		{
			Resist = FMath::Max(Resist, Mass * BrakeDeceleration * 100.f / NumWheels);
		}
		if (FMath::IsNearlyZero(Drive) && Resist <= 0.f)
		{
			Resist = Mass * CoastDeceleration * 100.f / NumWheels;
		}
		LongForce -= FMath::Sign(LongSpeed) * FMath::Min(Resist, StopForce);

		// Friction circle: the tyre cannot give more than its grip in any direction.
		FVector2D TyreForce(LongForce, LateralForce);
		if (TyreForce.SizeSquared() > FMath::Square(MaxFriction))
		{
			TyreForce = TyreForce.GetSafeNormal() * MaxFriction;
		}
		Chassis->AddForceAtLocation(WheelForward * TyreForce.X + WheelRight * TyreForce.Y, Hit.ImpactPoint);

		Wheel.SpinAngle = FMath::Fmod(Wheel.SpinAngle + FMath::RadiansToDegrees(LongSpeed / Wheel.Radius) * Dt, 360.f);
	}

	// Anti-roll bars: wheels facing each other across an axle share their compression difference, so the body
	// leans less in corners and on side slopes (and is much harder to roll over).
	const float AntiRollRate = AntiRoll * Stiffness * SuspensionLength; // force per unit of compression difference
	int32 GroundedWheels = 0;
	for (int32 A = 0; A < NumWheels; ++A)
	{
		const FLIVehicleWheel& Left = Wheels[A];
		GroundedWheels += Left.bGrounded ? 1 : 0;
		if (Left.MountPoint.Y >= 0.f)
		{
			continue;
		}
		for (int32 B = 0; B < NumWheels; ++B)
		{
			const FLIVehicleWheel& Right = Wheels[B];
			if (Right.MountPoint.Y <= 0.f || !FMath::IsNearlyEqual(static_cast<float>(Right.MountPoint.X), static_cast<float>(Left.MountPoint.X), 5.f))
			{
				continue;
			}
			const float Force = (Left.Compression - Right.Compression) * AntiRollRate;
			if (Left.bGrounded)
			{
				Chassis->AddForceAtLocation(Up * Force, Transform.TransformPosition(Left.MountPoint));
			}
			if (Right.bGrounded)
			{
				Chassis->AddForceAtLocation(Up * -Force, Transform.TransformPosition(Right.MountPoint));
			}
			break;
		}
	}

	// Air resistance (F = 1/2 rho CdA v^2, in Unreal units: kg cm / s^2).
	const float SpeedMs = static_cast<float>(Velocity.Size()) / 100.f;
	if (SpeedMs > 0.5f && DragArea > 0.f)
	{
		const float DragNewtons = 0.5f * 1.2f * DragArea * SpeedMs * SpeedMs;
		Chassis->AddForce(-Velocity.GetSafeNormal() * DragNewtons * 100.f);
	}

	// Airborne: gently turn the wheels back toward the ground so jumps end on the tyres, not the roof.
	if (GroundedWheels == 0 && AirControl > 0.f)
	{
		const FVector Correction = FVector::CrossProduct(Up, FVector::UpVector); // axis * sin(angle)
		const FVector AngularVelocity = Chassis->GetPhysicsAngularVelocityInRadians();
		Chassis->AddTorqueInRadians((Correction * 4.f - AngularVelocity * 0.8f) * AirControl, NAME_None, /*bAccelChange*/ true);
	}
}

void ALIVehicle::UpdateLights(float DeltaSeconds)
{
	LightCheckTimer -= DeltaSeconds;
	if (LightCheckTimer <= 0.f)
	{
		LightCheckTimer = 1.f;
		const ALIGameState* GameState = GetWorld()->GetGameState<ALIGameState>();
		const float Hours = GameState ? GameState->GetTimeOfDayHours() : 12.f;
		// On from dusk to a little after dawn, only while someone drives or a trailer is towed.
		bLightsOn = (Hours > 19.5f || Hours < 6.5f) && !IsParked();
		Headlight->SetVisibility(bLightsOn);
	}

	const bool bBraking = !IsParked() && ((ThrottleInput < -0.05f && GetSpeedKmh() > 2.f) || bHandbrake);
	const float TailTarget = bBraking ? 1600.f : (bLightsOn ? 350.f : 0.f);
	const float Tail = FMath::FInterpTo(TailLight->Intensity, TailTarget, DeltaSeconds, 12.f);
	if (!FMath::IsNearlyEqual(Tail, TailLight->Intensity, 1.f))
	{
		TailLight->SetIntensity(Tail);
	}
	TailLight->SetVisibility(Tail > 5.f);
}

void ALIVehicle::UpdateEffects(float DeltaSeconds)
{
	// Spawn: dust from every grounded wheel (not the small jockey wheels), more with speed.
	const float Speed = FMath::Abs(GetSpeedKmh());
	if (Speed > 6.f && DustPuffs->GetStaticMesh())
	{
		const float Amount = FMath::Clamp((Speed - 6.f) / 40.f, 0.f, 1.f);
		DustAccumulator += DustRate * Amount * DeltaSeconds;
		const FVector Velocity = GetVelocity();
		while (DustAccumulator >= 1.f && Puffs.Num() < 64)
		{
			DustAccumulator -= 1.f;
			for (const FLIVehicleWheel& Wheel : Wheels)
			{
				if (!Wheel.bGrounded || Wheel.Radius < 25.f || Puffs.Num() >= 64)
				{
					continue;
				}
				FDustPuff& Puff = Puffs.AddDefaulted_GetRef();
				Puff.Location = Wheel.ContactPoint + FVector(FMath::FRandRange(-20.f, 20.f), FMath::FRandRange(-20.f, 20.f), 15.f);
				Puff.Velocity = -Velocity * 0.15f + FVector(FMath::FRandRange(-40.f, 40.f), FMath::FRandRange(-40.f, 40.f), FMath::FRandRange(30.f, 90.f));
				Puff.Life = FMath::FRandRange(0.9f, 1.6f);
				Puff.Size = FMath::FRandRange(0.35f, 0.6f) * (Wheel.Radius / 40.f) * (0.6f + 0.4f * Amount);
			}
		}
	}
	else
	{
		DustAccumulator = 0.f;
	}

	if (Puffs.Num() == 0)
	{
		if (DustPuffs->GetInstanceCount() > 0)
		{
			DustPuffs->ClearInstances();
		}
		return;
	}

	// Age, drift (slowing, rising) and swell then shrink away.
	TArray<FTransform> Transforms;
	Transforms.Reserve(Puffs.Num());
	for (int32 Index = Puffs.Num() - 1; Index >= 0; --Index)
	{
		FDustPuff& Puff = Puffs[Index];
		Puff.Age += DeltaSeconds;
		if (Puff.Age >= Puff.Life)
		{
			Puffs.RemoveAtSwap(Index);
			continue;
		}
		Puff.Velocity *= FMath::Max(0.f, 1.f - 1.8f * DeltaSeconds);
		Puff.Location += Puff.Velocity * DeltaSeconds;
		const float T = Puff.Age / Puff.Life;
		const float Scale = Puff.Size * (0.5f + 1.8f * T) * (1.f - T * T); // swells, then shrinks away
		Transforms.Add(FTransform(FQuat::Identity, Puff.Location, FVector(FMath::Max(Scale, 0.01f))));
	}
	DustPuffs->ClearInstances();
	DustPuffs->AddInstances(Transforms, /*bShouldReturnIndices*/ false, /*bWorldSpace*/ true);
}

void ALIVehicle::UpdateWheelVisuals(float DeltaSeconds)
{
	for (FLIVehicleWheel& Wheel : Wheels)
	{
		if (!Wheel.Visual)
		{
			continue;
		}
		const float Drop = (1.f - Wheel.Compression) * SuspensionLength;
		Wheel.Visual->SetRelativeLocationAndRotation(
			Wheel.MountPoint - FVector(0.f, 0.f, Drop),
			FRotator(-Wheel.SpinAngle, Wheel.bSteers ? SmoothedSteer : 0.f, 0.f));
	}
}

void ALIVehicle::UpdateCamera(float DeltaSeconds)
{
	UWorld* World = GetWorld();
	if (World->GetTimeSeconds() - LastLookTime > 1.5)
	{
		LookYawOffset = FMath::FInterpTo(LookYawOffset, 0.f, DeltaSeconds, 1.5f);
		LookPitchOffset = FMath::FInterpTo(LookPitchOffset, 0.f, DeltaSeconds, 1.5f);
	}

	// Look back over the shoulder when reversing.
	const float Heading = GetActorRotation().Yaw + (GetSpeedKmh() < -8.f && ThrottleInput < 0.f ? 180.f : 0.f);
	const FRotator Target(-12.f + LookPitchOffset, Heading + LookYawOffset, 0.f);
	CameraBoom->SetWorldRotation(FMath::RInterpTo(CameraBoom->GetComponentRotation(), Target, DeltaSeconds, 4.f));
	CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength, CameraDistance * CameraZoom, DeltaSeconds, 6.f);

	// A wider view at speed sells the motion.
	const float SpeedAlpha = FMath::Clamp(FMath::Abs(GetSpeedKmh()) / 90.f, 0.f, 1.f);
	Camera->SetFieldOfView(FMath::FInterpTo(Camera->FieldOfView, FMath::Lerp(88.f, 102.f, SpeedAlpha), DeltaSeconds, 3.f));
}

// ---------------------------------------------------------------------------
// Model
// ---------------------------------------------------------------------------

void ALIVehicle::BuildVisuals()
{
	FLILowPolyMesh Body;
	BuildBody(Body);
	Body.ApplyTo(BodyMesh);

	for (FLIVehicleWheel& Wheel : Wheels)
	{
		if (!Wheel.Visual)
		{
			Wheel.Visual = LICreateVisualComponent(this, Chassis);
		}
		FLILowPolyMesh WheelMesh;
		BuildWheel(WheelMesh, Wheel);
		WheelMesh.ApplyTo(Wheel.Visual);
	}
	UpdateWheelVisuals(0.f);
}

void ALIVehicle::BuildBody(FLILowPolyMesh& Mesh) const
{
	Mesh.AddBox(FTransform::Identity, BoxExtent, FLinearColor(0.5f, 0.5f, 0.5f));
}

void ALIVehicle::BuildWheel(FLILowPolyMesh& Mesh, const FLIVehicleWheel& Wheel) const
{
	// Frustums stand on +Z: turn them so the axle runs along Y, centred on the hub.
	const FQuat AxleAlongY = FQuat::FindBetweenNormals(FVector::UpVector, FVector::RightVector);
	const FLinearColor Tyre(0.06f, 0.06f, 0.07f);
	const FLinearColor Rim(0.78f, 0.76f, 0.70f);
	const FLinearColor Hub(0.25f, 0.25f, 0.27f);

	Mesh.AddFrustum(FTransform(AxleAlongY, FVector(0.f, -Wheel.Width * 0.5f, 0.f)), 16, Wheel.Radius, Wheel.Radius, Wheel.Width, Tyre);
	const float RimWidth = Wheel.Width + 2.f;
	Mesh.AddFrustum(FTransform(AxleAlongY, FVector(0.f, -RimWidth * 0.5f, 0.f)), 10, Wheel.Radius * 0.55f, Wheel.Radius * 0.55f, RimWidth, Rim);
	const float HubWidth = Wheel.Width + 5.f;
	Mesh.AddFrustum(FTransform(AxleAlongY, FVector(0.f, -HubWidth * 0.5f, 0.f)), 6, Wheel.Radius * 0.18f, Wheel.Radius * 0.18f, HubWidth, Hub);
	// Tread blocks make the spin visible.
	for (int32 Index = 0; Index < 8; ++Index)
	{
		const float Angle = Index * 45.f;
		const FRotator Spoke(Angle, 0.f, 0.f);
		const FVector Offset = Spoke.RotateVector(FVector(Wheel.Radius * 0.36f, 0.f, 0.f));
		Mesh.AddBox(FTransform(Spoke, Offset), FVector(Wheel.Radius * 0.16f, RimWidth * 0.5f + 0.5f, 2.5f), Hub);
	}
}

// ---------------------------------------------------------------------------
// Pickup truck
// ---------------------------------------------------------------------------

ALIPickupTruck::ALIPickupTruck()
{
	DisplayName = LOCTEXT("Pickup", "Pickup");
	SetChassisExtent(FVector(235.f, 92.f, 42.f));
	Mass = 1700.f;
	CenterOfMassOffset = FVector(10.f, 0.f, -50.f);
	SuspensionLength = 40.f;
	MaxSpeedKmh = 95.f;
	Acceleration = 4.5f;
	ExitOffset = FVector(30.f, -(92.f + 110.f), 0.f);
	SeatOffset = FVector(30.f, -40.f, 30.f);
	DragArea = 0.9f;
	HeadlightOffset = FVector(238.f, 0.f, 8.f);

	auto MakeWheel = [](const FVector& Mount, bool bFront)
	{
		FLIVehicleWheel Wheel;
		Wheel.MountPoint = Mount;
		Wheel.Radius = 38.f;
		Wheel.Width = 28.f;
		Wheel.bSteers = bFront;
		Wheel.bDriven = true; // 4x4: it is a farm truck
		return Wheel;
	};
	Wheels =
	{
		MakeWheel(FVector( 148.f, -84.f, -22.f), true),
		MakeWheel(FVector( 148.f,  84.f, -22.f), true),
		MakeWheel(FVector(-148.f, -84.f, -22.f), false),
		MakeWheel(FVector(-148.f,  84.f, -22.f), false),
	};
}

void ALIPickupTruck::BuildBody(FLILowPolyMesh& Mesh) const
{
	// Sun-faded paint, original colours (not any real brand).
	const FLinearColor Paint(0.78f, 0.36f, 0.12f);
	const FLinearColor PaintDark(0.55f, 0.24f, 0.08f);
	const FLinearColor Glass(0.10f, 0.15f, 0.20f);
	const FLinearColor Trim(0.16f, 0.16f, 0.17f);
	const FLinearColor Chrome(0.70f, 0.70f, 0.68f);
	const FLinearColor Lamp(1.0f, 0.95f, 0.72f);
	const FLinearColor TailLamp(0.85f, 0.05f, 0.04f);
	const FLinearColor BedFloor(0.30f, 0.22f, 0.14f);

	auto Box = [&Mesh](const FVector& Center, const FVector& Half, const FLinearColor& Color)
	{
		Mesh.AddBox(FTransform(Center), Half, Color);
	};

	// Lower body: engine bay and cab floor (front), the cargo bed (rear).
	Box(FVector(100.f, 0.f, -6.f), FVector(135.f, 82.f, 36.f), Paint);          // x -35..235, z -42..30
	Box(FVector(-135.f, 0.f, -24.f), FVector(100.f, 82.f, 18.f), PaintDark);    // bed floor block, z -42..-6
	Box(FVector(-135.f, 0.f, -5.f), FVector(94.f, 76.f, 1.5f), BedFloor);       // wooden bed boards
	Box(FVector(-135.f, -77.f, 14.f), FVector(100.f, 5.f, 20.f), Paint);        // bed sides
	Box(FVector(-135.f, 77.f, 14.f), FVector(100.f, 5.f, 20.f), Paint);
	Box(FVector(-230.f, 0.f, 14.f), FVector(5.f, 72.f, 20.f), Paint);           // tailgate
	Box(FVector(-135.f, -77.f, 35.f), FVector(100.f, 6.f, 1.5f), Trim);         // bed rails
	Box(FVector(-135.f, 77.f, 35.f), FVector(100.f, 6.f, 1.5f), Trim);

	// Fenders over the wheels, a bit wider than the body.
	for (const float X : { 148.f, -148.f })
	{
		for (const float Y : { -1.f, 1.f })
		{
			Box(FVector(X, Y * 84.f, 8.f), FVector(48.f, 14.f, 6.f), PaintDark);
		}
	}

	// Cab with a glass band all round, a slanted windscreen and a roof.
	Box(FVector(25.f, 0.f, 48.f), FVector(60.f, 80.f, 18.f), Paint);            // cab lower, z 30..66
	Box(FVector(25.f, 0.f, 84.f), FVector(56.f, 81.f, 18.f), Glass);            // windows, z 66..102
	Box(FVector(25.f, 0.f, 84.f), FVector(4.f, 82.f, 18.f), Paint);             // door pillar
	Box(FVector(25.f, 0.f, 105.f), FVector(60.f, 80.f, 4.f), Paint);            // roof
	Mesh.AddBox(FTransform(FRotator(-28.f, 0.f, 0.f), FVector(88.f, 0.f, 82.f)), FVector(4.f, 76.f, 22.f), Glass); // windscreen
	Box(FVector(-37.f, 0.f, 84.f), FVector(2.f, 70.f, 16.f), Glass);            // rear window

	// Hood bulge, grille, lamps and bumpers.
	Box(FVector(165.f, 0.f, 32.f), FVector(62.f, 66.f, 3.f), PaintDark);
	Box(FVector(236.f, 0.f, 4.f), FVector(2.f, 46.f, 16.f), Trim);
	Box(FVector(236.f, -62.f, 8.f), FVector(2.f, 14.f, 9.f), Lamp);
	Box(FVector(236.f, 62.f, 8.f), FVector(2.f, 14.f, 9.f), Lamp);
	Box(FVector(240.f, 0.f, -28.f), FVector(6.f, 86.f, 9.f), Chrome);
	Box(FVector(-238.f, 0.f, -28.f), FVector(6.f, 86.f, 9.f), Trim);
	Box(FVector(-236.f, -70.f, 22.f), FVector(2.f, 7.f, 9.f), TailLamp);
	Box(FVector(-236.f, 70.f, 22.f), FVector(2.f, 7.f, 9.f), TailLamp);

	// Mirrors.
	Box(FVector(70.f, -90.f, 70.f), FVector(4.f, 9.f, 7.f), Trim);
	Box(FVector(70.f, 90.f, 70.f), FVector(4.f, 9.f, 7.f), Trim);
}

#undef LOCTEXT_NAMESPACE
