// Copyright LOST ISLAND Team. All Rights Reserved.

#include "LostIsland.h"
#include "Components/MeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogLostIsland);

void LITintPlaceholder(UMeshComponent* Mesh, const FLinearColor& Color)
{
	if (!Mesh)
	{
		return;
	}
	static TWeakObjectPtr<UMaterialInterface> CachedBaseMaterial;
	UMaterialInterface* BaseMaterial = CachedBaseMaterial.Get();
	if (!BaseMaterial)
	{
		BaseMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		CachedBaseMaterial = BaseMaterial;
	}
	if (!BaseMaterial)
	{
		UE_LOG(LogLostIsland, Warning, TEXT("BasicShapeMaterial not found; %s stays untinted."), *Mesh->GetName());
		return;
	}
	if (UMaterialInstanceDynamic* MID = Mesh->CreateDynamicMaterialInstance(0, BaseMaterial))
	{
		MID->SetVectorParameterValue(TEXT("Color"), Color);
	}
}

IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, LostIsland, "LostIsland");
