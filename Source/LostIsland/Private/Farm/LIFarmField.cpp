// Copyright LOST ISLAND Team. All Rights Reserved.

#include "Farm/LIFarmField.h"

#include "Core/LIGameState.h"
#include "Core/LIPlayerController.h"
#include "LostIsland.h"
#include "Visuals/LILowPolyMesh.h"

#include "Components/CapsuleComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Math/RandomStream.h"
#include "Net/UnrealNetwork.h"
#include "ProceduralMeshComponent.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "LIFarmField"

namespace
{
	const FLinearColor PlowedSoil(0.22f, 0.14f, 0.08f);
	const FLinearColor FurrowRidge(0.30f, 0.19f, 0.11f);
	const FLinearColor SownSoil(0.28f, 0.19f, 0.11f);
	const FLinearColor StubbleColor(0.66f, 0.55f, 0.30f);
	const FLinearColor PostColor(0.42f, 0.30f, 0.18f);

	const FLinearColor SproutColor(0.36f, 0.70f, 0.20f);
	const FLinearColor GrowingColor(0.30f, 0.60f, 0.16f);
	const FLinearColor RipeColor(0.90f, 0.72f, 0.28f);

	bool IsCrop(ELIFieldCell State)
	{
		return State == ELIFieldCell::Sown || State == ELIFieldCell::Sprout || State == ELIFieldCell::Growing;
	}
}

ALIFarmField::ALIFarmField()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;
	bReplicates = true;
	bAlwaysRelevant = true; // big and visible from far away; the state is small
	SetNetUpdateFrequency(4.f);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	SoilMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("SoilMesh"));
	SoilMesh->SetupAttachment(Root);
	SoilMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SoilMesh->SetCanEverAffectNavigation(false);
	SoilMesh->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeShape(TEXT("/Engine/BasicShapes/Cone.Cone"));
	auto MakePlants = [this](const TCHAR* Name) -> UHierarchicalInstancedStaticMeshComponent*
	{
		UHierarchicalInstancedStaticMeshComponent* Plants = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(Name);
		Plants->SetupAttachment(Root);
		Plants->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Plants->SetCanEverAffectNavigation(false);
		Plants->SetGenerateOverlapEvents(false);
		Plants->SetCastShadow(false);
		return Plants;
	};
	SproutPlants = MakePlants(TEXT("SproutPlants"));
	GrowingPlants = MakePlants(TEXT("GrowingPlants"));
	RipePlants = MakePlants(TEXT("RipePlants"));
	if (ConeShape.Succeeded())
	{
		SproutPlants->SetStaticMesh(ConeShape.Object);
		GrowingPlants->SetStaticMesh(ConeShape.Object);
		RipePlants->SetStaticMesh(ConeShape.Object);
	}
	GrowingPlants->SetCastShadow(true);
	RipePlants->SetCastShadow(true);
}

void ALIFarmField::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ALIFarmField, FieldIndex);
	DOREPLIFETIME(ALIFarmField, CellsX);
	DOREPLIFETIME(ALIFarmField, CellsY);
	DOREPLIFETIME(ALIFarmField, CellSize);
	DOREPLIFETIME(ALIFarmField, Cells);
}

void ALIFarmField::Setup(int32 InFieldIndex, int32 InCellsX, int32 InCellsY, float InCellSize)
{
	FieldIndex = InFieldIndex;
	CellsX = FMath::Max(1, InCellsX);
	CellsY = FMath::Max(1, InCellsY);
	CellSize = FMath::Max(50.f, InCellSize);
	Cells.Init(static_cast<uint8>(ELIFieldCell::Grass), CellsX * CellsY);
	Growth.Init(0.f, CellsX * CellsY);
}

void ALIFarmField::BeginPlay()
{
	Super::BeginPlay();

	SproutPlants->SetMaterial(0, LIGetTintedMaterial(SproutColor));
	GrowingPlants->SetMaterial(0, LIGetTintedMaterial(GrowingColor));
	RipePlants->SetMaterial(0, LIGetTintedMaterial(RipeColor));

	if (HasAuthority())
	{
		if (Cells.Num() != CellsX * CellsY)
		{
			Setup(FieldIndex, CellsX, CellsY, CellSize);
		}
		GetWorldTimerManager().SetTimer(GrowthTimer, this, &ALIFarmField::UpdateGrowth, 1.f, true);
	}
	bVisualDirty = true;
}

void ALIFarmField::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Rebuilding the whole field is cheap but not free: at most 4 times a second while a tractor works it.
	const double Now = GetWorld()->GetTimeSeconds();
	if (bVisualDirty && Now - LastRebuildTime >= 0.25)
	{
		RebuildVisuals();
		LastRebuildTime = Now;
		bVisualDirty = false;
	}
}

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

FVector ALIFarmField::CellCenterLocal(int32 X, int32 Y) const
{
	return FVector((X + 0.5f - CellsX * 0.5f) * CellSize, (Y + 0.5f - CellsY * 0.5f) * CellSize, 0.f);
}

int32 ALIFarmField::CellIndexAt(const FVector& WorldPoint) const
{
	const FVector Local = WorldPoint - GetActorLocation(); // fields are axis-aligned
	const int32 X = FMath::FloorToInt(static_cast<float>(Local.X) / CellSize + CellsX * 0.5f);
	const int32 Y = FMath::FloorToInt(static_cast<float>(Local.Y) / CellSize + CellsY * 0.5f);
	if (X < 0 || Y < 0 || X >= CellsX || Y >= CellsY)
	{
		return INDEX_NONE;
	}
	const int32 Index = Y * CellsX + X;
	return Cells.IsValidIndex(Index) ? Index : INDEX_NONE;
}

bool ALIFarmField::ContainsPoint(const FVector& WorldPoint) const
{
	return CellIndexAt(WorldPoint) != INDEX_NONE;
}

void ALIFarmField::SetCell(int32 Index, ELIFieldCell State)
{
	if (Cells[Index] != static_cast<uint8>(State))
	{
		Cells[Index] = static_cast<uint8>(State);
		bVisualDirty = true;
	}
}

int32 ALIFarmField::ApplyTool(const FVector& WorldPoint, ELIFarmTool Tool)
{
	if (!HasAuthority())
	{
		return 0;
	}
	const int32 Index = CellIndexAt(WorldPoint);
	if (Index == INDEX_NONE)
	{
		return 0;
	}

	const ELIFieldCell State = static_cast<ELIFieldCell>(Cells[Index]);
	switch (Tool)
	{
	case ELIFarmTool::Plough:
		// Ploughing turns everything under (grass, stubble, even a crop) into fresh furrows.
		if (State != ELIFieldCell::Plowed)
		{
			SetCell(Index, ELIFieldCell::Plowed);
			Growth[Index] = 0.f;
		}
		return 0;

	case ELIFarmTool::Seeder:
		if (State == ELIFieldCell::Plowed)
		{
			SetCell(Index, ELIFieldCell::Sown);
			Growth[Index] = 0.f;
		}
		return 0;

	case ELIFarmTool::Harvester:
		if (State == ELIFieldCell::Ripe)
		{
			SetCell(Index, ELIFieldCell::Stubble);
			Growth[Index] = 0.f;
			return YieldPerCell;
		}
		return 0;

	default:
		return 0;
	}
}

void ALIFarmField::UpdateGrowth()
{
	const float Step = 1.f / FMath::Max(1.f, GrowthSeconds);
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (!IsCrop(static_cast<ELIFieldCell>(Cells[Index])))
		{
			continue;
		}
		Growth[Index] = FMath::Min(1.f, Growth[Index] + Step);
		const float G = Growth[Index];
		SetCell(Index, G >= 1.f ? ELIFieldCell::Ripe : G >= 0.45f ? ELIFieldCell::Growing : G >= 0.12f ? ELIFieldCell::Sprout : ELIFieldCell::Sown);
	}
}

int32 ALIFarmField::CountCells(ELIFieldCell State) const
{
	int32 Count = 0;
	for (const uint8 Cell : Cells)
	{
		Count += Cell == static_cast<uint8>(State) ? 1 : 0;
	}
	return Count;
}

FText ALIFarmField::GetStatusText() const
{
	const int32 Total = FMath::Max(1, Cells.Num());
	auto Percent = [Total](int32 Count) { return FText::AsNumber(FMath::RoundToInt(100.f * Count / Total)); };
	const int32 Growing = CountCells(ELIFieldCell::Sown) + CountCells(ELIFieldCell::Sprout) + CountCells(ELIFieldCell::Growing);
	return FText::Format(LOCTEXT("Status", "Field {0}:  ploughed {1}%   growing {2}%   ripe {3}%"),
		FText::AsNumber(FieldIndex + 1), Percent(CountCells(ELIFieldCell::Plowed)), Percent(Growing), Percent(CountCells(ELIFieldCell::Ripe)));
}

void ALIFarmField::RestoreState(const TArray<uint8>& InCells, const TArray<float>& InGrowth)
{
	if (!HasAuthority() || InCells.Num() != CellsX * CellsY)
	{
		return;
	}
	Cells = InCells;
	if (InGrowth.Num() == InCells.Num())
	{
		Growth = InGrowth;
	}
	else
	{
		Growth.Init(0.f, Cells.Num());
	}
	bVisualDirty = true;
	ForceNetUpdate();
}

// ---------------------------------------------------------------------------
// Look
// ---------------------------------------------------------------------------

void ALIFarmField::RebuildVisuals()
{
	FLILowPolyMesh Soil;
	Soil.bSmooth = false;

	TArray<FTransform> Sprouts;
	TArray<FTransform> Growing;
	TArray<FTransform> Ripe;
	const float Half = CellSize * 0.5f;

	for (int32 Y = 0; Y < CellsY; ++Y)
	{
		for (int32 X = 0; X < CellsX; ++X)
		{
			const int32 Index = Y * CellsX + X;
			if (!Cells.IsValidIndex(Index))
			{
				continue;
			}
			const ELIFieldCell State = static_cast<ELIFieldCell>(Cells[Index]);
			if (State == ELIFieldCell::Grass)
			{
				continue; // the meadow terrain shows through
			}
			const FVector Center = CellCenterLocal(X, Y);

			if (State == ELIFieldCell::Stubble)
			{
				Soil.AddBox(FTransform(Center + FVector(0.f, 0.f, 2.f)), FVector(Half, Half, 3.f), StubbleColor);
			}
			else
			{
				// Soil with two furrow ridges running along X (the ploughing direction).
				Soil.AddBox(FTransform(Center + FVector(0.f, 0.f, 2.f)), FVector(Half, Half, 3.f), State == ELIFieldCell::Plowed ? PlowedSoil : SownSoil);
				for (const float Offset : { -0.25f, 0.25f })
				{
					Soil.AddBox(FTransform(Center + FVector(0.f, Offset * CellSize, 6.f)), FVector(Half, CellSize * 0.09f, 3.f), FurrowRidge);
				}
			}

			// Plants in rows on the ridges, deterministic per cell so a rebuild never shuffles them.
			TArray<FTransform>* Target = nullptr;
			float Height = 0.f;
			float Radius = 0.f;
			switch (State)
			{
			case ELIFieldCell::Sprout:  Target = &Sprouts; Height = 18.f; Radius = 5.f; break;
			case ELIFieldCell::Growing: Target = &Growing; Height = 55.f; Radius = 8.f; break;
			case ELIFieldCell::Ripe:    Target = &Ripe;    Height = 95.f; Radius = 9.f; break;
			default: break;
			}
			if (!Target)
			{
				continue;
			}
			FRandomStream Random(Index * 7919 + FieldIndex * 104729);
			for (const float Row : { -0.25f, 0.25f })
			{
				for (int32 Plant = 0; Plant < 4; ++Plant)
				{
					const float H = Height * Random.FRandRange(0.8f, 1.15f);
					const FVector Location = Center + FVector((Plant + 0.5f) / 4.f * CellSize - Half + Random.FRandRange(-8.f, 8.f),
						Row * CellSize + Random.FRandRange(-6.f, 6.f), 8.f + H * 0.5f);
					const FRotator Lean(Random.FRandRange(-6.f, 6.f), Random.FRandRange(0.f, 360.f), Random.FRandRange(-6.f, 6.f));
					// Engine cone: 100 cm tall, 50 cm radius, pivot in the middle.
					Target->Add(FTransform(Lean, Location, FVector(Radius / 50.f, Radius / 50.f, H / 100.f)));
				}
			}
		}
	}

	// Corner posts mark the field even before it is ploughed.
	const FVector Corner(CellsX * Half, CellsY * Half, 0.f);
	for (const FVector2D Sign : { FVector2D(-1.f, -1.f), FVector2D(1.f, -1.f), FVector2D(-1.f, 1.f), FVector2D(1.f, 1.f) })
	{
		Soil.AddBox(FTransform(FVector(Corner.X * Sign.X, Corner.Y * Sign.Y, 55.f)), FVector(7.f, 7.f, 55.f), PostColor);
	}

	SoilMesh->ClearAllMeshSections();
	Soil.ApplyTo(SoilMesh);

	SproutPlants->ClearInstances();
	GrowingPlants->ClearInstances();
	RipePlants->ClearInstances();
	SproutPlants->AddInstances(Sprouts, false);
	GrowingPlants->AddInstances(Growing, false);
	RipePlants->AddInstances(Ripe, false);
}

// ---------------------------------------------------------------------------
// Grain silo
// ---------------------------------------------------------------------------

ALIGrainSilo::ALIGrainSilo()
{
	bReplicates = true;

	Collision = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Collision"));
	Collision->InitCapsuleSize(260.f, 520.f);
	Collision->SetCollisionProfileName(TEXT("BlockAll"));
	Collision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	RootComponent = Collision;

	Visual = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(Collision);
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Visual->SetRelativeLocation(FVector(0.f, 0.f, -520.f)); // model origin on the ground
}

void ALIGrainSilo::BeginPlay()
{
	Super::BeginPlay();

	const FLinearColor Metal(0.78f, 0.80f, 0.80f);
	const FLinearColor Band(0.55f, 0.58f, 0.60f);
	const FLinearColor Roof(0.62f, 0.25f, 0.16f);
	const FLinearColor Wood(0.45f, 0.32f, 0.20f);
	FLILowPolyMesh Mesh;
	Mesh.AddFrustum(FTransform(FVector(0.f, 0.f, 0.f)), 16, 250.f, 250.f, 820.f, Metal);
	for (const float Z : { 200.f, 420.f, 640.f })
	{
		Mesh.AddFrustum(FTransform(FVector(0.f, 0.f, Z)), 16, 254.f, 254.f, 14.f, Band);
	}
	Mesh.AddFrustum(FTransform(FVector(0.f, 0.f, 820.f)), 16, 262.f, 0.f, 200.f, Roof);
	// Intake hopper and a sign board at the front.
	Mesh.AddFrustum(FTransform(FVector(330.f, 0.f, 0.f)), 4, 90.f, 140.f, 120.f, Band);
	Mesh.AddBox(FTransform(FVector(290.f, -230.f, 160.f)), FVector(6.f, 70.f, 45.f), Wood);
	Mesh.AddBox(FTransform(FVector(290.f, -230.f, 60.f)), FVector(6.f, 6.f, 60.f), Wood);
	Mesh.ApplyTo(Visual);
}

bool ALIGrainSilo::CanInteract_Implementation(APawn* InstigatorPawn) const
{
	const ALIGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ALIGameState>() : nullptr;
	return GameState && GameState->GetGrainKg() > 0;
}

FText ALIGrainSilo::GetInteractionPrompt_Implementation(APawn* InstigatorPawn) const
{
	const ALIGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ALIGameState>() : nullptr;
	if (!GameState || GameState->GetGrainKg() <= 0)
	{
		return LOCTEXT("SiloEmpty", "Grain silo (harvest a field first)");
	}
	return FText::Format(LOCTEXT("SellGrain", "Sell {0} kg grain (+{1} coins)"),
		FText::AsNumber(GameState->GetGrainKg()), FText::AsNumber(GameState->GetGrainValue(GameState->GetGrainKg())));
}

void ALIGrainSilo::Interact_Implementation(APawn* InstigatorPawn)
{
	ALIGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ALIGameState>() : nullptr;
	const int32 Earned = GameState ? GameState->SellGrain() : 0;
	if (Earned > 0 && InstigatorPawn)
	{
		if (ALIPlayerController* PC = Cast<ALIPlayerController>(InstigatorPawn->GetController()))
		{
			PC->ClientShowMessage(FText::Format(LOCTEXT("Sold", "Sold grain: +{0} coins"), FText::AsNumber(Earned)));
		}
	}
}

#undef LOCTEXT_NAMESPACE
