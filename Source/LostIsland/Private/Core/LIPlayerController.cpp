// Copyright LOST ISLAND Team. All Rights Reserved.

#include "Core/LIPlayerController.h"

#include "Character/LICharacter.h"
#include "LostIsland.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "Survival/LISurvivalComponent.h"
#include "Inventory/LIInventoryComponent.h"
#include "Core/LIGameMode.h"
#include "World/LIIslandGenerator.h"
#include "World/LIWaterSource.h"
#include "EngineUtils.h"
#include "Core/LIGameState.h"
#include "Save/LISaveSubsystem.h"
#include "Crafting/LICraftingComponent.h"

void ALIPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalController())
	{
		SetInputMode(FInputModeGameOnly());
		SetShowMouseCursor(false);
	}
}

// --- HUD messages ---

void ALIPlayerController::ClientShowMessage_Implementation(const FText& Message)
{
	HudMessage = Message;
	HudMessageExpireTime = GetWorld()->GetTimeSeconds() + 2.5;
}

FText ALIPlayerController::GetHudMessage() const
{
	return GetWorld() && GetWorld()->GetTimeSeconds() < HudMessageExpireTime ? HudMessage : FText::GetEmpty();
}

// --- Save commands (host only: in co-op the host owns the world save) ---

void ALIPlayerController::LISave() { ServerSaveGame(); }
void ALIPlayerController::LILoad() { ServerLoadGame(); }
void ALIPlayerController::LINewGame() { ServerNewGame(); }

void ALIPlayerController::ServerSaveGame_Implementation()
{
	if (!IsLocalController())
	{
		return; // only the host may save the shared world
	}
	if (ULISaveSubsystem* SaveSubsystem = ULISaveSubsystem::Get(this))
	{
		SaveSubsystem->SaveCurrentGame();
	}
}

void ALIPlayerController::ServerLoadGame_Implementation()
{
	if (!IsLocalController())
	{
		return;
	}
	ULISaveSubsystem* SaveSubsystem = ULISaveSubsystem::Get(this);
	if (SaveSubsystem && !SaveSubsystem->LoadAndRestart())
	{
		ClientShowMessage(NSLOCTEXT("LISave", "NoSave", "No save found"));
	}
}

void ALIPlayerController::ServerNewGame_Implementation()
{
	if (!IsLocalController())
	{
		return;
	}
	if (ULISaveSubsystem* SaveSubsystem = ULISaveSubsystem::Get(this))
	{
		SaveSubsystem->NewGameAndRestart();
	}
}

// --- Debug commands (client entry points) ---

void ALIPlayerController::LISetStat(const FString& StatName, float Value) { ServerDebugSetStat(StatName, Value); }
void ALIPlayerController::LIDamage(float Amount) { ServerDebugDamage(Amount); }
void ALIPlayerController::LIRevive() { ServerDebugRevive(); }
void ALIPlayerController::LIGive(const FString& ItemId, int32 Quantity) { ServerDebugGive(ItemId, Quantity); }
void ALIPlayerController::LIClearInventory() { ServerDebugClearInventory(); }
void ALIPlayerController::LIRegenerateIsland(int32 Seed) { ServerDebugRegenerateIsland(Seed); }
void ALIPlayerController::LIGoToCrashSite() { ServerDebugGoToCrashSite(); }
void ALIPlayerController::LIGoToSea() { ServerDebugGoToSea(); }
void ALIPlayerController::LISetTime(float Hours) { ServerDebugSetTime(Hours); }

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
	// Immediate respawn (sleeping bag or beach) with fresh stats.
	if (ALIGameMode* GameMode = GetWorld()->GetAuthGameMode<ALIGameMode>())
	{
		GameMode->RespawnPlayer(this);
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

void ALIPlayerController::ServerDebugGoToCrashSite_Implementation()
{
#if !UE_BUILD_SHIPPING
	const ALIGameMode* GameMode = GetWorld()->GetAuthGameMode<ALIGameMode>();
	const ALIIslandGenerator* Island = GameMode ? GameMode->GetIsland() : nullptr;
	APawn* MyPawn = GetPawn();
	if (!Island || !Island->HasCrashSite() || !MyPawn)
	{
		return;
	}

	// Stand 10 m to the side of the wreck, a little above ground, looking at it.
	const FTransform Wreck = Island->GetCrashSiteTransform();
	const FVector Location = Wreck.GetLocation() + Wreck.GetRotation().GetRightVector() * -1000.f + FVector(0.f, 0.f, 300.f);
	const FRotator LookAt = (Wreck.GetLocation() - Location).Rotation();
	MyPawn->TeleportTo(Location, FRotator(0.f, LookAt.Yaw, 0.f));
	SetControlRotation(FRotator(-10.f, LookAt.Yaw, 0.f));
#endif
}

void ALIPlayerController::ServerDebugSetTime_Implementation(float Hours)
{
#if !UE_BUILD_SHIPPING
	if (ALIGameState* GameState = GetWorld()->GetGameState<ALIGameState>())
	{
		GameState->SetTime(GameState->GetDayNumber(), FMath::Fmod(FMath::Max(0.f, Hours), 24.f));
	}
#endif
}

void ALIPlayerController::ServerDebugGoToSea_Implementation()
{
#if !UE_BUILD_SHIPPING
	const ALIGameMode* GameMode = GetWorld()->GetAuthGameMode<ALIGameMode>();
	const ALIIslandGenerator* Island = GameMode ? GameMode->GetIsland() : nullptr;
	APawn* MyPawn = GetPawn();
	if (!Island || !MyPawn)
	{
		return;
	}

	// Walk outward from the island centre through the start beach until the seabed is at least 5 m deep.
	const FVector Center = Island->GetActorLocation();
	const FVector Spawn = Island->FindPlayerSpawnTransform().GetLocation();
	const FVector Outward = FVector(Spawn.X - Center.X, Spawn.Y - Center.Y, 0.f).GetSafeNormal();
	FVector Location = Spawn;
	for (int32 Step = 0; Step < 100; ++Step)
	{
		Location += Outward * 200.f;
		const FVector Local = Island->GetActorTransform().InverseTransformPosition(Location);
		if (Island->GetHeightAt(FVector2D(Local.X, Local.Y)) < -500.f)
		{
			break;
		}
	}
	Location.Z = Center.Z + 50.f;
	MyPawn->TeleportTo(Location, FRotator(0.f, (-Outward).Rotation().Yaw, 0.f));
	SetControlRotation(FRotator(-10.f, (-Outward).Rotation().Yaw, 0.f));
#endif
}

void ALIPlayerController::LIPlace()
{
	if (const ALICharacter* LICharacter = Cast<ALICharacter>(GetPawn()))
	{
		if (ULICraftingComponent* Crafting = LICharacter->GetCraftingComponent())
		{
			Crafting->RequestPlace();
		}
	}
}

void ALIPlayerController::LIBuildMode()
{
	if (ALICharacter* LICharacter = Cast<ALICharacter>(GetPawn()))
	{
		LICharacter->ToggleBuildMode();
	}
}

void ALIPlayerController::LICraft(const FString& RecipeId)
{
	if (const ALICharacter* LICharacter = Cast<ALICharacter>(GetPawn()))
	{
		if (ULICraftingComponent* Crafting = LICharacter->GetCraftingComponent())
		{
			Crafting->RequestCraft(FName(*RecipeId));
		}
	}
}

void ALIPlayerController::LIGoToSpring() { ServerDebugGoToSpring(); }

void ALIPlayerController::ServerDebugGoToSpring_Implementation()
{
#if !UE_BUILD_SHIPPING
	APawn* MyPawn = GetPawn();
	const ALIWaterSource* Nearest = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	for (TActorIterator<ALIWaterSource> It(GetWorld()); MyPawn && It; ++It)
	{
		const float DistSq = FVector::DistSquared(It->GetActorLocation(), MyPawn->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Nearest = *It;
		}
	}
	if (Nearest)
	{
		MyPawn->TeleportTo(Nearest->GetActorLocation() + FVector(200.f, 0.f, 120.f), FRotator(0.f, 180.f, 0.f));
	}
#endif
}

void ALIPlayerController::LIConsume()
{
	if (const ALICharacter* LICharacter = Cast<ALICharacter>(GetPawn()))
	{
		LICharacter->GetInventoryComponent()->RequestConsumeBest();
	}
}
