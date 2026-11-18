// Copyright LOST ISLAND Team. All Rights Reserved.

#include "World/LISleepingBag.h"

#include "LostIsland.h"
#include "Core/LIGameState.h"
#include "Save/LISaveSubsystem.h"
#include "Survival/LISurvivalComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "LISleepingBag"

ALISleepingBag::ALISleepingBag()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	// 190 x 80 x 12 cm roll. Players walk over it, the interaction trace still hits it.
	Mat = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mat"));
	Mat->SetupAttachment(SceneRoot);
	Mat->SetStaticMesh(CubeMesh.Object);
	Mat->SetRelativeScale3D(FVector(1.9f, 0.8f, 0.12f));
	Mat->SetRelativeLocation(FVector(0.f, 0.f, 6.f));
	Mat->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Mat->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

	Pillow = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Pillow"));
	Pillow->SetupAttachment(SceneRoot);
	Pillow->SetStaticMesh(CylinderMesh.Object);
	Pillow->SetRelativeScale3D(FVector(0.35f, 0.7f, 0.15f));
	Pillow->SetRelativeRotation(FRotator(0.f, 0.f, 90.f));
	Pillow->SetRelativeLocation(FVector(75.f, 0.f, 18.f));
	Pillow->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
}

void ALISleepingBag::BeginPlay()
{
	Super::BeginPlay();
	LITintPlaceholder(Mat, FLinearColor(0.12f, 0.28f, 0.35f));    // faded teal canvas
	LITintPlaceholder(Pillow, FLinearColor(0.55f, 0.50f, 0.40f)); // rolled fibre
}

bool ALISleepingBag::CanInteract_Implementation(APawn* InstigatorPawn) const
{
	const ULISurvivalComponent* Survival = InstigatorPawn ? InstigatorPawn->FindComponentByClass<ULISurvivalComponent>() : nullptr;
	return Survival && !Survival->IsDead();
}

FText ALISleepingBag::GetInteractionPrompt_Implementation(APawn* InstigatorPawn) const
{
	const ALIGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ALIGameState>() : nullptr;
	if (GameState && GameState->IsNight() && GameState->PlayerArray.Num() <= 1)
	{
		return LOCTEXT("SleepNight", "Sleep until morning");
	}
	return LOCTEXT("SetRespawn", "Sleep (set respawn point)");
}

void ALISleepingBag::Interact_Implementation(APawn* InstigatorPawn)
{
	if (!HasAuthority() || !InstigatorPawn)
	{
		return;
	}

	ClaimFor(ULISaveSubsystem::GetPlayerKey(InstigatorPawn->GetController()));

	// Single player only: skipping time in co-op would need everyone to agree.
	ALIGameState* GameState = GetWorld()->GetGameState<ALIGameState>();
	if (GameState && GameState->IsNight() && GameState->PlayerArray.Num() <= 1)
	{
		const float Now = GameState->GetTimeOfDayHours();
		const int32 Day = GameState->GetDayNumber() + (Now >= 12.f ? 1 : 0);
		GameState->SetTime(Day, GameState->GetNightEndHours() + 0.5f);

		if (ULISurvivalComponent* Survival = InstigatorPawn->FindComponentByClass<ULISurvivalComponent>())
		{
			Survival->ModifyStat(ELISurvivalStat::Hunger, -SleepNeedsCost);
			Survival->ModifyStat(ELISurvivalStat::Thirst, -SleepNeedsCost);
		}
		UE_LOG(LogLostIsland, Log, TEXT("%s slept until morning (day %d)."), *GetNameSafe(InstigatorPawn), Day);
	}
}

void ALISleepingBag::ClaimFor(const FString& PlayerKey)
{
	if (!HasAuthority() || PlayerKey.IsEmpty())
	{
		return;
	}
	for (TActorIterator<ALISleepingBag> It(GetWorld()); It; ++It)
	{
		if (*It != this && It->OwnerKey == PlayerKey)
		{
			It->OwnerKey.Reset();
		}
	}
	OwnerKey = PlayerKey;
	UE_LOG(LogLostIsland, Log, TEXT("Sleeping bag %s is now the respawn point of '%s'."), *GetName(), *PlayerKey);
}

ALISleepingBag* ALISleepingBag::FindBagFor(const UWorld* World, const FString& PlayerKey)
{
	if (!World || PlayerKey.IsEmpty())
	{
		return nullptr;
	}
	for (TActorIterator<ALISleepingBag> It(World); It; ++It)
	{
		if (It->OwnerKey == PlayerKey)
		{
			return *It;
		}
	}
	return nullptr;
}

FTransform ALISleepingBag::GetRespawnTransform() const
{
	// Beside the bag (it doesn't block pawns, but standing on it looks odd), capsule above the ground.
	const FVector Location = GetActorLocation() + GetActorRightVector() * 90.f + FVector(0.f, 0.f, 110.f);
	return FTransform(FRotator(0.f, GetActorRotation().Yaw, 0.f), Location);
}

#undef LOCTEXT_NAMESPACE
