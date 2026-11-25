// Copyright LOST ISLAND Team. All Rights Reserved.

#include "UI/LIGameSettings.h"

#include "HAL/IConsoleManager.h"
#include "Misc/ConfigCacheIni.h"

namespace
{
	const TCHAR* SettingsSection = TEXT("LostIsland.Settings");
}

FLIGameSettings& FLIGameSettings::Get()
{
	static FLIGameSettings Settings;
	static bool bLoaded = false;
	if (!bLoaded)
	{
		bLoaded = true;
		Settings.Load();
		Settings.Apply();
	}
	return Settings;
}

void FLIGameSettings::Load()
{
	if (!GConfig)
	{
		return;
	}
	GConfig->GetFloat(SettingsSection, TEXT("MouseSensitivity"), MouseSensitivity, GGameUserSettingsIni);
	GConfig->GetBool(SettingsSection, TEXT("InvertY"), bInvertY, GGameUserSettingsIni);
	GConfig->GetFloat(SettingsSection, TEXT("FieldOfView"), FieldOfView, GGameUserSettingsIni);
	GConfig->GetFloat(SettingsSection, TEXT("RenderScale"), RenderScale, GGameUserSettingsIni);
	GConfig->GetBool(SettingsSection, TEXT("ShowFps"), bShowFps, GGameUserSettingsIni);

	MouseSensitivity = FMath::Clamp(MouseSensitivity, 0.1f, 3.f);
	FieldOfView = FMath::Clamp(FieldOfView, 60.f, 120.f);
	RenderScale = FMath::Clamp(RenderScale, 50.f, 100.f);
}

void FLIGameSettings::Save() const
{
	if (!GConfig)
	{
		return;
	}
	GConfig->SetFloat(SettingsSection, TEXT("MouseSensitivity"), MouseSensitivity, GGameUserSettingsIni);
	GConfig->SetBool(SettingsSection, TEXT("InvertY"), bInvertY, GGameUserSettingsIni);
	GConfig->SetFloat(SettingsSection, TEXT("FieldOfView"), FieldOfView, GGameUserSettingsIni);
	GConfig->SetFloat(SettingsSection, TEXT("RenderScale"), RenderScale, GGameUserSettingsIni);
	GConfig->SetBool(SettingsSection, TEXT("ShowFps"), bShowFps, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

void FLIGameSettings::Apply() const
{
	if (IConsoleVariable* ScreenPercentage = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage")))
	{
		ScreenPercentage->Set(RenderScale, ECVF_SetByGameSetting);
	}
}
