// Copyright LOST ISLAND Team. All Rights Reserved.

#include "Vehicles/LIFuelPump.h"

#include "Core/LIGameState.h"
#include "Core/LIPlayerController.h"
#include "Vehicles/LIVehicle.h"
#include "Visuals/LILowPolyMesh.h"

#include "Components/BoxComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "ProceduralMeshComponent.h"

#define LOCTEXT_NAMESPACE "LIFuelPump"

namespace
{
	void TellPumpCustomer(APawn* Pawn, const FText& Message)
	{
		if (ALIPlayerController* PC = Pawn ? Cast<ALIPlayerController>(Pawn->GetController()) : nullptr)
		{
			PC->ClientShowMessage(Message);
		}
	}
}

ALIFuelPump::ALIFuelPump()
{
	bReplicates = true;
	bAlwaysRelevant = true;

	Body = CreateDefaultSubobject<UBoxComponent>(TEXT("Body"));
	Body->InitBoxExtent(FVector(35.f, 50.f, 95.f));
	Body->SetCollisionProfileName(TEXT("BlockAll"));
	Body->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	RootComponent = Body;

	Visual = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(Body);
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Visual->SetCanEverAffectNavigation(false);
	Visual->SetRelativeLocation(FVector(0.f, 0.f, -95.f)); // model origin on the ground
}

void ALIFuelPump::BeginPlay()
{
	Super::BeginPlay();
	BuildVisual();
}

void ALIFuelPump::BuildVisual()
{
	const FLinearColor Concrete(0.62f, 0.61f, 0.58f);
	const FLinearColor PumpRed(0.75f, 0.12f, 0.10f);
	const FLinearColor White(0.94f, 0.93f, 0.90f);
	const FLinearColor Dark(0.10f, 0.10f, 0.11f);
	const FLinearColor Display(0.35f, 0.85f, 0.45f);
	const FLinearColor Roof(0.85f, 0.85f, 0.82f);
	const FLinearColor Post(0.30f, 0.30f, 0.32f);

	FLILowPolyMesh Mesh;
	// Island: a raised concrete slab with yellow-black ends.
	Mesh.AddBox(FTransform(FVector(0.f, 0.f, 8.f)), FVector(70.f, 160.f, 8.f), Concrete);
	for (const float Y : { -1.f, 1.f })
	{
		Mesh.AddBox(FTransform(FVector(0.f, Y * 152.f, 9.f)), FVector(71.f, 8.f, 9.f), FLinearColor(0.95f, 0.75f, 0.1f));
	}

	// The pump: red cabinet, white band, a small green display on both faces, hose and nozzle on the side.
	Mesh.AddBox(FTransform(FVector(0.f, 0.f, 100.f)), FVector(32.f, 46.f, 84.f), PumpRed);
	Mesh.AddBox(FTransform(FVector(0.f, 0.f, 150.f)), FVector(33.f, 47.f, 10.f), White);
	for (const float X : { -1.f, 1.f })
	{
		Mesh.AddBox(FTransform(FVector(X * 33.f, 0.f, 125.f)), FVector(1.5f, 26.f, 11.f), Display);
		Mesh.AddBox(FTransform(FVector(X * 33.f, 0.f, 85.f)), FVector(1.5f, 30.f, 18.f), Dark);
	}
	Mesh.AddBox(FTransform(FVector(0.f, 47.f, 110.f)), FVector(8.f, 4.f, 12.f), Dark);  // nozzle holder
	Mesh.AddBox(FTransform(FVector(0.f, 52.f, 70.f)), FVector(2.5f, 2.5f, 40.f), Dark); // hose
	Mesh.AddBox(FTransform(FRotator(0.f, 0.f, 30.f), FVector(0.f, 54.f, 116.f)), FVector(3.f, 3.f, 12.f), Dark); // nozzle

	// Canopy on two posts, so the pump reads from across the yard.
	for (const float Y : { -140.f, 140.f })
	{
		Mesh.AddBox(FTransform(FVector(0.f, Y, 180.f)), FVector(7.f, 7.f, 170.f), Post);
	}
	Mesh.AddBox(FTransform(FVector(0.f, 0.f, 355.f)), FVector(260.f, 200.f, 8.f), Roof);
	Mesh.AddBox(FTransform(FVector(0.f, 0.f, 343.f)), FVector(262.f, 202.f, 5.f), PumpRed);
	Mesh.ApplyTo(Visual);
}

ALIVehicle* ALIFuelPump::FindVehicle() const
{
	ALIVehicle* Best = nullptr;
	float BestDistSq = FMath::Square(Reach);
	for (TActorIterator<ALIVehicle> It(GetWorld()); It; ++It)
	{
		if (!It->UsesFuel() || It->GetDriver() != nullptr)
		{
			continue;
		}
		const float DistSq = FVector::DistSquared(It->GetActorLocation(), GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = *It;
		}
	}
	return Best;
}

bool ALIFuelPump::CanInteract_Implementation(APawn* InstigatorPawn) const
{
	return true;
}

FText ALIFuelPump::GetInteractionPrompt_Implementation(APawn* InstigatorPawn) const
{
	const ALIVehicle* Vehicle = FindVehicle();
	if (!Vehicle)
	{
		return LOCTEXT("NoVehicle", "Fuel pump: park a vehicle here to refuel");
	}
	const float Missing = Vehicle->GetFuelCapacity() - Vehicle->GetFuel();
	if (Missing < 0.5f)
	{
		return FText::Format(LOCTEXT("Full", "Fuel pump: the {0} is full"), Vehicle->GetDisplayName());
	}
	return FText::Format(LOCTEXT("Refuel", "Refuel the {0}: {1} L ({2} coins)"),
		Vehicle->GetDisplayName(), FText::AsNumber(FMath::RoundToInt(Missing)), FText::AsNumber(GetCost(Missing)));
}

void ALIFuelPump::Interact_Implementation(APawn* InstigatorPawn)
{
	if (!HasAuthority())
	{
		return;
	}
	ALIVehicle* Vehicle = FindVehicle();
	ALIGameState* GameState = GetWorld()->GetGameState<ALIGameState>();
	if (!Vehicle || !GameState)
	{
		TellPumpCustomer(InstigatorPawn, LOCTEXT("ParkFirst", "Park the vehicle next to the pump and get out first."));
		return;
	}

	float Litres = Vehicle->GetFuelCapacity() - Vehicle->GetFuel();
	if (Litres < 0.5f)
	{
		TellPumpCustomer(InstigatorPawn, FText::Format(LOCTEXT("AlreadyFull", "The {0} is already full."), Vehicle->GetDisplayName()));
		return;
	}
	// Not enough coins for a full tank: as much as the coins pay for.
	const int32 Money = FMath::Max(GameState->GetMoney(), 0);
	if (GetCost(Litres) > Money && PricePerLitre > 0.f)
	{
		Litres = FMath::FloorToFloat(Money / PricePerLitre);
	}
	const int32 Cost = GetCost(Litres);
	if (Litres < 1.f || !GameState->SpendMoney(Cost))
	{
		TellPumpCustomer(InstigatorPawn, LOCTEXT("NoMoney", "Not enough coins for fuel. Sell some grain first."));
		return;
	}
	const float Added = Vehicle->AddFuel(Litres);
	TellPumpCustomer(InstigatorPawn, FText::Format(LOCTEXT("Filled", "Filled the {0} with {1} L of diesel (-{2} coins)."),
		Vehicle->GetDisplayName(), FText::AsNumber(FMath::RoundToInt(Added)), FText::AsNumber(Cost)));
}

#undef LOCTEXT_NAMESPACE
