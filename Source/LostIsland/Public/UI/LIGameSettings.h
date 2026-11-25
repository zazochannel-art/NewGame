// Copyright LOST ISLAND Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * FLIGameSettings
 *
 * The player's own (per machine) gameplay settings that the engine's UGameUserSettings does not cover:
 * mouse sensitivity, invert Y, field of view, render scale and the FPS counter.
 * Stored in GameUserSettings.ini, section [LostIsland.Settings]; loaded on first use.
 * Graphics (quality, window mode, resolution, VSync, frame limit) live in UGameUserSettings.
 */
struct LOSTISLAND_API FLIGameSettings
{
	/** Multiplier on mouse and right-stick look. */
	float MouseSensitivity = 1.f;

	bool bInvertY = false;

	/** Horizontal field of view on foot (degrees). Vehicles add their speed effect on top. */
	float FieldOfView = 90.f;

	/** 3D render resolution in percent of the screen (r.ScreenPercentage). */
	float RenderScale = 100.f;

	bool bShowFps = false;

	/** The shared instance (loads from the ini the first time). */
	static FLIGameSettings& Get();

	/** Write to GameUserSettings.ini. */
	void Save() const;

	/** Push the settings that live in console variables (render scale) to the engine. */
	void Apply() const;

private:
	void Load();
};
