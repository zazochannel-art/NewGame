// Copyright LOST ISLAND Team. All Rights Reserved.

#include "Visuals/LILowPolyMesh.h"

#include "LostIsland.h"
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
		if (!bOk)
		{
			UE_LOG(LogLostIsland, Warning, TEXT("Water material: some connections failed; using the simple water."));
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

bool LIUseRealisticStyle()
{
	return CVarLIRealisticStyle.GetValueOnGameThread() != 0 && LIGetVertexColorMaterial() != nullptr;
}
