// Copyright LOST ISLAND Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class AActor;
class UMaterialInterface;
class USceneComponent;
class UProceduralMeshComponent;
struct FRandomStream;

/**
 * Shared lit material for a flat colour: a dynamic instance of the engine BasicShapeMaterial with "Color" set.
 * Instances are cached by colour (8 bits per channel), so every tree with the same bark shares one material.
 */
LOSTISLAND_API UMaterialInterface* LIGetTintedMaterial(const FLinearColor& Color);

/**
 * Lit material whose base colour is the vertex colour (smooth colour gradients, baked ground shading).
 * Built in code in editor builds; null in packaged builds (see LIMaterials.cpp).
 */
LOSTISLAND_API UMaterialInterface* LIGetVertexColorMaterial();

/** Translucent sea with depth colour, shoreline foam, a glossy surface and moving waves. Null in packaged builds. */
LOSTISLAND_API UMaterialInterface* LIGetWaterMaterial();

/**
 * Photo-textured terrain (grass, forest floor, rock, sand) blended by vertex colour weights:
 * R = grass, G = rock, B = sand, A = forest floor. Null unless the textures in ULIVisualAssets exist
 * (Starter Content) and this is an editor build.
 */
LOSTISLAND_API UMaterialInterface* LIGetTexturedTerrainMaterial();

/**
 * True when the realistic style is on (console variable li.RealisticStyle, default 1) and the vertex-colour
 * material exists: smooth shading, vertex colours, translucent water, volumetric clouds.
 */
LOSTISLAND_API bool LIUseRealisticStyle();

/**
 * Creates a cosmetic procedural mesh component at runtime (no collision), attached to Parent (or the root).
 * For actors that get a code-built model without declaring a component for it.
 */
LOSTISLAND_API UProceduralMeshComponent* LICreateVisualComponent(AActor* Owner, USceneComponent* Parent = nullptr);

/** Color * (1 +- Amount), alpha 1. Used to give every model a slightly different shade. */
LOSTISLAND_API FLinearColor LIVaryColor(const FLinearColor& Color, FRandomStream& Random, float Amount);

/** Deterministic per-actor seed from a location (rounded to 0.5 m, so client and server agree). */
LOSTISLAND_API int32 LIVisualSeed(const FVector& Location, int32 Salt = 0);

/**
 * FLILowPolyMesh
 *
 * Builds flat-shaded, faceted low-poly models in code (there are no art assets yet). Geometry is grouped into
 * one section per colour; ApplyTo() turns each section into a procedural mesh section with a tinted lit material.
 *
 * Every triangle is oriented automatically: callers pass a point INSIDE the shape and the triangle is wound so
 * it faces away from that point (Unreal front faces: CrossProduct(B - A, C - A) points into the surface; the
 * same convention the terrain uses, verified in the engine). This keeps every primitive correct regardless of
 * the order its vertices were generated in, and under any transform (including mirrored scale).
 *
 * All primitives take a local transform; models are built in the owning actor's local space.
 */
struct LOSTISLAND_API FLILowPolyMesh
{
	struct FSection
	{
		FLinearColor Color = FLinearColor::White;
		TArray<FVector> Vertices;
		TArray<int32> Triangles;
		TArray<FVector> Normals;
	};

	TArray<FSection> Sections;

	/**
	 * Smooth (rounded) normals on blobs and frustum sides instead of flat facets. Defaults to the realistic
	 * style. Boxes are always flat.
	 */
	bool bSmooth = LIUseRealisticStyle();

	/**
	 * Realistic style only: vertices below this local height are darkened (fake ambient occlusion where a model
	 * meets the ground). Negative = off. Use it for models whose origin is on the ground.
	 */
	float GroundShadingHeight = -1.f;

	/**
	 * Gives every surface built with this colour a real material instead (e.g. Starter Content wood or metal),
	 * mapped with planar UVs at one repeat per metre. Null removes the override.
	 */
	void SetSurfaceMaterial(const FLinearColor& Color, UMaterialInterface* Material);

	bool IsEmpty() const { return Sections.Num() == 0; }
	int32 NumTriangles() const;

	/** One flat triangle facing away from InteriorPoint. Degenerate triangles are skipped. */
	void AddTri(const FVector& A, const FVector& B, const FVector& C, const FLinearColor& Color, const FVector& InteriorPoint);

	/** Same, with explicit per-corner normals (smooth shading). The normals must point outward. */
	void AddTriSmooth(const FVector& A, const FVector& B, const FVector& C,
		const FVector& NormalA, const FVector& NormalB, const FVector& NormalC,
		const FLinearColor& Color, const FVector& InteriorPoint);

	/** Box centred on the transform's origin. */
	void AddBox(const FTransform& Transform, const FVector& HalfExtent, const FLinearColor& Color);

	/**
	 * Cylinder / cone / frustum standing on the transform's origin along local +Z (0..Height).
	 * TopRadius 0 makes a cone. Jitter (0..0.5) randomly scales each ring vertex for a hand-made look.
	 */
	void AddFrustum(const FTransform& Transform, int32 Sides, float BottomRadius, float TopRadius, float Height,
		const FLinearColor& Color, float Jitter = 0.f, FRandomStream* Random = nullptr);

	/**
	 * Lumpy ellipsoid (an icosphere with every vertex pushed in or out by Jitter), centred on the transform's
	 * origin. Subdivisions 0 = 20 faces (berries, pebbles), 1 = 80 faces (rocks, canopies).
	 * If TopColor is set, faces whose normal points up more than TopNormalZ use it (moss, sunlit leaves).
	 */
	void AddBlob(const FTransform& Transform, const FVector& Radii, float Jitter, FRandomStream& Random,
		const FLinearColor& Color, const FLinearColor* TopColor = nullptr, float TopNormalZ = 0.7f, int32 Subdivisions = 1);

	/**
	 * Replaces all sections of Component with this geometry (no collision; models are cosmetic).
	 * Realistic style: one section with vertex colours (small per-vertex shade noise, optional ground shading)
	 * and the vertex-colour material. Otherwise: one section per colour with tinted BasicShapeMaterial.
	 * Returns false if there was nothing to apply.
	 */
	bool ApplyTo(UProceduralMeshComponent* Component) const;

private:
	FSection& FindSection(const FLinearColor& Color);

	/** Quantised colour key -> real material (see SetSurfaceMaterial). */
	TMap<uint32, UMaterialInterface*> SurfaceMaterials; // assets, kept alive by the components that use them
};
