// Copyright LOST ISLAND Team. All Rights Reserved.

#include "Vehicles/LIFarmVehicles.h"

#include "Visuals/LILowPolyMesh.h"

#include "Components/BoxComponent.h"
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

	CameraDistance = 800.f;
	CameraHeight = 260.f;
	ExitOffset = FVector(55.f, -(62.f + 115.f), 0.f);
	SeatOffset = FVector(-50.f, 0.f, 60.f);

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

#undef LOCTEXT_NAMESPACE
