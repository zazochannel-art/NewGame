// Copyright LOST ISLAND Team. All Rights Reserved.

#include "World/LICampfire.h"

#include "LostIsland.h"
#include "Inventory/LIInventoryComponent.h"
#include "Items/LIItemTypes.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Math/RandomStream.h"
#include "ProceduralMeshComponent.h"
#include "Visuals/LILowPolyMesh.h"

#define LOCTEXT_NAMESPACE "LICampfire"

ALICampfire::ALICampfire()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	StoneRing = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StoneRing"));
	StoneRing->SetupAttachment(SceneRoot);
	StoneRing->SetStaticMesh(CylinderMesh.Object);
	StoneRing->SetRelativeScale3D(FVector(1.2f, 1.2f, 0.18f));
	StoneRing->SetRelativeLocation(FVector(0.f, 0.f, 9.f));
	StoneRing->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

	Logs = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Logs"));
	Logs->SetupAttachment(SceneRoot);
	Logs->SetStaticMesh(ConeMesh.Object);
	Logs->SetRelativeScale3D(FVector(0.6f, 0.6f, 0.45f));
	Logs->SetRelativeLocation(FVector(0.f, 0.f, 40.f));
	Logs->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

	Flame = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Flame"));
	Flame->SetupAttachment(SceneRoot);
	Flame->SetStaticMesh(ConeMesh.Object);
	Flame->SetRelativeScale3D(FVector(0.4f, 0.4f, 0.7f));
	Flame->SetRelativeLocation(FVector(0.f, 0.f, 70.f));
	Flame->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Flame->SetCastShadow(false);

	FireLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FireLight"));
	FireLight->SetupAttachment(SceneRoot);
	FireLight->SetRelativeLocation(FVector(0.f, 0.f, 90.f));
	FireLight->SetIntensity(5000.f);
	FireLight->SetAttenuationRadius(1200.f);
	FireLight->SetLightColor(FLinearColor(1.f, 0.55f, 0.2f));
	FireLight->SetCastShadows(false); // cheap: many fires in co-op

	Visual = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(SceneRoot);
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Visual->SetCanEverAffectNavigation(false);

	FlameVisual = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("FlameVisual"));
	FlameVisual->SetupAttachment(SceneRoot);
	FlameVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FlameVisual->SetCanEverAffectNavigation(false);
	FlameVisual->SetCastShadow(false);
}

void ALICampfire::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ALICampfire, Fuel);
}

void ALICampfire::BeginPlay()
{
	Super::BeginPlay();

	auto Tint = [](UStaticMeshComponent* Part, const FLinearColor& Color)
	{
		LITintPlaceholder(Part, Color);
	};
	Tint(StoneRing, FLinearColor(0.35f, 0.33f, 0.3f));
	Tint(Logs, FLinearColor(0.3f, 0.18f, 0.08f));
	Tint(Flame, FLinearColor(1.f, 0.45f, 0.05f));
	BuildVisuals();

	if (HasAuthority())
	{
		if (!bFuelInitialized)
		{
			Fuel = InitialFuel; // a loaded campfire already received its fuel via SetFuel
		}
		GetWorldTimerManager().SetTimer(BurnTimerHandle, this, &ALICampfire::BurnFuel, 1.f, true);
	}
	ApplyLitState();
}

void ALICampfire::BuildVisuals()
{
	FRandomStream Random(LIVisualSeed(GetActorLocation(), 0xF12E));

	// Stones, ash and a teepee of logs.
	FLILowPolyMesh Base;
	Base.AddFrustum(FTransform(FVector(0.f, 0.f, -2.f)), 12, 44.f, 40.f, 4.f, FLinearColor(0.12f, 0.11f, 0.10f), 0.08f, &Random);
	const int32 Stones = 10;
	for (int32 Index = 0; Index < Stones; ++Index)
	{
		const float Angle = 2.f * PI * Index / Stones + Random.FRandRange(-0.12f, 0.12f);
		const float R = Random.FRandRange(13.f, 18.f);
		Base.AddBlob(FTransform(FRotator(0.f, Random.FRandRange(0.f, 360.f), 0.f), FVector(FMath::Cos(Angle) * 52.f, FMath::Sin(Angle) * 52.f, 7.f)),
			FVector(R, R * 0.85f, R * 0.7f), 0.2f, Random, LIVaryColor(FLinearColor(0.42f, 0.40f, 0.37f), Random, 0.12f), nullptr, 0.7f, 0);
	}
	const int32 LogCount = 5;
	for (int32 Index = 0; Index < LogCount; ++Index)
	{
		const float Yaw = 360.f * Index / LogCount + Random.FRandRange(-8.f, 8.f);
		const FVector Out(FMath::Cos(FMath::DegreesToRadians(Yaw)), FMath::Sin(FMath::DegreesToRadians(Yaw)), 0.f);
		// Each log leans in toward the centre: yaw points it inward, pitch raises its inner end.
		const FTransform LogTransform(FRotator(38.f, Yaw + 180.f, 0.f), Out * 20.f + FVector(0.f, 0.f, 20.f));
		Base.AddBox(LogTransform, FVector(30.f, 6.f, 6.f), LIVaryColor(FLinearColor(0.30f, 0.18f, 0.08f), Random, 0.12f));
	}
	if (Base.ApplyTo(Visual))
	{
		StoneRing->SetHiddenInGame(true); // keeps its collision
		Logs->SetHiddenInGame(true);
	}

	// Flames: an outer orange cone and an inner yellow one, slightly jittered.
	FLILowPolyMesh Fire;
	Fire.AddFrustum(FTransform(FVector(0.f, 0.f, 16.f)), 7, 26.f, 0.f, 72.f, FLinearColor(1.f, 0.42f, 0.05f), 0.18f, &Random);
	Fire.AddFrustum(FTransform(FRotator(0.f, 25.f, 0.f), FVector(0.f, 0.f, 18.f)), 6, 15.f, 0.f, 52.f, FLinearColor(1.f, 0.80f, 0.20f), 0.15f, &Random);
	Fire.AddFrustum(FTransform(FVector(10.f, -6.f, 20.f)), 5, 9.f, 0.f, 38.f, FLinearColor(1.f, 0.60f, 0.10f), 0.2f, &Random);
	if (Fire.ApplyTo(FlameVisual))
	{
		Flame->SetHiddenInGame(true);
	}
}

void ALICampfire::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(BurnTimerHandle);
	Super::EndPlay(EndPlayReason);
}

void ALICampfire::SetFuel(float NewFuel)
{
	if (!HasAuthority())
	{
		return;
	}
	Fuel = FMath::Clamp(NewFuel, 0.f, MaxFuel);
	bFuelInitialized = true;
	ApplyLitState();
}

void ALICampfire::BurnFuel()
{
	if (Fuel <= 0.f)
	{
		return;
	}
	Fuel = FMath::Max(0.f, Fuel - 1.f);
	ApplyLitState();
}

void ALICampfire::OnRep_Fuel()
{
	ApplyLitState();
}

void ALICampfire::ApplyLitState()
{
	const bool bLit = IsLit();
	if (bLit == bWasLit && HasActorBegunPlay())
	{
		return;
	}
	bWasLit = bLit;
	Flame->SetVisibility(bLit);
	FlameVisual->SetVisibility(bLit);
	FireLight->SetVisibility(bLit);
}

bool ALICampfire::IsWarmAt(const UWorld* World, const FVector& Location)
{
	if (!World)
	{
		return false;
	}
	for (TActorIterator<ALICampfire> It(World); It; ++It)
	{
		const ALICampfire* Fire = *It;
		if (Fire->IsLit() && FVector::DistSquared(Fire->GetActorLocation(), Location) <= FMath::Square(Fire->WarmthRadius))
		{
			return true;
		}
	}
	return false;
}

bool ALICampfire::CanInteract_Implementation(APawn* InstigatorPawn) const
{
	const ULIInventoryComponent* Inventory = InstigatorPawn ? InstigatorPawn->FindComponentByClass<ULIInventoryComponent>() : nullptr;
	return Inventory && Inventory->HasItem(LIItems::Wood, 1) && Fuel + 1.f < MaxFuel;
}

void ALICampfire::Interact_Implementation(APawn* InstigatorPawn)
{
	if (!HasAuthority() || !InstigatorPawn)
	{
		return;
	}
	ULIInventoryComponent* Inventory = InstigatorPawn->FindComponentByClass<ULIInventoryComponent>();
	if (Inventory && Inventory->RemoveItem(LIItems::Wood, 1))
	{
		Fuel = FMath::Min(MaxFuel, Fuel + FuelPerWood);
		ApplyLitState();
	}
}

FText ALICampfire::GetInteractionPrompt_Implementation(APawn* InstigatorPawn) const
{
	const int32 Seconds = FMath::CeilToInt(Fuel);
	const FText Time = FText::FromString(FString::Printf(TEXT("%d:%02d"), Seconds / 60, Seconds % 60));
	return IsLit()
		? FText::Format(LOCTEXT("AddWood", "Add Wood  (burns {0})"), Time)
		: LOCTEXT("Relight", "Add Wood to light the fire");
}

void ALICampfire::OnFocusBegin_Implementation(APawn* InstigatorPawn)
{
	StoneRing->SetRenderCustomDepth(true);
	Visual->SetRenderCustomDepth(true);
}

void ALICampfire::OnFocusEnd_Implementation(APawn* InstigatorPawn)
{
	StoneRing->SetRenderCustomDepth(false);
	Visual->SetRenderCustomDepth(false);
}

#undef LOCTEXT_NAMESPACE
