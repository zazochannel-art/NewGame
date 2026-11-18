// Copyright LOST ISLAND Team. All Rights Reserved.

#include "Interaction/LIInteractable.h"

#define LOCTEXT_NAMESPACE "LIInteractable"

FText ILIInteractable::GetInteractionPrompt_Implementation(APawn* InstigatorPawn) const
{
	return LOCTEXT("DefaultPrompt", "Interact");
}

#undef LOCTEXT_NAMESPACE
