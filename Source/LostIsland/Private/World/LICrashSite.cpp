// Copyright LOST ISLAND Team. All Rights Reserved.

#include "World/LICrashSite.h"

#include "Math/RandomStream.h"
#include "ProceduralMeshComponent.h"
#include "Visuals/LILowPolyMesh.h"
#include "Visuals/LIVisualAssets.h"
#include "Materials/MaterialInterface.h"
#include "Inventory/LIInventoryComponent.h"
#include "Items/LIItemTypes.h"
#include "LostIsland.h"
#include "World/LIIslandGenerator.h"
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

	// Supplies a small plane might have carried. ScrapMetal feeds the metal tools and flasks (not enough for all).
	LootTable =
	{
		FLILootEntry(LIItems::ScrapMetal,   2, 4, 4.f),
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

	SettleOnGround();

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

	BuildDetailedWreck();
}

void ALICrashSite::SettleOnGround()
{
	// The site is picked from the height function, but the rendered terrain (smooth style, finer grid) can rise
	// above that point under the nose and wings, burying most of the plane so only the tail showed. Sample the
	// real ground under the footprint (every machine sees the same terrain) and lift the wreck to sit on it.
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const FTransform Frame = GetActorTransform();
	const FVector2D Footprint[] = { {500.f, 0.f}, {250.f, 0.f}, {0.f, 0.f}, {-250.f, 0.f}, {-450.f, 0.f},
		{60.f, -320.f}, {60.f, -600.f}, {60.f, 220.f}, {-420.f, -150.f}, {-420.f, 150.f} };
	FCollisionQueryParams Params(SCENE_QUERY_STAT(LICrashSiteSettle), false, this);
	float HighestGround = -TNumericLimits<float>::Max();
	for (const FVector2D& Point : Footprint)
	{
		const FVector World2D = Frame.TransformPosition(FVector(Point.X, Point.Y, 0.f));
		FHitResult Hit;
		if (World->LineTraceSingleByObjectType(Hit, World2D + FVector(0.f, 0.f, 2000.f), World2D - FVector(0.f, 0.f, 2000.f),
			FCollisionObjectQueryParams(ECC_WorldStatic), Params) && Cast<ALIIslandGenerator>(Hit.GetActor()))
		{
			HighestGround = FMath::Max(HighestGround, Hit.ImpactPoint.Z);
		}
	}
	if (HighestGround > GetActorLocation().Z)
	{
		// Sit on the highest point with the hull slightly dug in (it still reads as a crash, not a parked plane).
		const float Lift = HighestGround - GetActorLocation().Z - 35.f;
		if (Lift > 0.f)
		{
			AddActorWorldOffset(FVector(0.f, 0.f, Lift));
			UE_LOG(LogLostIsland, Log, TEXT("Crash site lifted %.0f cm onto the terrain."), Lift);
		}
	}
}

void ALICrashSite::BuildDetailedWreck()
{
	// Cosmetic, every machine. The part shapes above stay as hidden collision. Coordinates match them:
	// WreckPivot space, X = forward along the fuselage, all sizes in cm.
	const FLinearColor Glass(0.05f, 0.07f, 0.10f);
	const FLinearColor Dark(0.03f, 0.03f, 0.03f);
	FRandomStream Random(LIVisualSeed(GetActorLocation(), 0xA1C7));

	FLILowPolyMesh Plane;
	Plane.bSmooth = true; // rounded hull even in the low-poly style
	const FRotator AlongX(-90.f, 0.f, 0.f); // frustum axis (+Z) -> +X

	// Fuselage: tapered tail cone, main cabin, engine cowling, spinner.
	Plane.AddFrustum(FTransform(AlongX, FVector(-450.f, 0.f, 10.f)), 14, 30.f, 80.f, 200.f, HullColor);
	Plane.AddFrustum(FTransform(AlongX, FVector(-250.f, 0.f, 0.f)), 14, 80.f, 80.f, 630.f, HullColor);
	Plane.AddFrustum(FTransform(AlongX, FVector(380.f, 0.f, 0.f)), 14, 80.f, 48.f, 110.f, StripeColor);
	Plane.AddFrustum(FTransform(AlongX, FVector(490.f, 0.f, 0.f)), 10, 22.f, 0.f, 40.f, Dark);

	// Bent propeller: one straight blade, one folded back by the impact.
	Plane.AddBox(FTransform(FRotator(0.f, 0.f, 10.f), FVector(505.f, 0.f, 55.f)), FVector(3.f, 9.f, 55.f), Dark);
	Plane.AddBox(FTransform(FRotator(-35.f, 0.f, 10.f), FVector(515.f, 0.f, -45.f)), FVector(3.f, 9.f, 50.f), Dark);

	// Red stripe along both sides.
	for (const float Side : { -1.f, 1.f })
	{
		Plane.AddBox(FTransform(FVector(60.f, Side * 79.5f, -10.f)), FVector(330.f, 1.f, 7.f), StripeColor);
		// Cabin windows.
		for (int32 Window = 0; Window < 4; ++Window)
		{
			Plane.AddBox(FTransform(FVector(230.f - Window * 70.f, Side * 79.f, 30.f)), FVector(22.f, 2.f, 14.f), Glass);
		}
	}
	// Windshield (two slanted panes).
	for (const float Side : { -1.f, 1.f })
	{
		Plane.AddBox(FTransform(FRotator(-35.f, Side * 18.f, 0.f), FVector(330.f, Side * 28.f, 62.f)), FVector(30.f, 26.f, 2.f), Glass);
	}

	// Left wing (intact, slightly bent), struts, aileron gap; right wing torn off at the stub.
	Plane.AddBox(FTransform(FRotator(0.f, 0.f, -3.f), FVector(60.f, -340.f, -20.f)), FVector(110.f, 280.f, 7.f), HullColor);
	Plane.AddBox(FTransform(FRotator(0.f, 0.f, -3.f), FVector(-48.f, -470.f, -20.f)), FVector(4.f, 130.f, 7.5f), Dark);
	Plane.AddFrustum(FTransform(FRotator(0.f, 0.f, 55.f), FVector(60.f, -75.f, -55.f)), 6, 4.f, 4.f, 230.f, Dark);
	Plane.AddBox(FTransform(FRotator(0.f, 8.f, 6.f), FVector(60.f, 190.f, -20.f)), FVector(110.f, 110.f, 7.f), BurntColor);
	for (int32 Shard = 0; Shard < 4; ++Shard)
	{
		const FVector Tip(60.f + Random.FRandRange(-90.f, 90.f), 300.f + Random.FRandRange(-10.f, 25.f), -20.f + Random.FRandRange(-8.f, 8.f));
		Plane.AddBox(FTransform(FRotator(Random.FRandRange(-25.f, 25.f), Random.FRandRange(-40.f, 40.f), Random.FRandRange(-30.f, 30.f)), Tip),
			FVector(Random.FRandRange(12.f, 25.f), Random.FRandRange(8.f, 18.f), 3.f), BurntColor);
	}

	// Tail: fin with rudder line, horizontal stabiliser.
	Plane.AddBox(FTransform(FVector(-410.f, 0.f, 140.f)), FVector(65.f, 6.f, 90.f), HullColor);
	Plane.AddBox(FTransform(FVector(-470.f, 0.f, 140.f)), FVector(5.f, 7.f, 85.f), StripeColor);
	Plane.AddBox(FTransform(FVector(-420.f, 0.f, 60.f)), FVector(50.f, 160.f, 5.f), HullColor);

	// Ground pieces (SceneRoot space): the torn wing and debris along the skid path.
	FLILowPolyMesh Ground;
	Ground.bSmooth = false;
	// Exactly over the collision shapes (engine cube = 100 cm, centred), so nothing invisible blocks the player...
	auto AddOverPart = [&Ground](const UStaticMeshComponent* Part, const FLinearColor& Color)
	{
		if (Part)
		{
			const FTransform Transform = Part->GetRelativeTransform();
			Ground.AddBox(FTransform(Transform.GetRotation(), Transform.GetLocation()), Transform.GetScale3D() * 50.f, Color);
		}
	};
	AddOverPart(TornWing, HullColor);
	for (const UStaticMeshComponent* Piece : Debris)
	{
		AddOverPart(Piece, BurntColor);
	}
	// ...plus small scattered scraps without collision.
	for (int32 Index = 0; Index < 8; ++Index)
	{
		const FVector Location(Random.FRandRange(-1500.f, -500.f), Random.FRandRange(-350.f, 450.f), Random.FRandRange(2.f, 5.f));
		const FRotator Rotation(Random.FRandRange(-20.f, 20.f), Random.FRandRange(0.f, 360.f), Random.FRandRange(-25.f, 25.f));
		Ground.AddBox(FTransform(Rotation, Location), FVector(Random.FRandRange(6.f, 18.f), Random.FRandRange(5.f, 12.f), 1.5f),
			Index % 3 == 0 ? HullColor : BurntColor);
	}

	const ULIVisualAssets& Assets = ULIVisualAssets::Get();
	UMaterialInterface* Metal = ULIVisualAssets::LoadOptional<UMaterialInterface>(Assets.MetalMaterial);
	UMaterialInterface* Rust = ULIVisualAssets::LoadOptional<UMaterialInterface>(Assets.RustMaterial);
	for (FLILowPolyMesh* Model : { &Plane, &Ground })
	{
		Model->SetSurfaceMaterial(HullColor, Metal);
		Model->SetSurfaceMaterial(BurntColor, Rust);
	}

	if (Plane.ApplyTo(LICreateVisualComponent(this, WreckPivot)) && Ground.ApplyTo(LICreateVisualComponent(this, SceneRoot)))
	{
		TArray<UStaticMeshComponent*> Parts;
		GetComponents<UStaticMeshComponent>(Parts);
		for (UStaticMeshComponent* Part : Parts)
		{
			Part->SetHiddenInGame(true); // collision stays
		}
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
