// Copyright LOST ISLAND Team. All Rights Reserved.

#include "Core/LIGameState.h"

#include "World/LIIslandGenerator.h"
#include "LostIsland.h"
#include "Save/LISaveSubsystem.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

ALIGameState::ALIGameState()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ALIGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ALIGameState, RealSecondsPerGameHour);
	DOREPLIFETIME(ALIGameState, AnchorServerSeconds);
	DOREPLIFETIME(ALIGameState, AnchorTotalHours);
	DOREPLIFETIME(ALIGameState, bEscaped);
	DOREPLIFETIME(ALIGameState, EscapeDay);
	DOREPLIFETIME(ALIGameState, Money);
	DOREPLIFETIME(ALIGameState, GrainKg);
	DOREPLIFETIME(ALIGameState, GrainPriceMultiplier);
	DOREPLIFETIME(ALIGameState, bFarmEconomy);
}

void ALIGameState::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority() && AnchorServerSeconds == 0.0 && AnchorTotalHours == 0.0)
	{
		SetTime(1, StartHours); // a loaded save may call SetTime again afterwards
	}
	if (HasAuthority() && Money < 0)
	{
		Money = StartingMoney; // a loaded save may call SetFarmState afterwards
	}

	GetWorldTimerManager().SetTimer(LightingTimerHandle, this, &ALIGameState::UpdateLighting, LightingUpdateInterval, true, 0.f);
}

void ALIGameState::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(LightingTimerHandle);
	Super::EndPlay(EndPlayReason);
}

void ALIGameState::SetTime(int32 Day, float Hours)
{
	if (!HasAuthority())
	{
		return;
	}
	AnchorServerSeconds = GetServerWorldTimeSeconds();
	AnchorTotalHours = (FMath::Max(1, Day) - 1) * 24.0 + FMath::Clamp(Hours, 0.f, 23.999f);
	ForceNetUpdate();
	UpdateLighting();
}

double ALIGameState::GetTotalGameHours() const
{
	const double Elapsed = FMath::Max(0.0, GetServerWorldTimeSeconds() - AnchorServerSeconds);
	return AnchorTotalHours + Elapsed / FMath::Max(1.f, RealSecondsPerGameHour);
}

float ALIGameState::GetTimeOfDayHours() const
{
	return static_cast<float>(FMath::Fmod(GetTotalGameHours(), 24.0));
}

int32 ALIGameState::GetDayNumber() const
{
	return 1 + static_cast<int32>(FMath::FloorToDouble(GetTotalGameHours() / 24.0));
}

bool ALIGameState::IsNight() const
{
	const float Hours = GetTimeOfDayHours();
	return NightStartHours > NightEndHours
		? (Hours >= NightStartHours || Hours < NightEndHours)
		: (Hours >= NightStartHours && Hours < NightEndHours);
}

void ALIGameState::UpdateLighting()
{
	if (!Island.IsValid())
	{
		for (TActorIterator<ALIIslandGenerator> It(GetWorld()); It; ++It)
		{
			Island = *It;
			break;
		}
	}
	if (Island.IsValid())
	{
		// Sunrise = end of night, sunset = start of night: darkness and cold always match.
		Island->ApplyTimeOfDay(GetTimeOfDayHours(), NightEndHours, NightStartHours);
	}
}

void ALIGameState::SetEscaped()
{
	if (!HasAuthority() || bEscaped)
	{
		return;
	}
	bEscaped = true;
	EscapeDay = GetDayNumber();
	UE_LOG(LogLostIsland, Log, TEXT("Escaped the island on day %d."), EscapeDay);

	// The run is over: after the ending, start a fresh island (this also deletes the save).
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (ULISaveSubsystem* SaveSubsystem = ULISaveSubsystem::Get(this))
		{
			SaveSubsystem->NewGameAndRestart();
		}
	}), EscapeEndDelay, false);
}

// ---------------------------------------------------------------------------
// Farm economy
// ---------------------------------------------------------------------------

void ALIGameState::AddGrain(int32 Kg)
{
	if (HasAuthority() && Kg > 0)
	{
		GrainKg += Kg;
	}
}

int32 ALIGameState::SellGrain()
{
	if (!HasAuthority() || GrainKg <= 0)
	{
		return 0;
	}
	const int32 Earned = GetGrainValue(GrainKg);
	Money = FMath::Max(Money, 0) + Earned;
	UE_LOG(LogLostIsland, Log, TEXT("Sold %d kg of grain for %d coins (now %d)."), GrainKg, Earned, Money);
	GrainKg = 0;
	ForceNetUpdate();
	return Earned;
}

bool ALIGameState::SpendMoney(int32 Amount)
{
	if (!HasAuthority() || Amount < 0 || Money < Amount)
	{
		return false;
	}
	Money -= Amount;
	ForceNetUpdate();
	return true;
}

void ALIGameState::SetFarmState(int32 InMoney, int32 InGrainKg)
{
	if (HasAuthority())
	{
		Money = FMath::Max(InMoney, 0);
		GrainKg = FMath::Max(InGrainKg, 0);
		ForceNetUpdate();
	}
}
