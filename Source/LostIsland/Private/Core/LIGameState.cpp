// Copyright LOST ISLAND Team. All Rights Reserved.

#include "Core/LIGameState.h"

#include "World/LIIslandGenerator.h"
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
}

void ALIGameState::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority() && AnchorServerSeconds == 0.0 && AnchorTotalHours == 0.0)
	{
		SetTime(1, StartHours); // a loaded save may call SetTime again afterwards
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
		Island->ApplyTimeOfDay(GetTimeOfDayHours());
	}
}
