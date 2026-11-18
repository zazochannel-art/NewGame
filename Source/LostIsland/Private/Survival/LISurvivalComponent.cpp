// Copyright LOST ISLAND Team. All Rights Reserved.

#include "Survival/LISurvivalComponent.h"

#include "LostIsland.h"
#include "Core/LIGameState.h"
#include "World/LICampfire.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

ULISurvivalComponent::ULISurvivalComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetIsReplicatedByDefault(true);
}

void ULISurvivalComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Health is visible to teammates (co-op health bars). The rest is only needed by the owner.
	DOREPLIFETIME(ULISurvivalComponent, Health);
	DOREPLIFETIME(ULISurvivalComponent, bIsDead);
	DOREPLIFETIME_CONDITION(ULISurvivalComponent, Hunger, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ULISurvivalComponent, Thirst, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ULISurvivalComponent, Stamina, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ULISurvivalComponent, bIsExhausted, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ULISurvivalComponent, bIsCold, COND_OwnerOnly);
}

void ULISurvivalComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetOwnerRole() == ROLE_Authority)
	{
		// The first local player can spawn (and receive saved stats) before world BeginPlay.
		if (!bStatsInitialized)
		{
			ResetStats();
		}

		const float FirstDelay = FMath::FRandRange(0.f, UpdateInterval); // spread players across frames
		GetWorld()->GetTimerManager().SetTimer(UpdateTimerHandle, this, &ULISurvivalComponent::UpdateSurvival,
			UpdateInterval, true, FirstDelay);
	}
}

void ULISurvivalComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(UpdateTimerHandle);
	}
	Super::EndPlay(EndPlayReason);
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------

const FLISurvivalStatConfig& ULISurvivalComponent::GetConfig(ELISurvivalStat Stat) const
{
	switch (Stat)
	{
	case ELISurvivalStat::Hunger:  return HungerConfig;
	case ELISurvivalStat::Thirst:  return ThirstConfig;
	case ELISurvivalStat::Stamina: return StaminaConfig;
	case ELISurvivalStat::Health:
	default:                       return HealthConfig;
	}
}

float* ULISurvivalComponent::GetStatPtr(ELISurvivalStat Stat)
{
	switch (Stat)
	{
	case ELISurvivalStat::Hunger:  return &Hunger;
	case ELISurvivalStat::Thirst:  return &Thirst;
	case ELISurvivalStat::Stamina: return &Stamina;
	case ELISurvivalStat::Health:
	default:                       return &Health;
	}
}

float ULISurvivalComponent::GetStatValue(ELISurvivalStat Stat) const
{
	switch (Stat)
	{
	case ELISurvivalStat::Hunger:  return Hunger;
	case ELISurvivalStat::Thirst:  return Thirst;
	case ELISurvivalStat::Stamina: return Stamina;
	case ELISurvivalStat::Health:
	default:                       return Health;
	}
}

float ULISurvivalComponent::GetStatMax(ELISurvivalStat Stat) const
{
	return GetConfig(Stat).MaxValue;
}

float ULISurvivalComponent::GetStatPercent(ELISurvivalStat Stat) const
{
	const float Max = GetStatMax(Stat);
	return Max > 0.f ? FMath::Clamp(GetStatValue(Stat) / Max, 0.f, 1.f) : 0.f;
}

bool ULISurvivalComponent::CanSprint() const
{
	return !bIsDead && !bIsExhausted && Stamina > 0.f;
}

// ---------------------------------------------------------------------------
// Mutation (server)
// ---------------------------------------------------------------------------

void ULISurvivalComponent::ModifyStat(ELISurvivalStat Stat, float Delta)
{
	if (GetOwnerRole() != ROLE_Authority || bIsDead || FMath::IsNearlyZero(Delta))
	{
		return;
	}

	if (Stat == ELISurvivalStat::Health && Delta < 0.f)
	{
		ApplyDamage(-Delta);
		return;
	}

	if (Stat == ELISurvivalStat::Stamina && Delta < 0.f)
	{
		TimeSinceStaminaDrain = 0.f; // spending stamina (gathering, attacks) delays regeneration
	}

	SetStatInternal(Stat, GetStatValue(Stat) + Delta);
}

void ULISurvivalComponent::SetStatValue(ELISurvivalStat Stat, float NewValue)
{
	if (GetOwnerRole() != ROLE_Authority || bIsDead)
	{
		return;
	}

	SetStatInternal(Stat, NewValue);
	bStatsInitialized = true;

	if (Stat == ELISurvivalStat::Health && Health <= 0.f)
	{
		HandleDeath(nullptr);
	}
}

float ULISurvivalComponent::ApplyDamage(float Amount, AController* DamageInstigator)
{
	if (GetOwnerRole() != ROLE_Authority || bIsDead || Amount <= 0.f)
	{
		return 0.f;
	}

	const float OldHealth = Health;
	SetStatInternal(ELISurvivalStat::Health, Health - Amount);

	if (Health <= 0.f)
	{
		HandleDeath(DamageInstigator);
	}

	return OldHealth - Health;
}

void ULISurvivalComponent::ResetStats()
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	const bool bWasDead = bIsDead;
	bIsDead = false;

	for (ELISurvivalStat Stat : { ELISurvivalStat::Health, ELISurvivalStat::Hunger, ELISurvivalStat::Thirst, ELISurvivalStat::Stamina })
	{
		const FLISurvivalStatConfig& Config = GetConfig(Stat);
		SetStatInternal(Stat, Config.MaxValue * Config.StartFraction);
	}

	SetExhausted(false);
	TimeSinceStaminaDrain = StaminaRegenDelay;
	bStatsInitialized = true;

	if (bWasDead && !UpdateTimerHandle.IsValid() && GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(UpdateTimerHandle, this, &ULISurvivalComponent::UpdateSurvival, UpdateInterval, true);
	}
}

void ULISurvivalComponent::SetSprinting(bool bNewSprinting)
{
	bIsSprinting = bNewSprinting;
}

void ULISurvivalComponent::SetStatInternal(ELISurvivalStat Stat, float NewValue)
{
	float* Value = GetStatPtr(Stat);
	const float Clamped = FMath::Clamp(NewValue, 0.f, GetStatMax(Stat));
	if (FMath::IsNearlyEqual(*Value, Clamped))
	{
		return;
	}

	*Value = Clamped;
	OnStatChanged.Broadcast(Stat, Clamped, GetStatMax(Stat));
}

void ULISurvivalComponent::SetExhausted(bool bNewExhausted)
{
	if (bIsExhausted == bNewExhausted)
	{
		return;
	}

	bIsExhausted = bNewExhausted;
	OnExhaustedChanged.Broadcast(bIsExhausted);
}

void ULISurvivalComponent::HandleDeath(AController* Killer)
{
	if (bIsDead)
	{
		return;
	}

	bIsDead = true;
	bIsSprinting = false;
	GetWorld()->GetTimerManager().ClearTimer(UpdateTimerHandle);

	UE_LOG(LogLostIsland, Log, TEXT("%s died (killer: %s)."), *GetNameSafe(GetOwner()), *GetNameSafe(Killer));
	OnDeath.Broadcast(Killer);
}

// ---------------------------------------------------------------------------
// Server update (timer, not tick)
// ---------------------------------------------------------------------------

void ULISurvivalComponent::UpdateSurvival()
{
	if (bIsDead)
	{
		return;
	}

	const float Dt = UpdateInterval;
	const AActor* Owner = GetOwner();
	const bool bMoving = Owner && Owner->GetVelocity().SizeSquared2D() > FMath::Square(SprintMinSpeed);
	const bool bSprintingAndMoving = bIsSprinting && bMoving;

	// --- Temperature ---
	const ALIGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ALIGameState>() : nullptr;
	SetCold(GameState && GameState->IsNight() && Owner && !ALICampfire::IsWarmAt(GetWorld(), Owner->GetActorLocation()));

	// --- Needs ---
	const float NeedsMultiplier = bSprintingAndMoving ? SprintNeedsDecayMultiplier : 1.f;
	const float ColdMultiplier = bIsCold ? ColdHungerMultiplier : 1.f;
	SetStatInternal(ELISurvivalStat::Hunger, Hunger + HungerConfig.ChangePerSecond * NeedsMultiplier * ColdMultiplier * Dt);
	SetStatInternal(ELISurvivalStat::Thirst, Thirst + ThirstConfig.ChangePerSecond * NeedsMultiplier * Dt);

	const bool bWellFed = GetStatPercent(ELISurvivalStat::Hunger) >= WellFedThreshold
		&& GetStatPercent(ELISurvivalStat::Thirst) >= WellFedThreshold;

	// --- Stamina ---
	if (bSprintingAndMoving)
	{
		TimeSinceStaminaDrain = 0.f;
		SetStatInternal(ELISurvivalStat::Stamina, Stamina - SprintStaminaDrainPerSecond * Dt);
		if (Stamina <= 0.f)
		{
			SetExhausted(true); // owner listens and stops sprinting
		}
	}
	else
	{
		TimeSinceStaminaDrain += Dt;
		if (TimeSinceStaminaDrain >= StaminaRegenDelay)
		{
			const float RegenMultiplier = (bWellFed ? 1.f : HungryStaminaRegenMultiplier) * (bIsCold ? ColdStaminaRegenMultiplier : 1.f);
			SetStatInternal(ELISurvivalStat::Stamina, Stamina + StaminaConfig.ChangePerSecond * RegenMultiplier * Dt);
		}
	}

	if (bIsExhausted && GetStatPercent(ELISurvivalStat::Stamina) >= ExhaustionRecoverFraction)
	{
		SetExhausted(false);
	}

	// --- Health ---
	float HealthDelta = HealthConfig.ChangePerSecond;
	if (Hunger <= 0.f)
	{
		HealthDelta -= StarvationDamagePerSecond;
	}
	if (Thirst <= 0.f)
	{
		HealthDelta -= DehydrationDamagePerSecond;
	}
	if (bWellFed)
	{
		HealthDelta += WellFedHealthRegenPerSecond;
	}

	if (HealthDelta < 0.f)
	{
		ApplyDamage(-HealthDelta * Dt);
	}
	else if (HealthDelta > 0.f)
	{
		SetStatInternal(ELISurvivalStat::Health, Health + HealthDelta * Dt);
	}
}

// ---------------------------------------------------------------------------
// Replication callbacks (clients)
// ---------------------------------------------------------------------------

void ULISurvivalComponent::OnRep_Health()  { OnStatChanged.Broadcast(ELISurvivalStat::Health, Health, HealthConfig.MaxValue); }
void ULISurvivalComponent::OnRep_Hunger()  { OnStatChanged.Broadcast(ELISurvivalStat::Hunger, Hunger, HungerConfig.MaxValue); }
void ULISurvivalComponent::OnRep_Thirst()  { OnStatChanged.Broadcast(ELISurvivalStat::Thirst, Thirst, ThirstConfig.MaxValue); }
void ULISurvivalComponent::OnRep_Stamina() { OnStatChanged.Broadcast(ELISurvivalStat::Stamina, Stamina, StaminaConfig.MaxValue); }
void ULISurvivalComponent::OnRep_IsExhausted() { OnExhaustedChanged.Broadcast(bIsExhausted); }

void ULISurvivalComponent::SetCold(bool bNewCold)
{
	if (bIsCold != bNewCold)
	{
		bIsCold = bNewCold;
		OnColdChanged.Broadcast(bIsCold);
	}
}

void ULISurvivalComponent::OnRep_IsCold()
{
	OnColdChanged.Broadcast(bIsCold);
}

void ULISurvivalComponent::OnRep_IsDead()
{
	if (bIsDead)
	{
		OnDeath.Broadcast(nullptr);
	}
}
