// Copyright LOST ISLAND Team. All Rights Reserved.

#include "Vehicles/LIVehicleDealer.h"

#include "Core/LIGameState.h"
#include "Core/LIPlayerController.h"
#include "LostIsland.h"
#include "Vehicles/LIVehicle.h"
#include "Visuals/LILowPolyMesh.h"

#include "Components/BoxComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "ProceduralMeshComponent.h"

#define LOCTEXT_NAMESPACE "LIVehicleDealer"

namespace
{
	void TellDealerCustomer(APawn* Pawn, const FText& Message)
	{
		if (ALIPlayerController* PC = Pawn ? Cast<ALIPlayerController>(Pawn->GetController()) : nullptr)
		{
			PC->ClientShowMessage(Message);
		}
	}
}

ALIVehicleDealer::ALIVehicleDealer()
{
	bReplicates = true;
	bAlwaysRelevant = true;

	Board = CreateDefaultSubobject<UBoxComponent>(TEXT("Board"));
	Board->InitBoxExtent(FVector(20.f, 110.f, 120.f));
	Board->SetCollisionProfileName(TEXT("BlockAll"));
	Board->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	RootComponent = Board;

	Visual = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(Board);
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Visual->SetCanEverAffectNavigation(false);
	Visual->SetRelativeLocation(FVector(0.f, 0.f, -120.f)); // model origin on the ground
}

void ALIVehicleDealer::Setup(TSubclassOf<ALIVehicle> InVehicleClass, int32 InPrice)
{
	VehicleClass = InVehicleClass;
	Price = FMath::Max(InPrice, 0);
}

void ALIVehicleDealer::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ALIVehicleDealer, VehicleClass);
	DOREPLIFETIME(ALIVehicleDealer, Price);
}

void ALIVehicleDealer::BeginPlay()
{
	Super::BeginPlay();
	BuildVisual();
}

FText ALIVehicleDealer::GetVehicleName() const
{
	const ALIVehicle* Default = VehicleClass ? VehicleClass->GetDefaultObject<ALIVehicle>() : nullptr;
	return Default ? Default->GetDisplayName() : LOCTEXT("AVehicle", "vehicle");
}

void ALIVehicleDealer::BuildVisual()
{
	const FLinearColor Asphalt(0.25f, 0.25f, 0.26f);
	const FLinearColor Line(0.95f, 0.93f, 0.86f);
	const FLinearColor Post(0.30f, 0.30f, 0.32f);
	const FLinearColor BoardColor(0.13f, 0.30f, 0.52f); // dealer blue
	const FLinearColor PriceColor(1.0f, 0.82f, 0.25f);

	FLILowPolyMesh Mesh;
	// The bay: a dark pad with white edge lines, a few cm above the ground so it never flickers.
	const FVector Bay = BayOffset + FVector(0.f, 0.f, 3.f);
	Mesh.AddBox(FTransform(Bay), FVector(BayHalfSize.X, BayHalfSize.Y, 3.f), Asphalt);
	for (const float Y : { -1.f, 1.f })
	{
		Mesh.AddBox(FTransform(Bay + FVector(0.f, Y * (BayHalfSize.Y - 6.f), 1.f)), FVector(BayHalfSize.X, 6.f, 3.f), Line);
	}
	Mesh.AddBox(FTransform(Bay + FVector(-BayHalfSize.X + 6.f, 0.f, 1.f)), FVector(6.f, BayHalfSize.Y, 3.f), Line);

	// Price board on two posts, on the yard side of the bay.
	for (const float Y : { -90.f, 90.f })
	{
		Mesh.AddBox(FTransform(FVector(0.f, Y, 100.f)), FVector(6.f, 6.f, 100.f), Post);
	}
	Mesh.AddBox(FTransform(FVector(0.f, 0.f, 170.f)), FVector(6.f, 110.f, 55.f), BoardColor);
	Mesh.AddBox(FTransform(FVector(7.f, 0.f, 145.f)), FVector(2.f, 80.f, 14.f), PriceColor);
	// Little flags on top so the dealer reads from across the yard.
	for (const float Y : { -100.f, 0.f, 100.f })
	{
		Mesh.AddBox(FTransform(FVector(0.f, Y, 255.f)), FVector(2.f, 2.f, 30.f), Post);
		Mesh.AddBox(FTransform(FVector(0.f, Y + 18.f, 275.f)), FVector(1.f, 16.f, 10.f), Y == 0.f ? PriceColor : BoardColor);
	}
	Mesh.ApplyTo(Visual);
}

bool ALIVehicleDealer::IsBayClear() const
{
	const FVector Center = GetActorTransform().TransformPosition(BayOffset + FVector(0.f, 0.f, 150.f));
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_PhysicsBody);
	Objects.AddObjectTypesToQuery(ECC_Pawn);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(LIDealerBay), false, this);
	return !GetWorld()->OverlapAnyTestByObjectType(Center, GetActorQuat(), Objects,
		FCollisionShape::MakeBox(FVector(BayHalfSize.X, BayHalfSize.Y, 140.f)), Params);
}

bool ALIVehicleDealer::CanInteract_Implementation(APawn* InstigatorPawn) const
{
	return VehicleClass != nullptr;
}

FText ALIVehicleDealer::GetInteractionPrompt_Implementation(APawn* InstigatorPawn) const
{
	return FText::Format(LOCTEXT("BuyPrompt", "Buy a {0} ({1} coins)"), GetVehicleName(), FText::AsNumber(Price));
}

void ALIVehicleDealer::Interact_Implementation(APawn* InstigatorPawn)
{
	UWorld* World = GetWorld();
	if (!HasAuthority() || !VehicleClass || !World)
	{
		return;
	}

	int32 Count = 0;
	for (TActorIterator<ALIVehicle> It(World); It; ++It)
	{
		++Count;
	}
	if (Count >= MaxVehicles)
	{
		TellDealerCustomer(InstigatorPawn, FText::Format(LOCTEXT("TooMany", "The farm already has {0} vehicles: that is enough."), FText::AsNumber(Count)));
		return;
	}
	if (!IsBayClear())
	{
		TellDealerCustomer(InstigatorPawn, LOCTEXT("BayBlocked", "Clear the bay first: something is standing on it."));
		return;
	}
	ALIGameState* GameState = World->GetGameState<ALIGameState>();
	if (!GameState || !GameState->SpendMoney(Price))
	{
		TellDealerCustomer(InstigatorPawn, FText::Format(LOCTEXT("TooPoor", "Not enough coins: the {0} costs {1}."), GetVehicleName(), FText::AsNumber(Price)));
		return;
	}

	// Delivered on the bay facing the yard (the board's side), dropped onto its wheels.
	FVector Location = GetActorTransform().TransformPosition(BayOffset);
	FHitResult Hit;
	if (World->LineTraceSingleByObjectType(Hit, Location + FVector(0.f, 0.f, 500.f), Location - FVector(0.f, 0.f, 1500.f), FCollisionObjectQueryParams(ECC_WorldStatic)))
	{
		Location.Z = Hit.ImpactPoint.Z;
	}
	Location.Z += 130.f;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	const FRotator Rotation(0.f, GetActorRotation().Yaw, 0.f);
	if (!World->SpawnActor<ALIVehicle>(VehicleClass, Location, Rotation, Params))
	{
		GameState->SetFarmState(GameState->GetMoney() + Price, GameState->GetGrainKg()); // refund
		return;
	}
	UE_LOG(LogLostIsland, Log, TEXT("Dealer sold %s for %d coins."), *VehicleClass->GetName(), Price);
	TellDealerCustomer(InstigatorPawn, FText::Format(LOCTEXT("Bought", "New {0} delivered (-{1} coins)."), GetVehicleName(), FText::AsNumber(Price)));
}

#undef LOCTEXT_NAMESPACE
