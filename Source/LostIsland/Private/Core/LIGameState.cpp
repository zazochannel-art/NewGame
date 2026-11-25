// Copyright LOST ISLAND Team. All Rights Reserved.

#include "Core/LIGameState.h"

#include "World/LIIslandGenerator.h"
#include "World/LIRainEffect.h"
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
	DOREPLIFETIME(ALIGameState, RainFrom);
	DOREPLIFETIME(ALIGameState, RainTo);
	DOREPLIFETIME(ALIGameState, RainChangeServerSeconds);
	DOREPLIFETIME(ALIGameState, RainFadeActive);
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
	if (HasAuthority())
	{
		ScheduleWeather();
	}
}

void ALIGameState::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(LightingTimerHandle);
	GetWorldTimerManager().ClearTimer(WeatherTimerHandle);
	if (RainEffect)
	{
		RainEffect->Destroy();
		RainEffect = nullptr;
	}
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
		Island->ApplyTimeOfDay(GetTimeOfDayHours(), NightEndHours, NightStartHours, GetRainAmount());
	}
	EnsureRainEffect();
}

float ALIGameState::GetRainAmount() const
{
	if (RainFrom <= 0.f && RainTo <= 0.f)
	{
		return 0.f;
	}
	const double Now = GetServerWorldTimeSeconds();
	const float Alpha = FMath::Clamp(static_cast<float>((Now - RainChangeServerSeconds) / FMath::Max(1.f, RainFadeActive)), 0.f, 1.f);
	return FMath::Lerp(RainFrom, RainTo, FMath::SmoothStep(0.f, 1.f, Alpha));
}

void ALIGameState::SetRain(float Target, float FadeSeconds)
{
	if (!HasAuthority())
	{
		return;
	}
	RainFrom = GetRainAmount();
	RainTo = FMath::Clamp(Target, 0.f, 1.f);
	RainFadeActive = FadeSeconds > 0.f ? FadeSeconds : RainFadeSeconds;
	RainChangeServerSeconds = GetServerWorldTimeSeconds();
	ScheduleWeather();
}

void ALIGameState::ScheduleWeather()
{
	const FVector2D Range = RainTo > 0.f ? RainSeconds : ClearSeconds;
	const float Low = static_cast<float>(FMath::Min(Range.X, Range.Y));
	const float High = static_cast<float>(FMath::Max(Range.X, Range.Y));
	const float Delay = FMath::Max(10.f, FMath::FRandRange(Low, High));
	GetWorldTimerManager().SetTimer(WeatherTimerHandle, this, &ALIGameState::OnWeatherTimer, Delay, false);
}

void ALIGameState::OnWeatherTimer()
{
	// Showers only in the farm valley; the island keeps its clear sky.
	if (!bFarmEconomy)
	{
		if (RainTo > 0.f)
		{
			SetRain(0.f);
		}
		else
		{
			ScheduleWeather();
		}
		return;
	}
	SetRain(RainTo > 0.f ? 0.f : FMath::FRandRange(0.55f, 1.f));
}

void ALIGameState::EnsureRainEffect()
{
	if (RainEffect || GetNetMode() == NM_DedicatedServer || GetRainAmount() <= 0.f)
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;
	RainEffect = GetWorld()->SpawnActor<ALIRainEffect>(ALIRainEffect::StaticClass(), FTransform::Identity, Params);
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
