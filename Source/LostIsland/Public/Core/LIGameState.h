// Copyright LOST ISLAND Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "LIGameState.generated.h"

class ALIIslandGenerator;

/**
 * ALIGameState
 *
 * Owns the shared world clock (day/night).
 *
 * Networking without per-second traffic: the server replicates only an anchor
 * (server time at which the clock read AnchorHours, on day AnchorDay) and the
 * clock speed. Every machine computes the current time from the replicated
 * server world time (GetServerWorldTimeSeconds), so clients stay in sync for free.
 *
 * Lighting: a local timer (LightingUpdateInterval, default 0.1 s; not Tick) moves
 * the island generator's sun/moon on every machine.
 */
UCLASS()
class LOSTISLAND_API ALIGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ALIGameState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Hour of day, 0..24. */
	UFUNCTION(BlueprintPure, Category = "LostIsland|Time")
	float GetTimeOfDayHours() const;

	/** Day counter starting at 1. */
	UFUNCTION(BlueprintPure, Category = "LostIsland|Time")
	int32 GetDayNumber() const;

	UFUNCTION(BlueprintPure, Category = "LostIsland|Time")
	bool IsNight() const;

	/** Server: jump the clock (save loading, debug). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "LostIsland|Time")
	void SetTime(int32 Day, float Hours);

	UFUNCTION(BlueprintPure, Category = "LostIsland|Time")
	float GetRealSecondsPerGameHour() const { return RealSecondsPerGameHour; }

	UFUNCTION(BlueprintPure, Category = "LostIsland|Time")
	float GetNightEndHours() const { return NightEndHours; }

	/** Server: someone sailed away on the raft. Everyone sees the ending, then a new island starts. */
	void SetEscaped();

	UFUNCTION(BlueprintPure, Category = "LostIsland|Goal")
	bool HasEscaped() const { return bEscaped; }

	UFUNCTION(BlueprintPure, Category = "LostIsland|Goal")
	int32 GetEscapeDay() const { return EscapeDay; }

	// ---- Farm economy (valley) ----
	UFUNCTION(BlueprintPure, Category = "LostIsland|Farm")
	int32 GetMoney() const { return Money; }

	UFUNCTION(BlueprintPure, Category = "LostIsland|Farm")
	int32 GetGrainKg() const { return GrainKg; }

	/** Whether the HUD shows coins and grain (on in the farm valley). */
	UFUNCTION(BlueprintPure, Category = "LostIsland|Farm")
	bool IsFarmEconomyEnabled() const { return bFarmEconomy; }

	void SetFarmEconomyEnabled(bool bEnabled) { bFarmEconomy = bEnabled; }

	/** Server: harvested grain goes into the farm's store. */
	void AddGrain(int32 Kg);

	/** Server: coins for Kg of grain at the current price. */
	int32 GetGrainValue(int32 Kg) const { return FMath::FloorToInt(Kg * GrainPricePerKg * GrainPriceMultiplier); }

	/** Server: price multiplier from farm buildings (a finished barn). */
	void SetGrainPriceMultiplier(float Multiplier) { GrainPriceMultiplier = FMath::Max(Multiplier, 0.f); }

	UFUNCTION(BlueprintPure, Category = "LostIsland|Farm")
	float GetGrainPriceMultiplier() const { return GrainPriceMultiplier; }

	// ---- Weather (valley only) ----
	/** How hard it rains right now, 0 (clear) .. 1 (downpour). Same value on every machine. */
	UFUNCTION(BlueprintPure, Category = "LostIsland|Weather")
	float GetRainAmount() const;

	/** Server: start fading towards a rain amount (0 = clear). The next weather change is scheduled from here. */
	void SetRain(float Target, float FadeSeconds = -1.f);

	/** Crops grow this much faster in a full downpour (0.5 = 50% faster). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Weather", meta = (ClampMin = "0.0"))
	float RainGrowthBonus = 0.5f;

	/** Tyres lose this fraction of their grip in a full downpour. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Weather", meta = (ClampMin = "0.0", ClampMax = "0.9"))
	float RainGripLoss = 0.2f;

	/** Server: sell everything in store. Returns the coins earned. */
	int32 SellGrain();

	/** Server: spend coins. False (and nothing spent) if there are not enough. */
	bool SpendMoney(int32 Amount);

	/** Server: restore from a save. */
	void SetFarmState(int32 InMoney, int32 InGrainKg);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Farm", meta = (ClampMin = "0.0"))
	float GrainPricePerKg = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Farm", meta = (ClampMin = "0"))
	int32 StartingMoney = 500;

	/** Seconds the ending stays on screen before a new island starts. */
	UPROPERTY(EditDefaultsOnly, Category = "LostIsland|Goal", meta = (ClampMin = "1.0", Units = "s"))
	float EscapeEndDelay = 15.f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Real seconds per in-game hour. 60 = one full day every 24 minutes. */
	UPROPERTY(EditDefaultsOnly, Replicated, BlueprintReadOnly, Category = "LostIsland|Time", meta = (ClampMin = "1.0", Units = "s"))
	float RealSecondsPerGameHour = 60.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Time", meta = (ClampMin = "0.0", ClampMax = "24.0"))
	float StartHours = 8.f;

	/** Night = from NightStartHours until NightEndHours (wrapping midnight). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Time", meta = (ClampMin = "0.0", ClampMax = "24.0"))
	float NightStartHours = 20.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LostIsland|Time", meta = (ClampMin = "0.0", ClampMax = "24.0"))
	float NightEndHours = 5.5f;

	UPROPERTY(EditDefaultsOnly, Category = "LostIsland|Time", meta = (ClampMin = "0.02", Units = "s"))
	float LightingUpdateInterval = 0.1f;

	/** Real seconds of clear sky between showers (random in this range). */
	UPROPERTY(EditDefaultsOnly, Category = "LostIsland|Weather", meta = (Units = "s"))
	FVector2D ClearSeconds = FVector2D(360.f, 840.f);

	/** Real seconds a shower lasts (random in this range). */
	UPROPERTY(EditDefaultsOnly, Category = "LostIsland|Weather", meta = (Units = "s"))
	FVector2D RainSeconds = FVector2D(120.f, 300.f);

	/** How long the sky takes to cloud over or clear up. */
	UPROPERTY(EditDefaultsOnly, Category = "LostIsland|Weather", meta = (ClampMin = "1.0", Units = "s"))
	float RainFadeSeconds = 40.f;

private:
	/** Total in-game hours elapsed since day 1, 00:00. */
	double GetTotalGameHours() const;
	void UpdateLighting();

	/** Server: the scheduled weather change (clear -> rain -> clear ...). */
	void OnWeatherTimer();
	void ScheduleWeather();

	/** Local: the falling-rain effect around this machine's camera. */
	void EnsureRainEffect();

	UPROPERTY(Replicated)
	double AnchorServerSeconds = 0.0;

	/** Total game hours at AnchorServerSeconds. */
	UPROPERTY(Replicated)
	double AnchorTotalHours = 0.0;

	UPROPERTY(Transient)
	TWeakObjectPtr<ALIIslandGenerator> Island;

	FTimerHandle LightingTimerHandle;

private:
	UPROPERTY(Replicated)
	bool bEscaped = false;

	UPROPERTY(Replicated)
	int32 EscapeDay = 0;

	UPROPERTY(Replicated)
	int32 Money = -1; // set to StartingMoney in BeginPlay

	UPROPERTY(Replicated)
	int32 GrainKg = 0;

	UPROPERTY(Replicated)
	bool bFarmEconomy = false;

	UPROPERTY(Replicated)
	float GrainPriceMultiplier = 1.f;

	// Weather: like the clock, only the start and end of the current fade replicate.
	UPROPERTY(Replicated)
	float RainFrom = 0.f;

	UPROPERTY(Replicated)
	float RainTo = 0.f;

	UPROPERTY(Replicated)
	double RainChangeServerSeconds = 0.0;

	UPROPERTY(Replicated)
	float RainFadeActive = 40.f;

	FTimerHandle WeatherTimerHandle;

	UPROPERTY(Transient)
	TObjectPtr<AActor> RainEffect;
};
