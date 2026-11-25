// Copyright LOST ISLAND Team. All Rights Reserved.

#include "UI/LISettingsMenu.h"

#include "Core/LIPlayerController.h"
#include "UI/LIGameSettings.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/HUD.h"
#include "Kismet/KismetSystemLibrary.h"

#define LOCTEXT_NAMESPACE "LISettingsMenu"

namespace LISettingsMenuValues
{
	const FIntPoint Resolutions[] =
	{
		FIntPoint(1280, 720), FIntPoint(1366, 768), FIntPoint(1600, 900), FIntPoint(1920, 1080),
		FIntPoint(2560, 1080), FIntPoint(2560, 1440), FIntPoint(3440, 1440), FIntPoint(3840, 2160),
	};
	const float FrameLimits[] = { 30.f, 60.f, 90.f, 120.f, 144.f, 165.f, 240.f, 0.f }; // 0 = unlimited
	const EWindowMode::Type WindowModes[] = { EWindowMode::Fullscreen, EWindowMode::WindowedFullscreen, EWindowMode::Windowed };

	/** Index of the closest entry, then stepped by Direction (wrapping). */
	template <typename T, typename FDistance>
	int32 Step(const T* Values, int32 Num, FDistance Distance, int32 Direction)
	{
		int32 Best = 0;
		float BestDistance = TNumericLimits<float>::Max();
		for (int32 Index = 0; Index < Num; ++Index)
		{
			const float D = Distance(Values[Index]);
			if (D < BestDistance)
			{
				BestDistance = D;
				Best = Index;
			}
		}
		return ((Best + Direction) % Num + Num) % Num;
	}

	UGameUserSettings* GetUserSettings()
	{
		return GEngine ? GEngine->GetGameUserSettings() : nullptr;
	}
}

void ULISettingsMenu::Init(ALIPlayerController* InOwner)
{
	Owner = InOwner;
}

void ULISettingsMenu::SetOpen(bool bInOpen)
{
	bOpen = bInOpen;
	if (bOpen)
	{
		Selected = ERow::Resume;
	}
}

bool ULISettingsMenu::IsRowVisible(ERow Row) const
{
	if (Row == ERow::SaveGame)
	{
		return Owner && Owner->HasAuthority(); // only the host saves the shared world
	}
	return Row != ERow::Count;
}

bool ULISettingsMenu::HasValue(ERow Row) const
{
	return Row != ERow::Resume && Row != ERow::SaveGame && Row != ERow::Quit && Row != ERow::Count;
}

void ULISettingsMenu::MoveSelection(int32 Direction)
{
	const int32 Count = static_cast<int32>(ERow::Count);
	int32 Index = static_cast<int32>(Selected);
	for (int32 Tries = 0; Tries < Count; ++Tries)
	{
		Index = ((Index + Direction) % Count + Count) % Count;
		if (IsRowVisible(static_cast<ERow>(Index)))
		{
			Selected = static_cast<ERow>(Index);
			return;
		}
	}
}

void ULISettingsMenu::ChangeValue(int32 Direction)
{
	using namespace LISettingsMenuValues;
	FLIGameSettings& Game = FLIGameSettings::Get();
	UGameUserSettings* User = GetUserSettings();

	switch (Selected)
	{
	case ERow::Quality:
		if (User)
		{
			const int32 Current = User->GetOverallScalabilityLevel(); // -1 = custom mix
			User->SetOverallScalabilityLevel(Current < 0 ? 2 : FMath::Clamp(Current + Direction, 0, 4));
			ApplyGraphics();
		}
		break;
	case ERow::WindowMode:
		if (User)
		{
			const EWindowMode::Type Current = User->GetFullscreenMode();
			const int32 Next = Step(WindowModes, int32(UE_ARRAY_COUNT(WindowModes)),
				[Current](EWindowMode::Type Mode) { return Mode == Current ? 0.f : 1.f; }, Direction);
			User->SetFullscreenMode(WindowModes[Next]);
			ApplyGraphics();
		}
		break;
	case ERow::Resolution:
		if (User)
		{
			const FIntPoint Current = User->GetScreenResolution();
			const int32 Next = Step(Resolutions, int32(UE_ARRAY_COUNT(Resolutions)),
				[Current](const FIntPoint& R) { return static_cast<float>(FMath::Abs(R.X - Current.X) + FMath::Abs(R.Y - Current.Y)); }, Direction);
			User->SetScreenResolution(Resolutions[Next]);
			ApplyGraphics();
		}
		break;
	case ERow::VSync:
		if (User)
		{
			User->SetVSyncEnabled(!User->IsVSyncEnabled());
			ApplyGraphics();
		}
		break;
	case ERow::FrameLimit:
		if (User)
		{
			const float Current = User->GetFrameRateLimit();
			const int32 Next = Step(FrameLimits, int32(UE_ARRAY_COUNT(FrameLimits)),
				[Current](float Limit) { return FMath::Abs((Limit <= 0.f ? 10000.f : Limit) - (Current <= 0.f ? 10000.f : Current)); }, Direction);
			User->SetFrameRateLimit(FrameLimits[Next]);
			ApplyGraphics();
		}
		break;
	case ERow::RenderScale:
		Game.RenderScale = FMath::Clamp(Game.RenderScale + 5.f * Direction, 50.f, 100.f);
		Game.Apply();
		Game.Save();
		break;
	case ERow::MouseSensitivity:
		Game.MouseSensitivity = FMath::Clamp(FMath::RoundToFloat((Game.MouseSensitivity + 0.1f * Direction) * 10.f) / 10.f, 0.1f, 3.f);
		Game.Save();
		break;
	case ERow::InvertY:
		Game.bInvertY = !Game.bInvertY;
		Game.Save();
		break;
	case ERow::FieldOfView:
		Game.FieldOfView = FMath::Clamp(Game.FieldOfView + 5.f * Direction, 60.f, 120.f);
		Game.Save();
		break;
	case ERow::ShowFps:
		Game.bShowFps = !Game.bShowFps;
		Game.Save();
		break;
	default:
		break;
	}
}

void ULISettingsMenu::Activate()
{
	switch (Selected)
	{
	case ERow::Resume:
		if (Owner)
		{
			Owner->SetSettingsMenuOpen(false);
		}
		break;
	case ERow::SaveGame:
		if (Owner)
		{
			Owner->LISave();
			Owner->SetSettingsMenuOpen(false);
		}
		break;
	case ERow::Quit:
		if (Owner)
		{
			UKismetSystemLibrary::QuitGame(Owner, Owner, EQuitPreference::Quit, false);
		}
		break;
	default:
		ChangeValue(1);
		break;
	}
}

void ULISettingsMenu::ApplyGraphics()
{
	if (UGameUserSettings* User = LISettingsMenuValues::GetUserSettings())
	{
		User->ApplySettings(false); // applies and saves GameUserSettings.ini
	}
}

FText ULISettingsMenu::GetRowLabel(ERow Row) const
{
	switch (Row)
	{
	case ERow::Resume:           return LOCTEXT("Resume", "Resume");
	case ERow::Quality:          return LOCTEXT("Quality", "Graphics quality");
	case ERow::WindowMode:       return LOCTEXT("WindowMode", "Display mode");
	case ERow::Resolution:       return LOCTEXT("Resolution", "Resolution");
	case ERow::VSync:            return LOCTEXT("VSync", "VSync");
	case ERow::FrameLimit:       return LOCTEXT("FrameLimit", "Frame rate limit");
	case ERow::RenderScale:      return LOCTEXT("RenderScale", "Render scale");
	case ERow::MouseSensitivity: return LOCTEXT("MouseSensitivity", "Mouse sensitivity");
	case ERow::InvertY:          return LOCTEXT("InvertY", "Invert mouse Y");
	case ERow::FieldOfView:      return LOCTEXT("FieldOfView", "Field of view");
	case ERow::ShowFps:          return LOCTEXT("ShowFps", "Show FPS");
	case ERow::SaveGame:         return LOCTEXT("SaveGame", "Save game");
	case ERow::Quit:             return LOCTEXT("Quit", "Quit to desktop");
	default:                     return FText::GetEmpty();
	}
}

FText ULISettingsMenu::GetRowValue(ERow Row) const
{
	const FLIGameSettings& Game = FLIGameSettings::Get();
	const UGameUserSettings* User = LISettingsMenuValues::GetUserSettings();
	auto OnOff = [](bool bOn) { return bOn ? LOCTEXT("On", "On") : LOCTEXT("Off", "Off"); };

	switch (Row)
	{
	case ERow::Quality:
	{
		const int32 Level = User ? User->GetOverallScalabilityLevel() : -1;
		const FText Names[] = { LOCTEXT("Low", "Low"), LOCTEXT("Medium", "Medium"), LOCTEXT("High", "High"), LOCTEXT("Epic", "Epic"), LOCTEXT("Cinematic", "Cinematic") };
		return Level >= 0 && Level <= 4 ? Names[Level] : LOCTEXT("Custom", "Custom");
	}
	case ERow::WindowMode:
	{
		const EWindowMode::Type Mode = User ? User->GetFullscreenMode() : EWindowMode::Windowed;
		return Mode == EWindowMode::Fullscreen ? LOCTEXT("Fullscreen", "Fullscreen")
			: Mode == EWindowMode::WindowedFullscreen ? LOCTEXT("Borderless", "Borderless window") : LOCTEXT("Windowed", "Windowed");
	}
	case ERow::Resolution:
	{
		const FIntPoint Res = User ? User->GetScreenResolution() : FIntPoint::ZeroValue;
		const bool bDesktop = User && User->GetFullscreenMode() == EWindowMode::WindowedFullscreen;
		return FText::FromString(FString::Printf(TEXT("%d x %d%s"), Res.X, Res.Y, bDesktop ? TEXT("  (borderless uses the desktop size)") : TEXT("")));
	}
	case ERow::VSync:
		return OnOff(User && User->IsVSyncEnabled());
	case ERow::FrameLimit:
	{
		const float Limit = User ? User->GetFrameRateLimit() : 0.f;
		return Limit <= 0.f ? LOCTEXT("Unlimited", "Unlimited") : FText::FromString(FString::Printf(TEXT("%d FPS"), FMath::RoundToInt(Limit)));
	}
	case ERow::RenderScale:
		return FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Game.RenderScale)));
	case ERow::MouseSensitivity:
		return FText::FromString(FString::Printf(TEXT("%.1f"), Game.MouseSensitivity));
	case ERow::InvertY:
		return OnOff(Game.bInvertY);
	case ERow::FieldOfView:
		return FText::FromString(FString::Printf(TEXT("%d"), FMath::RoundToInt(Game.FieldOfView)));
	case ERow::ShowFps:
		return OnOff(Game.bShowFps);
	default:
		return FText::GetEmpty();
	}
}

bool ULISettingsMenu::ParseHitBox(FName BoxName, ERow& OutRow, int32& OutDirection) const
{
	FString Name = BoxName.ToString();
	if (!Name.RemoveFromStart(TEXT("Menu_")))
	{
		return false;
	}
	OutDirection = 0;
	if (Name.RemoveFromEnd(TEXT("_L")))
	{
		OutDirection = -1;
	}
	else if (Name.RemoveFromEnd(TEXT("_R")))
	{
		OutDirection = 1;
	}
	const int32 Index = FCString::Atoi(*Name);
	if (Index < 0 || Index >= static_cast<int32>(ERow::Count) || !IsRowVisible(static_cast<ERow>(Index)))
	{
		return false;
	}
	OutRow = static_cast<ERow>(Index);
	return true;
}

void ULISettingsMenu::OnHitBoxClick(FName BoxName)
{
	ERow Row;
	int32 Direction;
	if (!bOpen || !ParseHitBox(BoxName, Row, Direction))
	{
		return;
	}
	Selected = Row;
	if (Direction != 0 && HasValue(Row))
	{
		ChangeValue(Direction);
	}
	else
	{
		Activate();
	}
}

void ULISettingsMenu::OnHitBoxHover(FName BoxName)
{
	ERow Row;
	int32 Direction;
	if (bOpen && ParseHitBox(BoxName, Row, Direction))
	{
		Selected = Row;
	}
}

void ULISettingsMenu::Draw(AHUD* HUD, UCanvas* Canvas)
{
	if (!bOpen || !HUD || !Canvas)
	{
		return;
	}
	UFont* TitleFont = GEngine ? GEngine->GetLargeFont() : nullptr;
	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	UFont* SmallFont = GEngine ? GEngine->GetSmallFont() : nullptr;

	const FLinearColor Text(0.95f, 0.93f, 0.86f);
	const FLinearColor Dim(0.65f, 0.65f, 0.62f);
	const FLinearColor Accent(1.f, 0.78f, 0.25f);
	const FLinearColor Section(0.55f, 0.8f, 1.f);

	// Darken the game, then a panel in the middle.
	HUD->DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.55f), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);

	int32 VisibleRows = 0;
	for (int32 Index = 0; Index < static_cast<int32>(ERow::Count); ++Index)
	{
		VisibleRows += IsRowVisible(static_cast<ERow>(Index)) ? 1 : 0;
	}
	const float RowHeight = 34.f;
	const float PanelW = FMath::Min(760.f, Canvas->ClipX - 40.f);
	const float PanelH = 110.f + VisibleRows * RowHeight + 3 * 26.f + 50.f;
	const float PanelX = (Canvas->ClipX - PanelW) * 0.5f;
	const float PanelY = FMath::Max(10.f, (Canvas->ClipY - PanelH) * 0.5f);
	HUD->DrawRect(FLinearColor(0.06f, 0.07f, 0.08f, 0.92f), PanelX, PanelY, PanelW, PanelH);
	HUD->DrawRect(Accent, PanelX, PanelY, PanelW, 4.f);

	const FString Title = LOCTEXT("Title", "MENU").ToString();
	float TW = 0.f, TH = 0.f;
	HUD->GetTextSize(Title, TW, TH, TitleFont, 1.4f);
	HUD->DrawText(Title, Text, PanelX + (PanelW - TW) * 0.5f, PanelY + 18.f, TitleFont, 1.4f);

	float Y = PanelY + 80.f;
	const float LabelX = PanelX + 40.f;
	const float ValueX = PanelX + PanelW * 0.52f;
	const float ValueW = PanelW * 0.48f - 40.f;

	auto DrawSection = [&](const FText& Name)
	{
		HUD->DrawText(Name.ToString(), Section, LabelX, Y + 4.f, SmallFont, 1.f);
		Y += 26.f;
	};

	for (int32 Index = 0; Index < static_cast<int32>(ERow::Count); ++Index)
	{
		const ERow Row = static_cast<ERow>(Index);
		if (!IsRowVisible(Row))
		{
			continue;
		}
		if (Row == ERow::Quality)
		{
			DrawSection(LOCTEXT("SectionGraphics", "GRAPHICS"));
		}
		else if (Row == ERow::MouseSensitivity)
		{
			DrawSection(LOCTEXT("SectionControls", "CONTROLS"));
		}
		else if (Row == ERow::SaveGame || (Row == ERow::Quit && !IsRowVisible(ERow::SaveGame)))
		{
			DrawSection(LOCTEXT("SectionGame", "GAME"));
		}

		const bool bSelected = Row == Selected;
		if (bSelected)
		{
			HUD->DrawRect(FLinearColor(1.f, 0.78f, 0.25f, 0.16f), PanelX + 20.f, Y, PanelW - 40.f, RowHeight - 4.f);
			HUD->DrawRect(Accent, PanelX + 20.f, Y, 4.f, RowHeight - 4.f);
		}
		HUD->DrawText(GetRowLabel(Row).ToString(), bSelected ? Accent : Text, LabelX, Y + 5.f, Font, 1.f);

		const FString RowName = FString::Printf(TEXT("Menu_%d"), Index);
		HUD->AddHitBox(FVector2D(PanelX + 20.f, Y), FVector2D(PanelW - 40.f, RowHeight - 4.f), FName(*RowName), false, 0);

		if (HasValue(Row))
		{
			const FString Value = GetRowValue(Row).ToString();
			float VW = 0.f, VH = 0.f;
			HUD->GetTextSize(Value, VW, VH, Font, 1.f);
			const float Center = ValueX + ValueW * 0.5f;
			HUD->DrawText(TEXT("<"), bSelected ? Accent : Dim, ValueX, Y + 5.f, Font, 1.f);
			HUD->DrawText(TEXT(">"), bSelected ? Accent : Dim, ValueX + ValueW - 12.f, Y + 5.f, Font, 1.f);
			HUD->DrawText(Value, Text, Center - VW * 0.5f, Y + 5.f, Font, 1.f);
			// Arrow hit boxes sit on top of the row box (higher priority).
			HUD->AddHitBox(FVector2D(ValueX - 10.f, Y), FVector2D(44.f, RowHeight - 4.f), FName(*(RowName + TEXT("_L"))), true, 1);
			HUD->AddHitBox(FVector2D(ValueX + ValueW - 34.f, Y), FVector2D(44.f, RowHeight - 4.f), FName(*(RowName + TEXT("_R"))), true, 1);
		}
		Y += RowHeight;
	}

	const FString Help = LOCTEXT("Help", "Up/Down: choose    Left/Right: change    Enter: select    Esc: back    (mouse works too)").ToString();
	float HW = 0.f, HH = 0.f;
	HUD->GetTextSize(Help, HW, HH, SmallFont, 1.f);
	HUD->DrawText(Help, Dim, PanelX + (PanelW - HW) * 0.5f, PanelY + PanelH - 34.f, SmallFont, 1.f);
}

#undef LOCTEXT_NAMESPACE
