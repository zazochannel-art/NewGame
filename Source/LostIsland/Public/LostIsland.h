// Copyright LOST ISLAND Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Shared log category for all LOST ISLAND gameplay code. */
LOSTISLAND_API DECLARE_LOG_CATEGORY_EXTERN(LogLostIsland, Log, All);

class UMeshComponent;

/**
 * Tints a placeholder engine shape: gives material slot 0 a dynamic instance of the engine BasicShapeMaterial
 * with its "Color" parameter set. Basic shapes in UE 5.8 default to the grid material, which has no "Color"
 * parameter, so the instance is always made from BasicShapeMaterial rather than the slot's current material.
 */
LOSTISLAND_API void LITintPlaceholder(UMeshComponent* Mesh, const FLinearColor& Color);
