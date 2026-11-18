// Copyright LOST ISLAND Team. All Rights Reserved.

#include "World/LICrashSite.h"

#include "Inventory/LIInventoryComponent.h"
#include "Items/LIItemTypes.h"
#include "LostIsland.h"
#include "Survival/LISurvivalComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "LICrashSite"

ALICrashSite::ALICrashSite()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* Cylinder = CylinderMesh.Object;
	UStaticMesh* Cone = ConeMesh.Object;
	UStaticMesh* Cube = CubeMesh.Object;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	// Nose-down, rolled onto the right side (the broken wing), slightly sunk into the ground.
	WreckPivot = CreateDefaultSubobject<USceneComponent>(TEXT("WreckPivot"));
	WreckPivot->SetupAttachment(SceneRoot);
	WreckPivot->SetRelativeLocation(FVector(0.f, 0.f, 55.f));
	WreckPivot->SetRelativeRotation(FRotator(-7.f, 0.f, 11.f));

	// Engine shapes are 100 cm and centred. X = forward along the fuselage. All sizes in cm.
	Fuselage      = MakePart(TEXT("Fuselage"),      WreckPivot, Cylinder, FVector(0.f, 0.f, 0.f),      FRotator(90.f, 0.f, 0.f),  FVector(1.6f, 1.6f, 9.f));
	Nose          = MakePart(TEXT("Nose"),          WreckPivot, Cone,     FVector(510.f, 0.f, 0.f),    FRotator(-90.f, 0.f, 0.f), FVector(1.6f, 1.6f, 1.3f));
	LeftWing      = MakePart(TEXT("LeftWing"),      WreckPivot, Cube,     FVector(60.f, -340.f, -20.f), FRotator(0.f, 0.f, -3.f), FVector(2.2f, 5.6f, 0.14f));
	RightWingStub = MakePart(TEXT("RightWingStub"), WreckPivot, Cube,     FVector(60.f, 190.f, -20.f), FRotator(0.f, 8.f, 6.f),   FVector(2.2f, 2.2f, 0.14f));
	TailFin       = MakePart(TEXT("TailFin"),       WreckPivot, Cube,     FVector(-410.f, 0.f, 140.f), FRotator(0.f, 0.f, 0.f),   FVector(1.3f, 0.14f, 1.8f));
	TailPlane     = MakePart(TEXT("TailPlane"),     WreckPivot, Cube,     FVector(-420.f, 0.f, 60.f),  FRotator(0.f, 0.f, 0.f),   FVector(1.0f, 3.2f, 0.1f));

	// Torn-off wing lying in the sand behind the plane (not tilted with the body).
	TornWing = MakePart(TEXT("TornWing"), SceneRoot, Cube, FVector(-260.f, 720.f, 12.f), FRotator(4.f, 38.f, 9.f), FVector(2.1f, 3.2f, 0.14f));

	// Scattered debris along the skid path.
	struct FDebrisSpec { FVector Location; FRotator Rotation; FVector Scale; };
	const FDebrisSpec DebrisSpecs[] =
	{
		{ FVector(-780.f, -160.f, 10.f), FRotator(12.f, 25.f, 40.f),  FVector(0.5f, 0.35f, 0.2f) },
		{ FVector(-1050.f, 120.f, 8.f),  FRotator(-20.f, 70.f, 10.f), FVector(0.35f, 0.6f, 0.15f) },
		{ FVector(-1350.f, -60.f, 6.f),  FRotator(5.f, 140.f, 30.f),  FVector(0.25f, 0.25f, 0.12f) },
		{ FVector(780.f, 260.f, 12.f),   FRotator(30.f, 10.f, -15.f), FVector(0.45f, 0.3f, 0.25f) },
		{ FVector(-600.f, 420.f, 8.f),   FRotator(0.f, 55.f, 20.f),   FVector(0.3f, 0.5f, 0.12f) },
	};
	for (int32 Index = 0; Index < static_cast<int32>(UE_ARRAY_COUNT(DebrisSpecs)); ++Index)
	{
		const FString Name = FString::Printf(TEXT("Debris%d"), Index);
		Debris.Add(MakePart(*Name, SceneRoot, Cube, DebrisSpecs[Index].Location, DebrisSpecs[Index].Rotation, DebrisSpecs[Index].Scale));
	}

	// Supplies a small plane might have carried. ScrapMetal is reserved for future recipes.
	LootTable =
	{
		FLILootEntry(LIItems::ScrapMetal,   2, 4, 3.f),
		FLILootEntry(LIItems::Fiber,        3, 6, 2.f),
		FLILootEntry(LIItems::FiberBandage, 1, 1, 2.f),
		FLILootEntry(LIItems::Berries,      2, 4, 2.f),
		FLILootEntry(LIItems::Wood,         2, 4, 1.f),
	};
}

UStaticMeshComponent* ALICrashSite::MakePart(const TCHAR* Name, USceneComponent* Parent, UStaticMesh* Mesh,
	const FVector& Location, const FRotator& Rotation, const FVector& Scale)
{
	UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	Part->SetupAttachment(Parent);
	if (Mesh)
	{
		Part->SetStaticMesh(Mesh);
	}
	Part->SetRelativeLocationAndRotation(Location, Rotation);
	Part->SetRelativeScale3D(Scale);
	Part->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	return Part;
}

void ALICrashSite::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ALICrashSite, SearchesLeft);
}

void ALICrashSite::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		SearchesLeft = MaxSearches;
	}

	// Tint the placeholder parts (engine BasicShapeMaterial exposes "Color"). One-time, ~13 MIDs.
	auto Tint = [](UStaticMeshComponent* Part, const FLinearColor& Color)
	{
		LITintPlaceholder(Part, Color);
	};
	Tint(Fuselage, HullColor);
	Tint(Nose, StripeColor);
	Tint(LeftWing, HullColor);
	Tint(RightWingStub, BurntColor);
	Tint(TailFin, StripeColor);
	Tint(TailPlane, HullColor);
	Tint(TornWing, HullColor);
	for (UStaticMeshComponent* Piece : Debris)
	{
		Tint(Piece, BurntColor);
	}
}

void ALICrashSite::SetSearchesLeft(int32 NewSearchesLeft)
{
	if (HasAuthority())
	{
		SearchesLeft = FMath::Clamp(NewSearchesLeft, 0, MaxSearches);
		ForceNetUpdate();
	}
}

const FLILootEntry* ALICrashSite::PickLoot(FRandomStream& Random) const
{
	float TotalWeight = 0.f;
	for (const FLILootEntry& Entry : LootTable)
	{
		TotalWeight += Entry.Weight;
	}
	if (TotalWeight <= 0.f)
	{
		return nullptr;
	}

	float Roll = Random.FRandRange(0.f, TotalWeight);
	for (const FLILootEntry& Entry : LootTable)
	{
		Roll -= Entry.Weight;
		if (Roll <= 0.f)
		{
			return &Entry;
		}
	}
	return &LootTable.Last();
}

bool ALICrashSite::CanInteract_Implementation(APawn* InstigatorPawn) const
{
	if (SearchesLeft <= 0 || !InstigatorPawn)
	{
		return false;
	}
	if (const ULISurvivalComponent* Survival = InstigatorPawn->FindComponentByClass<ULISurvivalComponent>())
	{
		return !Survival->IsDead() && Survival->GetStatValue(ELISurvivalStat::Stamina) >= StaminaCostPerSearch;
	}
	return true;
}

void ALICrashSite::Interact_Implementation(APawn* InstigatorPawn)
{
	if (!HasAuthority() || SearchesLeft <= 0 || !InstigatorPawn)
	{
		return;
	}

	ULIInventoryComponent* Inventory = InstigatorPawn->FindComponentByClass<ULIInventoryComponent>();
	if (!Inventory)
	{
		return;
	}

	// Loot is random per search (not seed-based): the wreck's position is the deterministic part.
	FRandomStream Random(FMath::Rand());
	int32 TotalAdded = 0;
	for (int32 Roll = 0; Roll < RollsPerSearch; ++Roll)
	{
		if (const FLILootEntry* Entry = PickLoot(Random))
		{
			const int32 Quantity = Random.RandRange(Entry->MinQuantity, FMath::Max(Entry->MinQuantity, Entry->MaxQuantity));
			TotalAdded += Inventory->AddItem(Entry->ItemId, Quantity);
		}
	}

	if (TotalAdded <= 0)
	{
		return; // inventory full: the search isn't used up
	}

	if (ULISurvivalComponent* Survival = InstigatorPawn->FindComponentByClass<ULISurvivalComponent>())
	{
		Survival->ModifyStat(ELISurvivalStat::Stamina, -StaminaCostPerSearch);
	}

	--SearchesLeft;
	BP_OnSearched(InstigatorPawn, SearchesLeft);
	UE_LOG(LogLostIsland, Log, TEXT("%s searched the crash site (%d searches left)."), *GetNameSafe(InstigatorPawn), SearchesLeft);
}

FText ALICrashSite::GetInteractionPrompt_Implementation(APawn* InstigatorPawn) const
{
	return FText::Format(LOCTEXT("Search", "Search Wreckage  ({0} left)"), FText::AsNumber(SearchesLeft));
}

void ALICrashSite::OnFocusBegin_Implementation(APawn* InstigatorPawn)
{
	TArray<UStaticMeshComponent*> Parts;
	GetComponents<UStaticMeshComponent>(Parts);
	for (UStaticMeshComponent* Part : Parts)
	{
		Part->SetRenderCustomDepth(true);
	}
}

void ALICrashSite::OnFocusEnd_Implementation(APawn* InstigatorPawn)
{
	TArray<UStaticMeshComponent*> Parts;
	GetComponents<UStaticMeshComponent>(Parts);
	for (UStaticMeshComponent* Part : Parts)
	{
		Part->SetRenderCustomDepth(false);
	}
}

#undef LOCTEXT_NAMESPACE
