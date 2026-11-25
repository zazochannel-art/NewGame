// Copyright LOST ISLAND Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "LISettingsMenu.generated.h"

class AHUD;
class ALIPlayerController;
class UCanvas;

/**
 * ULISettingsMenu
 *
 * The in-game menu (Esc, P or the gamepad View/Back button), drawn on the canvas HUD like the rest of the UI:
 * Resume, graphics (quality preset, window mode, resolution, VSync, frame limit, render scale),
 * controls (mouse sensitivity, invert Y, field of view), FPS counter, Save game, Quit.
 *
 * Owned by the local ALIPlayerController, which feeds it the keys (Up/Down/W/S to pick a row, Left/Right/A/D to
 * change it, Enter/Space to activate, Esc to close) and mouse clicks (via HUD hit boxes).
 * Every change is applied and saved at once (GameUserSettings.ini). A standalone game pauses while it is open.
 */
UCLASS()
class LOSTISLAND_API ULISettingsMenu : public UObject
{
	GENERATED_BODY()

public:
	enum class ERow : uint8
	{
		Resume,
		Quality,
		WindowMode,
		Resolution,
		VSync,
		FrameLimit,
		RenderScale,
		MouseSensitivity,
		InvertY,
		FieldOfView,
		ShowFps,
		SaveGame,
		Quit,
		Count
	};

	void Init(ALIPlayerController* InOwner);

	bool IsOpen() const { return bOpen; }
	void SetOpen(bool bInOpen);

	// Input (from the player controller).
	void MoveSelection(int32 Direction);
	void ChangeValue(int32 Direction);
	void Activate();

	/** Mouse: a hit box named "Menu_<row>", "Menu_<row>_L" or "Menu_<row>_R" was clicked / hovered. */
	void OnHitBoxClick(FName BoxName);
	void OnHitBoxHover(FName BoxName);

	/** Draw the panel and register its hit boxes (called from the HUD while open). */
	void Draw(AHUD* HUD, UCanvas* Canvas);

private:
	bool IsRowVisible(ERow Row) const;
	FText GetRowLabel(ERow Row) const;
	FText GetRowValue(ERow Row) const;
	bool HasValue(ERow Row) const;
	bool ParseHitBox(FName BoxName, ERow& OutRow, int32& OutDirection) const;

	void ApplyGraphics();

	UPROPERTY(Transient)
	TObjectPtr<ALIPlayerController> Owner;

	bool bOpen = false;
	ERow Selected = ERow::Resume;
};
