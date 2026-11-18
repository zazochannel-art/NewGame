// Copyright LOST ISLAND Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "LIPlayerController.generated.h"

/**
 * ALIPlayerController
 *
 * Gameplay input lives on the pawn (ALICharacter) so it travels with possession.
 * This class owns input mode, and (later) UI such as inventory and menus.
 *
 * Development console commands (ignored in Shipping builds), routed to the server:
 *     LISetStat <Health|Hunger|Thirst|Stamina> <Value>
 *     LIDamage <Amount>
 *     LIRevive
 *     LIGive <ItemId> <Quantity>
 *     LIClearInventory
 *     LIRegenerateIsland <Seed>      (0 = random)
 *     LIGoToCrashSite
 *     LIGoToSea                      (deep water off the start beach, to test swimming)
 *     LIPlace                        (same as G)
 *     LISetTime <Hours>              (e.g. 21 for night)
 *
 * Save commands (all builds):  LISave, LILoad, LINewGame   (also F5 / F9 on the character)
 */
UCLASS()
class LOSTISLAND_API ALIPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	/** Show/hide the inventory panel (HUD reads IsInventoryOpen). */
	UFUNCTION(BlueprintCallable, Category = "LostIsland|UI")
	void ToggleInventoryUI() { bInventoryOpen = !bInventoryOpen; }

	UFUNCTION(BlueprintPure, Category = "LostIsland|UI")
	bool IsInventoryOpen() const { return bInventoryOpen; }

	/** Server -> owning client: short HUD message ("Game saved"). */
	UFUNCTION(Client, Unreliable)
	void ClientShowMessage(const FText& Message);

	/** Current HUD message, empty when expired. */
	FText GetHudMessage() const;

	UFUNCTION(Exec, BlueprintCallable, Category = "LostIsland|Save")
	void LISave();

	UFUNCTION(Exec, BlueprintCallable, Category = "LostIsland|Save")
	void LILoad();

	UFUNCTION(Exec, BlueprintCallable, Category = "LostIsland|Save")
	void LINewGame();

	UFUNCTION(Exec)
	void LIGive(const FString& ItemId, int32 Quantity);

	UFUNCTION(Exec)
	void LIClearInventory();

	UFUNCTION(Exec)
	void LIRegenerateIsland(int32 Seed);

	UFUNCTION(Exec)
	void LIGoToCrashSite();

	UFUNCTION(Exec)
	void LIGoToSea();

	/** Same as the G key: place the first placeable item (campfire, sleeping bag). */
	UFUNCTION(Exec)
	void LIPlace();

	UFUNCTION(Exec)
	void LISetTime(float Hours);

	UFUNCTION(Exec)
	void LISetStat(const FString& StatName, float Value);

	UFUNCTION(Exec)
	void LIDamage(float Amount);

	UFUNCTION(Exec)
	void LIRevive();

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION(Server, Reliable)
	void ServerDebugSetStat(const FString& StatName, float Value);

	UFUNCTION(Server, Reliable)
	void ServerDebugDamage(float Amount);

	UFUNCTION(Server, Reliable)
	void ServerDebugRevive();

	UFUNCTION(Server, Reliable)
	void ServerDebugGive(const FString& ItemId, int32 Quantity);

	UFUNCTION(Server, Reliable)
	void ServerDebugClearInventory();

	UFUNCTION(Server, Reliable)
	void ServerDebugRegenerateIsland(int32 Seed);

	UFUNCTION(Server, Reliable)
	void ServerDebugGoToCrashSite();

	UFUNCTION(Server, Reliable)
	void ServerDebugGoToSea();

	UFUNCTION(Server, Reliable)
	void ServerDebugSetTime(float Hours);

	UFUNCTION(Server, Reliable)
	void ServerSaveGame();

	UFUNCTION(Server, Reliable)
	void ServerLoadGame();

	UFUNCTION(Server, Reliable)
	void ServerNewGame();

	bool bInventoryOpen = false;

	FText HudMessage;
	double HudMessageExpireTime = 0.0;
};
