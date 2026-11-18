// Copyright LOST ISLAND Team. All Rights Reserved.

#include "UI/LIHUD.h"

#include "Character/LICharacter.h"
#include "Interaction/LIInteractionComponent.h"
#include "Survival/LISurvivalComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"

#define LOCTEXT_NAMESPACE "LIHUD"

void ALIHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas)
	{
		return;
	}

	const float CenterX = Canvas->ClipX * 0.5f;
	const float CenterY = Canvas->ClipY * 0.5f;

	const ALICharacter* Character = Cast<ALICharacter>(GetOwningPawn());
	if (!Character)
	{
		return;
	}

	DrawSurvivalBars(Character);

	if (Character->IsDead())
	{
		const FString Text = LOCTEXT("YouDied", "YOU DIED").ToString();
		UFont* Font = GEngine ? GEngine->GetLargeFont() : nullptr;
		float W = 0.f, H = 0.f;
		GetTextSize(Text, W, H, Font, 2.f);
		DrawText(Text, HealthColor, CenterX - W * 0.5f, CenterY - H * 0.5f, Font, 2.f);
		return;
	}

	DrawRect(CrosshairColor, CenterX - 2.f, CenterY - 2.f, 4.f, 4.f);
	DrawInteractionPrompt(Character, CenterX, CenterY);
}

void ALIHUD::DrawInteractionPrompt(const ALICharacter* Character, float CenterX, float CenterY)
{
	const ULIInteractionComponent* Interaction = Character->GetInteractionComponent();
	if (!Interaction || !Interaction->GetFocusedActor())
	{
		return;
	}

	const FString Prompt = FString::Printf(TEXT("%s %s"), *InteractKeyLabel.ToString(), *Interaction->GetFocusedPrompt().ToString());
	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;

	float TextW = 0.f;
	float TextH = 0.f;
	GetTextSize(Prompt, TextW, TextH, Font, 1.f);
	DrawText(Prompt, PromptColor, CenterX - TextW * 0.5f, CenterY + 40.f, Font, 1.f);
}

void ALIHUD::DrawSurvivalBars(const ALICharacter* Character)
{
	const ULISurvivalComponent* Survival = Character->GetSurvivalComponent();
	if (!Survival)
	{
		return;
	}

	const float X = 24.f;
	const float Spacing = BarSize.Y + 10.f;
	float Y = Canvas->ClipY - 24.f - Spacing * 4.f;

	DrawBar(X, Y, Survival->GetStatPercent(ELISurvivalStat::Health), HealthColor, TEXT("HP"));
	Y += Spacing;
	DrawBar(X, Y, Survival->GetStatPercent(ELISurvivalStat::Hunger), HungerColor, TEXT("FOOD"));
	Y += Spacing;
	DrawBar(X, Y, Survival->GetStatPercent(ELISurvivalStat::Thirst), ThirstColor, TEXT("WATER"));
	Y += Spacing;
	DrawBar(X, Y, Survival->GetStatPercent(ELISurvivalStat::Stamina),
		Survival->IsExhausted() ? ExhaustedColor : StaminaColor, TEXT("STAMINA"));
}

void ALIHUD::DrawBar(float X, float Y, float Percent, const FLinearColor& Color, const FString& Label)
{
	UFont* Font = GEngine ? GEngine->GetSmallFont() : nullptr;
	const float LabelWidth = 70.f;

	DrawText(Label, FLinearColor::White, X, Y - 1.f, Font, 1.f);
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.55f), X + LabelWidth, Y, BarSize.X, BarSize.Y);
	DrawRect(Color, X + LabelWidth + 1.f, Y + 1.f, (BarSize.X - 2.f) * FMath::Clamp(Percent, 0.f, 1.f), BarSize.Y - 2.f);
}

#undef LOCTEXT_NAMESPACE
