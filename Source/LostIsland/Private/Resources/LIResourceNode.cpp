// Copyright LOST ISLAND Team. All Rights Reserved.

#include "Resources/LIResourceNode.h"

#include "Inventory/LIInventoryComponent.h"
#include "Items/LIItemRegistrySubsystem.h"
#include "Items/LIItemTypes.h"
#include "LostIsland.h"
#include "Survival/LISurvivalComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "LIResourceNode"

ALIResourceNode::ALIResourceNode()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(SceneRoot);
	Mesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

	PromptText = LOCTEXT("DefaultPrompt", "Gather");
}

void ALIResourceNode::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ALIResourceNode, bDepleted);
	DOREPLIFETIME(ALIResourceNode, RemainingHits);
}

void ALIResourceNode::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		RemainingHits = HitsToDeplete;
		MaxYield = FMath::Max(MinYield, MaxYield);
	}
	ApplyDepletedState();
}

void ALIResourceNode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RespawnTimerHandle);
	}
	Super::EndPlay(EndPlayReason);
}

bool ALIResourceNode::CanInteract_Implementation(APawn* InstigatorPawn) const
{
	if (bDepleted || !InstigatorPawn)
	{
		return false;
	}

	// Stamina replicates to the owner, so this check also works for client-side focus.
	if (const ULISurvivalComponent* Survival = InstigatorPawn->FindComponentByClass<ULISurvivalComponent>())
	{
		if (Survival->IsDead() || Survival->GetStatValue(ELISurvivalStat::Stamina) < StaminaCostPerHit)
		{
			return false;
		}
	}
	return true;
}

void ALIResourceNode::Interact_Implementation(APawn* InstigatorPawn)
{
	if (!HasAuthority() || bDepleted || !InstigatorPawn)
	{
		return;
	}

	ULIInventoryComponent* Inventory = InstigatorPawn->FindComponentByClass<ULIInventoryComponent>();
	if (!Inventory)
	{
		return;
	}

	const int32 Yield = FMath::RandRange(MinYield, MaxYield);
	const int32 Added = Inventory->AddItem(ResourceItemId, Yield);
	if (Added <= 0)
	{
		return; // Inventory full: no stamina spent, node untouched.
	}

	if (ULISurvivalComponent* Survival = InstigatorPawn->FindComponentByClass<ULISurvivalComponent>())
	{
		Survival->ModifyStat(ELISurvivalStat::Stamina, -StaminaCostPerHit);
	}

	RemainingHits = FMath::Max(0, RemainingHits - 1);
	BP_OnGathered(InstigatorPawn, Added);

	if (RemainingHits == 0)
	{
		Deplete();
	}
}

FText ALIResourceNode::GetInteractionPrompt_Implementation(APawn* InstigatorPawn) const
{
	return FText::Format(LOCTEXT("PromptFormat", "{0}  ({1}/{2})"), PromptText, FText::AsNumber(RemainingHits), FText::AsNumber(HitsToDeplete));
}

void ALIResourceNode::OnFocusBegin_Implementation(APawn* InstigatorPawn)
{
	Mesh->SetRenderCustomDepth(true);
}

void ALIResourceNode::OnFocusEnd_Implementation(APawn* InstigatorPawn)
{
	Mesh->SetRenderCustomDepth(false);
}

void ALIResourceNode::Deplete()
{
	bDepleted = true;
	ApplyDepletedState();

	if (RespawnTime > 0.f)
	{
		GetWorld()->GetTimerManager().SetTimer(RespawnTimerHandle, this, &ALIResourceNode::Respawn, RespawnTime, false);
	}
}

void ALIResourceNode::Respawn()
{
	RemainingHits = HitsToDeplete;
	bDepleted = false;
	ApplyDepletedState();
}

void ALIResourceNode::OnRep_Depleted()
{
	ApplyDepletedState();
}

void ALIResourceNode::ApplyDepletedState()
{
	Mesh->SetVisibility(!bDepleted, true);
	Mesh->SetCollisionEnabled(bDepleted ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
	BP_OnDepletedChanged(bDepleted);
}

// ---------------------------------------------------------------------------
// Presets (engine basic shapes until low-poly art exists)
// ---------------------------------------------------------------------------

ALITreeNode::ALITreeNode()
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ShapeMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (ShapeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(ShapeMesh.Object);
	}
	Mesh->SetRelativeScale3D(FVector(0.5f, 0.5f, 4.f));   // 50 cm wide, 400 cm tall
	Mesh->SetRelativeLocation(FVector(0.f, 0.f, 200.f));  // origin at the base

	ResourceItemId = LIItems::Wood;
	MinYield = 2;
	MaxYield = 3;
	HitsToDeplete = 6;
	RespawnTime = 600.f;
	StaminaCostPerHit = 6.f;
	PromptText = LOCTEXT("ChopTree", "Chop Tree");
}

ALIRockNode::ALIRockNode()
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ShapeMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (ShapeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(ShapeMesh.Object);
	}
	Mesh->SetRelativeScale3D(FVector(1.4f, 1.2f, 0.9f));
	Mesh->SetRelativeLocation(FVector(0.f, 0.f, 35.f));

	ResourceItemId = LIItems::Stone;
	MinYield = 1;
	MaxYield = 2;
	HitsToDeplete = 8;
	RespawnTime = 900.f;
	StaminaCostPerHit = 8.f;
	PromptText = LOCTEXT("MineRock", "Mine Rock");
}

ALIFiberBushNode::ALIFiberBushNode()
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ShapeMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));
	if (ShapeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(ShapeMesh.Object);
	}
	Mesh->SetRelativeScale3D(FVector(0.8f, 0.8f, 0.7f));
	Mesh->SetRelativeLocation(FVector(0.f, 0.f, 35.f));

	ResourceItemId = LIItems::Fiber;
	MinYield = 2;
	MaxYield = 4;
	HitsToDeplete = 3;
	RespawnTime = 300.f;
	StaminaCostPerHit = 2.f;
	PromptText = LOCTEXT("GatherFiber", "Gather Fiber");
}

#undef LOCTEXT_NAMESPACE
