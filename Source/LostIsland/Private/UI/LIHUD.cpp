// Copyright LOST ISLAND Team. All Rights Reserved.

#include "UI/LIHUD.h"

#include "Character/LICharacter.h"
#include "Interaction/LIInteractionComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"

void ALIHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas)
	{
		return;
	}

	const float CenterX = Canvas->ClipX * 0.5f;
	const float CenterY = Canvas->ClipY * 0.5f;

	DrawRect(CrosshairColor, CenterX - 2.f, CenterY - 2.f, 4.f, 4.f);

	const ALICharacter* Character = Cast<ALICharacter>(GetOwningPawn());
	const ULIInteractionComponent* Interaction = Character ? Character->GetInteractionComponent() : nullptr;
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
