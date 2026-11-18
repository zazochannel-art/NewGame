// Copyright LOST ISLAND Team. All Rights Reserved.

#include "Items/LIItemDefinition.h"

const FPrimaryAssetType ULIItemDefinition::PrimaryAssetType(TEXT("LIItem"));

FPrimaryAssetId ULIItemDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(PrimaryAssetType, ItemId.IsNone() ? GetFName() : ItemId);
}
