// Copyright LOST ISLAND Team. All Rights Reserved.

#include "Construction/LIConstruction.h"

#include "Core/LIGameState.h"
#include "Core/LIPlayerController.h"
#include "LostIsland.h"
#include "Vehicles/LIFarmVehicles.h"
#include "Visuals/LILowPolyMesh.h"

#include "Components/BoxComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Math/RandomStream.h"
#include "Net/UnrealNetwork.h"
#include "ProceduralMeshComponent.h"

#define LOCTEXT_NAMESPACE "LIConstruction"

namespace LIConstruction
{
	FText GetMaterialName(ELIBuildMaterial Material)
	{
		switch (Material)
		{
		case ELIBuildMaterial::Gravel: return LOCTEXT("Gravel", "gravel");
		case ELIBuildMaterial::Lumber: return LOCTEXT("Lumber", "lumber");
		case ELIBuildMaterial::Bricks: return LOCTEXT("Bricks", "bricks");
		default:                       return LOCTEXT("Nothing", "nothing");
		}
	}

	int32 GetMaterialPrice(ELIBuildMaterial Material)
	{
		switch (Material)
		{
		case ELIBuildMaterial::Gravel: return 40;
		case ELIBuildMaterial::Lumber: return 80;
		case ELIBuildMaterial::Bricks: return 70;
		default:                       return 0;
		}
	}
}

namespace
{
	void AddSitePart(FLILowPolyMesh& Mesh, const FVector& Center, const FVector& Half, const FLinearColor& Color)
	{
		if (Half.X > 0.5f && Half.Y > 0.5f && Half.Z > 0.5f)
		{
			Mesh.AddBox(FTransform(Center), Half, Color);
		}
	}

	/** Two-sided triangle wall `Thickness` thick along X (gable ends). OuterX faces away from the building. */
	void AddGable(FLILowPolyMesh& Mesh, float OuterX, float Inward, float HalfY, float BaseZ, float Height, const FLinearColor& Color)
	{
		const float InnerX = OuterX + Inward;
		const FVector A(OuterX, -HalfY, BaseZ), B(OuterX, HalfY, BaseZ), C(OuterX, 0.f, BaseZ + Height);
		const FVector A2(InnerX, -HalfY, BaseZ), B2(InnerX, HalfY, BaseZ), C2(InnerX, 0.f, BaseZ + Height);
		// Outer face points away from the building (interior point inward), inner face points into it.
		Mesh.AddTri(A, B, C, Color, FVector(InnerX + Inward * 10.f, 0.f, BaseZ + Height * 0.3f));
		Mesh.AddTri(A2, B2, C2, Color, FVector(OuterX - Inward * 10.f, 0.f, BaseZ + Height * 0.3f));
	}

	ALIGameState* GetLIGameState(const UObject* Context)
	{
		const UWorld* World = Context ? Context->GetWorld() : nullptr;
		return World ? World->GetGameState<ALIGameState>() : nullptr;
	}

	void TellPawn(APawn* Pawn, const FText& Message)
	{
		if (ALIPlayerController* PC = Pawn ? Cast<ALIPlayerController>(Pawn->GetController()) : nullptr)
		{
			PC->ClientShowMessage(Message);
		}
	}
}

// ---------------------------------------------------------------------------
// Material depot
// ---------------------------------------------------------------------------

ALIMaterialDepot::ALIMaterialDepot()
{
	bReplicates = true;
	bAlwaysRelevant = true;

	Collision = CreateDefaultSubobject<UBoxComponent>(TEXT("Collision"));
	Collision->InitBoxExtent(FVector(300.f, 300.f, 120.f));
	Collision->SetCollisionProfileName(TEXT("BlockAll"));
	Collision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	RootComponent = Collision;

	Visual = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(Collision);
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Visual->SetRelativeLocation(FVector(0.f, 0.f, -120.f)); // model origin on the ground
}

void ALIMaterialDepot::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ALIMaterialDepot, Material);
}

void ALIMaterialDepot::BeginPlay()
{
	Super::BeginPlay();
	BuildVisual();
}

void ALIMaterialDepot::BuildVisual()
{
	FLILowPolyMesh Mesh;
	Mesh.GroundShadingHeight = 40.f;
	FRandomStream Random(int32(Material) * 97 + 11);
	const FLinearColor Post(0.40f, 0.28f, 0.17f);
	const FLinearColor Board(0.92f, 0.88f, 0.78f);
	FLinearColor Badge = FLinearColor::White;

	switch (Material)
	{
	case ELIBuildMaterial::Gravel:
	{
		Badge = FLinearColor(0.55f, 0.55f, 0.52f);
		const FLinearColor Stone(0.56f, 0.55f, 0.51f);
		const FLinearColor StoneLight(0.70f, 0.68f, 0.63f);
		Mesh.AddBlob(FTransform(FVector(-60.f, -80.f, 20.f)), FVector(250.f, 220.f, 190.f), 0.15f, Random, Stone, &StoneLight);
		Mesh.AddBlob(FTransform(FVector(120.f, 150.f, 10.f)), FVector(170.f, 150.f, 120.f), 0.18f, Random, Stone, &StoneLight);
		// Bin walls of old sleepers holding the heap.
		AddSitePart(Mesh, FVector(-290.f, 0.f, 50.f), FVector(10.f, 290.f, 50.f), Post);
		AddSitePart(Mesh, FVector(0.f, -290.f, 50.f), FVector(290.f, 10.f, 50.f), Post);
		break;
	}
	case ELIBuildMaterial::Lumber:
	{
		Badge = FLinearColor(0.72f, 0.52f, 0.30f);
		const FLinearColor Wood(0.72f, 0.54f, 0.33f);
		const FLinearColor WoodDark(0.50f, 0.35f, 0.21f);
		const FLinearColor Roof(0.30f, 0.32f, 0.30f);
		for (int32 Stack = 0; Stack < 2; ++Stack)
		{
			const float Y = Stack == 0 ? -140.f : 140.f;
			for (int32 Layer = 0; Layer < 5; ++Layer)
			{
				AddSitePart(Mesh, FVector(Random.FRandRange(-15.f, 15.f), Y, 20.f + Layer * 34.f), FVector(270.f, 110.f, 15.f),
					Layer % 2 == 0 ? Wood : WoodDark);
				AddSitePart(Mesh, FVector(0.f, Y, 3.f + Layer * 34.f), FVector(8.f, 112.f, 3.f), WoodDark); // spacer battens
			}
		}
		// Open shed over the stacks.
		for (const float X : { -280.f, 280.f })
		{
			for (const float Y : { -280.f, 280.f })
			{
				AddSitePart(Mesh, FVector(X, Y, 160.f), FVector(10.f, 10.f, 160.f), WoodDark);
			}
		}
		AddSitePart(Mesh, FVector(0.f, 0.f, 330.f), FVector(310.f, 310.f, 8.f), Roof);
		break;
	}
	case ELIBuildMaterial::Bricks:
	{
		Badge = FLinearColor(0.68f, 0.30f, 0.20f);
		const FLinearColor Brick(0.68f, 0.31f, 0.21f);
		const FLinearColor BrickDark(0.55f, 0.24f, 0.16f);
		const FLinearColor Pallet(0.60f, 0.47f, 0.29f);
		for (int32 X = 0; X < 3; ++X)
		{
			for (int32 Y = 0; Y < 3; ++Y)
			{
				const int32 Height = 1 + (X + 2 * Y) % 2;
				const FVector Base(-190.f + X * 190.f, -190.f + Y * 190.f, 0.f);
				for (int32 Level = 0; Level < Height; ++Level)
				{
					const float Z = Level * 110.f;
					AddSitePart(Mesh, Base + FVector(0.f, 0.f, Z + 7.f), FVector(70.f, 70.f, 7.f), Pallet);
					AddSitePart(Mesh, Base + FVector(0.f, 0.f, Z + 56.f), FVector(64.f, 64.f, 42.f), (X + Y + Level) % 2 ? Brick : BrickDark);
				}
			}
		}
		break;
	}
	default:
		break;
	}

	// Sign by the loading side (+X), coloured like the goods.
	AddSitePart(Mesh, FVector(360.f, -230.f, 90.f), FVector(7.f, 7.f, 90.f), Post);
	AddSitePart(Mesh, FVector(360.f, -230.f, 200.f), FVector(5.f, 80.f, 40.f), Board);
	AddSitePart(Mesh, FVector(366.f, -230.f, 200.f), FVector(2.f, 60.f, 22.f), Badge);
	Mesh.ApplyTo(Visual);
}

ALIDumpTruck* ALIMaterialDepot::FindTruck() const
{
	ALIDumpTruck* Best = nullptr;
	double BestDistSq = FMath::Square(LoadRadius);
	for (TActorIterator<ALIDumpTruck> It(GetWorld()); It; ++It)
	{
		const double DistSq = FVector::DistSquared2D(It->GetActorLocation(), GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = *It;
		}
	}
	return Best;
}

bool ALIMaterialDepot::CanInteract_Implementation(APawn* InstigatorPawn) const
{
	return true; // the prompt explains what is missing; Interact re-checks on the server
}

FText ALIMaterialDepot::GetInteractionPrompt_Implementation(APawn* InstigatorPawn) const
{
	const FText Name = LIConstruction::GetMaterialName(Material);
	const int32 Price = LIConstruction::GetMaterialPrice(Material);
	const ALIDumpTruck* Truck = FindTruck();
	if (!Truck)
	{
		return FText::Format(LOCTEXT("DepotNoTruck", "Load of {0}, {1} coins: park the dump truck here first"), Name, FText::AsNumber(Price));
	}
	if (Truck->GetCargo() != ELIBuildMaterial::None)
	{
		return FText::Format(LOCTEXT("DepotTruckFull", "The dump truck already carries {0}"), LIConstruction::GetMaterialName(Truck->GetCargo()));
	}
	return FText::Format(LOCTEXT("DepotBuy", "Load the dump truck with {0} ({1} coins)"), Name, FText::AsNumber(Price));
}

void ALIMaterialDepot::Interact_Implementation(APawn* InstigatorPawn)
{
	if (!HasAuthority())
	{
		return;
	}
	ALIDumpTruck* Truck = FindTruck();
	ALIGameState* GameState = GetLIGameState(this);
	const int32 Price = LIConstruction::GetMaterialPrice(Material);
	if (!Truck)
	{
		TellPawn(InstigatorPawn, LOCTEXT("NeedTruck", "Park the dump truck next to the depot first."));
		return;
	}
	if (!Truck->CanLoadCargo())
	{
		TellPawn(InstigatorPawn, Truck->GetCargo() != ELIBuildMaterial::None
			? LOCTEXT("TruckNotEmpty", "The dump truck is already loaded: tip it at a building site.")
			: LOCTEXT("TruckNotReady", "Stop the dump truck and lower its bed to load it."));
		return;
	}
	if (!GameState || !GameState->SpendMoney(Price))
	{
		TellPawn(InstigatorPawn, FText::Format(LOCTEXT("NoMoney", "Not enough coins: a load costs {0}. Sell grain at the silo."), FText::AsNumber(Price)));
		return;
	}
	Truck->LoadCargo(Material);
	UE_LOG(LogLostIsland, Log, TEXT("Depot loaded the dump truck with material %d for %d coins."), int32(Material), Price);
	TellPawn(InstigatorPawn, FText::Format(LOCTEXT("Loaded", "Loaded {0} (-{1} coins). Drive it to a building site and tip it with Q."),
		LIConstruction::GetMaterialName(Material), FText::AsNumber(Price)));
}

// ---------------------------------------------------------------------------
// Construction site
// ---------------------------------------------------------------------------

ALIConstructionSite::ALIConstructionSite()
{
	bReplicates = true;
	bAlwaysRelevant = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Visual = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(Root);
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Visual->SetCanEverAffectNavigation(false);

	SignCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("SignCollision"));
	SignCollision->SetupAttachment(Root);
	SignCollision->InitBoxExtent(FVector(20.f, 90.f, 110.f));
	SignCollision->SetCollisionProfileName(TEXT("BlockAll"));
	SignCollision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	for (int32 Index = 0; Index < 5; ++Index)
	{
		UBoxComponent* Wall = CreateDefaultSubobject<UBoxComponent>(*FString::Printf(TEXT("Wall%d"), Index));
		Wall->SetupAttachment(Root);
		Wall->SetCollisionProfileName(TEXT("BlockAll"));
		Wall->SetCollisionEnabled(ECollisionEnabled::NoCollision); // on once the walls stand
		Wall->SetCanEverAffectNavigation(false);
		WallCollision.Add(Wall);
	}
}

void ALIConstructionSite::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ALIConstructionSite, BuildingType);
	DOREPLIFETIME(ALIConstructionSite, SiteIndex);
	DOREPLIFETIME(ALIConstructionSite, DeliveredLoads);
}

void ALIConstructionSite::BeginPlay()
{
	Super::BeginPlay();
	const FVector Half = GetHalfSize();
	SignCollision->SetRelativeLocation(FVector(Half.X + 250.f, -Half.Y - 100.f, 110.f));
	RebuildVisual();
}

const TArray<FLIBuildStage>& ALIConstructionSite::GetStages() const
{
	static const TArray<FLIBuildStage> BarnStages =
	{
		{ ELIBuildMaterial::Gravel, 2, LOCTEXT("StageFoundation", "foundation") },
		{ ELIBuildMaterial::Lumber, 3, LOCTEXT("StageWalls", "walls") },
		{ ELIBuildMaterial::Lumber, 2, LOCTEXT("StageRoof", "roof") },
	};
	static const TArray<FLIBuildStage> HouseStages =
	{
		{ ELIBuildMaterial::Gravel, 1, LOCTEXT("StageFoundation", "foundation") },
		{ ELIBuildMaterial::Bricks, 3, LOCTEXT("StageWalls", "walls") },
		{ ELIBuildMaterial::Lumber, 2, LOCTEXT("StageRoof", "roof") },
	};
	return BuildingType == ELIBuildingType::House ? HouseStages : BarnStages;
}

int32 ALIConstructionSite::GetTotalLoads() const
{
	int32 Total = 0;
	for (const FLIBuildStage& Stage : GetStages())
	{
		Total += Stage.Loads;
	}
	return Total;
}

void ALIConstructionSite::GetStageProgress(int32& OutStage, int32& OutLoadsInStage) const
{
	const TArray<FLIBuildStage>& Stages = GetStages();
	int32 Remaining = DeliveredLoads;
	for (int32 Index = 0; Index < Stages.Num(); ++Index)
	{
		if (Remaining < Stages[Index].Loads)
		{
			OutStage = Index;
			OutLoadsInStage = Remaining;
			return;
		}
		Remaining -= Stages[Index].Loads;
	}
	OutStage = Stages.Num();
	OutLoadsInStage = 0;
}

float ALIConstructionSite::GetStageFraction(int32 Stage) const
{
	int32 Current = 0, InStage = 0;
	GetStageProgress(Current, InStage);
	if (Stage < Current)
	{
		return 1.f;
	}
	if (Stage > Current || !GetStages().IsValidIndex(Stage))
	{
		return 0.f;
	}
	return float(InStage) / float(GetStages()[Stage].Loads);
}

bool ALIConstructionSite::IsComplete() const
{
	return DeliveredLoads >= GetTotalLoads();
}

ELIBuildMaterial ALIConstructionSite::GetNeededMaterial() const
{
	int32 Stage = 0, InStage = 0;
	GetStageProgress(Stage, InStage);
	return GetStages().IsValidIndex(Stage) ? GetStages()[Stage].Material : ELIBuildMaterial::None;
}

FVector ALIConstructionSite::GetHalfSize() const
{
	return BuildingType == ELIBuildingType::House ? FVector(450.f, 380.f, 0.f) : FVector(700.f, 500.f, 0.f);
}

float ALIConstructionSite::GetWallHeight() const { return BuildingType == ELIBuildingType::House ? 320.f : 450.f; }
float ALIConstructionSite::GetRoofHeight() const { return BuildingType == ELIBuildingType::House ? 240.f : 320.f; }
float ALIConstructionSite::GetDoorWidth() const { return BuildingType == ELIBuildingType::House ? 130.f : 420.f; }

bool ALIConstructionSite::IsInDeliveryRange(const FVector& WorldPoint) const
{
	const FVector Local = GetActorTransform().InverseTransformPosition(WorldPoint);
	const FVector Half = GetHalfSize();
	constexpr float Margin = 450.f;
	return FMath::Abs(Local.X) <= Half.X + Margin && FMath::Abs(Local.Y) <= Half.Y + Margin && FMath::Abs(Local.Z) < 600.f;
}

bool ALIConstructionSite::DeliverLoad(ELIBuildMaterial Material)
{
	if (!HasAuthority() || Material == ELIBuildMaterial::None || Material != GetNeededMaterial())
	{
		return false;
	}
	++DeliveredLoads;
	RebuildVisual();

	if (IsComplete())
	{
		const FText Message = BuildingType == ELIBuildingType::Barn
			? FText::Format(LOCTEXT("BarnDone", "The barn is finished! Grain now sells for {0}% more."),
				FText::AsNumber(FMath::RoundToInt((GrainPriceBonus - 1.f) * 100.f)))
			: LOCTEXT("HouseDone", "The farmhouse is finished. Welcome home!");
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			if (ALIPlayerController* PC = Cast<ALIPlayerController>(It->Get()))
			{
				PC->ClientShowMessage(Message);
			}
		}
	}
	return true;
}

void ALIConstructionSite::RestoreProgress(int32 Loads)
{
	if (HasAuthority())
	{
		DeliveredLoads = FMath::Clamp(Loads, 0, GetTotalLoads());
		RebuildVisual();
	}
}

void ALIConstructionSite::OnRep_Progress()
{
	RebuildVisual();
}

void ALIConstructionSite::ApplyCompletionBonus()
{
	if (BuildingType == ELIBuildingType::Barn && IsComplete())
	{
		if (ALIGameState* GameState = GetLIGameState(this))
		{
			GameState->SetGrainPriceMultiplier(GrainPriceBonus);
		}
	}
}

FText ALIConstructionSite::GetDisplayName() const
{
	return BuildingType == ELIBuildingType::House ? LOCTEXT("House", "farmhouse") : LOCTEXT("Barn", "barn");
}

FText ALIConstructionSite::GetStatusText() const
{
	if (IsComplete())
	{
		return BuildingType == ELIBuildingType::Barn
			? FText::Format(LOCTEXT("BarnFinished", "Finished: grain sells for {0}% more."), FText::AsNumber(FMath::RoundToInt((GrainPriceBonus - 1.f) * 100.f)))
			: LOCTEXT("HouseFinished", "Finished.");
	}
	int32 Stage = 0, InStage = 0;
	GetStageProgress(Stage, InStage);
	const FLIBuildStage& Current = GetStages()[Stage];
	return FText::Format(LOCTEXT("SiteNeeds", "{0}: {1}/{2} loads of {3} ({4} coins a load)"),
		Current.Name, FText::AsNumber(InStage), FText::AsNumber(Current.Loads), LIConstruction::GetMaterialName(Current.Material),
		FText::AsNumber(LIConstruction::GetMaterialPrice(Current.Material)));
}

FText ALIConstructionSite::GetInteractionPrompt_Implementation(APawn* InstigatorPawn) const
{
	return FText::Format(LOCTEXT("SitePrompt", "Building site, {0}: {1}"), GetDisplayName(), GetStatusText());
}

void ALIConstructionSite::Interact_Implementation(APawn* InstigatorPawn)
{
	TellPawn(InstigatorPawn, IsComplete()
		? FText::Format(LOCTEXT("SiteInfoDone", "The {0} is finished. {1}"), GetDisplayName(), GetStatusText())
		: FText::Format(LOCTEXT("SiteInfo", "The {0} needs {1}. Buy a load at a depot, back the dump truck up to the plot and tip it with Q."),
			GetDisplayName(), GetStatusText()));
}

void ALIConstructionSite::RebuildVisual()
{
	FLILowPolyMesh Mesh;
	Mesh.GroundShadingHeight = 60.f;
	BuildModel(Mesh);
	Mesh.ApplyTo(Visual);
	UpdateCollision();
	if (HasAuthority())
	{
		ApplyCompletionBonus();
	}
}

void ALIConstructionSite::UpdateCollision()
{
	const bool bWalls = GetStageFraction(1) >= 1.f;
	const FVector Half = GetHalfSize();
	const float H = GetWallHeight();
	const float Base = 24.f;
	const float T = 8.f; // half thickness
	const float Door = GetDoorWidth() * 0.5f;
	const float FrontHalf = (Half.Y - Door) * 0.5f;
	const FVector Centers[5] =
	{
		FVector(-Half.X + T, 0.f, Base + H * 0.5f),                     // back
		FVector(0.f, -Half.Y + T, Base + H * 0.5f),                     // left
		FVector(0.f, Half.Y - T, Base + H * 0.5f),                      // right
		FVector(Half.X - T, -(Door + FrontHalf), Base + H * 0.5f),      // front, left of the door
		FVector(Half.X - T, Door + FrontHalf, Base + H * 0.5f),         // front, right of the door
	};
	const FVector Extents[5] =
	{
		FVector(T, Half.Y, H * 0.5f),
		FVector(Half.X, T, H * 0.5f),
		FVector(Half.X, T, H * 0.5f),
		FVector(T, FrontHalf, H * 0.5f),
		FVector(T, FrontHalf, H * 0.5f),
	};
	for (int32 Index = 0; Index < WallCollision.Num() && Index < 5; ++Index)
	{
		UBoxComponent* Wall = WallCollision[Index];
		Wall->SetRelativeLocation(Centers[Index]);
		Wall->SetBoxExtent(Extents[Index]);
		Wall->SetCollisionEnabled(bWalls ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	}
}

void ALIConstructionSite::BuildModel(FLILowPolyMesh& Mesh) const
{
	const bool bBarn = BuildingType == ELIBuildingType::Barn;
	const FVector Half = GetHalfSize();
	const float H = GetWallHeight();
	const float R = GetRoofHeight();
	const float Door = GetDoorWidth() * 0.5f;
	const float DoorH = bBarn ? 360.f : 220.f;
	const float T = 8.f;

	const FLinearColor Stake(0.52f, 0.38f, 0.22f);
	const FLinearColor Tape(0.95f, 0.55f, 0.10f);
	const FLinearColor Gravel(0.58f, 0.57f, 0.53f);
	const FLinearColor Concrete(0.74f, 0.73f, 0.70f);
	const FLinearColor Wall = bBarn ? FLinearColor(0.62f, 0.15f, 0.11f) : FLinearColor(0.70f, 0.36f, 0.24f);
	const FLinearColor Trim = bBarn ? FLinearColor(0.93f, 0.92f, 0.88f) : FLinearColor(0.90f, 0.88f, 0.82f);
	const FLinearColor Timber(0.55f, 0.40f, 0.24f);
	const FLinearColor Roof = bBarn ? FLinearColor(0.27f, 0.28f, 0.30f) : FLinearColor(0.30f, 0.34f, 0.40f);
	const FLinearColor Glass(0.16f, 0.24f, 0.30f);
	const FLinearColor Board(0.92f, 0.88f, 0.78f);

	const bool bComplete = IsComplete();
	const float F0 = GetStageFraction(0);
	const float F1 = GetStageFraction(1);
	const float F2 = GetStageFraction(2);

	// The site board (always) and the staked-out plot (until finished).
	const FVector SignPos(Half.X + 250.f, -Half.Y - 100.f, 0.f);
	AddSitePart(Mesh, SignPos + FVector(0.f, -70.f, 80.f), FVector(6.f, 6.f, 80.f), Stake);
	AddSitePart(Mesh, SignPos + FVector(0.f, 70.f, 80.f), FVector(6.f, 6.f, 80.f), Stake);
	AddSitePart(Mesh, SignPos + FVector(0.f, 0.f, 170.f), FVector(5.f, 90.f, 45.f), Board);
	AddSitePart(Mesh, SignPos + FVector(6.f, 0.f, 185.f), FVector(2.f, 70.f, 16.f), Wall);
	if (!bComplete)
	{
		const float SX = Half.X + 60.f, SY = Half.Y + 60.f;
		for (const float X : { -SX, SX })
		{
			for (const float Y : { -SY, SY })
			{
				AddSitePart(Mesh, FVector(X, Y, 50.f), FVector(5.f, 5.f, 50.f), Stake);
			}
			AddSitePart(Mesh, FVector(X, 0.f, 80.f), FVector(1.5f, SY, 1.5f), Tape);
		}
		for (const float Y : { -SY, SY })
		{
			AddSitePart(Mesh, FVector(0.f, Y, 80.f), FVector(SX, 1.5f, 1.5f), Tape);
		}
	}

	// Foundation: a gravel bed that rises with each load, then a finished slab.
	if (F0 > 0.f)
	{
		const float SlabH = 24.f * F0;
		AddSitePart(Mesh, FVector(0.f, 0.f, SlabH * 0.5f), FVector(Half.X + 30.f, Half.Y + 30.f, SlabH * 0.5f), F0 >= 1.f ? Concrete : Gravel);
	}
	const float Base = 24.f;

	// Walls grow from the slab up; the barn's timber frame (corner posts) goes up with the first load.
	if (F1 > 0.f)
	{
		const float WallH = H * F1;
		const float Z = Base + WallH * 0.5f;
		const float FrontHalf = (Half.Y - Door) * 0.5f;
		AddSitePart(Mesh, FVector(-Half.X + T, 0.f, Z), FVector(T, Half.Y, WallH * 0.5f), Wall);
		AddSitePart(Mesh, FVector(0.f, -Half.Y + T, Z), FVector(Half.X, T, WallH * 0.5f), Wall);
		AddSitePart(Mesh, FVector(0.f, Half.Y - T, Z), FVector(Half.X, T, WallH * 0.5f), Wall);
		AddSitePart(Mesh, FVector(Half.X - T, -(Door + FrontHalf), Z), FVector(T, FrontHalf, WallH * 0.5f), Wall);
		AddSitePart(Mesh, FVector(Half.X - T, Door + FrontHalf, Z), FVector(T, FrontHalf, WallH * 0.5f), Wall);
		if (WallH > DoorH)
		{
			AddSitePart(Mesh, FVector(Half.X - T, 0.f, Base + (DoorH + WallH) * 0.5f), FVector(T, Door, (WallH - DoorH) * 0.5f), Wall);
		}
		if (bBarn)
		{
			for (const float X : { -Half.X, Half.X })
			{
				for (const float Y : { -Half.Y, Half.Y })
				{
					AddSitePart(Mesh, FVector(X - FMath::Sign(X) * 6.f, Y - FMath::Sign(Y) * 6.f, Base + H * 0.5f), FVector(12.f, 12.f, H * 0.5f),
						F1 >= 1.f ? Trim : Timber);
				}
			}
		}
	}

	// Details once the walls stand: door frames, windows, the barn's doors.
	if (F1 >= 1.f)
	{
		AddSitePart(Mesh, FVector(Half.X + 2.f, 0.f, Base + DoorH + 8.f), FVector(4.f, Door + 10.f, 8.f), Trim);
		for (const float Y : { -1.f, 1.f })
		{
			AddSitePart(Mesh, FVector(Half.X + 2.f, Y * (Door + 5.f), Base + DoorH * 0.5f), FVector(4.f, 6.f, DoorH * 0.5f), Trim);
		}
		if (bBarn)
		{
			// Two big doors swung open against the front wall, white X-bracing.
			for (const float Y : { -1.f, 1.f })
			{
				const FVector Leaf(Half.X + 10.f, Y * (Door + Door * 0.5f + 12.f), Base + DoorH * 0.5f);
				AddSitePart(Mesh, Leaf, FVector(4.f, Door * 0.5f, DoorH * 0.5f), Wall);
				AddSitePart(Mesh, Leaf + FVector(4.f, 0.f, 0.f), FVector(2.f, Door * 0.5f, 8.f), Trim);
				const float Diagonal = FMath::RadiansToDegrees(FMath::Atan2(DoorH, Door));
				for (const float Sign : { -1.f, 1.f })
				{
					Mesh.AddBox(FTransform(FQuat(FVector::ForwardVector, FMath::DegreesToRadians(Sign * Diagonal)), Leaf + FVector(5.f, 0.f, 0.f)),
						FVector(2.f, FMath::Sqrt(FMath::Square(Door) + FMath::Square(DoorH)) * 0.5f, 7.f), Trim);
				}
			}
			// Hay loft hatch above the doors.
			AddSitePart(Mesh, FVector(Half.X + 3.f, 0.f, Base + H + R * 0.35f), FVector(3.f, 70.f, 60.f), Trim);
		}
		else
		{
			const FLinearColor DoorColor(0.25f, 0.38f, 0.30f);
			AddSitePart(Mesh, FVector(Half.X - 2.f, 0.f, Base + DoorH * 0.5f), FVector(3.f, Door, DoorH * 0.5f), DoorColor);
			const auto AddWindow = [&Mesh, &Trim, &Glass](const FVector& Center, bool bAlongX)
			{
				const FVector Frame = bAlongX ? FVector(62.f, 3.f, 52.f) : FVector(3.f, 62.f, 52.f);
				const FVector Pane = bAlongX ? FVector(52.f, 4.f, 42.f) : FVector(4.f, 52.f, 42.f);
				Mesh.AddBox(FTransform(Center), Frame, Trim);
				Mesh.AddBox(FTransform(Center), Pane, Glass);
			};
			for (const float Y : { -1.f, 1.f })
			{
				AddWindow(FVector(Half.X + 1.f, Y * Half.Y * 0.6f, Base + 170.f), false);
				for (const float X : { -0.5f, 0.5f })
				{
					AddWindow(FVector(Half.X * X, Y * (Half.Y + 1.f), Base + 170.f), true);
				}
			}
		}
	}

	// Roof: gable ends first, then the two slopes laid from the back toward the front.
	if (F2 > 0.f)
	{
		const float Top = Base + H;
		const float Overhang = 40.f;
		const float Length = (Half.X * 2.f + Overhang * 2.f) * F2;
		const float CenterX = -Half.X - Overhang + Length * 0.5f;
		const float Angle = FMath::Atan2(R, Half.Y);
		const float Slope = FMath::Sqrt(FMath::Square(R) + FMath::Square(Half.Y));
		const float Eave = 50.f;
		for (const float Side : { -1.f, 1.f })
		{
			// Panel centre: half way up the slope, pushed down-slope so the eave overhangs and the top meets the ridge.
			const FVector DownSlope(0.f, Side * FMath::Cos(Angle), -FMath::Sin(Angle));
			const FVector Center = FVector(CenterX, Side * Half.Y * 0.5f, Top + R * 0.5f) + DownSlope * (Eave * 0.5f) + FVector(0.f, 0.f, 8.f);
			Mesh.AddBox(FTransform(FQuat(FVector::ForwardVector, -Side * Angle), Center), FVector(Length * 0.5f, (Slope + Eave) * 0.5f, 6.f), Roof);
		}
		AddSitePart(Mesh, FVector(CenterX, 0.f, Top + R + 10.f), FVector(Length * 0.5f, 18.f, 8.f), Trim); // ridge cap
		AddGable(Mesh, -Half.X, 2.f * T, Half.Y, Top, R, Wall);
		if (F2 >= 1.f)
		{
			AddGable(Mesh, Half.X, -2.f * T, Half.Y, Top, R, Wall);
			if (!bBarn)
			{
				AddSitePart(Mesh, FVector(-Half.X * 0.45f, Half.Y * 0.45f, Top + R * 0.75f), FVector(30.f, 30.f, R * 0.5f), Wall); // chimney
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
