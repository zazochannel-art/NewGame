// Copyright LOST ISLAND Team. All Rights Reserved.

#include "World/LIBuildingPiece.h"

#include "LostIsland.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"

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
			if (It->GetPieceType() != Type || FVector::DistSquared(It->GetActorLocation(), Location) > FMath::Square(30.f))
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
	LITintPlaceholder(Mesh, WoodColor);
}

ALIWoodenFloor::ALIWoodenFloor()
{
	PieceType = ELIBuildingPieceType::Floor;
	SetupBox(Mesh, FVector(GridSize, GridSize, FloorThickness));
}

ALIWoodenWall::ALIWoodenWall()
{
	PieceType = ELIBuildingPieceType::Wall;
	SetupBox(Mesh, FVector(20.f, GridSize, WallHeight)); // thin along local X
	WoodColor = FLinearColor(0.50f, 0.33f, 0.17f);
}

ALIWoodenRoof::ALIWoodenRoof()
{
	PieceType = ELIBuildingPieceType::Roof;
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
		if (It->PieceType == ELIBuildingPieceType::Floor && DistSq < BestDistSq)
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
		if (It->PieceType != ELIBuildingPieceType::Roof)
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
