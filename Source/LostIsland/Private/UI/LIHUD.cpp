// Copyright LOST ISLAND Team. All Rights Reserved.

#include "UI/LIHUD.h"

#include "Character/LICharacter.h"
#include "Interaction/LIInteractionComponent.h"
#include "Survival/LISurvivalComponent.h"
#include "Inventory/LIInventoryComponent.h"
#include "Items/LIItemRegistrySubsystem.h"
#include "Items/LIItemDefinition.h"
#include "Crafting/LICraftingComponent.h"
#include "Crafting/LIRecipeDefinition.h"
#include "Core/LIPlayerController.h"
#include "Core/LIGameState.h"
#include "Vehicles/LIVehicle.h"
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

	// Ending: the raft sailed. Replaces the whole HUD until the new island starts.
	if (const ALIGameState* EndState = GetWorld() ? GetWorld()->GetGameState<ALIGameState>() : nullptr; EndState && EndState->HasEscaped())
	{
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
		UFont* BigFont = GEngine ? GEngine->GetLargeFont() : nullptr;
		UFont* SubFont = GEngine ? GEngine->GetMediumFont() : nullptr;
		const FString Title = LOCTEXT("Escaped", "YOU ESCAPED THE ISLAND").ToString();
		const FString Sub = FText::Format(LOCTEXT("EscapedSub", "Rescued on day {0}. A new island awaits..."), FText::AsNumber(EndState->GetEscapeDay())).ToString();
		float TW = 0.f, TH = 0.f, SW = 0.f, SH = 0.f;
		GetTextSize(Title, TW, TH, BigFont, 2.f);
		GetTextSize(Sub, SW, SH, SubFont, 1.f);
		DrawText(Title, StaminaColor, CenterX - TW * 0.5f, CenterY - TH, BigFont, 2.f);
		DrawText(Sub, PromptColor, CenterX - SW * 0.5f, CenterY + 10.f, SubFont, 1.f);
		return;
	}

	// At the wheel the HUD shows the driver's stats plus the speedometer, without hotbar or crosshair.
	const ALIVehicle* Vehicle = Cast<ALIVehicle>(GetOwningPawn());
	const ALICharacter* Character = ALIVehicle::GetCharacterFromPawn(GetOwningPawn());
	if (!Character)
	{
		return;
	}

	DrawSurvivalBars(Character);
	if (!Vehicle)
	{
		DrawHotbar(Character);
		DrawInventoryNotices(Character);
	}

	// Clock, top-left: "Day 2  21:30  Night"
	if (const ALIGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ALIGameState>() : nullptr)
	{
		const float Hours = GameState->GetTimeOfDayHours();
		const int32 H = FMath::FloorToInt(Hours);
		const int32 Min = FMath::FloorToInt((Hours - H) * 60.f);
		const FString Clock = FString::Printf(TEXT("Day %d   %02d:%02d   %s"), GameState->GetDayNumber(), H, Min,
			GameState->IsNight() ? TEXT("Night") : TEXT("Day"));
		const float Rain = GameState->GetRainAmount();
		const FString Weather = Rain > 0.6f ? TEXT("   Heavy rain (crops grow faster, roads slippery)")
			: Rain > 0.15f ? TEXT("   Rain (crops grow faster)") : TEXT("");
		DrawText(Clock + Weather, PromptColor, 24.f, 20.f, GEngine ? GEngine->GetMediumFont() : nullptr, 1.f);
		if (GameState->IsFarmEconomyEnabled())
		{
			FString Farm = FString::Printf(TEXT("Coins: %d      Grain: %d kg"), FMath::Max(GameState->GetMoney(), 0), GameState->GetGrainKg());
			if (GameState->GetGrainPriceMultiplier() > 1.001f)
			{
				Farm += FString::Printf(TEXT("  (barn: +%d%% price)"), FMath::RoundToInt((GameState->GetGrainPriceMultiplier() - 1.f) * 100.f));
			}
			DrawText(Farm, FLinearColor(1.f, 0.85f, 0.35f, 1.f), 24.f, 46.f, GEngine ? GEngine->GetMediumFont() : nullptr, 1.f);
		}
	}

	// Cold warning above the survival bars.
	if (const ULISurvivalComponent* Survival = Character->GetSurvivalComponent())
	{
		if (Survival->IsCold() && !Survival->IsDead())
		{
			const float Spacing = BarSize.Y + 10.f;
			DrawText(LOCTEXT("Cold", "COLD - get to a fire or under a roof").ToString(), ThirstColor,
				24.f, Canvas->ClipY - 24.f - Spacing * 5.f - 24.f, GEngine ? GEngine->GetSmallFont() : nullptr, 1.f);
		}
	}

	// Build mode banner (top centre).
	if (Character->IsInBuildMode())
	{
		const FString Banner = LOCTEXT("BuildMode", "BUILD MODE   [G] place   [E] dismantle   [B] exit").ToString();
		UFont* BannerFont = GEngine ? GEngine->GetMediumFont() : nullptr;
		float BW = 0.f, BH = 0.f;
		GetTextSize(Banner, BW, BH, BannerFont, 1.f);
		DrawText(Banner, StaminaColor, Canvas->ClipX * 0.5f - BW * 0.5f, 20.f, BannerFont, 1.f);
	}

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
		const FString SubText = LOCTEXT("Respawning", "Respawning... your items stay where you fell").ToString();
		UFont* SmallFont = GEngine ? GEngine->GetMediumFont() : nullptr;
		float SubW = 0.f, SubH = 0.f;
		GetTextSize(SubText, SubW, SubH, SmallFont, 1.f);
		DrawText(SubText, PromptColor, CenterX - SubW * 0.5f, CenterY + H, SmallFont, 1.f);
		return;
	}

	if (Vehicle)
	{
		DrawVehicleHUD(Vehicle);
		return;
	}

	DrawRect(CrosshairColor, CenterX - 2.f, CenterY - 2.f, 4.f, 4.f);
	DrawInteractionPrompt(Character, CenterX, CenterY);

	// "[G] Place Campfire" when carrying a placeable item.
	if (const ULICraftingComponent* Crafting = Character->GetCraftingComponent())
	{
		const FName Placeable = Crafting->FindPlaceableItem();
		if (!Placeable.IsNone())
		{
			const ULIItemRegistrySubsystem* Registry = ULIItemRegistrySubsystem::Get(this);
			const FString Hint = FString::Printf(TEXT("[G] Place %s"), *(Registry ? Registry->GetDisplayName(Placeable).ToString() : Placeable.ToString()));
			UFont* Font = GEngine ? GEngine->GetSmallFont() : nullptr;
			float W = 0.f, H = 0.f;
			GetTextSize(Hint, W, H, Font, 1.f);
			DrawText(Hint, PromptColor, CenterX - W * 0.5f, Canvas->ClipY - 148.f, Font, 1.f); // above the hotbar
		}
	}

	// "[F] Eat Berries (x4)" when something in the inventory would help.
	if (const ULIInventoryComponent* Inventory = Character->GetInventoryComponent())
	{
		const FName Best = Inventory->FindBestConsumable();
		if (!Best.IsNone())
		{
			const ULIItemRegistrySubsystem* Registry = ULIItemRegistrySubsystem::Get(this);
			const FString Name = Registry ? Registry->GetDisplayName(Best).ToString() : Best.ToString();
			const FString Hint = FString::Printf(TEXT("[F] Use %s (x%d)"), *Name, Inventory->GetItemCount(Best));
			UFont* Font = GEngine ? GEngine->GetSmallFont() : nullptr;
			float W = 0.f, H = 0.f;
			GetTextSize(Hint, W, H, Font, 1.f);
			DrawText(Hint, PromptColor, CenterX - W * 0.5f, Canvas->ClipY - 128.f, Font, 1.f);
		}
	}
}

void ALIHUD::DrawVehicleHUD(const ALIVehicle* Vehicle)
{
	UFont* BigFont = GEngine ? GEngine->GetLargeFont() : nullptr;
	UFont* Font = GEngine ? GEngine->GetSmallFont() : nullptr;

	// Speedometer, bottom right.
	const int32 Kmh = FMath::RoundToInt(FMath::Abs(Vehicle->GetSpeedKmh()));
	const FString Speed = FString::Printf(TEXT("%d"), Kmh);
	float SW = 0.f, SH = 0.f;
	GetTextSize(Speed, SW, SH, BigFont, 2.f);
	const float Right = Canvas->ClipX - 40.f;
	const float Bottom = Canvas->ClipY - 40.f;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.35f), Right - 190.f, Bottom - 110.f, 190.f, 110.f);
	DrawText(Speed, FLinearColor::White, Right - 70.f - SW, Bottom - 100.f, BigFont, 2.f);
	DrawText(TEXT("km/h"), PromptColor, Right - 62.f, Bottom - 52.f, Font, 1.f);
	const FString Name = Vehicle->GetDisplayName().ToString();
	DrawText(Name, PromptColor, Right - 180.f, Bottom - 24.f, Font, 1.f);

	// Fuel gauge above the speedometer: green, then amber, red when nearly empty.
	if (Vehicle->UsesFuel())
	{
		const float Fraction = FMath::Clamp(Vehicle->GetFuel() / FMath::Max(1.f, Vehicle->GetFuelCapacity()), 0.f, 1.f);
		const FLinearColor Gauge = Fraction < 0.15f ? FLinearColor(0.9f, 0.15f, 0.1f)
			: Fraction < 0.35f ? FLinearColor(1.f, 0.7f, 0.15f) : FLinearColor(0.3f, 0.85f, 0.35f);
		const float GaugeTop = Bottom - 134.f;
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.35f), Right - 190.f, GaugeTop, 190.f, 20.f);
		DrawRect(Gauge, Right - 140.f, GaugeTop + 5.f, 132.f * Fraction, 10.f);
		const FString FuelLabel = Fraction <= 0.f ? TEXT("EMPTY") : FString::Printf(TEXT("%d L"), FMath::RoundToInt(Vehicle->GetFuel()));
		DrawText(FuelLabel, Fraction < 0.15f ? Gauge : PromptColor, Right - 186.f, GaugeTop + 3.f, Font, 1.f);
	}

	// Controls, bottom centre.
	FString Help = Vehicle->IsTippedOver()
		? LOCTEXT("TippedOver", "[R] Put back on the wheels    [E] Get out").ToString()
		: LOCTEXT("DriveHelp", "[W/S] Drive / brake    [A/D] Steer    [Space] Handbrake    [E] Get out").ToString();
	if (Vehicle->HasTool() && !Vehicle->IsTippedOver())
	{
		Help += FString::Printf(TEXT("    [Q] %s"), *Vehicle->GetToolHint().ToString());
	}
	if (Vehicle->HasToolModes() && !Vehicle->IsTippedOver())
	{
		Help += FString::Printf(TEXT("    [T] Tool: %s"), *Vehicle->GetToolModeName().ToString());
	}
	const FString Status = Vehicle->GetStatusText().ToString();
	if (!Status.IsEmpty())
	{
		float StW = 0.f, StH = 0.f;
		GetTextSize(Status, StW, StH, Font, 1.f);
		DrawText(Status, StaminaColor, (Canvas->ClipX - StW) * 0.5f, Canvas->ClipY - 72.f, Font, 1.f);
	}
	float HW = 0.f, HH = 0.f;
	GetTextSize(Help, HW, HH, Font, 1.f);
	DrawText(Help, Vehicle->IsTippedOver() ? StaminaColor : PromptColor, (Canvas->ClipX - HW) * 0.5f, Canvas->ClipY - 48.f, Font, 1.f);
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

	// Breath only shows while it is being used (under water) or refilling.
	const float BreathPercent = Survival->GetBreathPercent();
	if (BreathPercent < 0.999f)
	{
		DrawBar(X, Y - Spacing, BreathPercent, BreathPercent > 0.f ? ThirstColor : HealthColor, TEXT("AIR"));
	}

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

		const ULIItemDefinition* Def = Registry ? Registry->FindItem(Slot.ItemId) : nullptr;
		if (Def && Def->MaxDurability > 0)
		{
			// Durability bar instead of a quantity for tools.
			const float Pct = FMath::Clamp(static_cast<float>(Slot.Durability) / Def->MaxDurability, 0.f, 1.f);
			const FLinearColor BarColor = Pct > 0.3f ? StaminaColor : HealthColor;
			DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f), X + 5.f, Y + InventorySlotSize - 10.f, InventorySlotSize - 10.f, 5.f);
			DrawRect(BarColor, X + 5.f, Y + InventorySlotSize - 10.f, (InventorySlotSize - 10.f) * Pct, 5.f);
		}
		else
		{
			const FString Qty = FString::Printf(TEXT("x%d"), Slot.Quantity);
			float W = 0.f, H = 0.f;
			GetTextSize(Qty, W, H, SmallFont, 1.f);
			DrawText(Qty, PromptColor, X + InventorySlotSize - W - 5.f, Y + InventorySlotSize - H - 4.f, SmallFont, 1.f);
		}
	}

	DrawCraftingPanel(Character, PanelX + PanelW + 12.f, PanelY, PanelH);
}

void ALIHUD::DrawCraftingPanel(const ALICharacter* Character, float X, float Y, float PanelHeight)
{
	const ULIItemRegistrySubsystem* Registry = ULIItemRegistrySubsystem::Get(this);
	const ULICraftingComponent* Crafting = Character->GetCraftingComponent();
	const ULIInventoryComponent* Inventory = Character->GetInventoryComponent();
	if (!Registry || !Crafting || !Inventory)
	{
		return;
	}

	UFont* SmallFont = GEngine ? GEngine->GetSmallFont() : nullptr;
	UFont* MediumFont = GEngine ? GEngine->GetMediumFont() : nullptr;
	const float Width = 300.f;
	const float Padding = 14.f;
	const float RowHeight = 40.f;
	const int32 PerPage = ALICharacter::CraftKeysPerPage;
	const int32 NumRecipes = Registry->GetRecipes().Num();
	const int32 NumPages = FMath::Max(1, FMath::DivideAndRoundUp(NumRecipes, PerPage));
	const int32 Page = FMath::Clamp(Character->GetCraftPage(), 0, NumPages - 1);
	const int32 FirstRecipe = Page * PerPage;
	const int32 NumRows = FMath::Min(NumRecipes - FirstRecipe, PerPage);
	const float Height = FMath::Max(PanelHeight, 40.f + NumRows * RowHeight + 30.f); // grow to fit every recipe

	DrawRect(FLinearColor(0.05f, 0.04f, 0.03f, 0.85f), X, Y, Width, Height);
	DrawText(NumPages > 1 ? FString::Printf(TEXT("CRAFTING  %d/%d  [Q] next page"), Page + 1, NumPages) : LOCTEXT("CraftingTitle", "CRAFTING").ToString(), PromptColor, X + Padding, Y + 8.f, MediumFont, 1.f);

	const TArray<TObjectPtr<ULIRecipeDefinition>>& Recipes = Registry->GetRecipes();
	float RowY = Y + 40.f;
	for (int32 Index = FirstRecipe; Index < Recipes.Num() && Index < FirstRecipe + PerPage; ++Index)
	{
		const ULIRecipeDefinition* Recipe = Recipes[Index];
		if (!Recipe || RowY + RowHeight > Y + Height)
		{
			break;
		}

		const bool bCanCraft = Crafting->CanCraft(Recipe->RecipeId);
		const FString Name = !Recipe->DisplayName.IsEmpty()
			? Recipe->DisplayName.ToString()
			: Registry->GetDisplayName(Recipe->Output.ItemId).ToString();
		DrawText(FString::Printf(TEXT("[%d] %s%s"), (Index - FirstRecipe + 1) % 10, *Name, Recipe->bRequiresFire ? TEXT("  (at a fire)") : (Recipe->bRequiresWater ? TEXT("  (at a spring)") : TEXT(""))), bCanCraft ? FLinearColor::White : FLinearColor(0.6f, 0.6f, 0.6f, 1.f),
			X + Padding, RowY, SmallFont, 1.f);

		// Ingredients: "3/3 Wood  1/2 Stone" (red when missing).
		float IngredientX = X + Padding + 18.f;
		for (const FLIItemQuantity& Ingredient : Recipe->Ingredients)
		{
			const int32 Have = Inventory->GetItemCount(Ingredient.ItemId);
			const FString Text = FString::Printf(TEXT("%d/%d %s  "), FMath::Min(Have, Ingredient.Quantity), Ingredient.Quantity,
				*Registry->GetDisplayName(Ingredient.ItemId).ToString());
			float W = 0.f, H = 0.f;
			GetTextSize(Text, W, H, SmallFont, 1.f);
			DrawText(Text, Have >= Ingredient.Quantity ? StaminaColor : HealthColor, IngredientX, RowY + 16.f, SmallFont, 1.f);
			IngredientX += W;
		}
		RowY += RowHeight;
	}

	DrawText(LOCTEXT("CraftHint", "Press the number to craft").ToString(), FLinearColor(0.7f, 0.7f, 0.7f, 1.f),
		X + Padding, Y + Height - 22.f, SmallFont, 1.f);
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

void ALIHUD::DrawHotbar(const ALICharacter* Character)
{
	// Bottom centre: the first HotbarSize inventory slots; keys 1-8 select, F uses and G places the selection.
	const ULIInventoryComponent* Inventory = Character->GetInventoryComponent();
	if (!Inventory || Character->IsDead())
	{
		return;
	}
	const ULIItemRegistrySubsystem* Registry = ULIItemRegistrySubsystem::Get(this);
	const TArray<FLIInventorySlot>& Slots = Inventory->GetSlots();
	const int32 Count = FMath::Min(ALICharacter::HotbarSize, Slots.Num());
	const float Size = 64.f;
	const float Gap = 6.f;
	const float TotalW = Count * Size + (Count - 1) * Gap;
	const float StartX = (Canvas->ClipX - TotalW) * 0.5f;
	const float Y = Canvas->ClipY - Size - 16.f;
	UFont* SmallFont = GEngine ? GEngine->GetSmallFont() : nullptr;

	for (int32 Index = 0; Index < Count; ++Index)
	{
		const float X = StartX + Index * (Size + Gap);
		const bool bSelected = Index == Character->GetSelectedHotbarSlot();
		if (bSelected)
		{
			DrawRect(FLinearColor(1.f, 0.85f, 0.35f, 0.95f), X - 3.f, Y - 3.f, Size + 6.f, Size + 6.f); // gold frame
		}
		DrawRect(FLinearColor(0.05f, 0.04f, 0.03f, bSelected ? 0.9f : 0.65f), X, Y, Size, Size);
		DrawText(FString::FromInt(Index + 1), FLinearColor(1.f, 1.f, 1.f, 0.6f), X + 4.f, Y + 2.f, SmallFont, 1.f);

		const FLIInventorySlot& Slot = Slots[Index];
		if (Slot.IsEmpty())
		{
			continue;
		}
		// Item name, wrapped to two short lines.
		FString Name = Registry ? Registry->GetDisplayName(Slot.ItemId).ToString() : Slot.ItemId.ToString();
		FString First = Name, Second;
		Name.Split(TEXT(" "), &First, &Second);
		if (First.IsEmpty())
		{
			First = Name;
		}
		DrawText(First, FLinearColor::White, X + 5.f, Y + 18.f, SmallFont, 1.f);
		if (!Second.IsEmpty())
		{
			DrawText(Second, FLinearColor::White, X + 5.f, Y + 31.f, SmallFont, 1.f);
		}

		const ULIItemDefinition* Def = Registry ? Registry->FindItem(Slot.ItemId) : nullptr;
		if (Def && Def->MaxDurability > 0)
		{
			const float Pct = FMath::Clamp(static_cast<float>(Slot.Durability) / Def->MaxDurability, 0.f, 1.f);
			DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f), X + 5.f, Y + Size - 9.f, Size - 10.f, 4.f);
			DrawRect(Pct > 0.3f ? StaminaColor : HealthColor, X + 5.f, Y + Size - 9.f, (Size - 10.f) * Pct, 4.f);
		}
		else if (Slot.Quantity > 1)
		{
			const FString Qty = FString::Printf(TEXT("x%d"), Slot.Quantity);
			float W = 0.f, H = 0.f;
			GetTextSize(Qty, W, H, SmallFont, 1.f);
			DrawText(Qty, PromptColor, X + Size - W - 4.f, Y + Size - H - 3.f, SmallFont, 1.f);
		}
	}
}
