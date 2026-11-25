// Copyright LOST ISLAND Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Interaction/LIInteractable.h"
#include "LIVehicle.generated.h"

class ALICharacter;
class UBoxComponent;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class UProceduralMeshComponent;
class USpringArmComponent;
struct FInputActionValue;
struct FLILowPolyMesh;

/** One wheel: where it sits on the chassis and what it does. */
USTRUCT(BlueprintType)
struct LOSTISLAND_API FLIVehicleWheel
{
	GENERATED_BODY()

	/** Suspension top (where the spring is mounted), in chassis space. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wheel")
	FVector MountPoint = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wheel", meta = (Units = "cm"))
	float Radius = 38.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wheel", meta = (Units = "cm"))
	float Width = 26.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wheel")
	bool bSteers = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wheel")
	bool bDriven = true;

	/** Multiplies the tyre's grip (a trailer's small jockey wheel barely holds sideways). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wheel", meta = (ClampMin = "0.0"))
	float GripScale = 1.f;

	// ---- Runtime (not edited) ----
	float Compression = 0.f;   // 0 = fully extended, 1 = fully compressed
	float SpinAngle = 0.f;     // degrees, cosmetic
	bool bGrounded = false;
	FVector ContactPoint = FVector::ZeroVector;
	FVector ContactNormal = FVector::UpVector;
	float NormalForce = 0.f;

	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> Visual;
};

/**
 * ALIVehicle
 *
 * A drivable vehicle built entirely in code (no skeletal mesh or physics asset needed): a physics-simulated box
 * chassis held up by four raycast wheels. Each wheel applies a spring/damper force, sideways tyre grip (limited by
 * how hard the wheel presses on the ground) and engine / brake force along its own heading.
 *
 * Getting in and out: the vehicle is an ILIInteractable ("E - Drive"). The server possesses the vehicle with the
 * character's controller and seats the character (hidden, attached). E again (or death of the driver) gets out on
 * the driver's side, or wherever there is room.
 *
 * Input (generated at runtime, like the character's):
 *     W / S or RT / LT     throttle / brake, reverse when stopped
 *     A / D or left stick  steer
 *     Space / B            handbrake
 *     E / X                get out
 *     R / Y                put the vehicle back on its wheels
 *     Mouse / right stick  look around (the camera drifts back behind the vehicle)
 *     Mouse wheel          camera distance
 *
 * Networking: the server simulates; the driving client sends its input with an unreliable RPC and sees the
 * replicated movement. Good enough for single player and a listen-server host; client prediction comes later.
 *
 * Subclasses set the dimensions and tuning in their constructor and override BuildBody for the model.
 */
UCLASS(Abstract)
class LOSTISLAND_API ALIVehicle : public APawn, public ILIInteractable
{
	GENERATED_BODY()

public:
	ALIVehicle();

	// ---- ILIInteractable ----
	virtual bool CanInteract_Implementation(APawn* InstigatorPawn) const override;
	virtual void Interact_Implementation(APawn* InstigatorPawn) override;
	virtual FText GetInteractionPrompt_Implementation(APawn* InstigatorPawn) const override;

	/** The seated character, or null. */
	UFUNCTION(BlueprintPure, Category = "LostIsland|Vehicle")
	ALICharacter* GetDriver() const { return Driver; }

	/** Forward speed in km/h (negative when reversing). */
	UFUNCTION(BlueprintPure, Category = "LostIsland|Vehicle")
	float GetSpeedKmh() const;

	UFUNCTION(BlueprintPure, Category = "LostIsland|Vehicle")
	FText GetDisplayName() const { return DisplayName; }

	/** Server: put Character in the driver's seat (possesses the vehicle with the character's controller). */
	bool EnterVehicle(ALICharacter* Character);

	/** Server: the driver gets out. Returns false if there was nobody driving. */
	bool ExitVehicle();

	/** The character behind a pawn: the pawn itself, or the driver of a vehicle. */
	static ALICharacter* GetCharacterFromPawn(APawn* Pawn);

	/** Server: back on the wheels, facing the same way, a little above the ground. */
	void ResetUpright();

	/** Whether this vehicle has a working part on Q (dump bed, later the plough). */
	UFUNCTION(BlueprintPure, Category = "LostIsland|Vehicle")
	virtual bool HasTool() const { return false; }

	/** HUD hint for Q, e.g. "Raise bed". Empty without a tool. */
	virtual FText GetToolHint() const { return FText::GetEmpty(); }

	/** Whether T switches between several tools (the tractor's implements). */
	virtual bool HasToolModes() const { return false; }

	/** Name of the current tool for the HUD ("Plough"). */
	virtual FText GetToolModeName() const { return FText::GetEmpty(); }

	/** One line of status for the HUD (e.g. the field under the tractor). Empty = none. */
	virtual FText GetStatusText() const { return FText::GetEmpty(); }

	/** The Q toggle (bed raised, plough lowered...). Replicated. */
	UFUNCTION(BlueprintPure, Category = "LostIsland|Vehicle")
	bool IsToolActive() const { return bToolActive; }

	/** The physics body (the root). */
	UBoxComponent* GetChassis() const { return Chassis; }

	/** Lying on its side or roof (the HUD suggests R). */
	UFUNCTION(BlueprintPure, Category = "LostIsland|Vehicle")
	bool IsTippedOver() const;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void NotifyControllerChanged() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Whether the brakes hold the vehicle (by default: nobody is driving). A towed trailer rolls freely. */
	virtual bool IsParked() const { return GetDriver() == nullptr; }

	/** Model of the body (chassis space, origin at the centre of the collision box). Wheels are built separately. */
	virtual void BuildBody(FLILowPolyMesh& Mesh) const;

	/** Model of one wheel, centred on the origin, axle along Y. */
	virtual void BuildWheel(FLILowPolyMesh& Mesh, const FLIVehicleWheel& Wheel) const;

	// ---- Components ----
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> Chassis;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UProceduralMeshComponent> BodyMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> Camera;

	// ---- Setup ----
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Vehicle")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Vehicle", meta = (ClampMin = "100.0", Units = "kg"))
	float Mass = 1600.f;

	/** Centre of mass offset from the box centre. Low = hard to roll over. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Vehicle")
	FVector CenterOfMassOffset = FVector(0.f, 0.f, -45.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Vehicle")
	TArray<FLIVehicleWheel> Wheels;

	/** Half size of the collision box (the chassis). Subclasses set it in their constructor with SetChassisExtent. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Vehicle")
	FVector BoxExtent = FVector(240.f, 95.f, 45.f);

	void SetChassisExtent(const FVector& InExtent);

	/** Server: Q was pressed. Toggles bToolActive by default; subclasses may refuse (e.g. while moving fast). */
	virtual void ToggleTool();

	/** Server: T was pressed. */
	virtual void CycleTool() {}

	UPROPERTY(Replicated)
	bool bToolActive = false;

	/** Where the character gets out, in chassis space (driver's side first, then the other side). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Vehicle")
	FVector ExitOffset = FVector(0.f, -220.f, 0.f);

	/** Where the (hidden) driver sits, in chassis space. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Vehicle")
	FVector SeatOffset = FVector(20.f, -40.f, 40.f);

	// ---- Suspension ----
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Vehicle|Suspension", meta = (Units = "cm"))
	float SuspensionLength = 40.f;

	/** Fraction of the travel used when standing still (sets the spring stiffness from the weight). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Vehicle|Suspension", meta = (ClampMin = "0.1", ClampMax = "0.9"))
	float RestCompression = 0.45f;

	/** Fraction of critical damping. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Vehicle|Suspension", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float DampingRatio = 0.55f;

	// ---- Drive ----
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Vehicle|Drive", meta = (Units = "km/h"))
	float MaxSpeedKmh = 95.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Vehicle|Drive", meta = (Units = "km/h"))
	float MaxReverseKmh = 22.f;

	/** Acceleration from standstill at full throttle (m/s^2). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Vehicle|Drive", meta = (ClampMin = "0.1"))
	float Acceleration = 4.5f;

	/** Braking deceleration (m/s^2). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Vehicle|Drive", meta = (ClampMin = "0.1"))
	float BrakeDeceleration = 9.f;

	/** Engine braking / rolling resistance with no throttle (m/s^2). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Vehicle|Drive", meta = (ClampMin = "0.0"))
	float CoastDeceleration = 1.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Vehicle|Drive", meta = (ClampMin = "1.0", ClampMax = "60.0"))
	float MaxSteerAngle = 34.f;

	/** Steering lock at top speed, as a fraction of MaxSteerAngle (keeps fast driving stable). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Vehicle|Drive", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float HighSpeedSteerFactor = 0.35f;

	/** Tyre friction coefficient (sideways grip = this x the load on the wheel). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Vehicle|Drive", meta = (ClampMin = "0.1"))
	float TyreGrip = 1.1f;

	/** Rear grip multiplier while the handbrake is held (lets the back slide). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Vehicle|Drive", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HandbrakeRearGrip = 0.35f;

	// ---- Camera ----
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Vehicle|Camera", meta = (Units = "cm"))
	float CameraDistance = 900.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Vehicle|Camera", meta = (Units = "cm"))
	float CameraHeight = 220.f;

private:
	// ---- Input ----
	void EnsureDefaultInput();
	void OnThrottle(const FInputActionValue& Value);
	void OnBrake(const FInputActionValue& Value);
	void OnSteer(const FInputActionValue& Value);
	void OnHandbrake(const FInputActionValue& Value);
	void OnLook(const FInputActionValue& Value);
	void OnZoom(const FInputActionValue& Value);
	void OnExitPressed();
	void OnResetPressed();
	void OnToolPressed();
	void OnCycleToolPressed();

	UFUNCTION(Server, Reliable)
	void ServerCycleTool();

	UFUNCTION(Server, Reliable)
	void ServerToggleTool();
	void SendInputIfChanged();

	UFUNCTION(Server, Unreliable)
	void ServerSetInput(float InThrottle, float InSteer, bool bInHandbrake);

	UFUNCTION(Server, Reliable)
	void ServerExit();

	UFUNCTION(Server, Reliable)
	void ServerReset();

	UPROPERTY(Transient) TObjectPtr<UInputMappingContext> MappingContext;
	UPROPERTY(Transient) TObjectPtr<UInputAction> ThrottleAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> BrakeAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> SteerAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> HandbrakeAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> LookAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> ZoomAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> ExitAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> ResetAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> ToolAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> CycleToolAction;

	/** Local controller the mapping context was added to (removed again when control is lost). */
	TWeakObjectPtr<class APlayerController> InputOwner;

	// Raw local input (keys and triggers combine).
	float LocalThrottle = 0.f;
	float LocalBrake = 0.f;
	float LocalSteer = 0.f;
	bool bLocalHandbrake = false;
	float SentThrottle = 0.f;
	float SentSteer = 0.f;
	bool bSentHandbrake = false;

	// Input the simulation uses. Set by the driving client locally, sent to the server, and replicated to the
	// other machines (which run the same suspension so the replicated physics only needs small corrections).
	// Throttle: -1 (brake / reverse) .. 1.
	UPROPERTY(Replicated) float ThrottleInput = 0.f;
	UPROPERTY(Replicated) float SteerInput = 0.f;
	UPROPERTY(Replicated) bool bHandbrake = false;
	double LastInputSendTime = 0.0;
	float SmoothedSteer = 0.f;

	// ---- Camera state (local) ----
	float LookYawOffset = 0.f;
	float LookPitchOffset = 0.f;
	double LastLookTime = 0.0;
	float CameraZoom = 1.f;

	// ---- Simulation ----
	void SimulatePhysics(float DeltaSeconds);
	void UpdateWheelVisuals(float DeltaSeconds);
	void UpdateCamera(float DeltaSeconds);
	void BuildVisuals();

	UPROPERTY(Replicated)
	TObjectPtr<ALICharacter> Driver;

};

/** Small farm pickup: quick, light, a cargo bed in the back. The first vehicle in the valley. */
UCLASS()
class LOSTISLAND_API ALIPickupTruck : public ALIVehicle
{
	GENERATED_BODY()

public:
	ALIPickupTruck();

protected:
	virtual void BuildBody(FLILowPolyMesh& Mesh) const override;
};
