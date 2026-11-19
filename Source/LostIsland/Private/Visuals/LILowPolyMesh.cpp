// Copyright LOST ISLAND Team. All Rights Reserved.

#include "Visuals/LILowPolyMesh.h"

#include "LostIsland.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Math/RandomStream.h"
#include "ProceduralMeshComponent.h"
#include "UObject/Package.h"
#include "GameFramework/Actor.h"
#include "UObject/UObjectGlobals.h"

// ---------------------------------------------------------------------------
// Shared helpers
// ---------------------------------------------------------------------------

UMaterialInterface* LIGetTintedMaterial(const FLinearColor& Color)
{
	static TWeakObjectPtr<UMaterialInterface> CachedBase;
	static TMap<uint32, TWeakObjectPtr<UMaterialInstanceDynamic>> Cache;

	const FColor Quantized = Color.ToFColor(/*bSRGB*/ false);
	const uint32 Key = Quantized.ToPackedARGB();
	if (const TWeakObjectPtr<UMaterialInstanceDynamic>* Found = Cache.Find(Key))
	{
		if (Found->IsValid())
		{
			return Found->Get();
		}
	}

	UMaterialInterface* Base = CachedBase.Get();
	if (!Base)
	{
		Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		CachedBase = Base;
	}
	if (!Base)
	{
		UE_LOG(LogLostIsland, Warning, TEXT("BasicShapeMaterial not found; low-poly models use the default material."));
		return nullptr;
	}

	// Outer = transient package; the instance lives as long as some component's material slot references it.
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, GetTransientPackage());
	if (MID)
	{
		MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(Color.R, Color.G, Color.B, 1.f));
		Cache.Add(Key, MID);
	}
	return MID;
}

FLinearColor LIVaryColor(const FLinearColor& Color, FRandomStream& Random, float Amount)
{
	const float Scale = 1.f + Random.FRandRange(-Amount, Amount);
	return FLinearColor(
		FMath::Clamp(Color.R * Scale, 0.f, 1.f),
		FMath::Clamp(Color.G * Scale, 0.f, 1.f),
		FMath::Clamp(Color.B * Scale, 0.f, 1.f),
		1.f);
}

int32 LIVisualSeed(const FVector& Location, int32 Salt)
{
	const int32 X = FMath::RoundToInt(Location.X / 50.0);
	const int32 Y = FMath::RoundToInt(Location.Y / 50.0);
	uint32 Hash = HashCombine(GetTypeHash(X), GetTypeHash(Y));
	Hash = HashCombine(Hash, GetTypeHash(Salt));
	return static_cast<int32>(Hash & 0x7FFFFFFF);
}

// ---------------------------------------------------------------------------
// Icosphere (unit radius), built once and shared
// ---------------------------------------------------------------------------

namespace
{
	struct FLIIcosphere
	{
		TArray<FVector> Vertices;
		TArray<FIntVector> Faces;
	};

	FLIIcosphere BuildIcosphere(int32 Subdivisions)
	{
		FLIIcosphere Sphere;
		const float T = (1.f + FMath::Sqrt(5.f)) * 0.5f;
		Sphere.Vertices = {
			FVector(-1.f,  T, 0.f), FVector( 1.f,  T, 0.f), FVector(-1.f, -T, 0.f), FVector( 1.f, -T, 0.f),
			FVector(0.f, -1.f,  T), FVector(0.f,  1.f,  T), FVector(0.f, -1.f, -T), FVector(0.f,  1.f, -T),
			FVector( T, 0.f, -1.f), FVector( T, 0.f,  1.f), FVector(-T, 0.f, -1.f), FVector(-T, 0.f,  1.f) };
		for (FVector& V : Sphere.Vertices)
		{
			V.Normalize();
		}
		Sphere.Faces = {
			FIntVector(0, 11, 5), FIntVector(0, 5, 1),  FIntVector(0, 1, 7),   FIntVector(0, 7, 10), FIntVector(0, 10, 11),
			FIntVector(1, 5, 9),  FIntVector(5, 11, 4), FIntVector(11, 10, 2), FIntVector(10, 7, 6), FIntVector(7, 1, 8),
			FIntVector(3, 9, 4),  FIntVector(3, 4, 2),  FIntVector(3, 2, 6),   FIntVector(3, 6, 8),  FIntVector(3, 8, 9),
			FIntVector(4, 9, 5),  FIntVector(2, 4, 11), FIntVector(6, 2, 10),  FIntVector(8, 6, 7),  FIntVector(9, 8, 1) };

		for (int32 Level = 0; Level < Subdivisions; ++Level)
		{
			TMap<uint64, int32> Midpoints;
			auto Midpoint = [&Sphere, &Midpoints](int32 A, int32 B) -> int32
			{
				const uint64 Key = (static_cast<uint64>(FMath::Min(A, B)) << 32) | static_cast<uint64>(FMath::Max(A, B));
				if (const int32* Found = Midpoints.Find(Key))
				{
					return *Found;
				}
				const int32 Index = Sphere.Vertices.Add(((Sphere.Vertices[A] + Sphere.Vertices[B]) * 0.5).GetSafeNormal());
				Midpoints.Add(Key, Index);
				return Index;
			};

			TArray<FIntVector> NewFaces;
			NewFaces.Reserve(Sphere.Faces.Num() * 4);
			for (const FIntVector& Face : Sphere.Faces)
			{
				const int32 AB = Midpoint(Face.X, Face.Y);
				const int32 BC = Midpoint(Face.Y, Face.Z);
				const int32 CA = Midpoint(Face.Z, Face.X);
				NewFaces.Add(FIntVector(Face.X, AB, CA));
				NewFaces.Add(FIntVector(Face.Y, BC, AB));
				NewFaces.Add(FIntVector(Face.Z, CA, BC));
				NewFaces.Add(FIntVector(AB, BC, CA));
			}
			Sphere.Faces = MoveTemp(NewFaces);
		}
		return Sphere;
	}

	const FLIIcosphere& GetIcosphere(int32 Subdivisions)
	{
		static const FLIIcosphere Level0 = BuildIcosphere(0);
		static const FLIIcosphere Level1 = BuildIcosphere(1);
		static const FLIIcosphere Level2 = BuildIcosphere(2);
		return Subdivisions <= 0 ? Level0 : (Subdivisions == 1 ? Level1 : Level2);
	}

	/** Face normal pointing away from InteriorPoint. */
	FVector OutwardNormal(const FVector& A, const FVector& B, const FVector& C, const FVector& InteriorPoint)
	{
		FVector Normal = FVector::CrossProduct(B - A, C - A).GetSafeNormal();
		if (FVector::DotProduct(Normal, (A + B + C) / 3.0 - InteriorPoint) < 0.0)
		{
			Normal = -Normal;
		}
		return Normal;
	}
}

// ---------------------------------------------------------------------------
// FLILowPolyMesh
// ---------------------------------------------------------------------------

int32 FLILowPolyMesh::NumTriangles() const
{
	int32 Count = 0;
	for (const FSection& Section : Sections)
	{
		Count += Section.Triangles.Num() / 3;
	}
	return Count;
}

FLILowPolyMesh::FSection& FLILowPolyMesh::FindSection(const FLinearColor& Color)
{
	// Quantize so near-identical shades share a section (fewer draw calls) and a cached material.
	auto Q = [](float V) { return FMath::RoundToFloat(FMath::Clamp(V, 0.f, 1.f) * 64.f) / 64.f; };
	const FLinearColor Key(Q(Color.R), Q(Color.G), Q(Color.B), 1.f);
	for (FSection& Section : Sections)
	{
		if (Section.Color.Equals(Key, 0.001f))
		{
			return Section;
		}
	}
	FSection& Section = Sections.AddDefaulted_GetRef();
	Section.Color = Key;
	return Section;
}

void FLILowPolyMesh::AddTri(const FVector& A, const FVector& InB, const FVector& InC, const FLinearColor& Color, const FVector& InteriorPoint)
{
	FVector B = InB;
	FVector C = InC;
	FVector Cross = FVector::CrossProduct(B - A, C - A);
	if (Cross.SizeSquared() < 1e-6)
	{
		return; // degenerate
	}

	// Front face in Unreal: the cross product points INTO the surface, i.e. toward the interior.
	const FVector Centroid = (A + B + C) / 3.0;
	if (FVector::DotProduct(Cross, Centroid - InteriorPoint) > 0.0)
	{
		Swap(B, C);
		Cross = -Cross;
	}
	const FVector Normal = (-Cross).GetSafeNormal();

	FSection& Section = FindSection(Color);
	for (const FVector& V : { A, B, C })
	{
		Section.Triangles.Add(Section.Vertices.Num());
		Section.Vertices.Add(V);
		Section.Normals.Add(Normal);
	}
}

void FLILowPolyMesh::AddTriSmooth(const FVector& A, const FVector& InB, const FVector& InC,
	const FVector& NormalA, const FVector& InNormalB, const FVector& InNormalC,
	const FLinearColor& Color, const FVector& InteriorPoint)
{
	FVector B = InB;
	FVector C = InC;
	FVector NormalB = InNormalB;
	FVector NormalC = InNormalC;
	const FVector Cross = FVector::CrossProduct(B - A, C - A);
	if (Cross.SizeSquared() < 1e-6)
	{
		return;
	}
	const FVector Centroid = (A + B + C) / 3.0;
	if (FVector::DotProduct(Cross, Centroid - InteriorPoint) > 0.0)
	{
		Swap(B, C);
		Swap(NormalB, NormalC);
	}

	FSection& Section = FindSection(Color);
	const FVector Corners[3] = { A, B, C };
	const FVector Normals[3] = { NormalA, NormalB, NormalC };
	for (int32 Corner = 0; Corner < 3; ++Corner)
	{
		Section.Triangles.Add(Section.Vertices.Num());
		Section.Vertices.Add(Corners[Corner]);
		Section.Normals.Add(Normals[Corner].GetSafeNormal());
	}
}

void FLILowPolyMesh::AddBox(const FTransform& Transform, const FVector& HalfExtent, const FLinearColor& Color)
{
	FVector Corners[8];
	for (int32 Index = 0; Index < 8; ++Index)
	{
		const FVector Local(
			(Index & 1) ? HalfExtent.X : -HalfExtent.X,
			(Index & 2) ? HalfExtent.Y : -HalfExtent.Y,
			(Index & 4) ? HalfExtent.Z : -HalfExtent.Z);
		Corners[Index] = Transform.TransformPosition(Local);
	}
	const FVector Center = Transform.GetLocation();

	// Each face as a 4-corner cycle.
	static const int32 Faces[6][4] = {
		{ 0, 1, 3, 2 }, { 4, 5, 7, 6 },   // -Z, +Z
		{ 0, 1, 5, 4 }, { 2, 3, 7, 6 },   // -Y, +Y
		{ 0, 2, 6, 4 }, { 1, 3, 7, 5 } }; // -X, +X
	for (const int32 (&Face)[4] : Faces)
	{
		AddTri(Corners[Face[0]], Corners[Face[1]], Corners[Face[2]], Color, Center);
		AddTri(Corners[Face[0]], Corners[Face[2]], Corners[Face[3]], Color, Center);
	}
}

void FLILowPolyMesh::AddFrustum(const FTransform& Transform, int32 Sides, float BottomRadius, float TopRadius, float Height,
	const FLinearColor& Color, float Jitter, FRandomStream* Random)
{
	Sides = FMath::Clamp(Sides, 3, 32);
	const bool bCone = TopRadius <= KINDA_SMALL_NUMBER;
	const float AngleOffset = Random ? Random->FRandRange(0.f, 2.f * PI) : 0.f;

	TArray<FVector> Bottom;
	TArray<FVector> Top;
	Bottom.SetNum(Sides);
	Top.SetNum(Sides);
	for (int32 Index = 0; Index < Sides; ++Index)
	{
		const float Angle = AngleOffset + 2.f * PI * Index / Sides;
		const float BottomJitter = (Random && Jitter > 0.f) ? 1.f + Random->FRandRange(-Jitter, Jitter) : 1.f;
		const float TopJitter = (Random && Jitter > 0.f) ? 1.f + Random->FRandRange(-Jitter, Jitter) : 1.f;
		const FVector2D Dir(FMath::Cos(Angle), FMath::Sin(Angle));
		Bottom[Index] = Transform.TransformPosition(FVector(Dir.X * BottomRadius * BottomJitter, Dir.Y * BottomRadius * BottomJitter, 0.f));
		Top[Index] = Transform.TransformPosition(FVector(Dir.X * TopRadius * TopJitter, Dir.Y * TopRadius * TopJitter, Height));
	}

	const FVector BottomCenter = Transform.TransformPosition(FVector::ZeroVector);
	const FVector TopCenter = Transform.TransformPosition(FVector(0.f, 0.f, Height));
	const FVector Interior = Transform.TransformPosition(FVector(0.f, 0.f, Height * (bCone ? 0.3f : 0.5f)));

	// Smooth side normals: radial, tilted by the taper (sides lean in when TopRadius < BottomRadius).
	TArray<FVector> SideNormals;
	if (bSmooth)
	{
		SideNormals.SetNum(Sides);
		const float Slope = Height > KINDA_SMALL_NUMBER ? (BottomRadius - TopRadius) / Height : 0.f;
		for (int32 Index = 0; Index < Sides; ++Index)
		{
			const float Angle = AngleOffset + 2.f * PI * Index / Sides;
			const FVector Local(FMath::Cos(Angle), FMath::Sin(Angle), Slope);
			SideNormals[Index] = Transform.TransformVectorNoScale(Local.GetSafeNormal());
		}
	}

	for (int32 Index = 0; Index < Sides; ++Index)
	{
		const int32 Next = (Index + 1) % Sides;
		if (bSmooth)
		{
			const FVector& NI = SideNormals[Index];
			const FVector& NN = SideNormals[Next];
			if (bCone)
			{
				AddTriSmooth(Bottom[Index], Bottom[Next], TopCenter, NI, NN, (NI + NN) * 0.5, Color, Interior);
			}
			else
			{
				AddTriSmooth(Bottom[Index], Bottom[Next], Top[Next], NI, NN, NN, Color, Interior);
				AddTriSmooth(Bottom[Index], Top[Next], Top[Index], NI, NN, NI, Color, Interior);
				AddTri(TopCenter, Top[Index], Top[Next], Color, Interior);
			}
		}
		else if (bCone)
		{
			AddTri(Bottom[Index], Bottom[Next], TopCenter, Color, Interior);
		}
		else
		{
			AddTri(Bottom[Index], Bottom[Next], Top[Next], Color, Interior);
			AddTri(Bottom[Index], Top[Next], Top[Index], Color, Interior);
			AddTri(TopCenter, Top[Index], Top[Next], Color, Interior);
		}
		AddTri(BottomCenter, Bottom[Next], Bottom[Index], Color, Interior);
	}
}

void FLILowPolyMesh::AddBlob(const FTransform& Transform, const FVector& Radii, float Jitter, FRandomStream& Random,
	const FLinearColor& Color, const FLinearColor* TopColor, float TopNormalZ, int32 Subdivisions)
{
	// Smooth (realistic) shapes get one more subdivision and a gentler, finer lumpiness.
	const FLIIcosphere& Sphere = GetIcosphere(bSmooth ? FMath::Min(Subdivisions + 1, 2) : Subdivisions);
	if (bSmooth)
	{
		Jitter *= 0.7f;
	}

	// Jitter shared vertices (not per face) so the shape stays closed.
	TArray<FVector> Points;
	Points.SetNum(Sphere.Vertices.Num());
	for (int32 Index = 0; Index < Sphere.Vertices.Num(); ++Index)
	{
		const float Scale = Jitter > 0.f ? 1.f + Random.FRandRange(-Jitter, Jitter) : 1.f;
		const FVector& Unit = Sphere.Vertices[Index];
		Points[Index] = Transform.TransformPosition(FVector(Unit.X * Radii.X, Unit.Y * Radii.Y, Unit.Z * Radii.Z) * Scale);
	}

	const FVector Center = Transform.GetLocation();

	// Smooth shading: each shared vertex gets the average of its faces' normals.
	TArray<FVector> VertexNormals;
	if (bSmooth)
	{
		VertexNormals.Init(FVector::ZeroVector, Points.Num());
		for (const FIntVector& Face : Sphere.Faces)
		{
			const FVector FaceNormal = OutwardNormal(Points[Face.X], Points[Face.Y], Points[Face.Z], Center);
			VertexNormals[Face.X] += FaceNormal;
			VertexNormals[Face.Y] += FaceNormal;
			VertexNormals[Face.Z] += FaceNormal;
		}
	}

	for (const FIntVector& Face : Sphere.Faces)
	{
		const FVector& A = Points[Face.X];
		const FVector& B = Points[Face.Y];
		const FVector& C = Points[Face.Z];
		const bool bTop = TopColor && OutwardNormal(A, B, C, Center).Z > TopNormalZ;
		const FLinearColor& FaceColor = bTop ? *TopColor : Color;
		if (bSmooth)
		{
			AddTriSmooth(A, B, C, VertexNormals[Face.X], VertexNormals[Face.Y], VertexNormals[Face.Z], FaceColor, Center);
		}
		else
		{
			AddTri(A, B, C, FaceColor, Center);
		}
	}
}

namespace
{
	uint32 LIColorKey(const FLinearColor& Color)
	{
		return Color.ToFColor(/*bSRGB*/ false).ToPackedARGB();
	}

	/** Planar UVs (1 repeat per metre) projected along the normal's dominant axis, for textured materials. */
	FVector2D LIPlanarUV(const FVector& Vertex, const FVector& Normal)
	{
		const FVector Abs = Normal.GetAbs();
		if (Abs.Z >= Abs.X && Abs.Z >= Abs.Y)
		{
			return FVector2D(Vertex.X, Vertex.Y) / 100.f;
		}
		if (Abs.X >= Abs.Y)
		{
			return FVector2D(Vertex.Y, Vertex.Z) / 100.f;
		}
		return FVector2D(Vertex.X, Vertex.Z) / 100.f;
	}
}

void FLILowPolyMesh::SetSurfaceMaterial(const FLinearColor& Color, UMaterialInterface* Material)
{
	auto Q = [](float V) { return FMath::RoundToFloat(FMath::Clamp(V, 0.f, 1.f) * 64.f) / 64.f; };
	const uint32 Key = LIColorKey(FLinearColor(Q(Color.R), Q(Color.G), Q(Color.B), 1.f));
	if (Material)
	{
		SurfaceMaterials.Add(Key, Material);
	}
	else
	{
		SurfaceMaterials.Remove(Key);
	}
}

UProceduralMeshComponent* LICreateVisualComponent(AActor* Owner, USceneComponent* Parent)
{
	if (!Owner)
	{
		return nullptr;
	}
	UProceduralMeshComponent* Component = NewObject<UProceduralMeshComponent>(Owner, NAME_None, RF_Transient);
	Component->SetupAttachment(Parent ? Parent : Owner->GetRootComponent());
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetCanEverAffectNavigation(false);
	Component->SetGenerateOverlapEvents(false);
	Component->RegisterComponent();
	Owner->AddInstanceComponent(Component);
	return Component;
}

bool FLILowPolyMesh::ApplyTo(UProceduralMeshComponent* Component) const
{
	if (!Component || IsEmpty())
	{
		return false;
	}

	Component->ClearAllMeshSections();
	const TArray<FVector2D> NoUVs;
	const TArray<FLinearColor> NoColors;
	const TArray<FProcMeshTangent> NoTangents;
	int32 NextSection = 0;

	// Surfaces with a real material (e.g. Starter Content wood): own section with planar UVs.
	auto FindSurfaceMaterial = [this](const FSection& Section) -> UMaterialInterface*
	{
		UMaterialInterface* const* Found = SurfaceMaterials.Find(LIColorKey(Section.Color));
		return Found ? *Found : nullptr;
	};
	for (const FSection& Section : Sections)
	{
		if (UMaterialInterface* Material = FindSurfaceMaterial(Section))
		{
			TArray<FVector2D> UVs;
			UVs.Reserve(Section.Vertices.Num());
			for (int32 Index = 0; Index < Section.Vertices.Num(); ++Index)
			{
				UVs.Add(LIPlanarUV(Section.Vertices[Index], Section.Normals[Index]));
			}
			Component->CreateMeshSection_LinearColor(NextSection, Section.Vertices, Section.Triangles, Section.Normals,
				UVs, NoColors, NoTangents, /*bCreateCollision*/ false);
			Component->SetMaterial(NextSection, Material);
			++NextSection;
		}
	}

	// Realistic style: everything else in one section, colour per vertex.
	UMaterialInterface* VertexColorMaterial = LIUseRealisticStyle() ? LIGetVertexColorMaterial() : nullptr;
	if (VertexColorMaterial)
	{
		TArray<FVector> Vertices;
		TArray<int32> Triangles;
		TArray<FVector> Normals;
		TArray<FLinearColor> Colors;
		for (const FSection& Section : Sections)
		{
			if (FindSurfaceMaterial(Section))
			{
				continue;
			}
			const int32 BaseIndex = Vertices.Num();
			Vertices.Append(Section.Vertices);
			Normals.Append(Section.Normals);
			for (const int32 Triangle : Section.Triangles)
			{
				Triangles.Add(BaseIndex + Triangle);
			}
			for (const FVector& Vertex : Section.Vertices)
			{
				// Gentle, position-based shade noise (identical for vertices shared between faces) plus
				// darker shading near the ground.
				const uint32 Hash = HashCombine(GetTypeHash(FMath::RoundToInt(Vertex.X * 0.5)),
					HashCombine(GetTypeHash(FMath::RoundToInt(Vertex.Y * 0.5)), GetTypeHash(FMath::RoundToInt(Vertex.Z * 0.5))));
				float Shade = 0.93f + 0.14f * static_cast<float>(Hash % 1000) / 999.f;
				if (GroundShadingHeight > 0.f)
				{
					Shade *= FMath::Lerp(0.55f, 1.f, FMath::Clamp(Vertex.Z / GroundShadingHeight, 0.f, 1.f));
				}
				Colors.Add(FLinearColor(Section.Color.R * Shade, Section.Color.G * Shade, Section.Color.B * Shade, 1.f));
			}
		}
		if (Vertices.Num() > 0)
		{
			Component->CreateMeshSection_LinearColor(NextSection, Vertices, Triangles, Normals, NoUVs, Colors, NoTangents,
				/*bCreateCollision*/ false, /*bSRGBConversion*/ false);
			Component->SetMaterial(NextSection, VertexColorMaterial);
		}
		return true;
	}

	for (const FSection& Section : Sections)
	{
		if (FindSurfaceMaterial(Section))
		{
			continue;
		}
		Component->CreateMeshSection_LinearColor(NextSection, Section.Vertices, Section.Triangles, Section.Normals,
			NoUVs, NoColors, NoTangents, /*bCreateCollision*/ false);
		if (UMaterialInterface* Material = LIGetTintedMaterial(Section.Color))
		{
			Component->SetMaterial(NextSection, Material);
		}
		++NextSection;
	}
	return true;
}
