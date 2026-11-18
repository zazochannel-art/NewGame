// Copyright LOST ISLAND Team. All Rights Reserved.

#include "Core/LIPlayerController.h"

void ALIPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalController())
	{
		SetInputMode(FInputModeGameOnly());
		SetShowMouseCursor(false);
	}
}
