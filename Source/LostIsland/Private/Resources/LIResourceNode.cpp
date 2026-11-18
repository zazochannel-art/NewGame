// Copyright LOST ISLAND Team. All Rights Reserved.

#include "Resources/LIResourceNode.h"

#include "Inventory/LIInventoryComponent.h"
#include "Items/LIItemDefinition.h"
#include "Items/LIItemRegistrySubsystem.h"
#include "Items/LIItemTypes.h"
#include "LostIsland.h"
#include "Survival/LISurvivalComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
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

	// Cosmetic, every machine: untinted placeholder shapes read as near-black under the island's exposure.
	if (PlaceholderColor.A > 0.f)
	{
		LITintPlaceholder(Mesh, PlaceholderColor);
	}

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
		if (Survival->IsDead() || Survival->GetStatValue(ELISurvivalStat::Stamina) < GetStaminaCostFor(InstigatorPawn))
		{
			return false;
		}
	}
	return true;
}

const ULIItemDefinition* ALIResourceNode::FindToolFor(const APawn* Gatherer, int32* OutSlotIndex) const
{
	if (OutSlotIndex)
	{
		*OutSlotIndex = INDEX_NONE;
	}
	const ULIInventoryComponent* Inventory = Gatherer ? Gatherer->FindComponentByClass<ULIInventoryComponent>() : nullptr;
	const ULIItemRegistrySubsystem* Registry = ULIItemRegistrySubsystem::Get(this);
	if (!Inventory || !Registry || EffectiveTool == ELIToolType::None)
	{
		return nullptr;
	}
	const int32 SlotIndex = Inventory->FindBestToolSlot(EffectiveTool);
	if (SlotIndex == INDEX_NONE)
	{
		return nullptr;
	}
	if (OutSlotIndex)
	{
		*OutSlotIndex = SlotIndex;
	}
	return Registry->FindItem(Inventory->GetSlots()[SlotIndex].ItemId);
}

float ALIResourceNode::GetStaminaCostFor(const APawn* Gatherer) const
{
	const ULIItemDefinition* Tool = FindToolFor(Gatherer, nullptr);
	return StaminaCostPerHit * (Tool ? Tool->ToolStaminaMultiplier : 1.f);
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

	int32 ToolSlot = INDEX_NONE;
	const ULIItemDefinition* Tool = FindToolFor(InstigatorPawn, &ToolSlot);
	const float YieldMultiplier = Tool ? Tool->ToolYieldMultiplier : 1.f;

	const int32 Yield = FMath::Max(1, FMath::RoundToInt(FMath::RandRange(MinYield, MaxYield) * YieldMultiplier));
	const int32 Added = Inventory->AddItem(ResourceItemId, Yield);
	if (Added <= 0)
	{
		return; // Inventory full: no stamina spent, node untouched, tool not worn.
	}

	if (ULISurvivalComponent* Survival = InstigatorPawn->FindComponentByClass<ULISurvivalComponent>())
	{
		Survival->ModifyStat(ELISurvivalStat::Stamina, -StaminaCostPerHit * (Tool ? Tool->ToolStaminaMultiplier : 1.f));
	}
	if (Tool)
	{
		Inventory->ConsumeToolDurability(ToolSlot);
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
	const FText Base = FText::Format(LOCTEXT("PromptFormat", "{0}  ({1}/{2})"), PromptText, FText::AsNumber(RemainingHits), FText::AsNumber(HitsToDeplete));
	if (const ULIItemDefinition* Tool = FindToolFor(InstigatorPawn, nullptr))
	{
		return FText::Format(LOCTEXT("PromptWithTool", "{0}  with {1}"), Base, Tool->DisplayName);
	}
	return Base;
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

float ALIResourceNode::GetRespawnTimeRemaining() const
{
	const UWorld* World = GetWorld();
	return World && World->GetTimerManager().IsTimerActive(RespawnTimerHandle)
		? World->GetTimerManager().GetTimerRemaining(RespawnTimerHandle)
		: -1.f;
}

void ALIResourceNode::RestoreSavedState(int32 InRemainingHits, bool bInDepleted, float RespawnSecondsRemaining)
{
	UWorld* World = GetWorld();
	if (!HasAuthority() || !World)
	{
		return;
	}

	World->GetTimerManager().ClearTimer(RespawnTimerHandle);
	RemainingHits = FMath::Clamp(InRemainingHits, 0, HitsToDeplete);
	bDepleted = bInDepleted || RemainingHits == 0;

	if (bDepleted && RespawnTime > 0.f)
	{
		// Unknown countdown (-1) falls back to a full respawn time.
		const float Delay = RespawnSecondsRemaining >= 0.f ? FMath::Min(RespawnSecondsRemaining, RespawnTime) : RespawnTime;
		World->GetTimerManager().SetTimer(RespawnTimerHandle, this, &ALIResourceNode::Respawn, FMath::Max(0.1f, Delay), false);
	}
	ApplyDepletedState();
	ForceNetUpdate();
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
	EffectiveTool = ELIToolType::Axe;
	MinYield = 2;
	MaxYield = 3;
	HitsToDeplete = 6;
	RespawnTime = 600.f;
	StaminaCostPerHit = 6.f;
	PromptText = LOCTEXT("ChopTree", "Chop Tree");
	PlaceholderColor = FLinearColor(0.32f, 0.20f, 0.10f, 1.f); // bark brown
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
	EffectiveTool = ELIToolType::Pickaxe;
	MinYield = 1;
	MaxYield = 2;
	HitsToDeplete = 8;
	RespawnTime = 900.f;
	StaminaCostPerHit = 8.f;
	PromptText = LOCTEXT("MineRock", "Mine Rock");
	PlaceholderColor = FLinearColor(0.60f, 0.58f, 0.55f, 1.f); // grey stone
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
	PlaceholderColor = FLinearColor(0.55f, 0.62f, 0.25f, 1.f); // dry grass
}

ALIBerryBushNode::ALIBerryBushNode()
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ShapeMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (ShapeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(ShapeMesh.Object);
	}
	Mesh->SetRelativeScale3D(FVector(0.9f, 0.9f, 0.7f));
	Mesh->SetRelativeLocation(FVector(0.f, 0.f, 35.f));

	// Hard difficulty: ~18 bushes x ~3 berries per 15 min ~= 430 food vs ~37 needed per player,
	// but bushes are spread out, so travel time (and its hunger/thirst cost) is the real price.
	ResourceItemId = LIItems::Berries;
	MinYield = 1;
	MaxYield = 2;
	HitsToDeplete = 2;
	RespawnTime = 900.f;
	StaminaCostPerHit = 1.f;
	PromptText = LOCTEXT("PickBerries", "Pick Berries");
	PlaceholderColor = FLinearColor(0.70f, 0.08f, 0.10f, 1.f); // berry red
}

#undef LOCTEXT_NAMESPACE

// ---------------------------------------------------------------------------
// Crab
// ---------------------------------------------------------------------------

#define LOCTEXT_NAMESPACE "LIResourceNode"

ALICrabNode::ALICrabNode()
{
	PrimaryActorTick.bCanEverTick = true;
	SetReplicateMovement(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> ShapeMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (ShapeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(ShapeMesh.Object);
	}
	Mesh->SetRelativeScale3D(FVector(0.45f, 0.6f, 0.22f));
	Mesh->SetRelativeLocation(FVector(0.f, 0.f, 12.f));
	Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore); // players don't get stuck on crabs

	ResourceItemId = LIItems::RawMeat;
	MinYield = 1;
	MaxYield = 2;
	HitsToDeplete = 2;
	RespawnTime = 600.f;
	StaminaCostPerHit = 3.f;
	PromptText = LOCTEXT("CatchCrab", "Catch Crab");
	PlaceholderColor = FLinearColor(0.85f, 0.30f, 0.12f, 1.f); // crab orange
}

void ALICrabNode::BeginPlay()
{
	Super::BeginPlay();
	Home = GetActorLocation();
	WanderTarget = Home;
	SetActorTickEnabled(HasAuthority()); // movement is server-side and replicated
}

bool ALICrabNode::FindGround(const FVector& XY, FVector& OutGround) const
{
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(LICrabGround), false, this);
	const FVector Start(XY.X, XY.Y, Home.Z + 400.f);
	const FVector End(XY.X, XY.Y, Home.Z - 600.f);
	if (!GetWorld()->LineTraceSingleByObjectType(Hit, Start, End, FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		return false;
	}
	if (Hit.ImpactPoint.Z < 15.f) // the sea is at Z = 0: stay on the sand
	{
		return false;
	}
	OutGround = Hit.ImpactPoint;
	return true;
}

bool ALICrabNode::PickWanderTarget()
{
	for (int32 Attempt = 0; Attempt < 6; ++Attempt)
	{
		const FVector2D Offset = FMath::RandPointInCircle(WanderRadius);
		FVector Ground;
		if (FindGround(Home + FVector(Offset.X, Offset.Y, 0.f), Ground))
		{
			WanderTarget = Ground;
			return true;
		}
	}
	WanderTarget = Home;
	return false;
}

void ALICrabNode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (IsDepleted())
	{
		bWasDepleted = true;
		return;
	}
	if (bWasDepleted)
	{
		// Respawned: back home.
		bWasDepleted = false;
		SetActorLocation(Home);
		WanderTarget = Home;
	}

	// Run from the nearest player within FleeRadius, else wander with short pauses.
	FVector Location = GetActorLocation();
	FVector Desired = WanderTarget;
	float Speed = WanderSpeed;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APawn* Pawn = It->IsValid() ? (*It)->GetPawn() : nullptr;
		if (Pawn && FVector::DistSquared2D(Pawn->GetActorLocation(), Location) < FMath::Square(FleeRadius))
		{
			const FVector Away = (Location - Pawn->GetActorLocation()).GetSafeNormal2D();
			Desired = Location + Away * 200.f;
			Speed = FleeSpeed;
			PauseTime = 0.f;
			break;
		}
	}

	if (Speed == WanderSpeed)
	{
		if (PauseTime > 0.f)
		{
			PauseTime -= DeltaSeconds;
			return;
		}
		if (FVector::DistSquared2D(Location, WanderTarget) < FMath::Square(20.f))
		{
			PauseTime = FMath::FRandRange(1.f, 4.f);
			PickWanderTarget();
			return;
		}
	}

	const FVector Step = (Desired - Location).GetSafeNormal2D() * Speed * DeltaSeconds;
	FVector Ground;
	if (!FindGround(Location + Step, Ground) || FVector::DistSquared2D(Ground, Home) > FMath::Square(WanderRadius * 1.5f))
	{
		PickWanderTarget(); // water or too far from home: turn around
		return;
	}
	SetActorLocationAndRotation(Ground, FRotator(0.f, Step.Rotation().Yaw, 0.f));
}

#undef LOCTEXT_NAMESPACE
