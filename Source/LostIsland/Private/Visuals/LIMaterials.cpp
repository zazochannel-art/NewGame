// Copyright LOST ISLAND Team. All Rights Reserved.

#include "Visuals/LILowPolyMesh.h"

#include "LostIsland.h"
#include "Visuals/LIVisualAssets.h"
#include "Engine/Texture.h"
#include "HAL/IConsoleManager.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "UObject/Package.h"

#if WITH_EDITOR
#include "MaterialEditingLibrary.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionDepthFade.h"
#include "Materials/MaterialExpressionFresnel.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionOneMinus.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionPanner.h"
#include "Materials/MaterialExpressionTextureBase.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#endif

/**
 * Realistic materials, built in code.
 *
 * The project has no .uasset materials, and the engine ships no lit material that reads vertex colours, so in
 * editor builds (PIE and "Standalone Game" from the editor) the two materials below are generated at startup
 * with UMaterialEditingLibrary and compiled like any other material. In a packaged build they don't exist and
 * every caller falls back to the tinted BasicShapeMaterial look. Once real material assets exist, point these
 * functions at them instead.
 */

static TAutoConsoleVariable<int32> CVarLIRealisticStyle(
	TEXT("li.RealisticStyle"), 1,
	TEXT("1 = smooth vertex-coloured models, translucent water and volumetric clouds (editor builds). ")
	TEXT("0 = faceted low-poly look. Takes effect for objects built after the change (e.g. LIRegenerateIsland)."),
	ECVF_Default);

namespace
{
#if WITH_EDITOR
	template <typename T>
	T* AddExpression(UMaterial* Material, int32 X, int32 Y)
	{
		return Cast<T>(UMaterialEditingLibrary::CreateMaterialExpression(Material, T::StaticClass(), X, Y));
	}

	UMaterialExpressionConstant* AddScalar(UMaterial* Material, float Value, int32 X, int32 Y)
	{
		UMaterialExpressionConstant* Constant = AddExpression<UMaterialExpressionConstant>(Material, X, Y);
		if (Constant)
		{
			Constant->R = Value;
		}
		return Constant;
	}

	UMaterialExpressionConstant3Vector* AddColor(UMaterial* Material, const FLinearColor& Value, int32 X, int32 Y)
	{
		UMaterialExpressionConstant3Vector* Constant = AddExpression<UMaterialExpressionConstant3Vector>(Material, X, Y);
		if (Constant)
		{
			Constant->Constant = Value;
		}
		return Constant;
	}

	UMaterialExpressionTextureSample* AddTextureSample(UMaterial* Material, UTexture* Texture, UMaterialExpression* UVs, int32 X, int32 Y)
	{
		UMaterialExpressionTextureSample* Sample = AddExpression<UMaterialExpressionTextureSample>(Material, X, Y);
		if (Sample)
		{
			Sample->Texture = Texture;
			Sample->SamplerType = UMaterialExpressionTextureBase::GetSamplerTypeForTexture(Texture);
			if (UVs)
			{
				UMaterialEditingLibrary::ConnectMaterialExpressions(UVs, TEXT(""), Sample, TEXT("UVs"));
			}
		}
		return Sample;
	}

	/** Sum of Inputs[i] * VertexColor.channel[i] (R, G, B, A). */
	UMaterialExpression* BlendByVertexColor(UMaterial* Material, UMaterialExpressionVertexColor* VertexColor,
		UMaterialExpression* const (&Inputs)[4], int32 X, int32 Y)
	{
		static const TCHAR* Channels[4] = { TEXT("R"), TEXT("G"), TEXT("B"), TEXT("A") };
		UMaterialExpression* Sum = nullptr;
		for (int32 Index = 0; Index < 4; ++Index)
		{
			UMaterialExpressionMultiply* Weighted = AddExpression<UMaterialExpressionMultiply>(Material, X, Y + Index * 120);
			if (!Weighted)
			{
				return nullptr;
			}
			UMaterialEditingLibrary::ConnectMaterialExpressions(Inputs[Index], TEXT(""), Weighted, TEXT("A"));
			UMaterialEditingLibrary::ConnectMaterialExpressions(VertexColor, Channels[Index], Weighted, TEXT("B"));
			if (!Sum)
			{
				Sum = Weighted;
				continue;
			}
			UMaterialExpressionAdd* Added = AddExpression<UMaterialExpressionAdd>(Material, X + 200, Y + Index * 120);
			if (!Added)
			{
				return nullptr;
			}
			UMaterialEditingLibrary::ConnectMaterialExpressions(Sum, TEXT(""), Added, TEXT("A"));
			UMaterialEditingLibrary::ConnectMaterialExpressions(Weighted, TEXT(""), Added, TEXT("B"));
			Sum = Added;
		}
		return Sum;
	}

	UMaterial* NewTransientMaterial(const TCHAR* Name)
	{
		UMaterial* Material = NewObject<UMaterial>(GetTransientPackage(), Name, RF_Transient);
		Material->AddToRoot(); // lives for the editor session; rebuilt on the next launch
		return Material;
	}

	/** Opaque, lit: BaseColor = vertex colour, rough and only slightly specular (sand, bark, leaves, stone). */
	UMaterial* BuildVertexColorMaterial()
	{
		UMaterial* Material = NewTransientMaterial(TEXT("M_LI_VertexColorLit"));

		UMaterialExpressionVertexColor* VertexColor = AddExpression<UMaterialExpressionVertexColor>(Material, -400, 0);
		UMaterialExpressionConstant* Roughness = AddScalar(Material, 0.82f, -400, 200);
		UMaterialExpressionConstant* Specular = AddScalar(Material, 0.3f, -400, 300);
		if (!VertexColor || !Roughness || !Specular)
		{
			return nullptr;
		}

		bool bOk = UMaterialEditingLibrary::ConnectMaterialProperty(VertexColor, TEXT(""), MP_BaseColor);
		bOk &= UMaterialEditingLibrary::ConnectMaterialProperty(Roughness, TEXT(""), MP_Roughness);
		bOk &= UMaterialEditingLibrary::ConnectMaterialProperty(Specular, TEXT(""), MP_Specular);
		if (!bOk)
		{
			return nullptr;
		}

		UMaterialEditingLibrary::RecompileMaterial(Material);
		return Material;
	}

	/**
	 * Translucent sea: clear and turquoise over sand near the shore, deep blue-green further out (depth fade
	 * against the seabed), a white foam line where the water meets the beach, glossy with a Fresnel sheen.
	 */
	UMaterial* BuildWaterMaterial()
	{
		UMaterial* Material = NewTransientMaterial(TEXT("M_LI_Water"));
		Material->BlendMode = BLEND_Translucent;
		Material->TranslucencyLightingMode = TLM_SurfacePerPixelLighting;

		// Depth below the surface, 0 at the shore -> 1 at ~5 m of water.
		UMaterialExpressionDepthFade* Depth = AddExpression<UMaterialExpressionDepthFade>(Material, -900, 0);
		// Thin band right at the waterline -> foam.
		UMaterialExpressionDepthFade* Shore = AddExpression<UMaterialExpressionDepthFade>(Material, -900, 300);
		UMaterialExpressionOneMinus* Foam = AddExpression<UMaterialExpressionOneMinus>(Material, -700, 300);

		UMaterialExpressionConstant3Vector* Shallow = AddColor(Material, FLinearColor(0.10f, 0.55f, 0.52f), -700, -200);
		UMaterialExpressionConstant3Vector* Deep = AddColor(Material, FLinearColor(0.01f, 0.09f, 0.16f), -700, -100);
		UMaterialExpressionConstant3Vector* FoamColor = AddColor(Material, FLinearColor(0.85f, 0.90f, 0.90f), -500, -300);
		UMaterialExpressionLinearInterpolate* WaterColor = AddExpression<UMaterialExpressionLinearInterpolate>(Material, -500, -150);
		UMaterialExpressionLinearInterpolate* BaseColor = AddExpression<UMaterialExpressionLinearInterpolate>(Material, -300, -150);

		UMaterialExpressionLinearInterpolate* DepthOpacity = AddExpression<UMaterialExpressionLinearInterpolate>(Material, -500, 100);
		UMaterialExpressionLinearInterpolate* Opacity = AddExpression<UMaterialExpressionLinearInterpolate>(Material, -300, 100);
		UMaterialExpressionFresnel* Fresnel = AddExpression<UMaterialExpressionFresnel>(Material, -700, 500);
		UMaterialExpressionLinearInterpolate* Roughness = AddExpression<UMaterialExpressionLinearInterpolate>(Material, -300, 500);
		UMaterialExpressionConstant* Specular = AddScalar(Material, 0.9f, -300, 650);

		if (!Depth || !Shore || !Foam || !Shallow || !Deep || !FoamColor || !WaterColor || !BaseColor
			|| !DepthOpacity || !Opacity || !Fresnel || !Roughness || !Specular)
		{
			return nullptr;
		}

		Depth->FadeDistanceDefault = 500.f;
		Shore->FadeDistanceDefault = 22.f;
		DepthOpacity->ConstA = 0.28f; // clear over the sand
		DepthOpacity->ConstB = 0.94f; // nearly opaque in deep water
		Opacity->ConstB = 1.f;        // foam is opaque
		Roughness->ConstA = 0.04f;    // glossy facing the camera
		Roughness->ConstB = 0.12f;    // a little softer at grazing angles

		bool bOk = true;
		bOk &= UMaterialEditingLibrary::ConnectMaterialExpressions(Shore, TEXT(""), Foam, TEXT(""));

		bOk &= UMaterialEditingLibrary::ConnectMaterialExpressions(Shallow, TEXT(""), WaterColor, TEXT("A"));
		bOk &= UMaterialEditingLibrary::ConnectMaterialExpressions(Deep, TEXT(""), WaterColor, TEXT("B"));
		bOk &= UMaterialEditingLibrary::ConnectMaterialExpressions(Depth, TEXT(""), WaterColor, TEXT("Alpha"));
		bOk &= UMaterialEditingLibrary::ConnectMaterialExpressions(WaterColor, TEXT(""), BaseColor, TEXT("A"));
		bOk &= UMaterialEditingLibrary::ConnectMaterialExpressions(FoamColor, TEXT(""), BaseColor, TEXT("B"));
		bOk &= UMaterialEditingLibrary::ConnectMaterialExpressions(Foam, TEXT(""), BaseColor, TEXT("Alpha"));

		bOk &= UMaterialEditingLibrary::ConnectMaterialExpressions(Depth, TEXT(""), DepthOpacity, TEXT("Alpha"));
		bOk &= UMaterialEditingLibrary::ConnectMaterialExpressions(DepthOpacity, TEXT(""), Opacity, TEXT("A"));
		bOk &= UMaterialEditingLibrary::ConnectMaterialExpressions(Foam, TEXT(""), Opacity, TEXT("Alpha"));

		bOk &= UMaterialEditingLibrary::ConnectMaterialExpressions(Fresnel, TEXT(""), Roughness, TEXT("Alpha"));

		bOk &= UMaterialEditingLibrary::ConnectMaterialProperty(BaseColor, TEXT(""), MP_BaseColor);
		bOk &= UMaterialEditingLibrary::ConnectMaterialProperty(Opacity, TEXT(""), MP_Opacity);
		bOk &= UMaterialEditingLibrary::ConnectMaterialProperty(Roughness, TEXT(""), MP_Roughness);
		bOk &= UMaterialEditingLibrary::ConnectMaterialProperty(Specular, TEXT(""), MP_Specular);

		// Moving waves: two copies of a water normal map, panned in different directions at different scales
		// (UVs from world XY so the huge water plane tiles correctly), softened toward flat.
		if (UTexture* WaveTexture = ULIVisualAssets::LoadOptional<UTexture>(ULIVisualAssets::Get().WaterNormalTexture))
		{
			UMaterialExpressionWorldPosition* WorldPosition = AddExpression<UMaterialExpressionWorldPosition>(Material, -1500, 800);
			UMaterialExpressionComponentMask* XY = AddExpression<UMaterialExpressionComponentMask>(Material, -1300, 800);
			UMaterialExpressionDivide* LargeScale = AddExpression<UMaterialExpressionDivide>(Material, -1100, 750);
			UMaterialExpressionDivide* SmallScale = AddExpression<UMaterialExpressionDivide>(Material, -1100, 900);
			UMaterialExpressionPanner* LargePan = AddExpression<UMaterialExpressionPanner>(Material, -900, 750);
			UMaterialExpressionPanner* SmallPan = AddExpression<UMaterialExpressionPanner>(Material, -900, 900);
			UMaterialExpressionConstant3Vector* Flat = AddColor(Material, FLinearColor(0.f, 0.f, 1.f), -500, 700);
			UMaterialExpressionAdd* Waves = AddExpression<UMaterialExpressionAdd>(Material, -500, 850);
			UMaterialExpressionLinearInterpolate* Normal = AddExpression<UMaterialExpressionLinearInterpolate>(Material, -300, 800);
			if (WorldPosition && XY && LargeScale && SmallScale && LargePan && SmallPan && Flat && Waves && Normal)
			{
				XY->R = true;
				XY->G = true;
				LargeScale->ConstB = 1400.f; // one repeat per 14 m
				SmallScale->ConstB = 520.f;  // one repeat per 5.2 m
				LargePan->SpeedX = 0.012f;
				LargePan->SpeedY = 0.007f;
				SmallPan->SpeedX = -0.018f;
				SmallPan->SpeedY = 0.011f;
				Normal->ConstAlpha = 0.45f;

				UMaterialEditingLibrary::ConnectMaterialExpressions(WorldPosition, TEXT(""), XY, TEXT(""));
				UMaterialEditingLibrary::ConnectMaterialExpressions(XY, TEXT(""), LargeScale, TEXT("A"));
				UMaterialEditingLibrary::ConnectMaterialExpressions(XY, TEXT(""), SmallScale, TEXT("A"));
				UMaterialEditingLibrary::ConnectMaterialExpressions(LargeScale, TEXT(""), LargePan, TEXT("Coordinate"));
				UMaterialEditingLibrary::ConnectMaterialExpressions(SmallScale, TEXT(""), SmallPan, TEXT("Coordinate"));
				UMaterialExpressionTextureSample* LargeWaves = AddTextureSample(Material, WaveTexture, LargePan, -700, 750);
				UMaterialExpressionTextureSample* SmallWaves = AddTextureSample(Material, WaveTexture, SmallPan, -700, 900);
				if (LargeWaves && SmallWaves)
				{
					UMaterialEditingLibrary::ConnectMaterialExpressions(LargeWaves, TEXT(""), Waves, TEXT("A"));
					UMaterialEditingLibrary::ConnectMaterialExpressions(SmallWaves, TEXT(""), Waves, TEXT("B"));
					UMaterialEditingLibrary::ConnectMaterialExpressions(Flat, TEXT(""), Normal, TEXT("A"));
					UMaterialEditingLibrary::ConnectMaterialExpressions(Waves, TEXT(""), Normal, TEXT("B"));
					UMaterialEditingLibrary::ConnectMaterialProperty(Normal, TEXT(""), MP_Normal);
				}
			}
		}

		if (!bOk)
		{
			UE_LOG(LogLostIsland, Warning, TEXT("Water material: some connections failed; using the simple water."));
			return nullptr;
		}

		UMaterialEditingLibrary::RecompileMaterial(Material);
		return Material;
	}

	/**
	 * Photo-textured ground: grass, rock, sand and forest floor (colour + normal maps) blended by the terrain's
	 * vertex colour weights, tiled in world space, with optional large-scale variation against visible tiling.
	 */
	UMaterial* BuildTexturedTerrainMaterial()
	{
		const ULIVisualAssets& Assets = ULIVisualAssets::Get();
		// Order = vertex colour channel: R grass, G rock, B sand, A forest floor.
		UTexture* Colors[4] = {
			ULIVisualAssets::LoadOptional<UTexture>(Assets.GrassColorTexture),
			ULIVisualAssets::LoadOptional<UTexture>(Assets.RockColorTexture),
			ULIVisualAssets::LoadOptional<UTexture>(Assets.SandColorTexture),
			ULIVisualAssets::LoadOptional<UTexture>(Assets.ForestFloorColorTexture) };
		UTexture* Normals[4] = {
			ULIVisualAssets::LoadOptional<UTexture>(Assets.GrassNormalTexture),
			ULIVisualAssets::LoadOptional<UTexture>(Assets.RockNormalTexture),
			ULIVisualAssets::LoadOptional<UTexture>(Assets.SandNormalTexture),
			ULIVisualAssets::LoadOptional<UTexture>(Assets.ForestFloorNormalTexture) };
		for (int32 Index = 0; Index < 4; ++Index)
		{
			if (!Colors[Index] || !Normals[Index])
			{
				return nullptr; // Starter Content (or the configured textures) not in the project
			}
		}

		UMaterial* Material = NewTransientMaterial(TEXT("M_LI_TerrainTextured"));
		UMaterialExpressionVertexColor* VertexColor = AddExpression<UMaterialExpressionVertexColor>(Material, -1400, 0);
		UMaterialExpressionTextureCoordinate* UV = AddExpression<UMaterialExpressionTextureCoordinate>(Material, -1400, -300);
		if (!VertexColor || !UV)
		{
			return nullptr;
		}
		// Terrain UVs are world XY / 1000 cm, i.e. one UV unit = 10 m.
		const float Tiling = 10.f / FMath::Max(0.5f, Assets.TerrainTextureTileMeters);
		UV->UTiling = Tiling;
		UV->VTiling = Tiling;

		UMaterialExpression* ColorSamples[4];
		UMaterialExpression* NormalSamples[4];
		for (int32 Index = 0; Index < 4; ++Index)
		{
			ColorSamples[Index] = AddTextureSample(Material, Colors[Index], UV, -1100, -600 + Index * 250);
			NormalSamples[Index] = AddTextureSample(Material, Normals[Index], UV, -1100, 500 + Index * 250);
			if (!ColorSamples[Index] || !NormalSamples[Index])
			{
				return nullptr;
			}
		}

		UMaterialExpression* BaseColor = BlendByVertexColor(Material, VertexColor, ColorSamples, -800, -600);
		UMaterialExpression* Normal = BlendByVertexColor(Material, VertexColor, NormalSamples, -800, 500);
		if (!BaseColor || !Normal)
		{
			return nullptr;
		}

		// Break up tiling with a large, slow brightness variation.
		if (UTexture* Macro = ULIVisualAssets::LoadOptional<UTexture>(Assets.MacroVariationTexture))
		{
			UMaterialExpressionTextureCoordinate* MacroUV = AddExpression<UMaterialExpressionTextureCoordinate>(Material, -1400, -900);
			UMaterialExpressionLinearInterpolate* Variation = AddExpression<UMaterialExpressionLinearInterpolate>(Material, -500, -900);
			UMaterialExpressionMultiply* Varied = AddExpression<UMaterialExpressionMultiply>(Material, -300, -700);
			if (MacroUV && Variation && Varied)
			{
				MacroUV->UTiling = 0.25f; // one repeat per 40 m
				MacroUV->VTiling = 0.25f;
				Variation->ConstA = 0.8f;
				Variation->ConstB = 1.15f;
				if (UMaterialExpressionTextureSample* MacroSample = AddTextureSample(Material, Macro, MacroUV, -1100, -900))
				{
					UMaterialEditingLibrary::ConnectMaterialExpressions(MacroSample, TEXT("R"), Variation, TEXT("Alpha"));
					UMaterialEditingLibrary::ConnectMaterialExpressions(BaseColor, TEXT(""), Varied, TEXT("A"));
					UMaterialEditingLibrary::ConnectMaterialExpressions(Variation, TEXT(""), Varied, TEXT("B"));
					BaseColor = Varied;
				}
			}
		}

		UMaterialExpressionConstant* Roughness = AddScalar(Material, 0.9f, -300, 300);
		UMaterialExpressionConstant* Specular = AddScalar(Material, 0.25f, -300, 400);
		bool bOk = Roughness && Specular;
		bOk = bOk && UMaterialEditingLibrary::ConnectMaterialProperty(BaseColor, TEXT(""), MP_BaseColor);
		bOk = bOk && UMaterialEditingLibrary::ConnectMaterialProperty(Normal, TEXT(""), MP_Normal);
		bOk = bOk && UMaterialEditingLibrary::ConnectMaterialProperty(Roughness, TEXT(""), MP_Roughness);
		bOk = bOk && UMaterialEditingLibrary::ConnectMaterialProperty(Specular, TEXT(""), MP_Specular);
		if (!bOk)
		{
			UE_LOG(LogLostIsland, Warning, TEXT("Textured terrain material: connections failed; using vertex colours."));
			return nullptr;
		}

		UMaterialEditingLibrary::RecompileMaterial(Material);
		return Material;
	}
#endif // WITH_EDITOR
}

UMaterialInterface* LIGetVertexColorMaterial()
{
#if WITH_EDITOR
	static TWeakObjectPtr<UMaterial> Cached;
	static bool bTried = false;
	if (!bTried)
	{
		bTried = true;
		Cached = BuildVertexColorMaterial();
		UE_LOG(LogLostIsland, Log, TEXT("Vertex-colour material %s."), Cached.IsValid() ? TEXT("built") : TEXT("unavailable"));
	}
	return Cached.Get();
#else
	return nullptr;
#endif
}

UMaterialInterface* LIGetWaterMaterial()
{
#if WITH_EDITOR
	static TWeakObjectPtr<UMaterial> Cached;
	static bool bTried = false;
	if (!bTried)
	{
		bTried = true;
		Cached = BuildWaterMaterial();
		UE_LOG(LogLostIsland, Log, TEXT("Water material %s."), Cached.IsValid() ? TEXT("built") : TEXT("unavailable"));
	}
	return Cached.Get();
#else
	return nullptr;
#endif
}

UMaterialInterface* LIGetTexturedTerrainMaterial()
{
#if WITH_EDITOR
	static TWeakObjectPtr<UMaterial> Cached;
	static bool bTried = false;
	if (!bTried)
	{
		bTried = true;
		Cached = BuildTexturedTerrainMaterial();
		UE_LOG(LogLostIsland, Log, TEXT("Textured terrain material %s."),
			Cached.IsValid() ? TEXT("built") : TEXT("unavailable (add Starter Content for photo textures)"));
	}
	return Cached.Get();
#else
	return nullptr;
#endif
}

bool LIUseRealisticStyle()
{
	return CVarLIRealisticStyle.GetValueOnGameThread() != 0 && LIGetVertexColorMaterial() != nullptr;
}
