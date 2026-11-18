// Copyright LOST ISLAND Team. All Rights Reserved.

#include "Core/LIPlayerController.h"

#include "Character/LICharacter.h"
#include "LostIsland.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "Survival/LISurvivalComponent.h"
#include "Inventory/LIInventoryComponent.h"
#include "Core/LIGameMode.h"

void ALIPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalController())
	{
		SetInputMode(FInputModeGameOnly());
		SetShowMouseCursor(false);
	}
}

// --- Debug commands (client entry points) ---

void ALIPlayerController::LISetStat(const FString& StatName, float Value) { ServerDebugSetStat(StatName, Value); }
void ALIPlayerController::LIDamage(float Amount) { ServerDebugDamage(Amount); }
void ALIPlayerController::LIRevive() { ServerDebugRevive(); }
void ALIPlayerController::LIGive(const FString& ItemId, int32 Quantity) { ServerDebugGive(ItemId, Quantity); }
void ALIPlayerController::LIClearInventory() { ServerDebugClearInventory(); }
void ALIPlayerController::LIRegenerateIsland(int32 Seed) { ServerDebugRegenerateIsland(Seed); }

// --- Debug commands (server) ---

void ALIPlayerController::ServerDebugSetStat_Implementation(const FString& StatName, float Value)
{
#if !UE_BUILD_SHIPPING
	const ALICharacter* LICharacter = Cast<ALICharacter>(GetPawn());
	ULISurvivalComponent* Survival = LICharacter ? LICharacter->GetSurvivalComponent() : nullptr;
	if (!Survival)
	{
		return;
	}

	const UEnum* StatEnum = StaticEnum<ELISurvivalStat>();
	const int64 EnumValue = StatEnum->GetValueByNameString(StatName);
	if (EnumValue == INDEX_NONE)
	{
		UE_LOG(LogLostIsland, Warning, TEXT("LISetStat: unknown stat '%s' (use Health, Hunger, Thirst or Stamina)."), *StatName);
		return;
	}

	Survival->SetStatValue(static_cast<ELISurvivalStat>(EnumValue), Value);
#endif
}

void ALIPlayerController::ServerDebugDamage_Implementation(float Amount)
{
#if !UE_BUILD_SHIPPING
	if (const ALICharacter* LICharacter = Cast<ALICharacter>(GetPawn()))
	{
		LICharacter->GetSurvivalComponent()->ApplyDamage(Amount, this);
	}
#endif
}

void ALIPlayerController::ServerDebugRevive_Implementation()
{
#if !UE_BUILD_SHIPPING
	// Prototype revive: destroy the dead pawn and let the GameMode spawn a fresh one.
	if (APawn* OldPawn = GetPawn())
	{
		UnPossess();
		OldPawn->Destroy();
	}
	if (AGameModeBase* GameMode = GetWorld()->GetAuthGameMode())
	{
		GameMode->RestartPlayer(this);
	}
#endif
}

void ALIPlayerController::ServerDebugGive_Implementation(const FString& ItemId, int32 Quantity)
{
#if !UE_BUILD_SHIPPING
	if (const ALICharacter* LICharacter = Cast<ALICharacter>(GetPawn()))
	{
		const int32 Added = LICharacter->GetInventoryComponent()->AddItem(FName(*ItemId), Quantity);
		UE_LOG(LogLostIsland, Log, TEXT("LIGive: added %d/%d %s."), Added, Quantity, *ItemId);
	}
#endif
}

void ALIPlayerController::ServerDebugClearInventory_Implementation()
{
#if !UE_BUILD_SHIPPING
	if (const ALICharacter* LICharacter = Cast<ALICharacter>(GetPawn()))
	{
		LICharacter->GetInventoryComponent()->ClearInventory();
	}
#endif
}

void ALIPlayerController::ServerDebugRegenerateIsland_Implementation(int32 Seed)
{
#if !UE_BUILD_SHIPPING
	if (ALIGameMode* GameMode = GetWorld()->GetAuthGameMode<ALIGameMode>())
	{
		GameMode->RegenerateIsland(Seed);
	}
#endif
}
