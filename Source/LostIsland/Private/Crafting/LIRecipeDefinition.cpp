// Copyright LOST ISLAND Team. All Rights Reserved.

#include "Crafting/LIRecipeDefinition.h"

const FPrimaryAssetType ULIRecipeDefinition::PrimaryAssetType(TEXT("LIRecipe"));

FPrimaryAssetId ULIRecipeDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(PrimaryAssetType, RecipeId.IsNone() ? GetFName() : RecipeId);
}
