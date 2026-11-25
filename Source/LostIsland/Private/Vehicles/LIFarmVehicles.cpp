// Copyright LOST ISLAND Team. All Rights Reserved.

#include "Vehicles/LIFarmVehicles.h"

#include "Core/LIGameState.h"
#include "Core/LIPlayerController.h"
#include "LostIsland.h"
#include "Visuals/LILowPolyMesh.h"

#include "Components/BoxComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Math/RandomStream.h"
#include "Net/UnrealNetwork.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "ProceduralMeshComponent.h"

#define LOCTEXT_NAMESPACE "LIFarmVehicles"

namespace
{
	FLIVehicleWheel MakeFarmWheel(const FVector& Mount, float Radius, float Width, bool bSteers, bool bDriven)
	{
		FLIVehicleWheel Wheel;
		Wheel.MountPoint = Mount;
		Wheel.Radius = Radius;
		Wheel.Width = Width;
		Wheel.bSteers = bSteers;
		Wheel.bDriven = bDriven;
		return Wheel;
	}

	void AddPart(FLILowPolyMesh& Mesh, const FVector& Center, const FVector& Half, const FLinearColor& Color)
	{
		Mesh.AddBox(FTransform(Center), Half, Color);
	}
}

// ---------------------------------------------------------------------------
// Tractor
// ---------------------------------------------------------------------------

ALITractor::ALITractor()
{
	DisplayName = LOCTEXT("Tractor", "Tractor");
	SetChassisExtent(FVector(175.f, 62.f, 50.f));
	Mass = 3800.f;
	CenterOfMassOffset = FVector(-20.f, 0.f, -45.f);
	SuspensionLength = 30.f;
	RestCompression = 0.5f;
	DampingRatio = 0.7f;

	MaxSpeedKmh = 32.f;
	MaxReverseKmh = 14.f;
	Acceleration = 3.f;
	BrakeDeceleration = 7.f;
	CoastDeceleration = 2.f;
	MaxSteerAngle = 42.f;
	HighSpeedSteerFactor = 0.6f;
	TyreGrip = 1.3f;
	DragArea = 2.5f;
	HeadlightOffset = FVector(175.f, 0.f, 38.f); // between the grille lamps

	CameraDistance = 800.f;
	CameraHeight = 260.f;
	ExitOffset = FVector(55.f, -(62.f + 115.f), 0.f);
	SeatOffset = FVector(-50.f, 0.f, 60.f);
	RoadMaxSpeedKmh = MaxSpeedKmh;

	ImplementMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ImplementMesh"));
	ImplementMesh->SetupAttachment(Chassis);
	ImplementMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ImplementMesh->SetCanEverAffectNavigation(false);

	// Big driven rear wheels, small steering front wheels; mounts set so both touch the ground together
	// (wheel bottom = mount - (1 - RestCompression) * SuspensionLength - Radius = -100 for all four).
	Wheels =
	{
		MakeFarmWheel(FVector( 112.f, -64.f, -43.f), 42.f, 22.f, true,  false),
		MakeFarmWheel(FVector( 112.f,  64.f, -43.f), 42.f, 22.f, true,  false),
		MakeFarmWheel(FVector(-92.f,  -86.f, -15.f), 70.f, 44.f, false, true),
		MakeFarmWheel(FVector(-92.f,   86.f, -15.f), 70.f, 44.f, false, true),
	};
}

void ALITractor::BuildBody(FLILowPolyMesh& Mesh) const
{
	// Original livery: deep teal with cream details (not a real brand's colours).
	const FLinearColor Paint(0.10f, 0.42f, 0.48f);
	const FLinearColor PaintDark(0.06f, 0.28f, 0.33f);
	const FLinearColor Cream(0.92f, 0.86f, 0.70f);
	const FLinearColor Iron(0.20f, 0.20f, 0.21f);
	const FLinearColor Seat(0.15f, 0.12f, 0.10f);
	const FLinearColor Lamp(1.0f, 0.95f, 0.72f);
	const FLinearColor Glass(0.12f, 0.17f, 0.22f);

	// Frame and engine hood (long and narrow, between the front wheels).
	AddPart(Mesh, FVector(10.f, 0.f, -20.f), FVector(165.f, 34.f, 22.f), Iron);       // frame rails
	AddPart(Mesh, FVector(85.f, 0.f, 22.f), FVector(85.f, 36.f, 34.f), Paint);        // hood, z -12..56
	AddPart(Mesh, FVector(85.f, 0.f, 58.f), FVector(80.f, 30.f, 4.f), PaintDark);     // hood top
	AddPart(Mesh, FVector(171.f, 0.f, 18.f), FVector(3.f, 30.f, 26.f), Iron);         // grille
	AddPart(Mesh, FVector(173.f, -22.f, 38.f), FVector(2.f, 8.f, 6.f), Lamp);
	AddPart(Mesh, FVector(173.f, 22.f, 38.f), FVector(2.f, 8.f, 6.f), Lamp);
	AddPart(Mesh, FVector(176.f, 0.f, -28.f), FVector(8.f, 44.f, 8.f), Iron);         // front weight bar
	AddPart(Mesh, FVector(112.f, 0.f, -43.f), FVector(8.f, 56.f, 5.f), Iron);         // front axle

	// Exhaust stack in front of the cab.
	Mesh.AddFrustum(FTransform(FVector(40.f, 26.f, 56.f)), 8, 5.f, 5.f, 90.f, Iron);

	// Body over the rear axle: fuel tank, platform and big fenders.
	AddPart(Mesh, FVector(-70.f, 0.f, 10.f), FVector(70.f, 48.f, 32.f), PaintDark);
	AddPart(Mesh, FVector(-80.f, 0.f, 44.f), FVector(70.f, 62.f, 3.f), Iron);         // platform
	for (const float Y : { -1.f, 1.f })
	{
		AddPart(Mesh, FVector(-92.f, Y * 86.f, 66.f), FVector(76.f, 26.f, 4.f), Paint); // fender top
		AddPart(Mesh, FVector(-92.f, Y * 63.f, 40.f), FVector(76.f, 3.f, 28.f), Paint); // fender inner wall
		AddPart(Mesh, FVector(-20.f, Y * 86.f, 52.f), FVector(4.f, 24.f, 16.f), Paint); // fender front lip
		AddPart(Mesh, FVector(-160.f, Y * 104.f, 64.f), FVector(4.f, 5.f, 4.f), FLinearColor(0.85f, 0.05f, 0.04f)); // tail lamps
	}
	AddPart(Mesh, FVector(-92.f, 0.f, -15.f), FVector(10.f, 66.f, 8.f), Iron);        // rear axle

	// Seat and steering wheel.
	AddPart(Mesh, FVector(-80.f, 0.f, 60.f), FVector(20.f, 22.f, 12.f), Seat);
	AddPart(Mesh, FVector(-100.f, 0.f, 82.f), FVector(4.f, 22.f, 18.f), Seat);
	Mesh.AddBox(FTransform(FRotator(-55.f, 0.f, 0.f), FVector(-20.f, 0.f, 88.f)), FVector(2.f, 14.f, 14.f), Iron);
	AddPart(Mesh, FVector(-12.f, 0.f, 70.f), FVector(4.f, 4.f, 18.f), Iron);          // steering column

	// Open cab: four posts and a sun roof, a windscreen in front.
	for (const float X : { -150.f, -10.f })
	{
		for (const float Y : { -56.f, 56.f })
		{
			AddPart(Mesh, FVector(X, Y, 110.f), FVector(3.f, 3.f, 64.f), Iron);
		}
	}
	AddPart(Mesh, FVector(-80.f, 0.f, 176.f), FVector(80.f, 64.f, 4.f), Cream);       // roof
	AddPart(Mesh, FVector(-10.f, 0.f, 130.f), FVector(1.5f, 54.f, 40.f), Glass);      // windscreen

	// Hitch at the back (tools attach here).
	AddPart(Mesh, FVector(-172.f, 0.f, -10.f), FVector(10.f, 36.f, 6.f), Iron);
	AddPart(Mesh, HitchPoint, FVector(14.f, 6.f, 6.f), Cream);
}

void ALITractor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ALITractor, Implement);
	DOREPLIFETIME(ALITractor, Trailer);
}

ALIGrainTrailer* ALITractor::GetTrailer() const
{
	return Trailer.Get();
}

void ALITractor::SetTrailer(ALIGrainTrailer* InTrailer)
{
	if (HasAuthority())
	{
		Trailer = InTrailer;
		ForceNetUpdate();
	}
}

void ALITractor::ApplySaveState(uint8 Mode, int32 Amount)
{
	if (HasAuthority() && Mode < uint8(ELIFarmTool::Count))
	{
		Implement = static_cast<ELIFarmTool>(Mode);
		BuildImplement();
	}
}

void ALITractor::TellDriver(const FText& Message)
{
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now - LastHarvestWarning < 4.0)
	{
		return;
	}
	LastHarvestWarning = Now;
	if (ALIPlayerController* PC = Cast<ALIPlayerController>(GetController()))
	{
		PC->ClientShowMessage(Message);
	}
}

void ALITractor::BeginPlay()
{
	Super::BeginPlay();
	ImplementMesh->SetRelativeLocation(HitchPoint + FVector(0.f, 0.f, 25.f)); // raised
	BuildImplement();
}

FText ALITractor::GetToolModeName() const
{
	switch (Implement)
	{
	case ELIFarmTool::Seeder:    return LOCTEXT("Seeder", "Seeder");
	case ELIFarmTool::Harvester: return LOCTEXT("Harvester", "Harvester");
	case ELIFarmTool::Plough:
	default:                     return LOCTEXT("Plough", "Plough");
	}
}

FText ALITractor::GetToolHint() const
{
	return FText::Format(bToolActive ? LOCTEXT("Raise", "Raise {0}") : LOCTEXT("Lower", "Lower {0}"), GetToolModeName());
}

FText ALITractor::GetStatusText() const
{
	// The field under the tractor (clients search their own copy of the fields).
	const FVector Location = GetActorLocation();
	FText FieldText;
	for (TActorIterator<ALIFarmField> It(GetWorld()); It; ++It)
	{
		if (It->ContainsPoint(Location))
		{
			FieldText = It->GetStatusText();
			break;
		}
	}
	if (!Trailer)
	{
		return Implement == ELIFarmTool::Harvester
			? FText::Format(LOCTEXT("NoTrailerStatus", "{0}   No trailer: the harvester needs one for the grain"), FieldText)
			: FieldText;
	}
	return FText::Format(LOCTEXT("TrailerStatus", "{0}   Trailer: {1} / {2} kg"), FieldText,
		FText::AsNumber(Trailer->GetGrainKg()), FText::AsNumber(Trailer->GetCapacityKg()));
}

void ALITractor::CycleTool()
{
	if (bToolActive)
	{
		return; // raise it first
	}
	Implement = static_cast<ELIFarmTool>((static_cast<uint8>(Implement) + 1) % static_cast<uint8>(ELIFarmTool::Count));
	BuildImplement();
	ForceNetUpdate();
}

void ALITractor::Tick(float DeltaSeconds)
{
	// Working speed limit (every machine runs the same suspension, so all of them apply it).
	MaxSpeedKmh = bToolActive ? WorkingMaxSpeedKmh : RoadMaxSpeedKmh;

	Super::Tick(DeltaSeconds);

	// The implement drops onto the ground or lifts off it.
	const float TargetDrop = bToolActive ? 1.f : 0.f;
	if (!FMath::IsNearlyEqual(ImplementDrop, TargetDrop))
	{
		ImplementDrop = FMath::FInterpConstantTo(ImplementDrop, TargetDrop, DeltaSeconds, 2.5f);
		ImplementMesh->SetRelativeLocation(HitchPoint + FVector(0.f, 0.f, FMath::Lerp(25.f, -38.f, ImplementDrop)));
	}

	if (HasAuthority() && bToolActive && ImplementDrop > 0.9f && GetDriver() && FMath::Abs(GetSpeedKmh()) > 0.8f)
	{
		WorkFields();
	}
}

void ALITractor::RefreshFields()
{
	Fields.Reset();
	for (TActorIterator<ALIFarmField> It(GetWorld()); It; ++It)
	{
		Fields.Add(*It);
	}
	LastFieldSearch = GetWorld()->GetTimeSeconds();
}

ALIFarmField* ALITractor::FindFieldUnder(const FVector& WorldPoint)
{
	// Fields are replaced when the world is regenerated: look again every few seconds.
	if (GetWorld()->GetTimeSeconds() - LastFieldSearch > 3.0)
	{
		RefreshFields();
	}
	for (const TWeakObjectPtr<ALIFarmField>& Field : Fields)
	{
		if (Field.IsValid() && Field->ContainsPoint(WorldPoint))
		{
			return Field.Get();
		}
	}
	return nullptr;
}

void ALITractor::WorkFields()
{
	// The harvester needs somewhere to put the grain.
	ALIGrainTrailer* GrainTrailer = (Trailer && IsValid(Trailer) && Trailer->GetTractor() == this) ? Trailer.Get() : nullptr;
	if (Implement == ELIFarmTool::Harvester)
	{
		if (!GrainTrailer)
		{
			TellDriver(LOCTEXT("NeedTrailer", "Hitch the grain trailer first: the harvester has nowhere to put the grain."));
			return;
		}
		if (GrainTrailer->IsFull())
		{
			TellDriver(LOCTEXT("TrailerFull", "The trailer is full: take it to the grain silo and unload it (E at the silo)."));
			return;
		}
	}

	// Sample points across the working width, under the middle of the implement.
	const FTransform Transform = GetActorTransform();
	const FVector Center = HitchPoint - FVector(90.f, 0.f, 0.f);
	int32 Harvested = 0;
	const int32 Samples = FMath::Max(2, FMath::CeilToInt(ImplementWidth / 50.f));
	for (int32 Index = 0; Index <= Samples; ++Index)
	{
		const float Y = -ImplementWidth * 0.5f + ImplementWidth * Index / Samples;
		for (const float X : { 0.f, -60.f })
		{
			const FVector Point = Transform.TransformPosition(Center + FVector(X, Y, 0.f));
			if (ALIFarmField* Field = FindFieldUnder(Point))
			{
				Harvested += Field->ApplyTool(Point, Implement);
			}
		}
	}
	if (Harvested > 0 && GrainTrailer)
	{
		GrainTrailer->AddGrain(Harvested); // the last bit of a full trailer spills
	}
}

void ALITractor::BuildImplement()
{
	// Modelled at the hitch (origin), trailing toward -X, ImplementWidth wide.
	const FLinearColor Iron(0.20f, 0.20f, 0.21f);
	const FLinearColor Teal(0.10f, 0.42f, 0.48f);
	const FLinearColor Cream(0.92f, 0.86f, 0.70f);
	const FLinearColor Steel(0.62f, 0.62f, 0.60f);
	const FLinearColor Orange(0.85f, 0.42f, 0.10f);
	const float HalfWidth = ImplementWidth * 0.5f;

	FLILowPolyMesh Mesh;
	AddPart(Mesh, FVector(-25.f, 0.f, 0.f), FVector(25.f, 8.f, 8.f), Iron);                 // drawbar
	AddPart(Mesh, FVector(-55.f, 0.f, 0.f), FVector(8.f, HalfWidth, 8.f), Iron);            // cross beam

	switch (Implement)
	{
	case ELIFarmTool::Plough:
	{
		// Five angled shares on curved legs.
		for (int32 Index = 0; Index < 5; ++Index)
		{
			const float Y = -HalfWidth + 30.f + Index * (ImplementWidth - 60.f) / 4.f;
			const float X = -70.f - Index * 18.f;
			AddPart(Mesh, FVector(X, Y, -18.f), FVector(6.f, 6.f, 22.f), Teal);            // leg
			Mesh.AddBox(FTransform(FRotator(0.f, 30.f, -20.f), FVector(X - 10.f, Y, -42.f)), FVector(26.f, 4.f, 8.f), Steel); // share
		}
		AddPart(Mesh, FVector(-110.f, 0.f, 10.f), FVector(60.f, 10.f, 6.f), Teal);          // frame
		break;
	}
	case ELIFarmTool::Seeder:
	{
		// A long hopper over a row of seed discs.
		AddPart(Mesh, FVector(-110.f, 0.f, 45.f), FVector(45.f, HalfWidth - 10.f, 35.f), Teal);  // hopper
		AddPart(Mesh, FVector(-110.f, 0.f, 82.f), FVector(47.f, HalfWidth - 6.f, 3.f), Cream);   // lid
		AddPart(Mesh, FVector(-110.f, 0.f, 4.f), FVector(40.f, HalfWidth, 6.f), Iron);           // frame
		const FQuat AxleAlongY = FQuat::FindBetweenNormals(FVector::UpVector, FVector::RightVector);
		for (int32 Index = 0; Index < 8; ++Index)
		{
			const float Y = -HalfWidth + 20.f + Index * (ImplementWidth - 40.f) / 7.f;
			Mesh.AddFrustum(FTransform(AxleAlongY, FVector(-140.f, Y - 2.f, -30.f)), 10, 22.f, 22.f, 4.f, Steel);
		}
		break;
	}
	case ELIFarmTool::Harvester:
	default:
	{
		// Header with a reel and cutter bar, and a grain bin behind it.
		const FQuat AxleAlongY = FQuat::FindBetweenNormals(FVector::UpVector, FVector::RightVector);
		AddPart(Mesh, FVector(-120.f, 0.f, -25.f), FVector(55.f, HalfWidth, 6.f), Orange);        // header floor
		AddPart(Mesh, FVector(-170.f, 0.f, -8.f), FVector(6.f, HalfWidth, 22.f), Orange);         // back wall
		AddPart(Mesh, FVector(-120.f, -HalfWidth, -5.f), FVector(55.f, 4.f, 26.f), Orange);       // side walls
		AddPart(Mesh, FVector(-120.f, HalfWidth, -5.f), FVector(55.f, 4.f, 26.f), Orange);
		AddPart(Mesh, FVector(-68.f, 0.f, -32.f), FVector(4.f, HalfWidth, 3.f), Steel);           // cutter bar
		Mesh.AddFrustum(FTransform(AxleAlongY, FVector(-105.f, -HalfWidth + 6.f, 25.f)), 8, 26.f, 26.f, ImplementWidth - 12.f, Cream); // reel
		AddPart(Mesh, FVector(-215.f, 0.f, 40.f), FVector(40.f, 70.f, 45.f), Teal);               // grain bin
		break;
	}
	}

	ImplementMesh->ClearAllMeshSections();
	Mesh.ApplyTo(ImplementMesh);
}

// ---------------------------------------------------------------------------
// Dump truck
// ---------------------------------------------------------------------------

ALIDumpTruck::ALIDumpTruck()
{
	DisplayName = LOCTEXT("DumpTruck", "Dump Truck");
	SetChassisExtent(FVector(360.f, 118.f, 60.f));
	Mass = 9000.f;
	CenterOfMassOffset = FVector(-20.f, 0.f, -65.f);
	SuspensionLength = 40.f;
	RestCompression = 0.45f;
	DampingRatio = 0.6f;

	MaxSpeedKmh = 70.f;
	MaxReverseKmh = 15.f;
	DragArea = 5.0f; // a brick on wheels
	AntiRoll = 0.9f;
	HeadlightOffset = FVector(368.f, 0.f, 50.f);
	Acceleration = 2.4f;
	BrakeDeceleration = 6.5f;
	CoastDeceleration = 1.5f;
	MaxSteerAngle = 30.f;
	HighSpeedSteerFactor = 0.4f;
	TyreGrip = 1.05f;

	CameraDistance = 1350.f;
	CameraHeight = 340.f;
	ExitOffset = FVector(270.f, -(118.f + 100.f), 0.f);
	SeatOffset = FVector(280.f, -50.f, 80.f);

	// Steering front axle, two driven rear axles. Wheel bottom = -40 - 22 - 55 = -117 (box bottom -60).
	Wheels =
	{
		MakeFarmWheel(FVector( 255.f, -100.f, -40.f), 55.f, 34.f, true,  false),
		MakeFarmWheel(FVector( 255.f,  100.f, -40.f), 55.f, 34.f, true,  false),
		MakeFarmWheel(FVector(-150.f, -100.f, -40.f), 55.f, 44.f, false, true),
		MakeFarmWheel(FVector(-150.f,  100.f, -40.f), 55.f, 44.f, false, true),
		MakeFarmWheel(FVector(-275.f, -100.f, -40.f), 55.f, 44.f, false, true),
		MakeFarmWheel(FVector(-275.f,  100.f, -40.f), 55.f, 44.f, false, true),
	};

	BedMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("BedMesh"));
	BedMesh->SetupAttachment(Chassis);
	BedMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BedMesh->SetCanEverAffectNavigation(false);

	CargoMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("CargoMesh"));
	CargoMesh->SetupAttachment(BedMesh);
	CargoMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CargoMesh->SetCanEverAffectNavigation(false);
}

void ALIDumpTruck::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ALIDumpTruck, Cargo);
}

void ALIDumpTruck::ApplySaveState(uint8 Mode, int32 Amount)
{
	if (Mode <= uint8(ELIBuildMaterial::Bricks))
	{
		LoadCargo(static_cast<ELIBuildMaterial>(Mode));
	}
}

bool ALIDumpTruck::CanLoadCargo() const
{
	return Cargo == ELIBuildMaterial::None && !bToolActive && BedAngle < 2.f && FMath::Abs(GetSpeedKmh()) < 3.f;
}

void ALIDumpTruck::LoadCargo(ELIBuildMaterial Material)
{
	if (HasAuthority())
	{
		Cargo = Material;
		BuildCargo();
	}
}

FVector ALIDumpTruck::GetTipPoint() const
{
	return GetActorTransform().TransformPosition(BedHinge + FVector(-120.f, 0.f, -60.f));
}

FText ALIDumpTruck::GetStatusText() const
{
	if (Cargo == ELIBuildMaterial::None)
	{
		return LOCTEXT("BedEmpty", "Bed empty: park by a depot and buy a load (E at the depot)");
	}
	return FText::Format(LOCTEXT("BedLoaded", "Load: {0}. Back up to a building site and tip it (Q)"),
		LIConstruction::GetMaterialName(Cargo));
}

void ALIDumpTruck::TellDriver(const FText& Message) const
{
	if (ALIPlayerController* PC = Cast<ALIPlayerController>(GetController()))
	{
		PC->ClientShowMessage(Message);
	}
}

void ALIDumpTruck::TryUnload()
{
	if (Cargo == ELIBuildMaterial::None || bUnloadTried)
	{
		return;
	}
	bUnloadTried = true;

	const FVector TipPoint = GetTipPoint();
	ALIConstructionSite* InRange = nullptr;
	for (TActorIterator<ALIConstructionSite> It(GetWorld()); It; ++It)
	{
		if (!It->IsInDeliveryRange(TipPoint))
		{
			continue;
		}
		InRange = *It;
		if (It->DeliverLoad(Cargo))
		{
			TellDriver(FText::Format(LOCTEXT("Delivered", "{0} delivered to the {1}. {2}"),
				LIConstruction::GetMaterialName(Cargo), It->GetDisplayName(), It->GetStatusText()));
			Cargo = ELIBuildMaterial::None;
			BuildCargo();
			return;
		}
	}

	if (InRange)
	{
		const ELIBuildMaterial Needed = InRange->GetNeededMaterial();
		TellDriver(Needed == ELIBuildMaterial::None
			? FText::Format(LOCTEXT("SiteDone", "The {0} is already finished."), InRange->GetDisplayName())
			: FText::Format(LOCTEXT("SiteNeedsOther", "The {0} needs {1} now, not {2}."),
				InRange->GetDisplayName(), LIConstruction::GetMaterialName(Needed), LIConstruction::GetMaterialName(Cargo)));
	}
	else
	{
		TellDriver(LOCTEXT("NoSite", "No building site behind the truck: back the tail up to a staked plot."));
	}
}

FText ALIDumpTruck::GetToolHint() const
{
	return bToolActive ? LOCTEXT("LowerBed", "Lower bed") : LOCTEXT("RaiseBed", "Raise bed");
}

void ALIDumpTruck::ToggleTool()
{
	// Tipping while driving would throw the truck over: only when (nearly) stopped. Lowering is always fine.
	if (!bToolActive && FMath::Abs(GetSpeedKmh()) > MaxTipSpeedKmh)
	{
		return;
	}
	Super::ToggleTool();
}

void ALIDumpTruck::BeginPlay()
{
	Super::BeginPlay();

	// The bed is modelled with its hinge at the origin, lying toward +X.
	const FLinearColor Steel(0.85f, 0.62f, 0.12f); // site yellow-ochre
	const FLinearColor SteelDark(0.55f, 0.40f, 0.08f);
	const FLinearColor Floor(0.32f, 0.30f, 0.28f);
	FLILowPolyMesh Bed;
	const float Length = 500.f;
	const float HalfWidth = 116.f;
	const float WallHeight = 95.f;
	AddPart(Bed, FVector(Length * 0.5f, 0.f, 8.f), FVector(Length * 0.5f, HalfWidth, 8.f), SteelDark);              // floor plate
	AddPart(Bed, FVector(Length * 0.5f, 0.f, 17.f), FVector(Length * 0.5f - 8.f, HalfWidth - 8.f, 1.f), Floor);     // worn floor
	for (const float Y : { -1.f, 1.f })
	{
		AddPart(Bed, FVector(Length * 0.5f, Y * (HalfWidth - 4.f), 16.f + WallHeight * 0.5f), FVector(Length * 0.5f, 4.f, WallHeight * 0.5f), Steel);
		AddPart(Bed, FVector(Length * 0.5f, Y * (HalfWidth + 1.f), 16.f + WallHeight), FVector(Length * 0.5f, 6.f, 4.f), SteelDark); // top rail
		for (int32 Rib = 1; Rib < 5; ++Rib)
		{
			AddPart(Bed, FVector(Rib * Length / 5.f, Y * (HalfWidth + 1.f), 16.f + WallHeight * 0.5f), FVector(4.f, 5.f, WallHeight * 0.5f), SteelDark);
		}
	}
	AddPart(Bed, FVector(Length - 4.f, 0.f, 16.f + WallHeight * 0.75f), FVector(4.f, HalfWidth, WallHeight * 0.75f), Steel); // front wall (taller)
	AddPart(Bed, FVector(Length + 20.f, 0.f, 16.f + WallHeight * 1.5f), FVector(28.f, HalfWidth - 10.f, 3.f), Steel);       // cab guard
	AddPart(Bed, FVector(4.f, 0.f, 16.f + WallHeight * 0.5f), FVector(4.f, HalfWidth - 8.f, WallHeight * 0.5f), SteelDark);  // tailgate
	Bed.ApplyTo(BedMesh);
	BedMesh->SetRelativeLocation(BedHinge);
	BuildCargo();
}

void ALIDumpTruck::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const float Target = bToolActive ? BedMaxAngle : 0.f;
	if (!FMath::IsNearlyEqual(BedAngle, Target))
	{
		BedAngle = FMath::FInterpConstantTo(BedAngle, Target, DeltaSeconds, BedSpeed);
		BedMesh->SetRelativeRotation(FRotator(BedAngle, 0.f, 0.f)); // pitch up: the front of the bed rises
	}

	if (HasAuthority())
	{
		if (!bToolActive)
		{
			bUnloadTried = false; // lowered again: the next raise may try once more
		}
		else if (BedAngle >= UnloadAngle)
		{
			TryUnload();
		}
	}
}

void ALIDumpTruck::BuildCargo()
{
	CargoMesh->ClearAllMeshSections();
	if (Cargo == ELIBuildMaterial::None)
	{
		return;
	}

	// Bed space: x 10..490 (hinge at 0), y -108..108, floor at z 18.
	FLILowPolyMesh Mesh;
	FRandomStream Random(1234);
	switch (Cargo)
	{
	case ELIBuildMaterial::Gravel:
	{
		const FLinearColor Stone(0.55f, 0.54f, 0.50f);
		const FLinearColor StoneLight(0.68f, 0.66f, 0.61f);
		for (int32 Index = 0; Index < 4; ++Index)
		{
			Mesh.AddBlob(FTransform(FVector(90.f + Index * 105.f, 0.f, 40.f)), FVector(90.f, 100.f, 55.f), 0.18f, Random, Stone, &StoneLight);
		}
		break;
	}
	case ELIBuildMaterial::Lumber:
	{
		const FLinearColor Wood(0.70f, 0.52f, 0.32f);
		const FLinearColor WoodDark(0.50f, 0.35f, 0.20f);
		for (int32 Layer = 0; Layer < 4; ++Layer)
		{
			for (int32 Row = 0; Row < 6; ++Row)
			{
				const float Y = -90.f + Row * 36.f;
				AddPart(Mesh, FVector(250.f + Random.FRandRange(-12.f, 12.f), Y, 30.f + Layer * 22.f), FVector(225.f, 16.f, 10.f),
					(Row + Layer) % 3 == 0 ? WoodDark : Wood);
			}
		}
		for (const float X : { 90.f, 410.f })
		{
			AddPart(Mesh, FVector(X, 0.f, 64.f), FVector(4.f, 112.f, 46.f), WoodDark); // binding straps
		}
		break;
	}
	case ELIBuildMaterial::Bricks:
	{
		const FLinearColor Brick(0.66f, 0.30f, 0.20f);
		const FLinearColor Pallet(0.58f, 0.45f, 0.28f);
		for (int32 Index = 0; Index < 4; ++Index)
		{
			for (const float Y : { -54.f, 54.f })
			{
				const FVector Center(75.f + Index * 112.f, Y, 18.f);
				AddPart(Mesh, Center + FVector(0.f, 0.f, 6.f), FVector(50.f, 50.f, 6.f), Pallet);
				AddPart(Mesh, Center + FVector(0.f, 0.f, 47.f), FVector(46.f, 46.f, 35.f), Brick);
			}
		}
		break;
	}
	default:
		break;
	}
	Mesh.ApplyTo(CargoMesh);
}

void ALIDumpTruck::BuildBody(FLILowPolyMesh& Mesh) const
{
	const FLinearColor Paint(0.85f, 0.62f, 0.12f);
	const FLinearColor Frame(0.18f, 0.18f, 0.19f);
	const FLinearColor Glass(0.10f, 0.15f, 0.20f);
	const FLinearColor Lamp(1.0f, 0.95f, 0.72f);
	const FLinearColor White(0.90f, 0.90f, 0.86f);
	const FLinearColor TailLamp(0.85f, 0.05f, 0.04f);

	// Chassis frame under the bed, with the tipping ram.
	AddPart(Mesh, FVector(-60.f, 0.f, 0.f), FVector(300.f, 60.f, 22.f), Frame);       // frame, top at z 22
	AddPart(Mesh, FVector(-60.f, 0.f, 26.f), FVector(290.f, 70.f, 4.f), Frame);       // bed rest, top at z 30
	Mesh.AddFrustum(FTransform(FVector(150.f, 0.f, 22.f)), 8, 10.f, 10.f, 30.f, White); // ram base

	// Wheel guards.
	for (const float Y : { -1.f, 1.f })
	{
		AddPart(Mesh, FVector(-212.f, Y * 100.f, 30.f), FVector(130.f, 24.f, 4.f), Frame);
		AddPart(Mesh, FVector(255.f, Y * 100.f, 32.f), FVector(52.f, 22.f, 5.f), Paint);
		AddPart(Mesh, FVector(-356.f, Y * 90.f, 0.f), FVector(4.f, 12.f, 8.f), TailLamp);
	}
	AddPart(Mesh, FVector(-356.f, 0.f, -30.f), FVector(6.f, 110.f, 8.f), Frame);      // rear bumper

	// Cab over the front axle.
	AddPart(Mesh, FVector(290.f, 0.f, 70.f), FVector(70.f, 112.f, 48.f), Paint);      // cab body, z 22..118
	AddPart(Mesh, FVector(292.f, 0.f, 150.f), FVector(66.f, 113.f, 32.f), Glass);     // windows, z 118..182
	AddPart(Mesh, FVector(292.f, 0.f, 150.f), FVector(6.f, 114.f, 32.f), Paint);      // door pillar
	AddPart(Mesh, FVector(290.f, 0.f, 186.f), FVector(70.f, 112.f, 5.f), Paint);      // roof
	AddPart(Mesh, FVector(290.f, 0.f, 196.f), FVector(18.f, 60.f, 5.f), Lamp);        // roof beacon bar
	AddPart(Mesh, FVector(361.f, 0.f, 60.f), FVector(3.f, 70.f, 30.f), Frame);        // grille
	AddPart(Mesh, FVector(362.f, -86.f, 50.f), FVector(2.f, 18.f, 10.f), Lamp);
	AddPart(Mesh, FVector(362.f, 86.f, 50.f), FVector(2.f, 18.f, 10.f), Lamp);
	AddPart(Mesh, FVector(366.f, 0.f, -10.f), FVector(8.f, 116.f, 14.f), Frame);      // front bumper
	AddPart(Mesh, FVector(330.f, -118.f, 150.f), FVector(6.f, 8.f, 16.f), Frame);     // mirrors
	AddPart(Mesh, FVector(330.f, 118.f, 150.f), FVector(6.f, 8.f, 16.f), Frame);
	Mesh.AddFrustum(FTransform(FVector(215.f, 90.f, 22.f)), 8, 7.f, 7.f, 190.f, Frame); // exhaust stack behind the cab
}

// ---------------------------------------------------------------------------
// Grain trailer
// ---------------------------------------------------------------------------

ALIGrainTrailer::ALIGrainTrailer()
{
	DisplayName = LOCTEXT("GrainTrailer", "Grain Trailer");
	SetChassisExtent(FVector(220.f, 105.f, 55.f));
	Mass = EmptyMass;
	CenterOfMassOffset = FVector(0.f, 0.f, -25.f);
	SuspensionLength = 30.f;
	RestCompression = 0.5f;
	DampingRatio = 0.6f;
	CoastDeceleration = 0.25f; // free-rolling when towed
	BrakeDeceleration = 8.f;
	TyreGrip = 1.1f;

	// One axle a little behind the middle; a small, almost grip-less jockey wheel under the drawbar.
	// Wheel bottoms: -30 - 15 - 50 = -95 and -60 - 15 - 20 = -95.
	Wheels =
	{
		MakeFarmWheel(FVector(-20.f, -118.f, -30.f), 50.f, 30.f, false, false),
		MakeFarmWheel(FVector(-20.f,  118.f, -30.f), 50.f, 30.f, false, false),
		MakeFarmWheel(FVector(250.f,    0.f, -60.f), 20.f, 10.f, false, false),
	};
	Wheels[2].GripScale = 0.05f;
	Headlight->SetIntensity(0.f); // trailers have no headlights, only the tail glow
	DustRate = 8.f;
	DragArea = 1.2f;

	Coupling = CreateDefaultSubobject<UPhysicsConstraintComponent>(TEXT("Coupling"));
	Coupling->SetupAttachment(Chassis);
	Coupling->SetRelativeLocation(DrawbarTip);
	Coupling->SetLinearXLimit(ELinearConstraintMotion::LCM_Locked, 0.f);
	Coupling->SetLinearYLimit(ELinearConstraintMotion::LCM_Locked, 0.f);
	Coupling->SetLinearZLimit(ELinearConstraintMotion::LCM_Locked, 0.f);
	Coupling->SetAngularSwing1Limit(EAngularConstraintMotion::ACM_Free, 0.f);    // yaw: turning
	Coupling->SetAngularSwing2Limit(EAngularConstraintMotion::ACM_Limited, 35.f); // pitch: bumps and slopes
	Coupling->SetAngularTwistLimit(EAngularConstraintMotion::ACM_Limited, 30.f);  // roll
	Coupling->SetDisableCollision(true);

	GrainMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("GrainMesh"));
	GrainMesh->SetupAttachment(Chassis);
	GrainMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GrainMesh->SetCanEverAffectNavigation(false);
}

void ALIGrainTrailer::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ALIGrainTrailer, HitchedTractor);
	DOREPLIFETIME(ALIGrainTrailer, GrainKg);
}

void ALIGrainTrailer::BeginPlay()
{
	Super::BeginPlay();
	UpdateGrain();
}

void ALIGrainTrailer::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority())
	{
		Unhitch();
	}
	Super::EndPlay(EndPlayReason);
}

void ALIGrainTrailer::Tick(float DeltaSeconds)
{
	// The tractor was removed (world reset): stand on the brakes again.
	if (HasAuthority() && HitchedTractor && !IsValid(HitchedTractor))
	{
		Unhitch();
	}
	// The tractor was teleported (R puts it back on its wheels) or the coupling tore apart: let go.
	if (HasAuthority() && HitchedTractor)
	{
		const FVector TowWorld = HitchedTractor->GetActorTransform().TransformPosition(HitchedTractor->GetTowPoint());
		if (FVector::DistSquared(TowWorld, GetActorTransform().TransformPosition(DrawbarTip)) > FMath::Square(300.f))
		{
			Unhitch();
		}
	}
	Super::Tick(DeltaSeconds);
}

ALITractor* ALIGrainTrailer::FindTractorInReach() const
{
	const FVector Tip = GetActorTransform().TransformPosition(DrawbarTip);
	ALITractor* Best = nullptr;
	double BestDistSq = FMath::Square(HitchReach);
	for (TActorIterator<ALITractor> It(GetWorld()); It; ++It)
	{
		if (It->GetTrailer() && It->GetTrailer() != this)
		{
			continue; // already towing another one
		}
		const double DistSq = FVector::DistSquared(It->GetActorTransform().TransformPosition(It->GetTowPoint()), Tip);
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = *It;
		}
	}
	return Best;
}

bool ALIGrainTrailer::HitchTo(ALITractor* Tractor)
{
	if (!HasAuthority() || !Tractor || HitchedTractor || !Tractor->GetChassis() || (Tractor->GetTrailer() && Tractor->GetTrailer() != this))
	{
		return false;
	}

	// Line the trailer up straight behind the tractor, drawbar on the coupling.
	const FTransform TractorTransform = Tractor->GetActorTransform();
	const FRotator Rotation(0.f, TractorTransform.Rotator().Yaw, 0.f);
	const FVector TowWorld = TractorTransform.TransformPosition(Tractor->GetTowPoint());
	const FVector Location = TowWorld - Rotation.RotateVector(DrawbarTip);
	SetActorLocationAndRotation(Location, Rotation, false, nullptr, ETeleportType::TeleportPhysics);
	Chassis->SetPhysicsLinearVelocity(Tractor->GetChassis()->GetPhysicsLinearVelocity());
	Chassis->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);

	Coupling->SetWorldLocationAndRotation(TowWorld, Rotation);
	Coupling->SetConstrainedComponents(Tractor->GetChassis(), NAME_None, Chassis, NAME_None);

	HitchedTractor = Tractor;
	Tractor->SetTrailer(this);
	ForceNetUpdate();
	UE_LOG(LogLostIsland, Log, TEXT("%s hitched to %s."), *GetName(), *Tractor->GetName());
	return true;
}

void ALIGrainTrailer::Unhitch()
{
	if (!HasAuthority())
	{
		return;
	}
	Coupling->BreakConstraint();
	if (HitchedTractor && IsValid(HitchedTractor) && HitchedTractor->GetTrailer() == this)
	{
		HitchedTractor->SetTrailer(nullptr);
	}
	HitchedTractor = nullptr;
	ForceNetUpdate();
}

void ALIGrainTrailer::ApplySaveState(uint8 Mode, int32 Amount)
{
	if (HasAuthority())
	{
		GrainKg = FMath::Clamp(Amount, 0, CapacityKg);
		UpdateGrain();
	}
}

int32 ALIGrainTrailer::AddGrain(int32 Kg)
{
	if (!HasAuthority() || Kg <= 0)
	{
		return 0;
	}
	const int32 Fitted = FMath::Min(Kg, CapacityKg - GrainKg);
	if (Fitted > 0)
	{
		GrainKg += Fitted;
		UpdateGrain();
	}
	return Fitted;
}

int32 ALIGrainTrailer::EmptyGrain()
{
	if (!HasAuthority())
	{
		return 0;
	}
	const int32 Amount = GrainKg;
	GrainKg = 0;
	UpdateGrain();
	return Amount;
}

void ALIGrainTrailer::UpdateGrain()
{
	// Heavier when full (the suspension stiffness follows Mass too).
	Mass = EmptyMass + GrainKg;
	if (Chassis->IsSimulatingPhysics() || HasActorBegunPlay())
	{
		Chassis->SetMassOverrideInKg(NAME_None, Mass, true);
	}

	const int32 Step = FMath::CeilToInt(20.f * GrainKg / FMath::Max(1, CapacityKg)); // 5% steps
	if (Step == BuiltFillStep)
	{
		return;
	}
	BuiltFillStep = Step;
	GrainMesh->ClearAllMeshSections();
	if (Step <= 0)
	{
		return;
	}
	// Bin inside: x -205..205, y -97..97, floor at z 60 (box top 55 + floor plate), walls up to 160.
	const float Fill = Step / 20.f;
	const float Height = 100.f * Fill;
	FLILowPolyMesh Mesh;
	const FLinearColor Grain(0.86f, 0.70f, 0.32f);
	const FLinearColor GrainLight(0.95f, 0.82f, 0.45f);
	AddPart(Mesh, FVector(0.f, 0.f, 60.f + Height * 0.5f), FVector(203.f, 95.f, Height * 0.5f), Grain);
	FRandomStream Random(77);
	Mesh.AddBlob(FTransform(FVector(0.f, 0.f, 60.f + Height)), FVector(170.f, 80.f, 12.f + 18.f * Fill), 0.12f, Random, GrainLight, &GrainLight, 0.7f, 0);
	Mesh.ApplyTo(GrainMesh);
}

bool ALIGrainTrailer::CanInteract_Implementation(APawn* InstigatorPawn) const
{
	return InstigatorPawn != nullptr; // never driven: E hitches or unhitches
}

FText ALIGrainTrailer::GetInteractionPrompt_Implementation(APawn* InstigatorPawn) const
{
	const FText Load = FText::Format(LOCTEXT("TrailerLoad", "{0} / {1} kg"), FText::AsNumber(GrainKg), FText::AsNumber(CapacityKg));
	if (HitchedTractor)
	{
		return FText::Format(LOCTEXT("UnhitchPrompt", "Unhitch grain trailer ({0})"), Load);
	}
	if (FindTractorInReach())
	{
		return FText::Format(LOCTEXT("HitchPrompt", "Hitch grain trailer to the tractor ({0})"), Load);
	}
	return FText::Format(LOCTEXT("TrailerIdle", "Grain trailer ({0}): back the tractor up to the drawbar to hitch it"), Load);
}

void ALIGrainTrailer::Interact_Implementation(APawn* InstigatorPawn)
{
	if (!HasAuthority())
	{
		return;
	}
	ALIPlayerController* PC = InstigatorPawn ? Cast<ALIPlayerController>(InstigatorPawn->GetController()) : nullptr;
	if (HitchedTractor)
	{
		Unhitch();
		if (PC)
		{
			PC->ClientShowMessage(LOCTEXT("Unhitched", "Trailer unhitched."));
		}
		return;
	}
	ALITractor* Tractor = FindTractorInReach();
	if (Tractor && HitchTo(Tractor))
	{
		if (PC)
		{
			PC->ClientShowMessage(LOCTEXT("Hitched", "Trailer hitched. The harvester now fills it."));
		}
	}
	else if (PC)
	{
		PC->ClientShowMessage(LOCTEXT("NoTractorNear", "Back the tractor up until its coupling is at the trailer's drawbar."));
	}
}

void ALIGrainTrailer::BuildBody(FLILowPolyMesh& Mesh) const
{
	const FLinearColor Paint(0.14f, 0.45f, 0.50f); // teal like the tractor
	const FLinearColor Cream(0.92f, 0.86f, 0.70f);
	const FLinearColor Iron(0.20f, 0.20f, 0.21f);
	const FLinearColor TailLamp(0.85f, 0.05f, 0.04f);

	AddPart(Mesh, FVector(0.f, 0.f, -20.f), FVector(200.f, 50.f, 14.f), Iron);       // frame
	AddPart(Mesh, FVector(0.f, 0.f, 50.f), FVector(215.f, 105.f, 8.f), Iron);        // bin floor, top at 58
	for (const float Y : { -1.f, 1.f })
	{
		AddPart(Mesh, FVector(0.f, Y * 101.f, 110.f), FVector(215.f, 4.f, 52.f), Paint); // side walls, up to 162
		AddPart(Mesh, FVector(0.f, Y * 103.f, 162.f), FVector(217.f, 6.f, 4.f), Cream);  // top rail
		AddPart(Mesh, FVector(-20.f, Y * 118.f, 40.f), FVector(60.f, 18.f, 4.f), Iron);  // mudguards
		AddPart(Mesh, FVector(-219.f, Y * 80.f, 20.f), FVector(3.f, 12.f, 8.f), TailLamp);
	}
	AddPart(Mesh, FVector(211.f, 0.f, 110.f), FVector(4.f, 97.f, 52.f), Paint);      // front wall
	AddPart(Mesh, FVector(-211.f, 0.f, 110.f), FVector(4.f, 97.f, 52.f), Cream);     // tailgate
	AddPart(Mesh, FVector(-20.f, 0.f, -30.f), FVector(10.f, 112.f, 7.f), Iron);      // axle

	// Drawbar from the frame to the coupling eye, and the jockey wheel's leg.
	const FVector From(200.f, 0.f, -20.f);
	const FVector To = DrawbarTip;
	const FVector Mid = (From + To) * 0.5f;
	const float Length = FVector::Dist(From, To);
	const FRotator Pitch = (To - From).Rotation();
	Mesh.AddBox(FTransform(Pitch, Mid), FVector(Length * 0.5f, 7.f, 7.f), Iron);
	Mesh.AddBox(FTransform(To), FVector(10.f, 10.f, 5.f), Iron);                     // eye
	AddPart(Mesh, FVector(250.f, 0.f, -40.f), FVector(4.f, 4.f, 22.f), Iron);        // jockey leg
}

#undef LOCTEXT_NAMESPACE
