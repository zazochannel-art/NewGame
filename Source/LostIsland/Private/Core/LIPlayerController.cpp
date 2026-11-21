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
#include "Resources/LIResourceNode.h"
#include "World/LIStorageChest.h"
#include "EngineUtils.h"
#include "Core/LIGameState.h"
#include "Save/LISaveSubsystem.h"
#include "Crafting/LICraftingComponent.h"
#include "Resources/LIResourceNode.h"
#include "GameFramework/HUD.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#if WITH_EDITOR
#include "ShaderCompiler.h"
#endif

void ALIPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalController())
	{
		SetInputMode(FInputModeGameOnly());
		SetShowMouseCursor(false);

#if !UE_BUILD_SHIPPING
		// -LIScreenshotTour on the command line: run the visual smoke test, then quit.
		if (HasAuthority() && FParse::Param(FCommandLine::Get(), TEXT("LIScreenshotTour")))
		{
			LIScreenshotTour(1);
		}
#endif
	}
}

// --- Screenshot tour (visual smoke test) ---

namespace LIScreenshotTourSteps
{
	enum class ELocation : uint8 { Spring, Shore, Forest, CrashSite, Underwater };

	struct FStep
	{
		const TCHAR* Name;
		float Hours;
		ELocation Location;
	};

	static const FStep Steps[] = {
		{ TEXT("day"),    12.0f, ELocation::Spring },
		{ TEXT("beach"),  10.0f, ELocation::Shore },
		{ TEXT("forest"), 14.0f, ELocation::Forest },
		{ TEXT("wreck"),  15.0f, ELocation::CrashSite },
		{ TEXT("sunset"), 19.4f, ELocation::Shore }, // sun about 10 degrees above the horizon
		{ TEXT("underwater"), 12.0f, ELocation::Underwater },
	};

	/** Seconds to let lighting, streaming and the camera settle after each teleport. */
	constexpr double SettleSeconds = 8.0;
	/** Seconds over which the FPS is averaged. */
	constexpr double MeasureSeconds = 4.0;
	/** Upper bound on waiting for shader compilation at a step. */
	constexpr double MaxShaderWaitSeconds = 300.0;
	constexpr float PollInterval = 0.5f;

	enum class EPhase : uint8 { Settle, Measure, Capture };
	static EPhase Phase = EPhase::Settle;
	static double PhaseStartSeconds = 0.0;
}

void ALIPlayerController::LIScreenshotTour(int32 bQuitWhenDone)
{
#if !UE_BUILD_SHIPPING
	if (!HasAuthority())
	{
		UE_LOG(LogLostIsland, Warning, TEXT("LIScreenshotTour: run it on the host or in standalone."));
		return;
	}
	bScreenshotTourQuit = bQuitWhenDone != 0;
	ScreenshotTourStep = INDEX_NONE;
	ScreenshotTourResults.Reset();
	UE_LOG(LogLostIsland, Log, TEXT("LIScreenshotTour: starting (%d views)."), UE_ARRAY_COUNT(LIScreenshotTourSteps::Steps));
	GetWorldTimerManager().SetTimer(ScreenshotTourTimer, this, &ALIPlayerController::AdvanceScreenshotTour,
		LIScreenshotTourSteps::PollInterval, true, 2.f);
#endif
}

void ALIPlayerController::AdvanceScreenshotTour()
{
#if !UE_BUILD_SHIPPING
	using namespace LIScreenshotTourSteps;
	const double Now = FPlatformTime::Seconds();

	if (!GetPawn())
	{
		return; // still spawning; poll again
	}

	if (ScreenshotTourStep == INDEX_NONE)
	{
		if (MyHUD)
		{
			MyHUD->bShowHUD = false;
		}
		ScreenshotTourStep = 0;
		Phase = EPhase::Capture; // falls through to "set up the step" below
		PhaseStartSeconds = -1.0;
	}

	if (Phase == EPhase::Capture && PhaseStartSeconds >= 0.0)
	{
		// The screenshot was requested on the previous poll and has been written by now.
		++ScreenshotTourStep;
	}

	if (Phase == EPhase::Capture)
	{
		if (ScreenshotTourStep >= static_cast<int32>(UE_ARRAY_COUNT(Steps)))
		{
			GetWorldTimerManager().ClearTimer(ScreenshotTourTimer);
			if (MyHUD)
			{
				MyHUD->bShowHUD = true;
			}
			UE_LOG(LogLostIsland, Log, TEXT("LIScreenshotTour: done. Screenshots in %s"), *FPaths::ConvertRelativePathToFull(FPaths::ScreenShotDir()));
			for (const FString& Line : ScreenshotTourResults)
			{
				UE_LOG(LogLostIsland, Log, TEXT("LIScreenshotTour:   %s"), *Line);
			}
			ScreenshotTourStep = INDEX_NONE;
			if (bScreenshotTourQuit)
			{
				ConsoleCommand(TEXT("quit"));
			}
			return;
		}

		const FStep& Step = Steps[ScreenshotTourStep];
		ServerDebugSetTime(Step.Hours);
		ALICharacter* TourCharacter = Cast<ALICharacter>(GetPawn());
		if (TourCharacter)
		{
			TourCharacter->StopDive();
		}
		bool bPlaced = true;
		switch (Step.Location)
		{
		case ELocation::Spring:    ServerDebugGoToSpring(); break;
		case ELocation::Shore:     ServerDebugGoToShore(); break;
		case ELocation::CrashSite: ServerDebugGoToCrashSite(); break;
		case ELocation::Forest:    bPlaced = TeleportToForest(); break;
		case ELocation::Underwater:
			ServerDebugGoToSea();
			if (TourCharacter)
			{
				TourCharacter->StartDive(); // held for the settle time: about 8 s of diving
			}
			break;
		}
		if (!bPlaced)
		{
			UE_LOG(LogLostIsland, Warning, TEXT("LIScreenshotTour: no location for '%s'; capturing from where the player stands."), Step.Name);
		}
		Phase = EPhase::Settle;
		PhaseStartSeconds = Now;
		return;
	}

	if (Phase == EPhase::Settle)
	{
		bool bShadersPending = false;
#if WITH_EDITOR
		bShadersPending = GShaderCompilingManager && GShaderCompilingManager->GetNumRemainingJobs() > 0;
#endif
		const double Waited = Now - PhaseStartSeconds;
		if (Waited < SettleSeconds || (bShadersPending && Waited < MaxShaderWaitSeconds))
		{
			return;
		}
		Phase = EPhase::Measure;
		PhaseStartSeconds = Now;
		ScreenshotTourStartFrame = GFrameCounter;
		ScreenshotTourStartSeconds = Now;
		return;
	}

	if (Phase == EPhase::Measure && Now - PhaseStartSeconds >= MeasureSeconds)
	{
		CaptureScreenshotTourStep();
		Phase = EPhase::Capture;
		PhaseStartSeconds = Now;
	}
#endif
}

void ALIPlayerController::CaptureScreenshotTourStep()
{
#if !UE_BUILD_SHIPPING
	const LIScreenshotTourSteps::FStep& Step = LIScreenshotTourSteps::Steps[ScreenshotTourStep];
	const double Elapsed = FPlatformTime::Seconds() - ScreenshotTourStartSeconds;
	const double Fps = Elapsed > 0.0 ? static_cast<double>(GFrameCounter - ScreenshotTourStartFrame) / Elapsed : 0.0;
	const FString File = FPaths::ScreenShotDir() / FString::Printf(TEXT("LITour_%s.png"), Step.Name);
	FScreenshotRequest::RequestScreenshot(File, false, false);

	const FString Line = FString::Printf(TEXT("%-7s %5.2fh  %6.1f FPS (%.1f ms)  %s"),
		Step.Name, Step.Hours, Fps, Fps > 0.0 ? 1000.0 / Fps : 0.0, *FPaths::GetCleanFilename(File));
	ScreenshotTourResults.Add(Line);
	UE_LOG(LogLostIsland, Log, TEXT("LIScreenshotTour: %s"), *Line);
#endif
}

bool ALIPlayerController::TeleportToForest()
{
	APawn* MyPawn = GetPawn();
	if (!MyPawn)
	{
		return false;
	}

	// The tree with the most other trees within 20 m is the middle of a forest.
	TArray<FVector> Trees;
	for (TActorIterator<ALITreeNode> It(GetWorld()); It; ++It)
	{
		Trees.Add(It->GetActorLocation());
	}
	int32 Best = INDEX_NONE;
	int32 BestCount = -1;
	for (int32 Index = 0; Index < Trees.Num(); ++Index)
	{
		int32 Count = 0;
		for (const FVector& Other : Trees)
		{
			Count += FVector::DistSquared2D(Trees[Index], Other) < FMath::Square(2000.f) ? 1 : 0;
		}
		if (Count > BestCount)
		{
			BestCount = Count;
			Best = Index;
		}
	}
	if (Best == INDEX_NONE)
	{
		return false;
	}

	// Stand 5 m from that tree, looking past it into the forest.
	const FVector Tree = Trees[Best];
	const FVector Location = Tree + FVector(500.f, 0.f, 250.f);
	const float Yaw = (Tree - Location).Rotation().Yaw + 20.f;
	MyPawn->TeleportTo(Location, FRotator(0.f, Yaw, 0.f));
	SetControlRotation(FRotator(-5.f, Yaw, 0.f));
	return true;
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
		SetControlRotation(FRotator(-10.f, 180.f, 0.f)); // camera behind the player, looking at the pond
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

void ALIPlayerController::LIEscape() { ServerDebugEscape(); }

void ALIPlayerController::ServerDebugEscape_Implementation()
{
#if !UE_BUILD_SHIPPING
	if (ALIGameState* GameState = GetWorld()->GetGameState<ALIGameState>())
	{
		GameState->SetEscaped();
	}
#endif
}

void ALIPlayerController::LIGoToShore() { ServerDebugGoToShore(); }

void ALIPlayerController::ServerDebugGoToShore_Implementation()
{
#if !UE_BUILD_SHIPPING
	const ALIGameMode* GameMode = GetWorld()->GetAuthGameMode<ALIGameMode>();
	const ALIIslandGenerator* Island = GameMode ? GameMode->GetIsland() : nullptr;
	APawn* MyPawn = GetPawn();
	if (!Island || !MyPawn)
	{
		return;
	}
	// Walk outward from the start beach to the last dry point before the water.
	const FVector Center = Island->GetActorLocation();
	const FVector Spawn = Island->FindPlayerSpawnTransform().GetLocation();
	const FVector Outward = FVector(Spawn.X - Center.X, Spawn.Y - Center.Y, 0.f).GetSafeNormal();
	FVector Dry = Spawn;
	for (int32 Step = 1; Step < 200; ++Step)
	{
		const FVector Candidate = Spawn + Outward * (Step * 50.f);
		const FVector Local = Island->GetActorTransform().InverseTransformPosition(Candidate);
		const float Height = Island->GetHeightAt(FVector2D(Local.X, Local.Y));
		if (Height < 20.f)
		{
			break;
		}
		Dry = FVector(Candidate.X, Candidate.Y, Center.Z + Height);
	}
	const float Yaw = Outward.Rotation().Yaw;
	MyPawn->TeleportTo(Dry + FVector(0.f, 0.f, 110.f), FRotator(0.f, Yaw, 0.f));
	SetControlRotation(FRotator(-10.f, Yaw, 0.f));
#endif
}

void ALIPlayerController::LIChest(const FString& Action) { ServerDebugChest(Action); }

void ALIPlayerController::ServerDebugChest_Implementation(const FString& Action)
{
#if !UE_BUILD_SHIPPING
	const ALICharacter* LICharacter = Cast<ALICharacter>(GetPawn());
	ALIStorageChest* Nearest = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	for (TActorIterator<ALIStorageChest> It(GetWorld()); LICharacter && It; ++It)
	{
		const float DistSq = FVector::DistSquared(It->GetActorLocation(), LICharacter->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Nearest = *It;
		}
	}
	if (!Nearest)
	{
		return;
	}
	if (Action.Equals(TEXT("take"), ESearchCase::IgnoreCase))
	{
		Nearest->TakeAllInto(LICharacter->GetInventoryComponent());
	}
	else
	{
		Nearest->StoreFrom(LICharacter->GetInventoryComponent());
	}
#endif
}

void ALIPlayerController::LIGoToBoar() { ServerDebugGoToBoar(); }

void ALIPlayerController::ServerDebugGoToBoar_Implementation()
{
#if !UE_BUILD_SHIPPING
	APawn* MyPawn = GetPawn();
	const ALIBoarNode* Nearest = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	for (TActorIterator<ALIBoarNode> It(GetWorld()); MyPawn && It; ++It)
	{
		const float DistSq = FVector::DistSquared(It->GetActorLocation(), MyPawn->GetActorLocation());
		if (!It->IsDepleted() && DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Nearest = *It;
		}
	}
	if (Nearest)
	{
		MyPawn->TeleportTo(Nearest->GetActorLocation() + FVector(600.f, 0.f, 150.f), FRotator(0.f, 180.f, 0.f));
		UE_LOG(LogLostIsland, Log, TEXT("LIGoToBoar: next to %s."), *Nearest->GetName());
	}
#endif
}

void ALIPlayerController::LIShotIn(float Seconds)
{
#if !UE_BUILD_SHIPPING
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		ConsoleCommand(TEXT("HighResShot 1280x720"));
	}), FMath::Max(0.1f, Seconds), false);
#endif
}

void ALIPlayerController::LIAction(const FString& Action)
{
#if !UE_BUILD_SHIPPING
	if (ALICharacter* LICharacter = Cast<ALICharacter>(GetPawn()))
	{
		LICharacter->MulticastPlayAction(Action.Equals(TEXT("hit"), ESearchCase::IgnoreCase)
			? ELICharacterAction::HitReact : ELICharacterAction::Gather);
	}
#endif
}
