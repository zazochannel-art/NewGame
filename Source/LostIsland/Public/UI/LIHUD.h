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
	void DrawInteractionPrompt(const class ALICharacter* Character, float CenterX, float CenterY);
	void DrawSurvivalBars(const class ALICharacter* Character);
	void DrawBar(float X, float Y, float Percent, const FLinearColor& Color, const FString& Label);
	void DrawInventoryPanel(const class ALICharacter* Character);
	void DrawHotbar(const class ALICharacter* Character);
	void DrawInventoryNotices(const class ALICharacter* Character);
	void DrawCraftingPanel(const class ALICharacter* Character, float X, float Y, float Height);

	/** Inventory grid layout. */
	UPROPERTY(EditDefaultsOnly, Category = "LostIsland|HUD")
	int32 InventoryColumns = 5;

	UPROPERTY(EditDefaultsOnly, Category = "LostIsland|HUD")
	float InventorySlotSize = 72.f;

	UPROPERTY(EditDefaultsOnly, Category = "LostIsland|HUD")
	FLinearColor HealthColor = FLinearColor(0.80f, 0.15f, 0.12f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category = "LostIsland|HUD")
	FLinearColor HungerColor = FLinearColor(0.85f, 0.55f, 0.15f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category = "LostIsland|HUD")
	FLinearColor ThirstColor = FLinearColor(0.20f, 0.55f, 0.90f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category = "LostIsland|HUD")
	FLinearColor StaminaColor = FLinearColor(0.35f, 0.80f, 0.30f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category = "LostIsland|HUD")
	FLinearColor ExhaustedColor = FLinearColor(0.45f, 0.45f, 0.45f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category = "LostIsland|HUD")
	FVector2D BarSize = FVector2D(220.f, 14.f);

	UPROPERTY(EditDefaultsOnly, Category = "LostIsland|HUD")
	FLinearColor PromptColor = FLinearColor(1.f, 0.95f, 0.8f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category = "LostIsland|HUD")
	FLinearColor CrosshairColor = FLinearColor(1.f, 1.f, 1.f, 0.6f);

	/** Key label shown before the prompt. Will come from input settings later. */
	UPROPERTY(EditDefaultsOnly, Category = "LostIsland|HUD")
	FText InteractKeyLabel = NSLOCTEXT("LIHUD", "InteractKey", "[E]");
};
