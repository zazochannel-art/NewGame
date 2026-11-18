// Copyright LOST ISLAND Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Survival/LISurvivalTypes.h"
#include "LISurvivalComponent.generated.h"

class AController;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FLIOnSurvivalStatChanged, ELISurvivalStat, Stat, float, NewValue, float, MaxValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLIOnExhaustedChanged, bool, bIsExhausted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLIOnDeath, AController*, Killer);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLIOnColdChanged, bool, bIsCold);

/**
 * ULISurvivalComponent
 *
 * Owns Health, Hunger, Thirst and Stamina for a pawn.
 *
 * Authority & performance:
 *  - NO TICK. The server runs one looping timer (UpdateInterval, default 0.25 s)
 *    that applies decay, regeneration, sprint drain and starvation damage.
 *  - Values are replicated; clients receive OnStatChanged via OnRep so UI is
 *    purely event-driven.
 *  - All mutating functions are server-only (they early-out on clients).
 *
 * Rules (defaults tuned for HARD difficulty):
 *  - Hunger and thirst decay constantly; sprinting multiplies the decay.
 *  - At 0 hunger / 0 thirst the pawn loses health every second.
 *  - Health slowly regenerates only while hunger AND thirst are above WellFedThreshold.
 *  - Sprinting while moving drains stamina. At 0 the pawn becomes Exhausted and
 *    cannot sprint again until stamina recovers to ExhaustionRecoverFraction.
 *  - Stamina regenerates after StaminaRegenDelay seconds without draining, and
 *    regenerates slower while hungry or thirsty.
 *
 * Integration: the owner tells the component when sprinting starts/stops
 * (SetSprinting) and listens to OnExhaustedChanged / OnDeath.
 */
UCLASS(ClassGroup = (LostIsland), meta = (BlueprintSpawnableComponent))
class LOSTISLAND_API ULISurvivalComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULISurvivalComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ---------------- Queries (any machine) ----------------

	UFUNCTION(BlueprintPure, Category = "LostIsland|Survival")
	float GetStatValue(ELISurvivalStat Stat) const;

	UFUNCTION(BlueprintPure, Category = "LostIsland|Survival")
	float GetStatMax(ELISurvivalStat Stat) const;

	/** 0..1 */
	UFUNCTION(BlueprintPure, Category = "LostIsland|Survival")
	float GetStatPercent(ELISurvivalStat Stat) const;

	UFUNCTION(BlueprintPure, Category = "LostIsland|Survival")
	bool IsDead() const { return bIsDead; }

	UFUNCTION(BlueprintPure, Category = "LostIsland|Survival")
	bool IsExhausted() const { return bIsExhausted; }

	/** Night and no lit campfire nearby. Server and owning client. */
	UFUNCTION(BlueprintPure, Category = "LostIsland|Survival")
	bool IsCold() const { return bIsCold; }

	/** True when stamina allows sprinting. Safe to call on clients for prediction. */
	UFUNCTION(BlueprintPure, Category = "LostIsland|Survival")
	bool CanSprint() const;

	// ---------------- Mutation (server only) ----------------

	/** Add (positive) or remove (negative) an amount from a stat. Server only. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "LostIsland|Survival")
	void ModifyStat(ELISurvivalStat Stat, float Delta);

	/** Set a stat to an absolute value (clamped). Server only. Used by SaveGame and debug. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "LostIsland|Survival")
	void SetStatValue(ELISurvivalStat Stat, float NewValue);

	/** Apply damage to health. Returns damage actually applied. Server only. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "LostIsland|Survival")
	float ApplyDamage(float Amount, AController* DamageInstigator = nullptr);

	/** Owner reports sprint state changes. Called on server (and harmlessly on clients). */
	void SetSprinting(bool bNewSprinting);

	/** Owner reports water state: swimming in deep water, and whether the head is under the surface. */
	void SetWaterState(bool bNewSwimming, bool bNewSubmerged);

	UFUNCTION(BlueprintPure, Category = "LostIsland|Survival")
	bool IsSwimming() const { return bIsSwimming; }

	/** Breath left, 0..1. Below 1 only after being under water. */
	UFUNCTION(BlueprintPure, Category = "LostIsland|Survival")
	float GetBreathPercent() const { return MaxBreathSeconds > 0.f ? Breath / MaxBreathSeconds : 1.f; }

	/** Restore all stats to their start values and clear death. Server only. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "LostIsland|Survival")
	void ResetStats();

	// ---------------- Events ----------------

	/** Fired on server and clients whenever a stat value changes. */
	UPROPERTY(BlueprintAssignable, Category = "LostIsland|Survival")
	FLIOnSurvivalStatChanged OnStatChanged;

	/** Fired on server and clients when the exhausted state toggles. */
	UPROPERTY(BlueprintAssignable, Category = "LostIsland|Survival")
	FLIOnExhaustedChanged OnExhaustedChanged;

	/** Fired on server and owning client when the cold state toggles. */
	UPROPERTY(BlueprintAssignable, Category = "LostIsland|Survival")
	FLIOnColdChanged OnColdChanged;

	/** Fired on server and clients once when health reaches zero. Killer is only valid on the server. */
	UPROPERTY(BlueprintAssignable, Category = "LostIsland|Survival")
	FLIOnDeath OnDeath;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ---------------- Tuning ----------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Survival|Stats")
	FLISurvivalStatConfig HealthConfig = FLISurvivalStatConfig(100.f, 1.f, 0.f);

	/** Default: full -> empty in ~40 minutes. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Survival|Stats")
	FLISurvivalStatConfig HungerConfig = FLISurvivalStatConfig(100.f, 0.8f, -0.042f);

	/** Default: full -> empty in ~25 minutes (thirst is the harsher clock). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Survival|Stats")
	FLISurvivalStatConfig ThirstConfig = FLISurvivalStatConfig(100.f, 0.8f, -0.067f);

	/** ChangePerSecond here is the regeneration rate when resting. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Survival|Stats")
	FLISurvivalStatConfig StaminaConfig = FLISurvivalStatConfig(100.f, 1.f, 12.f);

	/** Seconds between server survival updates. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Survival", meta = (ClampMin = "0.05", Units = "s"))
	float UpdateInterval = 0.25f;

	/** Stamina drained per second while sprinting and moving. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Survival|Stamina", meta = (ClampMin = "0.0"))
	float SprintStaminaDrainPerSecond = 18.f;

	/** Seconds after the last stamina drain before regeneration starts. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Survival|Stamina", meta = (ClampMin = "0.0", Units = "s"))
	float StaminaRegenDelay = 1.5f;

	/** Once exhausted, sprint is locked until stamina reaches this fraction of max. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Survival|Stamina", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ExhaustionRecoverFraction = 0.3f;

	/** Stamina regen multiplier while hunger or thirst is below WellFedThreshold. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Survival|Stamina", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HungryStaminaRegenMultiplier = 0.5f;

	/** Hunger/thirst decay multiplier while sprinting. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Survival", meta = (ClampMin = "1.0"))
	float SprintNeedsDecayMultiplier = 2.f;

	/** Health lost per second while hunger is 0. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Survival|Health", meta = (ClampMin = "0.0"))
	float StarvationDamagePerSecond = 0.5f;

	/** Health lost per second while thirst is 0. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Survival|Health", meta = (ClampMin = "0.0"))
	float DehydrationDamagePerSecond = 0.8f;

	/** Health regained per second while well fed and hydrated. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Survival|Health", meta = (ClampMin = "0.0"))
	float WellFedHealthRegenPerSecond = 0.1f;

	/** Hunger and thirst fraction above which the pawn counts as well fed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Survival|Health", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float WellFedThreshold = 0.6f;

	/** Hunger decay multiplier while cold (the body burns calories to stay warm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Survival|Cold", meta = (ClampMin = "1.0"))
	float ColdHungerMultiplier = 1.6f;

	/** Stamina regeneration multiplier while cold. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Survival|Cold", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ColdStaminaRegenMultiplier = 0.5f;

	/** Minimum speed (cm/s) that counts as "moving" for sprint stamina drain. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Survival|Stamina", meta = (ClampMin = "0.0"))
	float SprintMinSpeed = 50.f;

	/** Stamina drained per second while swimming and moving. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Survival|Water", meta = (ClampMin = "0.0"))
	float SwimStaminaDrainPerSecond = 4.f;

	/** Stamina drained per second while treading water in place (no regeneration in deep water). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Survival|Water", meta = (ClampMin = "0.0"))
	float TreadWaterStaminaDrainPerSecond = 1.f;

	/** Seconds of air while the head is under water. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Survival|Water", meta = (ClampMin = "1.0", Units = "s"))
	float MaxBreathSeconds = 15.f;

	/** Breath regained per second at the surface (in seconds of air). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Survival|Water", meta = (ClampMin = "0.0"))
	float BreathRecoveryPerSecond = 5.f;

	/** Health lost per second once breath is gone. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Survival|Water", meta = (ClampMin = "0.0"))
	float DrowningDamagePerSecond = 10.f;

private:
	void UpdateSurvival();
	void SetStatInternal(ELISurvivalStat Stat, float NewValue);
	void SetExhausted(bool bNewExhausted);
	void HandleDeath(AController* Killer);

	float* GetStatPtr(ELISurvivalStat Stat);
	const FLISurvivalStatConfig& GetConfig(ELISurvivalStat Stat) const;

	UFUNCTION() void OnRep_Health();
	UFUNCTION() void OnRep_Hunger();
	UFUNCTION() void OnRep_Thirst();
	UFUNCTION() void OnRep_Stamina();
	UFUNCTION() void OnRep_IsExhausted();
	UFUNCTION() void OnRep_IsDead();
	UFUNCTION() void OnRep_IsCold();
	void SetCold(bool bNewCold);

	UPROPERTY(ReplicatedUsing = OnRep_Health)
	float Health = 100.f;

	UPROPERTY(ReplicatedUsing = OnRep_Hunger)
	float Hunger = 100.f;

	UPROPERTY(ReplicatedUsing = OnRep_Thirst)
	float Thirst = 100.f;

	UPROPERTY(ReplicatedUsing = OnRep_Stamina)
	float Stamina = 100.f;

	UPROPERTY(ReplicatedUsing = OnRep_IsExhausted)
	bool bIsExhausted = false;

	UPROPERTY(ReplicatedUsing = OnRep_IsDead)
	bool bIsDead = false;

	UPROPERTY(ReplicatedUsing = OnRep_IsCold)
	bool bIsCold = false;

	bool bIsSprinting = false;
	bool bIsSwimming = false;
	bool bIsSubmerged = false;

	/** Seconds of air left. Replicated so the owner's HUD can show the breath bar. */
	UPROPERTY(Replicated)
	float Breath = 15.f;
	float TimeSinceStaminaDrain = 0.f;

	/** True once stats were set (ResetStats or a save load), so BeginPlay doesn't overwrite loaded values. */
	bool bStatsInitialized = false;
	FTimerHandle UpdateTimerHandle;
};
