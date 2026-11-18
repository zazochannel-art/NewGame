// Copyright LOST ISLAND Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "LISurvivalTypes.generated.h"

/** The four core survival stats. */
UENUM(BlueprintType)
enum class ELISurvivalStat : uint8
{
	Health,
	Hunger,
	Thirst,
	Stamina
};

/**
 * Static tuning for one stat. ChangePerSecond is the passive rate applied by the
 * survival update (negative = decay, positive = regeneration).
 */
USTRUCT(BlueprintType)
struct LOSTISLAND_API FLISurvivalStatConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Survival", meta = (ClampMin = "1.0"))
	float MaxValue = 100.f;

	/** Value at spawn, as a fraction of MaxValue. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Survival", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float StartFraction = 1.f;

	/** Passive change per second. Negative values decay the stat. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Survival")
	float ChangePerSecond = 0.f;

	FLISurvivalStatConfig() = default;
	FLISurvivalStatConfig(float InMax, float InStartFraction, float InChangePerSecond)
		: MaxValue(InMax), StartFraction(InStartFraction), ChangePerSecond(InChangePerSecond)
	{
	}
};
