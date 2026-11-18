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
	for (int32 Index = 0; Index < Decorations.Num() && Index < DecorationColors.Num(); ++Index)
	{
		LITintPlaceholder(Decorations[Index], DecorationColors[Index]);
	}

	// Natural variety: size and yaw from the spawn position (identical on server and clients).
	if (bRandomizeLook)
	{
		const FVector Snapped = GetActorLocation().GridSnap(10.f);
		FRandomStream Look(static_cast<int32>(HashCombine(GetTypeHash(Snapped.X), GetTypeHash(Snapped.Y))));
		const float Size = Look.FRandRange(SizeVariation.X, SizeVariation.Y);
		SceneRoot->SetRelativeScale3D(FVector(Size));
		SceneRoot->SetRelativeRotation(FRotator(0.f, Look.FRandRange(0.f, 360.f), 0.f));
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
	SceneRoot->SetVisibility(!bDepleted, true); // mesh and decorations
	Mesh->SetCollisionEnabled(bDepleted ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
	BP_OnDepletedChanged(bDepleted);
}

// ---------------------------------------------------------------------------
// Presets (engine basic shapes until low-poly art exists)
// ---------------------------------------------------------------------------

UStaticMeshComponent* ALIResourceNode::AddDecoration(FName Name, const TCHAR* Shape, const FVector& Location, const FVector& Scale,
	const FLinearColor& Color, const FRotator& Rotation)
{
	UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	Part->SetupAttachment(SceneRoot);
	Part->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), Shape, Shape)));
	Part->SetRelativeLocation(Location);
	Part->SetRelativeRotation(Rotation);
	Part->SetRelativeScale3D(Scale);
	Part->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Part->SetCanEverAffectNavigation(false);
	Decorations.Add(Part);
	DecorationColors.Add(Color);
	return Part;
}


ALITreeNode::ALITreeNode()
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ShapeMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (ShapeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(ShapeMesh.Object);
	}
	Mesh->SetRelativeScale3D(FVector(0.35f, 0.35f, 2.8f));  // trunk: 35 cm wide, 280 cm tall
	Mesh->SetRelativeLocation(FVector(0.f, 0.f, 140.f));    // origin at the base
	// Low-poly pine: three stacked cones, darker at the bottom.
	AddDecoration(TEXT("Canopy1"), TEXT("Cone"), FVector(0.f, 0.f, 300.f), FVector(2.6f, 2.6f, 2.2f), FLinearColor(0.10f, 0.28f, 0.10f));
	AddDecoration(TEXT("Canopy2"), TEXT("Cone"), FVector(0.f, 0.f, 420.f), FVector(2.0f, 2.0f, 1.9f), FLinearColor(0.13f, 0.34f, 0.12f));
	AddDecoration(TEXT("Canopy3"), TEXT("Cone"), FVector(0.f, 0.f, 520.f), FVector(1.2f, 1.2f, 1.4f), FLinearColor(0.17f, 0.40f, 0.14f));

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
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ShapeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (ShapeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(ShapeMesh.Object);
	}
	// Faceted boulder: tilted cubes read as chipped low-poly rock.
	Mesh->SetRelativeScale3D(FVector(1.2f, 0.95f, 0.75f));
	Mesh->SetRelativeLocation(FVector(0.f, 0.f, 25.f));
	Mesh->SetRelativeRotation(FRotator(14.f, 20.f, 9.f));
	AddDecoration(TEXT("Chunk"), TEXT("Cube"), FVector(60.f, 40.f, 15.f), FVector(0.55f, 0.5f, 0.45f), FLinearColor(0.48f, 0.47f, 0.45f), FRotator(-20.f, 50.f, 25.f));
	AddDecoration(TEXT("Cap"), TEXT("Cube"), FVector(-15.f, -10.f, 62.f), FVector(0.7f, 0.6f, 0.3f), FLinearColor(0.66f, 0.64f, 0.60f), FRotator(8.f, -35.f, -12.f));

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
	Mesh->SetRelativeScale3D(FVector(0.5f, 0.5f, 0.9f));
	Mesh->SetRelativeLocation(FVector(0.f, 0.f, 45.f));
	// Tuft of tall grass: a few leaning cones.
	AddDecoration(TEXT("Blade1"), TEXT("Cone"), FVector(25.f, 10.f, 35.f), FVector(0.35f, 0.35f, 0.75f), FLinearColor(0.50f, 0.58f, 0.22f), FRotator(0.f, 0.f, 18.f));
	AddDecoration(TEXT("Blade2"), TEXT("Cone"), FVector(-20.f, 18.f, 32.f), FVector(0.35f, 0.35f, 0.7f), FLinearColor(0.60f, 0.66f, 0.28f), FRotator(15.f, 0.f, -12.f));
	AddDecoration(TEXT("Blade3"), TEXT("Cone"), FVector(0.f, -25.f, 30.f), FVector(0.3f, 0.3f, 0.65f), FLinearColor(0.45f, 0.55f, 0.20f), FRotator(-16.f, 0.f, 0.f));

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
	// Green bush dotted with red berries.
	const FVector BerrySpots[] = { {38.f, 10.f, 45.f}, {-30.f, 25.f, 50.f}, {5.f, -40.f, 42.f}, {-15.f, -20.f, 65.f}, {20.f, 30.f, 62.f} };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(BerrySpots); ++Index)
	{
		AddDecoration(*FString::Printf(TEXT("Berry%d"), Index), TEXT("Sphere"), BerrySpots[Index], FVector(0.14f), FLinearColor(0.75f, 0.06f, 0.10f));
	}

	// Hard difficulty: ~18 bushes x ~3 berries per 15 min ~= 430 food vs ~37 needed per player,
	// but bushes are spread out, so travel time (and its hunger/thirst cost) is the real price.
	ResourceItemId = LIItems::Berries;
	MinYield = 1;
	MaxYield = 2;
	HitsToDeplete = 2;
	RespawnTime = 900.f;
	StaminaCostPerHit = 1.f;
	PromptText = LOCTEXT("PickBerries", "Pick Berries");
	PlaceholderColor = FLinearColor(0.12f, 0.32f, 0.12f, 1.f); // leafy green (berries are decorations)
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
	bRandomizeLook = false; // they move: their yaw is the walking direction
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
	AddDecoration(TEXT("ClawL"), TEXT("Sphere"), FVector(28.f, -22.f, 12.f), FVector(0.16f, 0.12f, 0.1f), FLinearColor(0.90f, 0.35f, 0.15f));
	AddDecoration(TEXT("ClawR"), TEXT("Sphere"), FVector(28.f, 22.f, 12.f), FVector(0.16f, 0.12f, 0.1f), FLinearColor(0.90f, 0.35f, 0.15f));

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
	const FLinearColor Bristle(0.22f, 0.15f, 0.10f);
	AddDecoration(TEXT("Head"), TEXT("Sphere"), FVector(70.f, 0.f, 48.f), FVector(0.5f, 0.42f, 0.45f), Bristle);
	AddDecoration(TEXT("Snout"), TEXT("Cylinder"), FVector(98.f, 0.f, 40.f), FVector(0.2f, 0.2f, 0.12f), FLinearColor(0.45f, 0.30f, 0.26f), FRotator(90.f, 0.f, 0.f));
	AddDecoration(TEXT("TuskL"), TEXT("Cone"), FVector(95.f, -10.f, 34.f), FVector(0.05f, 0.05f, 0.15f), FLinearColor(0.85f, 0.82f, 0.72f), FRotator(-30.f, 0.f, 0.f));
	AddDecoration(TEXT("TuskR"), TEXT("Cone"), FVector(95.f, 10.f, 34.f), FVector(0.05f, 0.05f, 0.15f), FLinearColor(0.85f, 0.82f, 0.72f), FRotator(-30.f, 0.f, 0.f));
	for (int32 Leg = 0; Leg < 4; ++Leg)
	{
		const FVector LegPos((Leg < 2 ? 40.f : -40.f), (Leg % 2 == 0 ? -20.f : 20.f), 12.f);
		AddDecoration(*FString::Printf(TEXT("Leg%d"), Leg), TEXT("Cylinder"), LegPos, FVector(0.12f, 0.12f, 0.25f), Bristle);
	}

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
