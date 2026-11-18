// Copyright LOST ISLAND Team. All Rights Reserved.

#include "UI/LIHUD.h"

#include "Character/LICharacter.h"
#include "Interaction/LIInteractionComponent.h"
#include "Survival/LISurvivalComponent.h"
#include "Inventory/LIInventoryComponent.h"
#include "Items/LIItemRegistrySubsystem.h"
#include "Core/LIPlayerController.h"
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
	DrawInventoryNotices(Character);

	const ALIPlayerController* PC = Cast<ALIPlayerController>(GetOwningPlayerController());
	if (PC)
	{
		const FText Message = PC->GetHudMessage();
		if (!Message.IsEmpty())
		{
			UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
			const FString Text = Message.ToString();
			float W = 0.f, H = 0.f;
			GetTextSize(Text, W, H, Font, 1.f);
			DrawText(Text, PromptColor, Canvas->ClipX - W - 24.f, 24.f, Font, 1.f);
		}
	}
	if (PC && PC->IsInventoryOpen())
	{
		DrawInventoryPanel(Character);
	}

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

	// "[F] Eat Berries (x4)" when something in the inventory would help.
	if (const ULIInventoryComponent* Inventory = Character->GetInventoryComponent())
	{
		const FName Best = Inventory->FindBestConsumable();
		if (!Best.IsNone())
		{
			const ULIItemRegistrySubsystem* Registry = ULIItemRegistrySubsystem::Get(this);
			const FString Name = Registry ? Registry->GetDisplayName(Best).ToString() : Best.ToString();
			const FString Hint = FString::Printf(TEXT("[F] Eat %s (x%d)"), *Name, Inventory->GetItemCount(Best));
			UFont* Font = GEngine ? GEngine->GetSmallFont() : nullptr;
			float W = 0.f, H = 0.f;
			GetTextSize(Hint, W, H, Font, 1.f);
			DrawText(Hint, PromptColor, CenterX - W * 0.5f, Canvas->ClipY - 48.f, Font, 1.f);
		}
	}
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

void ALIHUD::DrawInventoryPanel(const ALICharacter* Character)
{
	const ULIInventoryComponent* Inventory = Character->GetInventoryComponent();
	if (!Inventory)
	{
		return;
	}

	const ULIItemRegistrySubsystem* Registry = ULIItemRegistrySubsystem::Get(this);
	const TArray<FLIInventorySlot>& Slots = Inventory->GetSlots();

	const int32 Columns = FMath::Max(1, InventoryColumns);
	const int32 Rows = FMath::DivideAndRoundUp(Slots.Num(), Columns);
	const float Gap = 6.f;
	const float Padding = 14.f;
	const float HeaderHeight = 28.f;
	const float PanelW = Columns * InventorySlotSize + (Columns - 1) * Gap + Padding * 2.f;
	const float PanelH = Rows * InventorySlotSize + FMath::Max(0, Rows - 1) * Gap + Padding * 2.f + HeaderHeight;
	const float PanelX = (Canvas->ClipX - PanelW) * 0.5f;
	const float PanelY = (Canvas->ClipY - PanelH) * 0.5f;

	UFont* SmallFont = GEngine ? GEngine->GetSmallFont() : nullptr;
	UFont* MediumFont = GEngine ? GEngine->GetMediumFont() : nullptr;

	DrawRect(FLinearColor(0.05f, 0.04f, 0.03f, 0.85f), PanelX, PanelY, PanelW, PanelH);
	DrawText(LOCTEXT("InventoryTitle", "INVENTORY").ToString(), PromptColor, PanelX + Padding, PanelY + 8.f, MediumFont, 1.f);

	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		const int32 Col = Index % Columns;
		const int32 Row = Index / Columns;
		const float X = PanelX + Padding + Col * (InventorySlotSize + Gap);
		const float Y = PanelY + Padding + HeaderHeight + Row * (InventorySlotSize + Gap);

		const FLIInventorySlot& Slot = Slots[Index];
		DrawRect(Slot.IsEmpty() ? FLinearColor(1.f, 1.f, 1.f, 0.06f) : FLinearColor(0.85f, 0.7f, 0.45f, 0.25f),
			X, Y, InventorySlotSize, InventorySlotSize);

		if (Slot.IsEmpty())
		{
			continue;
		}

		const FString Name = Registry ? Registry->GetDisplayName(Slot.ItemId).ToString() : Slot.ItemId.ToString();
		DrawText(Name, FLinearColor::White, X + 5.f, Y + 5.f, SmallFont, 1.f);

		const FString Qty = FString::Printf(TEXT("x%d"), Slot.Quantity);
		float W = 0.f, H = 0.f;
		GetTextSize(Qty, W, H, SmallFont, 1.f);
		DrawText(Qty, PromptColor, X + InventorySlotSize - W - 5.f, Y + InventorySlotSize - H - 4.f, SmallFont, 1.f);
	}
}

void ALIHUD::DrawInventoryNotices(const ALICharacter* Character)
{
	ULIInventoryComponent* Inventory = Character->GetInventoryComponent();
	if (!Inventory)
	{
		return;
	}

	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	float Y = Canvas->ClipY * 0.5f + 80.f;
	for (const FLIInventoryNotice& Notice : Inventory->GetActiveNotices())
	{
		const FString Text = Notice.Text.ToString();
		float W = 0.f, H = 0.f;
		GetTextSize(Text, W, H, Font, 1.f);
		DrawText(Text, Notice.bIsWarning ? HealthColor : StaminaColor, Canvas->ClipX * 0.5f - W * 0.5f, Y, Font, 1.f);
		Y += H + 2.f;
	}
}

#undef LOCTEXT_NAMESPACE
