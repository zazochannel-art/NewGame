// Copyright LOST ISLAND Team. All Rights Reserved.

#include "World/LIWaterSource.h"

#include "LostIsland.h"
#include "Survival/LISurvivalComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Math/RandomStream.h"
#include "ProceduralMeshComponent.h"
#include "Visuals/LILowPolyMesh.h"

#define LOCTEXT_NAMESPACE "LIWaterSource"

ALIWaterSource::ALIWaterSource()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true; // static placement, but replicated so clients get the actor for focus/prompt

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	// Placeholder: a wide, flat cylinder "pool".
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(SceneRoot);
	Mesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CylinderMesh.Object);
	}
	Mesh->SetRelativeScale3D(FVector(2.2f, 2.2f, 0.25f));
	Mesh->SetRelativeLocation(FVector(0.f, 0.f, 5.f));

	Visual = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(SceneRoot);
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Visual->SetCanEverAffectNavigation(false);

	PromptText = LOCTEXT("Drink", "Drink Fresh Water");
}

void ALIWaterSource::BeginPlay()
{
	Super::BeginPlay();

	// Low-poly pond (every machine). The flat cylinder stays as the hidden collision / interaction shape.
	FRandomStream Random(LIVisualSeed(GetActorLocation(), 0x5E1));
	FLILowPolyMesh Model;

	// Dark rim, then the water surface slightly above it.
	Model.AddFrustum(FTransform(FVector(0.f, 0.f, -4.f)), 14, 118.f, 112.f, 9.f, FLinearColor(0.20f, 0.17f, 0.12f), 0.05f, &Random);
	Model.AddFrustum(FTransform(FVector(0.f, 0.f, 2.f)), 14, 104.f, 104.f, 7.f, WaterColor, 0.04f, &Random);

	// Stones around the edge.
	const int32 Stones = Random.RandRange(10, 13);
	for (int32 Index = 0; Index < Stones; ++Index)
	{
		const float Angle = 2.f * PI * Index / Stones + Random.FRandRange(-0.15f, 0.15f);
		const float R = Random.FRandRange(15.f, 24.f);
		const FVector Position(FMath::Cos(Angle) * 120.f, FMath::Sin(Angle) * 120.f, 6.f);
		Model.AddBlob(FTransform(FRotator(0.f, Random.FRandRange(0.f, 360.f), 0.f), Position), FVector(R, R * 0.85f, R * 0.6f),
			0.22f, Random, LIVaryColor(FLinearColor(0.52f, 0.50f, 0.47f), Random, 0.12f), nullptr, 0.7f, 0);
	}

	// Two clumps of reeds.
	for (int32 Clump = 0; Clump < 2; ++Clump)
	{
		const float Angle = Random.FRandRange(0.f, 2.f * PI);
		const FVector Center(FMath::Cos(Angle) * 132.f, FMath::Sin(Angle) * 132.f, 0.f);
		const int32 Reeds = Random.RandRange(4, 7);
		for (int32 Index = 0; Index < Reeds; ++Index)
		{
			const FVector Offset(Random.FRandRange(-18.f, 18.f), Random.FRandRange(-18.f, 18.f), -4.f);
			const FRotator Lean(Random.FRandRange(-12.f, 0.f), Random.FRandRange(0.f, 360.f), 0.f);
			Model.AddFrustum(FTransform(Lean, Center + Offset), 3, 3.5f, 0.f, Random.FRandRange(70.f, 120.f),
				LIVaryColor(FLinearColor(0.30f, 0.50f, 0.18f), Random, 0.12f));
		}
	}

	if (Model.ApplyTo(Visual))
	{
		Mesh->SetHiddenInGame(true);
	}
	else
	{
		LITintPlaceholder(Mesh, WaterColor);
	}
}

bool ALIWaterSource::CanInteract_Implementation(APawn* InstigatorPawn) const
{
	const ULISurvivalComponent* Survival = InstigatorPawn ? InstigatorPawn->FindComponentByClass<ULISurvivalComponent>() : nullptr;
	if (!Survival || Survival->IsDead())
	{
		return false;
	}
	const float Deficit = Survival->GetStatMax(ELISurvivalStat::Thirst) - Survival->GetStatValue(ELISurvivalStat::Thirst);
	return Deficit >= MinDeficitToDrink;
}

void ALIWaterSource::Interact_Implementation(APawn* InstigatorPawn)
{
	if (!HasAuthority() || !InstigatorPawn)
	{
		return;
	}
	if (ULISurvivalComponent* Survival = InstigatorPawn->FindComponentByClass<ULISurvivalComponent>())
	{
		Survival->ModifyStat(ELISurvivalStat::Thirst, ThirstPerDrink);
	}
}

FText ALIWaterSource::GetInteractionPrompt_Implementation(APawn* InstigatorPawn) const
{
	return PromptText;
}

void ALIWaterSource::OnFocusBegin_Implementation(APawn* InstigatorPawn)
{
	Visual->SetRenderCustomDepth(true);
	Mesh->SetRenderCustomDepth(true);
}

void ALIWaterSource::OnFocusEnd_Implementation(APawn* InstigatorPawn)
{
	Visual->SetRenderCustomDepth(false);
	Mesh->SetRenderCustomDepth(false);
}

#undef LOCTEXT_NAMESPACE

bool ALIWaterSource::IsNearWater(const UWorld* World, const FVector& Location, float Radius)
{
	if (!World)
	{
		return false;
	}
	for (TActorIterator<ALIWaterSource> It(World); It; ++It)
	{
		if (FVector::DistSquared(It->GetActorLocation(), Location) <= FMath::Square(Radius))
		{
			return true;
		}
	}
	return false;
}
