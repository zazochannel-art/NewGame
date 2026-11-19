// Copyright LOST ISLAND Team. All Rights Reserved.

#include "World/LIBuildingPiece.h"

#include "LostIsland.h"
#include "Inventory/LIInventoryComponent.h"
#include "Items/LIItemRegistrySubsystem.h"
#include "Items/LIItemTypes.h"
#include "GameFramework/Pawn.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"
#include "Math/RandomStream.h"
#include "ProceduralMeshComponent.h"
#include "Visuals/LILowPolyMesh.h"
#include "Visuals/LIVisualAssets.h"
#include "Materials/MaterialInterface.h"

namespace LIBuildingVisuals
{
	// Two shades each, so neighbouring planks differ; each maps to a real wood material when available.
	const FLinearColor PlankA(0.56f, 0.39f, 0.21f);
	const FLinearColor PlankB(0.48f, 0.33f, 0.17f);
	const FLinearColor BeamA(0.32f, 0.21f, 0.11f);
	const FLinearColor BeamB(0.27f, 0.18f, 0.09f);

	/** A log lying along local +Y from Y0, centred at (X, Z). */
	void AddLog(FLILowPolyMesh& Out, FRandomStream& Random, float X, float Y0, float Z, float Length, float Radius, const FLinearColor& Color)
	{
		const float R = Radius * Random.FRandRange(0.93f, 1.07f);
		Out.AddFrustum(FTransform(FRotator(0.f, 0.f, -90.f), FVector(X, Y0, Z)), 8, R, R * Random.FRandRange(0.9f, 1.0f), Length, Color, 0.04f, &Random);
	}
}

namespace
{
	/** The engine cube is 100 cm with a centred pivot; pieces are placed by their bottom centre. */
	void SetupBox(UStaticMeshComponent* Mesh, const FVector& SizeCm)
	{
		Mesh->SetRelativeScale3D(SizeCm / 100.f);
		Mesh->SetRelativeLocation(FVector(0.f, 0.f, SizeCm.Z * 0.5f));
	}

	float SnapYaw(float Yaw)
	{
		return FMath::RoundToFloat(Yaw / 90.f) * 90.f;
	}

	/** Same type already at (about) this spot. Two walls only collide if they are parallel. */
	bool IsOccupied(const UWorld* World, ELIBuildingPieceType Type, const FVector& Location, const FRotator& Rotation)
	{
		for (TActorIterator<ALIBuildingPiece> It(World); It; ++It)
		{
			if (It->IsPreview() || It->GetPieceType() != Type || FVector::DistSquared(It->GetActorLocation(), Location) > FMath::Square(30.f))
			{
				continue;
			}
			if (Type != ELIBuildingPieceType::Wall)
			{
				return true;
			}
			const float YawDelta = FMath::Fmod(FMath::Abs(It->GetActorRotation().Yaw - Rotation.Yaw) + 1.f, 180.f);
			if (YawDelta < 2.f)
			{
				return true;
			}
		}
		return false;
	}
}

ALIBuildingPiece::ALIBuildingPiece()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	Mesh->SetStaticMesh(CubeMesh.Object);
	Mesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
}

void ALIBuildingPiece::BeginPlay()
{
	Super::BeginPlay();
	const FLinearColor Color = bIsPreview ? FLinearColor(0.15f, 0.75f, 0.25f) : WoodColor; // preview: green ghost
	LITintPlaceholder(Mesh, Color);
	for (UStaticMeshComponent* Part : ExtraParts)
	{
		LITintPlaceholder(Part, Color);
	}

	// Detailed wooden model (cosmetic, every machine); the boxes stay as hidden collision.
	if (!bIsPreview)
	{
		FLILowPolyMesh Model;
		Model.bSmooth = false; // sawn timber: keep crisp edges
		FRandomStream Random(LIVisualSeed(GetActorLocation(), static_cast<int32>(PieceType) + 77));
		BuildVisual(Model, Random);
		if (!Model.IsEmpty())
		{
			const ULIVisualAssets& Assets = ULIVisualAssets::Get();
			UMaterialInterface* Wood = ULIVisualAssets::LoadOptional<UMaterialInterface>(Assets.WoodMaterial);
			UMaterialInterface* DarkWood = ULIVisualAssets::LoadOptional<UMaterialInterface>(Assets.DarkWoodMaterial);
			Model.SetSurfaceMaterial(LIBuildingVisuals::PlankA, Wood);
			Model.SetSurfaceMaterial(LIBuildingVisuals::PlankB, Wood);
			Model.SetSurfaceMaterial(LIBuildingVisuals::BeamA, DarkWood);
			Model.SetSurfaceMaterial(LIBuildingVisuals::BeamB, DarkWood);
			if (Model.ApplyTo(LICreateVisualComponent(this)))
			{
				Mesh->SetHiddenInGame(true);
				for (UStaticMeshComponent* Part : ExtraParts)
				{
					Part->SetHiddenInGame(true);
				}
			}
		}
	}
}

ALIBuildingPiece* ALIBuildingPiece::SpawnPreview(UWorld* World, TSubclassOf<ALIBuildingPiece> PieceClass)
{
	if (!World || !PieceClass)
	{
		return nullptr;
	}
	ALIBuildingPiece* Preview = World->SpawnActorDeferred<ALIBuildingPiece>(PieceClass, FTransform::Identity, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Preview)
	{
		return nullptr;
	}
	Preview->bIsPreview = true;
	Preview->SetReplicates(false); // never sent to clients, even on a listen server
	Preview->SetActorEnableCollision(false);
	Preview->FinishSpawning(FTransform::Identity);
	Preview->SetActorHiddenInGame(true);
	return Preview;
}

bool ALIBuildingPiece::CanInteract_Implementation(APawn* InstigatorPawn) const
{
	return !bIsPreview && !ItemId.IsNone() && InstigatorPawn;
}

FText ALIBuildingPiece::GetInteractionPrompt_Implementation(APawn* InstigatorPawn) const
{
	const ULIItemRegistrySubsystem* Registry = ULIItemRegistrySubsystem::Get(this);
	return FText::Format(NSLOCTEXT("LIBuildingPiece", "Dismantle", "Dismantle {0}"),
		Registry ? Registry->GetDisplayName(ItemId) : FText::FromName(ItemId));
}

void ALIBuildingPiece::Interact_Implementation(APawn* InstigatorPawn)
{
	ULIInventoryComponent* Inventory = InstigatorPawn ? InstigatorPawn->FindComponentByClass<ULIInventoryComponent>() : nullptr;
	if (!HasAuthority() || bIsPreview || !Inventory)
	{
		return;
	}
	if (Inventory->AddItem(ItemId, 1) == 1) // full inventory: nothing happens (the inventory shows why)
	{
		UE_LOG(LogLostIsland, Log, TEXT("%s dismantled %s."), *GetNameSafe(InstigatorPawn), *GetName());
		Destroy();
	}
}

ALIWoodenFloor::ALIWoodenFloor()
{
	PieceType = ELIBuildingPieceType::Floor;
	ItemId = LIItems::WoodenFloor;
	SetupBox(Mesh, FVector(GridSize, GridSize, FloorThickness));
}

ALIWoodenWall::ALIWoodenWall()
{
	PieceType = ELIBuildingPieceType::Wall;
	ItemId = LIItems::WoodenWall;
	SetupBox(Mesh, FVector(20.f, GridSize, WallHeight)); // thin along local X
	WoodColor = FLinearColor(0.50f, 0.33f, 0.17f);
}

ALIWoodenDoorway::ALIWoodenDoorway()
{
	PieceType = ELIBuildingPieceType::Wall;
	ItemId = LIItems::WoodenDoorway;
	WoodColor = FLinearColor(0.50f, 0.33f, 0.17f);

	// Left post = Mesh, right post, lintel: a 100 cm wide, 220 cm high opening in the middle.
	const float PostWidth = (GridSize - 100.f) * 0.5f;
	Mesh->SetRelativeScale3D(FVector(0.2f, PostWidth / 100.f, WallHeight / 100.f));
	Mesh->SetRelativeLocation(FVector(0.f, -(50.f + PostWidth * 0.5f), WallHeight * 0.5f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	RightPost = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightPost"));
	RightPost->SetupAttachment(GetRootComponent());
	RightPost->SetStaticMesh(CubeMesh.Object);
	RightPost->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	RightPost->SetRelativeScale3D(Mesh->GetRelativeScale3D());
	RightPost->SetRelativeLocation(FVector(0.f, 50.f + PostWidth * 0.5f, WallHeight * 0.5f));

	Lintel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Lintel"));
	Lintel->SetupAttachment(GetRootComponent());
	Lintel->SetStaticMesh(CubeMesh.Object);
	Lintel->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Lintel->SetRelativeScale3D(FVector(0.2f, 1.f, (WallHeight - 220.f) / 100.f));
	Lintel->SetRelativeLocation(FVector(0.f, 0.f, 220.f + (WallHeight - 220.f) * 0.5f));

	ExtraParts = { RightPost, Lintel };
}

ALIWoodenRoof::ALIWoodenRoof()
{
	PieceType = ELIBuildingPieceType::Roof;
	ItemId = LIItems::WoodenRoof;
	SetupBox(Mesh, FVector(GridSize + 20.f, GridSize + 20.f, 15.f)); // small overhang
	WoodColor = FLinearColor(0.33f, 0.22f, 0.12f);
}

bool ALIBuildingPiece::ComputePlacement(const UWorld* World, TSubclassOf<ALIBuildingPiece> PieceClass,
	const FVector& BuilderLocation, const FVector& BuilderForward, const AActor* Builder, FTransform& OutTransform)
{
	const ALIBuildingPiece* Defaults = PieceClass ? PieceClass->GetDefaultObject<ALIBuildingPiece>() : nullptr;
	if (!World || !Defaults)
	{
		return false;
	}
	const ELIBuildingPieceType Type = Defaults->PieceType;
	const FVector Forward = FVector(BuilderForward.X, BuilderForward.Y, 0.f).GetSafeNormal();
	const FVector Target = BuilderLocation + Forward * (GridSize * 0.8f);

	// The nearest floor anchors the grid.
	const ALIBuildingPiece* Anchor = nullptr;
	float BestDistSq = FMath::Square(GridSize * 2.f);
	for (TActorIterator<ALIBuildingPiece> It(World); It; ++It)
	{
		const float DistSq = FVector::DistSquared2D(It->GetActorLocation(), Target);
		if (!It->bIsPreview && It->PieceType == ELIBuildingPieceType::Floor && DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Anchor = *It;
		}
	}

	if (Type == ELIBuildingPieceType::Floor && !Anchor)
	{
		// First floor: on the ground in front of the builder, above sea level, not too steep.
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(LIBuildFloor), false, Builder);
		if (!World->LineTraceSingleByObjectType(Hit, Target + FVector(0.f, 0.f, 200.f), Target - FVector(0.f, 0.f, 600.f),
			FCollisionObjectQueryParams(ECC_WorldStatic), Params)
			|| Hit.ImpactPoint.Z < 0.f || Hit.ImpactNormal.Z < 0.75f)
		{
			return false;
		}
		OutTransform = FTransform(FRotator(0.f, SnapYaw(Forward.Rotation().Yaw), 0.f), Hit.ImpactPoint);
		return true;
	}
	if (!Anchor)
	{
		return false; // walls and roofs need a floor
	}

	// Grid cell (in the anchor's frame) closest to the target.
	const FTransform Grid = Anchor->GetActorTransform();
	const FVector Local = Grid.InverseTransformPosition(Target);
	const FVector Cell(FMath::RoundToFloat(Local.X / GridSize) * GridSize, FMath::RoundToFloat(Local.Y / GridSize) * GridSize, 0.f);

	FVector LocalPos = Cell;
	FRotator Rotation(0.f, Anchor->GetActorRotation().Yaw, 0.f);
	switch (Type)
	{
	case ELIBuildingPieceType::Wall:
	{
		// Edge of that cell nearest the target; walls stand on top of the floor.
		const FVector Offset = Local - Cell;
		if (FMath::Abs(Offset.X) >= FMath::Abs(Offset.Y))
		{
			LocalPos.X += (Offset.X < 0.f ? -1.f : 1.f) * GridSize * 0.5f;
		}
		else
		{
			LocalPos.Y += (Offset.Y < 0.f ? -1.f : 1.f) * GridSize * 0.5f;
			Rotation.Yaw += 90.f;
		}
		LocalPos.Z = FloorThickness;
		break;
	}
	case ELIBuildingPieceType::Roof:
		LocalPos.Z = FloorThickness + WallHeight;
		break;
	case ELIBuildingPieceType::Floor:
	default:
		break;
	}

	const FVector WorldPos = Grid.TransformPosition(LocalPos);
	if (IsOccupied(World, Type, WorldPos, Rotation))
	{
		return false;
	}
	OutTransform = FTransform(Rotation, WorldPos);
	return true;
}

bool ALIBuildingPiece::IsSheltered(const UWorld* World, const FVector& Location)
{
	if (!World)
	{
		return false;
	}
	for (TActorIterator<ALIBuildingPiece> It(World); It; ++It)
	{
		if (It->bIsPreview || It->PieceType != ELIBuildingPieceType::Roof)
		{
			continue;
		}
		const FVector Local = It->GetActorTransform().InverseTransformPosition(Location);
		if (FMath::Abs(Local.X) <= GridSize * 0.5f + 10.f && FMath::Abs(Local.Y) <= GridSize * 0.5f + 10.f
			&& Local.Z < 0.f && Local.Z > -WallHeight * 2.f)
		{
			return true;
		}
	}
	return false;
}

// ---------------------------------------------------------------------------
// Detailed models (cosmetic). Pieces are placed by their bottom centre; walls are thin along local X.
// ---------------------------------------------------------------------------

void ALIWoodenFloor::BuildVisual(FLILowPolyMesh& Out, FRandomStream& Random) const
{
	using namespace LIBuildingVisuals;
	const float Half = GridSize * 0.5f;

	// Three joists underneath, ten planks on top with small gaps.
	for (int32 Index = -1; Index <= 1; ++Index)
	{
		Out.AddBox(FTransform(FVector(Index * 120.f, 0.f, 6.f)), FVector(8.f, Half, 6.f), Index == 0 ? BeamA : BeamB);
	}
	const int32 Planks = 10;
	const float PlankWidth = GridSize / Planks;
	for (int32 Index = 0; Index < Planks; ++Index)
	{
		const float Y = -Half + PlankWidth * (Index + 0.5f);
		const float Length = Half - Random.FRandRange(0.f, 3.f);
		const FVector Center(Random.FRandRange(-1.5f, 1.5f), Y, FloorThickness - 4.f + Random.FRandRange(-0.4f, 0.4f));
		Out.AddBox(FTransform(FRotator(0.f, Random.FRandRange(-0.4f, 0.4f), 0.f), Center), FVector(Length, PlankWidth * 0.5f - 0.8f, 4.f),
			(Index % 2) ? PlankA : PlankB);
	}
}

void ALIWoodenWall::BuildVisual(FLILowPolyMesh& Out, FRandomStream& Random) const
{
	using namespace LIBuildingVisuals;
	// Ten stacked logs, overhanging the ends a little like a log cabin.
	const int32 Logs = 10;
	const float Radius = WallHeight / Logs * 0.5f;
	for (int32 Index = 0; Index < Logs; ++Index)
	{
		AddLog(Out, Random, 0.f, -GridSize * 0.5f - 10.f, Radius + Index * Radius * 2.f, GridSize + 20.f, Radius, (Index % 2) ? PlankA : PlankB);
	}
}

void ALIWoodenDoorway::BuildVisual(FLILowPolyMesh& Out, FRandomStream& Random) const
{
	using namespace LIBuildingVisuals;
	const float Half = GridSize * 0.5f;
	const float OpeningHalf = 50.f;
	const float OpeningTop = 220.f;
	const int32 Logs = 10;
	const float Radius = WallHeight / Logs * 0.5f;
	for (int32 Index = 0; Index < Logs; ++Index)
	{
		const float Z = Radius + Index * Radius * 2.f;
		const FLinearColor& Color = (Index % 2) ? PlankA : PlankB;
		if (Z - Radius < OpeningTop)
		{
			// Split around the opening.
			AddLog(Out, Random, 0.f, -Half - 10.f, Z, Half + 10.f - OpeningHalf, Radius, Color);
			AddLog(Out, Random, 0.f, OpeningHalf, Z, Half + 10.f - OpeningHalf, Radius, Color);
		}
		else
		{
			AddLog(Out, Random, 0.f, -Half - 10.f, Z, GridSize + 20.f, Radius, Color);
		}
	}

	// Door frame: two jambs and a header board.
	Out.AddBox(FTransform(FVector(0.f, -OpeningHalf - 4.f, OpeningTop * 0.5f)), FVector(Radius + 2.f, 4.f, OpeningTop * 0.5f), BeamA);
	Out.AddBox(FTransform(FVector(0.f, OpeningHalf + 4.f, OpeningTop * 0.5f)), FVector(Radius + 2.f, 4.f, OpeningTop * 0.5f), BeamA);
	Out.AddBox(FTransform(FVector(0.f, 0.f, OpeningTop + 6.f)), FVector(Radius + 2.f, OpeningHalf + 8.f, 6.f), BeamB);
}

void ALIWoodenRoof::BuildVisual(FLILowPolyMesh& Out, FRandomStream& Random) const
{
	using namespace LIBuildingVisuals;
	const float Half = GridSize * 0.5f + 10.f; // matches the 20 cm overhang of the collision box

	// Rafters underneath, then overlapping rows of shingle boards.
	for (int32 Index = -1; Index <= 1; ++Index)
	{
		Out.AddBox(FTransform(FVector(0.f, Index * 110.f, 5.f)), FVector(Half, 7.f, 5.f), BeamA);
	}
	const int32 Rows = 8;
	const float RowDepth = Half * 2.f / Rows;
	for (int32 Row = 0; Row < Rows; ++Row)
	{
		const float X = -Half + RowDepth * (Row + 0.5f);
		const int32 Boards = 6;
		const float BoardWidth = Half * 2.f / Boards;
		for (int32 Board = 0; Board < Boards; ++Board)
		{
			// Alternate rows are offset half a board, like real shingles.
			const float Offset = (Row % 2) ? BoardWidth * 0.5f : 0.f;
			const float Y = FMath::Clamp(-Half + BoardWidth * (Board + 0.5f) + Offset, -Half + BoardWidth * 0.5f, Half - BoardWidth * 0.5f);
			const FVector Center(X, Y, 12.f + Random.FRandRange(-0.3f, 0.3f));
			Out.AddBox(FTransform(FRotator(6.f, Random.FRandRange(-0.8f, 0.8f), 0.f), Center),
				FVector(RowDepth * 0.62f, BoardWidth * 0.5f - 0.7f, 1.6f), ((Row + Board) % 2) ? BeamA : BeamB);
		}
	}
}

