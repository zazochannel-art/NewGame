// Copyright LOST ISLAND Team. All Rights Reserved.

#include "World/LIWaterSource.h"

#include "LostIsland.h"
#include "Survival/LISurvivalComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

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

	PromptText = LOCTEXT("Drink", "Drink Fresh Water");
}

void ALIWaterSource::BeginPlay()
{
	Super::BeginPlay();

	// Tint the placeholder (engine BasicShapeMaterial exposes "Color"). Only a few springs exist.
	LITintPlaceholder(Mesh, WaterColor);
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
	Mesh->SetRenderCustomDepth(true);
}

void ALIWaterSource::OnFocusEnd_Implementation(APawn* InstigatorPawn)
{
	Mesh->SetRenderCustomDepth(false);
}

#undef LOCTEXT_NAMESPACE
