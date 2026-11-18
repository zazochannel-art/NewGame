// Copyright LOST ISLAND Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "LIHUD.generated.h"

/**
 * ALIHUD
 *
 * Prototype canvas HUD: a centre dot plus the interaction prompt for whatever
 * ULIInteractionComponent is focusing. Will be replaced by UMG widgets
 * (bound to OnFocusChanged) once the UI milestone starts; kept asset-free so the
 * prototype runs out of the box.
 */
UCLASS()
class LOSTISLAND_API ALIHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "LostIsland|HUD")
	FLinearColor PromptColor = FLinearColor(1.f, 0.95f, 0.8f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category = "LostIsland|HUD")
	FLinearColor CrosshairColor = FLinearColor(1.f, 1.f, 1.f, 0.6f);

	/** Key label shown before the prompt. Will come from input settings later. */
	UPROPERTY(EditDefaultsOnly, Category = "LostIsland|HUD")
	FText InteractKeyLabel = NSLOCTEXT("LIHUD", "InteractKey", "[E]");
};
