// Copyright LOST ISLAND Team. All Rights Reserved.

#include "Resources/LIResourceNode.h"

#include "Inventory/LIInventoryComponent.h"
#include "Items/LIItemDefinition.h"
#include "Items/LIItemRegistrySubsystem.h"
#include "Items/LIItemTypes.h"
#include "LostIsland.h"
#include "Survival/LISurvivalComponent.h"
#include "Visuals/LILowPolyMesh.h"
#include "Visuals/LIVisualAssets.h"
#include "Math/RandomStream.h"
#include "ProceduralMeshComponent.h"

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

	DetailMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DetailMesh"));
	DetailMesh->SetupAttachment(SceneRoot);
	DetailMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DetailMesh->SetCanEverAffectNavigation(false);
	DetailMesh->SetGenerateOverlapEvents(false);
	DetailMesh->SetVisibility(false);

	Visual = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(SceneRoot);
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Visual->SetCanEverAffectNavigation(false);
	Visual->SetGenerateOverlapEvents(false);

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

	// Cosmetic, every machine. Real art if the project has it, else the code-built model; the placeholder shape
	// always stays as the hidden collision / interaction proxy.
	FRandomStream VisualRandom(LIVisualSeed(GetActorLocation(), static_cast<int32>(GetTypeHash(GetClass()->GetFName()))));
	if (TrySetupRealMesh(VisualRandom))
	{
		// Optional code-built extras on top of the real mesh (berries) go into Visual.
		Mesh->SetHiddenInGame(true);
		FLILowPolyMesh Overlay;
		const FBox FittedBox = DetailMesh->GetStaticMesh()->GetBoundingBox().TransformBy(DetailMesh->GetRelativeTransform());
		BuildRealMeshOverlay(Overlay, VisualRandom, FittedBox);
		Overlay.ApplyTo(Visual);
	}
	else
	{
		FLILowPolyMesh Model;
		BuildVisual(Model, VisualRandom);
		if (Model.ApplyTo(Visual))
		{
			Mesh->SetHiddenInGame(true);
		}
		else if (PlaceholderColor.A > 0.f)
		{
			// Untinted placeholder shapes read as near-black under the island's exposure.
			LITintPlaceholder(Mesh, PlaceholderColor);
		}
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
	Visual->SetRenderCustomDepth(true);
	DetailMesh->SetRenderCustomDepth(true);
}

void ALIResourceNode::OnFocusEnd_Implementation(APawn* InstigatorPawn)
{
	Mesh->SetRenderCustomDepth(false);
	Visual->SetRenderCustomDepth(false);
	DetailMesh->SetRenderCustomDepth(false);
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
	Visual->SetVisibility(!bDepleted, true);
	DetailMesh->SetVisibility(!bDepleted && DetailMesh->GetStaticMesh() != nullptr, true);
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
// Animals (crab, boar)
// ---------------------------------------------------------------------------

#define LOCTEXT_NAMESPACE "LIResourceNode"

ALIAnimalNode::ALIAnimalNode()
{
	PrimaryActorTick.bCanEverTick = true;
	SetReplicateMovement(true);
	Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore); // players do not get stuck on animals
}

void ALIAnimalNode::BeginPlay()
{
	Super::BeginPlay();
	Home = GetActorLocation();
	WanderTarget = Home;
	SetActorTickEnabled(HasAuthority()); // movement is server-side and replicated
}

bool ALIAnimalNode::FindGround(const FVector& XY, FVector& OutGround) const
{
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(LIAnimalGround), false, this);
	const FVector Start(XY.X, XY.Y, Home.Z + 600.f);
	const FVector End(XY.X, XY.Y, Home.Z - 800.f);
	if (!GetWorld()->LineTraceSingleByObjectType(Hit, Start, End, FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		return false;
	}
	if (Hit.ImpactPoint.Z < 15.f || Hit.ImpactNormal.Z < 0.6f)
	{
		return false; // the sea is at Z = 0; also avoid cliffs
	}
	OutGround = Hit.ImpactPoint;
	return true;
}

bool ALIAnimalNode::PickWanderTarget()
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

void ALIAnimalNode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (IsDepleted())
	{
		bWasDepleted = true;
		return;
	}
	if (bWasDepleted)
	{
		bWasDepleted = false; // respawned: back home
		SetActorLocation(Home);
		WanderTarget = Home;
	}
	AttackTimer = FMath::Max(0.f, AttackTimer - DeltaSeconds);

	// Nearest living player within ReactRadius.
	const FVector Location = GetActorLocation();
	APawn* Threat = nullptr;
	float ThreatDistSq = FMath::Square(ReactRadius);
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APawn* Pawn = It->IsValid() ? (*It)->GetPawn() : nullptr;
		const ULISurvivalComponent* Survival = Pawn ? Pawn->FindComponentByClass<ULISurvivalComponent>() : nullptr;
		const float DistSq = Pawn ? FVector::DistSquared2D(Pawn->GetActorLocation(), Location) : 0.f;
		if (Pawn && Survival && !Survival->IsDead() && DistSq < ThreatDistSq)
		{
			Threat = Pawn;
			ThreatDistSq = DistSq;
		}
	}

	FVector Desired = WanderTarget;
	float Speed = WanderSpeed;
	if (Threat)
	{
		PauseTime = 0.f;
		const FVector ToThreat = (Threat->GetActorLocation() - Location).GetSafeNormal2D();
		Speed = RunSpeed;
		if (!bAggressive)
		{
			Desired = Location - ToThreat * 200.f;
		}
		else if (ThreatDistSq <= FMath::Square(AttackRange))
		{
			// In range: face the player and bite on cooldown instead of pushing into them.
			SetActorRotation(FRotator(0.f, ToThreat.Rotation().Yaw, 0.f));
			if (AttackTimer <= 0.f)
			{
				AttackTimer = AttackCooldown;
				if (ULISurvivalComponent* Survival = Threat->FindComponentByClass<ULISurvivalComponent>())
				{
					Survival->ApplyDamage(AttackDamage);
					UE_LOG(LogLostIsland, Log, TEXT("%s bit %s for %.0f."), *GetName(), *Threat->GetName(), AttackDamage);
				}
			}
			return;
		}
		else
		{
			Desired = Threat->GetActorLocation();
		}
	}
	else
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
	const float Leash = (Threat && bAggressive) ? WanderRadius * 3.f : WanderRadius * 1.5f; // chasers stray further
	if (!FindGround(Location + Step, Ground) || FVector::DistSquared2D(Ground, Home) > FMath::Square(Leash))
	{
		PickWanderTarget(); // water, cliff or too far from home: turn around
		return;
	}
	SetActorLocationAndRotation(Ground, FRotator(0.f, Step.Rotation().Yaw, 0.f));
}

ALICrabNode::ALICrabNode()
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ShapeMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (ShapeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(ShapeMesh.Object);
	}
	Mesh->SetRelativeScale3D(FVector(0.45f, 0.6f, 0.22f));
	Mesh->SetRelativeLocation(FVector(0.f, 0.f, 12.f));

	ResourceItemId = LIItems::RawMeat;
	MinYield = 1;
	MaxYield = 2;
	HitsToDeplete = 2;
	RespawnTime = 600.f;
	StaminaCostPerHit = 3.f;
	PromptText = LOCTEXT("CatchCrab", "Catch Crab");
	PlaceholderColor = FLinearColor(0.85f, 0.30f, 0.12f, 1.f); // crab orange

	WanderSpeed = 70.f;
	RunSpeed = 200.f;
	ReactRadius = 400.f;
	WanderRadius = 600.f;
	bAggressive = false;
}

ALIBoarNode::ALIBoarNode()
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ShapeMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (ShapeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(ShapeMesh.Object);
	}
	Mesh->SetRelativeScale3D(FVector(1.3f, 0.7f, 0.75f)); // long body
	Mesh->SetRelativeLocation(FVector(0.f, 0.f, 40.f));

	ResourceItemId = LIItems::RawMeat;
	MinYield = 1;
	MaxYield = 2;
	HitsToDeplete = 6;
	RespawnTime = 1200.f;
	StaminaCostPerHit = 5.f;
	PromptText = LOCTEXT("FightBoar", "Fight Boar");
	PlaceholderColor = FLinearColor(0.25f, 0.17f, 0.12f, 1.f); // dark bristly brown

	WanderSpeed = 90.f;
	RunSpeed = 330.f; // slower than a sprint (650), faster than swimming (260)
	ReactRadius = 900.f;
	WanderRadius = 900.f;
	bAggressive = true;
	AttackDamage = 8.f;
	AttackRange = 170.f;
	AttackCooldown = 1.5f;
}

#undef LOCTEXT_NAMESPACE

// ---------------------------------------------------------------------------
// Low-poly models (cosmetic; built on every machine at BeginPlay)
// ---------------------------------------------------------------------------

namespace LIResourceVisuals
{
	const FLinearColor Bark(0.30f, 0.19f, 0.10f);
	const FLinearColor PineNeedles(0.11f, 0.33f, 0.17f);
	const FLinearColor Leaves(0.24f, 0.50f, 0.17f);
	const FLinearColor LeavesSunlit(0.40f, 0.64f, 0.22f);
	const FLinearColor Stone(0.52f, 0.50f, 0.47f);
	const FLinearColor Moss(0.36f, 0.47f, 0.25f);
	const FLinearColor DryGrass(0.62f, 0.66f, 0.28f);
	const FLinearColor GreenGrass(0.40f, 0.58f, 0.20f);
	const FLinearColor BushLeaves(0.17f, 0.42f, 0.16f);
	const FLinearColor Berry(0.78f, 0.06f, 0.14f);
	const FLinearColor CrabShell(0.86f, 0.32f, 0.12f);
	const FLinearColor CrabBelly(0.95f, 0.62f, 0.40f);
	const FLinearColor Black(0.03f, 0.03f, 0.03f);
}

void ALITreeNode::BuildVisual(FLILowPolyMesh& Out, FRandomStream& Random) const
{
	using namespace LIResourceVisuals;
	Out.GroundShadingHeight = 70.f; // darker where it meets the ground (realistic style)
	const float S = Random.FRandRange(0.85f, 1.2f);
	const FLinearColor TrunkColor = LIVaryColor(Bark, Random, 0.12f);

	if (Random.FRand() < 0.6f)
	{
		// Pine: short trunk and stacked, slightly rotated cones.
		Out.AddFrustum(FTransform(FVector(0.f, 0.f, -30.f)), 6, 24.f * S, 15.f * S, 190.f * S, TrunkColor, 0.1f, &Random);

		const int32 Tiers = Random.FRand() < 0.4f ? 4 : 3;
		float Z = 110.f * S;
		float Radius = 175.f * S;
		float Height = 210.f * S;
		for (int32 Tier = 0; Tier < Tiers; ++Tier)
		{
			const FLinearColor Needles = LIVaryColor(PineNeedles * (1.f + 0.14f * Tier), Random, 0.07f);
			const FTransform TierTransform(FRotator(0.f, Random.FRandRange(0.f, 360.f), 0.f), FVector(0.f, 0.f, Z));
			Out.AddFrustum(TierTransform, 7, Radius, 0.f, Height, Needles, 0.14f, &Random);
			Z += Height * 0.45f;
			Radius *= 0.74f;
			Height *= 0.86f;
		}
	}
	else
	{
		// Broadleaf: taller trunk, one side branch, a cluster of lumpy canopy blobs lit brighter on top.
		Out.AddFrustum(FTransform(FVector(0.f, 0.f, -30.f)), 7, 27.f * S, 16.f * S, 300.f * S, TrunkColor, 0.08f, &Random);
		const FTransform Branch(FRotator(0.f, Random.FRandRange(0.f, 360.f), 38.f), FVector(0.f, 0.f, 170.f * S));
		Out.AddFrustum(Branch, 5, 11.f * S, 6.f * S, 120.f * S, TrunkColor, 0.f, nullptr);

		const FLinearColor Leaf = LIVaryColor(Leaves, Random, 0.1f);
		const FLinearColor Lit = LIVaryColor(LeavesSunlit, Random, 0.08f);
		Out.AddBlob(FTransform(FVector(0.f, 0.f, 340.f * S)), FVector(150.f, 150.f, 115.f) * S, 0.18f, Random, Leaf, &Lit, 0.55f);

		const int32 Extra = Random.RandRange(2, 3);
		for (int32 Index = 0; Index < Extra; ++Index)
		{
			const float Angle = Random.FRandRange(0.f, 2.f * PI);
			const float Distance = Random.FRandRange(70.f, 110.f) * S;
			const FVector Offset(FMath::Cos(Angle) * Distance, FMath::Sin(Angle) * Distance, Random.FRandRange(260.f, 330.f) * S);
			const float R = Random.FRandRange(85.f, 115.f) * S;
			Out.AddBlob(FTransform(Offset), FVector(R, R, R * 0.8f), 0.2f, Random, Leaf, &Lit, 0.55f);
		}
	}
}

void ALIRockNode::BuildVisual(FLILowPolyMesh& Out, FRandomStream& Random) const
{
	using namespace LIResourceVisuals;
	Out.GroundShadingHeight = 40.f; // darker where it meets the ground (realistic style)
	const float S = Random.FRandRange(0.85f, 1.15f);
	const FLinearColor RockColor = LIVaryColor(Stone, Random, 0.1f);
	const FLinearColor MossColor = LIVaryColor(Moss, Random, 0.1f);
	const bool bMossy = Random.FRand() < 0.5f;

	// Main boulder matches the collision ellipsoid (70 x 60 x 45 cm radii, centre 35 cm up).
	Out.AddBlob(FTransform(FRotator(0.f, Random.FRandRange(0.f, 360.f), 0.f), FVector(0.f, 0.f, 30.f * S)),
		FVector(74.f, 63.f, 50.f) * S, 0.22f, Random, RockColor, bMossy ? &MossColor : nullptr, 0.78f);

	const int32 Pebbles = Random.RandRange(1, 3);
	for (int32 Index = 0; Index < Pebbles; ++Index)
	{
		const float Angle = Random.FRandRange(0.f, 2.f * PI);
		const float Distance = Random.FRandRange(80.f, 115.f) * S;
		const float R = Random.FRandRange(13.f, 24.f);
		Out.AddBlob(FTransform(FVector(FMath::Cos(Angle) * Distance, FMath::Sin(Angle) * Distance, R * 0.3f)),
			FVector(R, R * 0.9f, R * 0.65f), 0.25f, Random, LIVaryColor(RockColor, Random, 0.08f), nullptr, 0.7f, 0);
	}
}

void ALIFiberBushNode::BuildVisual(FLILowPolyMesh& Out, FRandomStream& Random) const
{
	using namespace LIResourceVisuals;
	Out.GroundShadingHeight = 35.f; // darker where it meets the ground (realistic style)

	// A clump of thin grass blades leaning outward, over a low leafy base.
	Out.AddBlob(FTransform(FVector(0.f, 0.f, 6.f)), FVector(34.f, 34.f, 16.f), 0.2f, Random, LIVaryColor(GreenGrass, Random, 0.1f), nullptr, 0.7f, 0);

	const int32 Blades = Random.RandRange(10, 15);
	for (int32 Index = 0; Index < Blades; ++Index)
	{
		const float Yaw = 360.f * Index / Blades + Random.FRandRange(-14.f, 14.f);
		const float Lean = Random.FRandRange(8.f, 34.f);
		const float Height = Random.FRandRange(60.f, 110.f);
		const FVector Base(FMath::Cos(FMath::DegreesToRadians(Yaw)) * 10.f, FMath::Sin(FMath::DegreesToRadians(Yaw)) * 10.f, -4.f);
		const FLinearColor BladeColor = LIVaryColor(FMath::Lerp(DryGrass, GreenGrass, Random.FRand()), Random, 0.08f);
		Out.AddFrustum(FTransform(FRotator(-Lean, Yaw, 0.f), Base), 3, 6.f, 0.f, Height, BladeColor);
	}
}

void ALIBerryBushNode::BuildVisual(FLILowPolyMesh& Out, FRandomStream& Random) const
{
	using namespace LIResourceVisuals;
	Out.GroundShadingHeight = 35.f; // darker where it meets the ground (realistic style)
	const FLinearColor LeafColor = LIVaryColor(BushLeaves, Random, 0.1f);
	const FLinearColor LeafLit = LIVaryColor(BushLeaves * 1.35f, Random, 0.06f);

	const FVector MainCenter(0.f, 0.f, 40.f);
	const FVector MainRadii(56.f, 56.f, 42.f);
	Out.AddBlob(FTransform(MainCenter), MainRadii, 0.18f, Random, LeafColor, &LeafLit, 0.6f);
	for (int32 Index = 0; Index < 2; ++Index)
	{
		const float Angle = Random.FRandRange(0.f, 2.f * PI);
		const float R = Random.FRandRange(34.f, 44.f);
		Out.AddBlob(FTransform(FVector(FMath::Cos(Angle) * 38.f, FMath::Sin(Angle) * 38.f, 30.f)), FVector(R, R, R * 0.8f),
			0.2f, Random, LeafColor, &LeafLit, 0.6f);
	}

	// Berries dotted over the upper part of the bush.
	const int32 Berries = Random.RandRange(12, 18);
	for (int32 Index = 0; Index < Berries; ++Index)
	{
		const float Yaw = Random.FRandRange(0.f, 2.f * PI);
		const float FromTop = Random.FRandRange(0.25f, 1.35f); // radians from straight up
		const FVector Dir(FMath::Sin(FromTop) * FMath::Cos(Yaw), FMath::Sin(FromTop) * FMath::Sin(Yaw), FMath::Cos(FromTop));
		const FVector Position = MainCenter + FVector(Dir.X * MainRadii.X, Dir.Y * MainRadii.Y, Dir.Z * MainRadii.Z) * 1.02f;
		const float R = Random.FRandRange(5.5f, 8.f);
		Out.AddBlob(FTransform(Position), FVector(R), 0.1f, Random, LIVaryColor(Berry, Random, 0.12f), nullptr, 0.7f, 0);
	}
}

void ALICrabNode::BuildVisual(FLILowPolyMesh& Out, FRandomStream& Random) const
{
	using namespace LIResourceVisuals;
	const FLinearColor Shell = LIVaryColor(CrabShell, Random, 0.08f);
	const FLinearColor Dark(Shell.R * 0.72f, Shell.G * 0.72f, Shell.B * 0.72f, 1.f);

	// Body: a wide, flat shell (forward = +X, matching the movement direction), paler underneath.
	Out.AddBlob(FTransform(FVector(0.f, 0.f, 12.f)), FVector(20.f, 27.f, 9.f), 0.1f, Random, CrabBelly, &Shell, -0.2f);

	for (int32 SideIndex = 0; SideIndex < 2; ++SideIndex)
	{
		const float Side = SideIndex == 0 ? 1.f : -1.f;

		// Three legs per side, angled down to the sand.
		for (int32 Leg = 0; Leg < 3; ++Leg)
		{
			const float X = -11.f + Leg * 9.f;
			Out.AddBox(FTransform(FRotator(0.f, Side * (Leg - 1) * 12.f, Side * -28.f), FVector(X, Side * 31.f, 6.f)),
				FVector(2.2f, 9.f, 1.8f), Dark);
		}

		// Arm and pincer in front.
		Out.AddBox(FTransform(FRotator(0.f, Side * 28.f, 0.f), FVector(21.f, Side * 15.f, 11.f)), FVector(7.f, 2.4f, 2.4f), Dark);
		Out.AddBlob(FTransform(FVector(29.f, Side * 20.f, 11.f)), FVector(7.5f, 5.f, 4.f), 0.1f, Random, Shell, nullptr, 0.7f, 0);

		// Eye stalks.
		Out.AddBox(FTransform(FVector(15.f, Side * 6.f, 21.f)), FVector(1.3f, 1.3f, 3.5f), Dark);
		Out.AddBlob(FTransform(FVector(15.f, Side * 6.f, 25.5f)), FVector(2.6f), 0.f, Random, Black, nullptr, 0.7f, 0);
	}
}

// ---------------------------------------------------------------------------
// Real art (ULIVisualAssets): fitted to the node's size, code models as fallback
// ---------------------------------------------------------------------------

bool ALIResourceNode::TrySetupRealMesh(FRandomStream& Random)
{
	float TargetSize = 100.f;
	bool bFitHeight = false;
	const TArray<FSoftObjectPath>* Paths = GetRealMeshPaths(TargetSize, bFitHeight);
	if (!Paths || Paths->Num() == 0)
	{
		return false;
	}
	UStaticMesh* RealMesh = ULIVisualAssets::PickOptional<UStaticMesh>(*Paths, Random.RandRange(0, 1000));
	if (!RealMesh)
	{
		return false;
	}

	const FBox Bounds = RealMesh->GetBoundingBox();
	const FVector Size = Bounds.GetSize();
	const float Measured = bFitHeight ? Size.Z : FMath::Max(Size.X, Size.Y);
	if (Measured <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const float Scale = TargetSize / Measured * Random.FRandRange(0.85f, 1.15f);
	const float Yaw = Random.FRandRange(0.f, 360.f);
	// Stand the mesh's lowest point on the node origin (the ground), slightly sunk to hide gaps on slopes.
	const FVector Location(0.f, 0.f, -Bounds.Min.Z * Scale - 6.f);

	DetailMesh->SetStaticMesh(RealMesh);
	DetailMesh->SetRelativeTransform(FTransform(FRotator(0.f, Yaw, 0.f), Location, FVector(Scale)));
	DetailMesh->SetVisibility(true);
	return true;
}

const TArray<FSoftObjectPath>* ALITreeNode::GetRealMeshPaths(float& OutTargetSize, bool& bOutFitHeight) const
{
	OutTargetSize = 750.f; // ~7.5 m tall
	bOutFitHeight = true;
	return &ULIVisualAssets::Get().TreeMeshes;
}

const TArray<FSoftObjectPath>* ALIRockNode::GetRealMeshPaths(float& OutTargetSize, bool& bOutFitHeight) const
{
	OutTargetSize = 170.f; // matches the collision ellipsoid's width
	bOutFitHeight = false;
	return &ULIVisualAssets::Get().RockMeshes;
}

const TArray<FSoftObjectPath>* ALIFiberBushNode::GetRealMeshPaths(float& OutTargetSize, bool& bOutFitHeight) const
{
	OutTargetSize = 130.f;
	bOutFitHeight = false;
	return &ULIVisualAssets::Get().BushMeshes;
}

const TArray<FSoftObjectPath>* ALIBerryBushNode::GetRealMeshPaths(float& OutTargetSize, bool& bOutFitHeight) const
{
	OutTargetSize = 140.f;
	bOutFitHeight = false;
	return &ULIVisualAssets::Get().BushMeshes;
}

void ALIBerryBushNode::BuildRealMeshOverlay(FLILowPolyMesh& Out, FRandomStream& Random, const FBox& FittedBox) const
{
	// Red berries on the upper part of the real bush, so berry bushes stay easy to spot.
	const FVector Center = FittedBox.GetCenter();
	const FVector Radii = FittedBox.GetExtent() * 0.92f;
	const int32 Berries = Random.RandRange(16, 24);
	for (int32 Index = 0; Index < Berries; ++Index)
	{
		const float Yaw = Random.FRandRange(0.f, 2.f * PI);
		const float FromTop = Random.FRandRange(0.3f, 1.4f);
		const FVector Dir(FMath::Sin(FromTop) * FMath::Cos(Yaw), FMath::Sin(FromTop) * FMath::Sin(Yaw), FMath::Cos(FromTop));
		const FVector Position = Center + FVector(Dir.X * Radii.X, Dir.Y * Radii.Y, Dir.Z * Radii.Z);
		const float R = Random.FRandRange(4.5f, 6.5f);
		Out.AddBlob(FTransform(Position), FVector(R), 0.1f, Random, LIVaryColor(LIResourceVisuals::Berry, Random, 0.12f), nullptr, 0.7f, 0);
	}
}

