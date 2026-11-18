// Copyright LOST ISLAND Team. All Rights Reserved.

#include "Inventory/LIInventoryComponent.h"

#include "Items/LIItemDefinition.h"
#include "Items/LIItemRegistrySubsystem.h"
#include "Survival/LISurvivalComponent.h"
#include "LostIsland.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

#define LOCTEXT_NAMESPACE "LIInventory"

ULIInventoryComponent::ULIInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetIsReplicatedByDefault(true);
}

void ULIInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ULIInventoryComponent, Slots, COND_OwnerOnly);
}

void ULIInventoryComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetOwnerRole() == ROLE_Authority && Slots.Num() == 0)
	{
		Slots.SetNum(DefaultSlotCount);
		NotifyChanged();
	}
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------

int32 ULIInventoryComponent::GetMaxStack(FName ItemId) const
{
	const ULIItemRegistrySubsystem* Registry = ULIItemRegistrySubsystem::Get(this);
	const int32 MaxStack = Registry ? Registry->GetMaxStackSize(ItemId) : 0;
	// Items with durability are individual objects: never stack them.
	return (MaxStack > 0 && GetMaxDurability(ItemId) > 0) ? 1 : MaxStack;
}

int32 ULIInventoryComponent::GetMaxDurability(FName ItemId) const
{
	const ULIItemRegistrySubsystem* Registry = ULIItemRegistrySubsystem::Get(this);
	const ULIItemDefinition* Def = Registry ? Registry->FindItem(ItemId) : nullptr;
	return Def ? FMath::Max(0, Def->MaxDurability) : 0;
}

int32 ULIInventoryComponent::FindBestToolSlot(ELIToolType ToolType) const
{
	if (ToolType == ELIToolType::None)
	{
		return INDEX_NONE;
	}
	const ULIItemRegistrySubsystem* Registry = ULIItemRegistrySubsystem::Get(this);
	if (!Registry)
	{
		return INDEX_NONE;
	}

	int32 BestIndex = INDEX_NONE;
	float BestMultiplier = 0.f;
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		const FLIInventorySlot& Slot = Slots[Index];
		const ULIItemDefinition* Def = Slot.IsEmpty() ? nullptr : Registry->FindItem(Slot.ItemId);
		if (Def && Def->ToolType == ToolType && Def->ToolYieldMultiplier > BestMultiplier)
		{
			BestMultiplier = Def->ToolYieldMultiplier;
			BestIndex = Index;
		}
	}
	return BestIndex;
}

void ULIInventoryComponent::ConsumeToolDurability(int32 SlotIndex)
{
	if (GetOwnerRole() != ROLE_Authority || !Slots.IsValidIndex(SlotIndex) || Slots[SlotIndex].IsEmpty())
	{
		return;
	}

	FLIInventorySlot& Slot = Slots[SlotIndex];
	if (GetMaxDurability(Slot.ItemId) <= 0)
	{
		return; // indestructible
	}

	Slot.Durability = FMath::Max(0, Slot.Durability - 1);
	if (Slot.Durability == 0)
	{
		const FName Broken = Slot.ItemId;
		Slot.Clear();
		ClientNotifyToolBroke(Broken);
	}
	NotifyChanged();
}

void ULIInventoryComponent::ClientNotifyToolBroke_Implementation(FName ItemId)
{
	const ULIItemRegistrySubsystem* Registry = ULIItemRegistrySubsystem::Get(this);
	const FText Name = Registry ? Registry->GetDisplayName(ItemId) : FText::FromName(ItemId);
	PushNotice(FText::Format(LOCTEXT("Broke", "{0} broke!"), Name), true);
}

int32 ULIInventoryComponent::GetItemCount(FName ItemId) const
{
	int32 Count = 0;
	for (const FLIInventorySlot& Slot : Slots)
	{
		if (Slot.ItemId == ItemId)
		{
			Count += Slot.Quantity;
		}
	}
	return Count;
}

int32 ULIInventoryComponent::GetRoomFor(FName ItemId) const
{
	const int32 MaxStack = GetMaxStack(ItemId);
	if (MaxStack <= 0)
	{
		return 0;
	}

	int32 Room = 0;
	for (const FLIInventorySlot& Slot : Slots)
	{
		if (Slot.IsEmpty())
		{
			Room += MaxStack;
		}
		else if (Slot.ItemId == ItemId)
		{
			Room += FMath::Max(0, MaxStack - Slot.Quantity);
		}
	}
	return Room;
}

// ---------------------------------------------------------------------------
// Consumables
// ---------------------------------------------------------------------------

FName ULIInventoryComponent::FindBestConsumable() const
{
	const ULIItemRegistrySubsystem* Registry = ULIItemRegistrySubsystem::Get(this);
	const ULISurvivalComponent* Survival = GetOwner() ? GetOwner()->FindComponentByClass<ULISurvivalComponent>() : nullptr;
	if (!Registry || !Survival || Survival->IsDead())
	{
		return NAME_None;
	}

	const float HungerDeficit = Survival->GetStatMax(ELISurvivalStat::Hunger) - Survival->GetStatValue(ELISurvivalStat::Hunger);
	const float ThirstDeficit = Survival->GetStatMax(ELISurvivalStat::Thirst) - Survival->GetStatValue(ELISurvivalStat::Thirst);
	const float HealthDeficit = Survival->GetStatMax(ELISurvivalStat::Health) - Survival->GetStatValue(ELISurvivalStat::Health);

	FName BestId = NAME_None;
	float BestScore = 0.f;
	for (const FLIInventorySlot& Slot : Slots)
	{
		if (Slot.IsEmpty() || Slot.ItemId == BestId)
		{
			continue;
		}
		const ULIItemDefinition* Def = Registry->FindItem(Slot.ItemId);
		if (!Def || !Def->IsConsumable())
		{
			continue;
		}

		// Only count the part of each value that is actually needed (no reward for over-eating).
		const float Score = FMath::Min(Def->FoodValue, HungerDeficit) + FMath::Min(Def->WaterValue, ThirstDeficit)
			+ FMath::Min(FMath::Max(Def->HealthValue, 0.f), HealthDeficit);
		if (Score > BestScore + KINDA_SMALL_NUMBER)
		{
			BestScore = Score;
			BestId = Slot.ItemId;
		}
	}

	// Require a meaningful benefit so a nearly-full player doesn't waste food.
	return BestScore >= 1.f ? BestId : NAME_None;
}

void ULIInventoryComponent::RequestConsumeBest()
{
	if (GetOwnerRole() == ROLE_Authority)
	{
		ServerConsumeBest_Implementation();
	}
	else
	{
		ServerConsumeBest();
	}
}

void ULIInventoryComponent::ServerConsumeBest_Implementation()
{
	const FName ItemId = FindBestConsumable();
	if (!ItemId.IsNone())
	{
		ConsumeItem(ItemId);
	}
}

bool ULIInventoryComponent::ConsumeItem(FName ItemId)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return false;
	}

	const ULIItemRegistrySubsystem* Registry = ULIItemRegistrySubsystem::Get(this);
	const ULIItemDefinition* Def = Registry ? Registry->FindItem(ItemId) : nullptr;
	ULISurvivalComponent* Survival = GetOwner() ? GetOwner()->FindComponentByClass<ULISurvivalComponent>() : nullptr;
	if (!Def || !Def->IsConsumable() || !Survival || Survival->IsDead())
	{
		return false;
	}

	if (!RemoveItem(ItemId, 1))
	{
		return false;
	}

	Survival->ModifyStat(ELISurvivalStat::Hunger, Def->FoodValue);
	Survival->ModifyStat(ELISurvivalStat::Thirst, Def->WaterValue);
	Survival->ModifyStat(ELISurvivalStat::Health, Def->HealthValue);
	if (!Def->ConsumeReturnsItem.IsNone())
	{
		AddItem(Def->ConsumeReturnsItem, 1); // lost only if the inventory is completely full
	}

	UE_LOG(LogLostIsland, Log, TEXT("%s consumed %s%s."), *GetNameSafe(GetOwner()), *ItemId.ToString(),
		Def->ConsumeReturnsItem.IsNone() ? TEXT("") : *FString::Printf(TEXT(" (got %s back)"), *Def->ConsumeReturnsItem.ToString()));
	ClientNotifyConsumed(ItemId);
	return true;
}

void ULIInventoryComponent::ClientNotifyConsumed_Implementation(FName ItemId)
{
	const ULIItemRegistrySubsystem* Registry = ULIItemRegistrySubsystem::Get(this);
	const FText Name = Registry ? Registry->GetDisplayName(ItemId) : FText::FromName(ItemId);
	PushNotice(FText::Format(LOCTEXT("Consumed", "Consumed {0}"), Name), false);
}

// ---------------------------------------------------------------------------
// Mutation (server)
// ---------------------------------------------------------------------------

int32 ULIInventoryComponent::AddItem(FName ItemId, int32 Quantity, int32 DurabilityOverride)
{
	if (GetOwnerRole() != ROLE_Authority || ItemId.IsNone() || Quantity <= 0)
	{
		return 0;
	}

	const int32 MaxStack = GetMaxStack(ItemId);
	if (MaxStack <= 0)
	{
		UE_LOG(LogLostIsland, Warning, TEXT("AddItem: unknown item '%s'."), *ItemId.ToString());
		return 0;
	}

	int32 Remaining = Quantity;

	// 1) Top up existing stacks of the same item.
	for (FLIInventorySlot& Slot : Slots)
	{
		if (Remaining <= 0)
		{
			break;
		}
		if (Slot.ItemId == ItemId && Slot.Quantity < MaxStack)
		{
			const int32 ToAdd = FMath::Min(Remaining, MaxStack - Slot.Quantity);
			Slot.Quantity += ToAdd;
			Remaining -= ToAdd;
		}
	}

	// 2) Fill empty slots.
	for (FLIInventorySlot& Slot : Slots)
	{
		if (Remaining <= 0)
		{
			break;
		}
		if (Slot.IsEmpty())
		{
			const int32 ToAdd = FMath::Min(Remaining, MaxStack);
			Slot.ItemId = ItemId;
			Slot.Quantity = ToAdd;
			const int32 MaxDurability = GetMaxDurability(ItemId);
			Slot.Durability = DurabilityOverride > 0 ? FMath::Min(DurabilityOverride, MaxDurability) : MaxDurability;
			Remaining -= ToAdd;
		}
	}

	const int32 Added = Quantity - Remaining;
	if (Added > 0)
	{
		NotifyChanged();
		ClientNotifyItemAdded(ItemId, Added);
	}
	if (Remaining > 0)
	{
		ClientNotifyInventoryFull(ItemId);
	}

	return Added;
}

bool ULIInventoryComponent::RemoveItem(FName ItemId, int32 Quantity)
{
	if (GetOwnerRole() != ROLE_Authority || ItemId.IsNone() || Quantity <= 0 || GetItemCount(ItemId) < Quantity)
	{
		return false;
	}

	int32 Remaining = Quantity;

	// Take from the last stacks first so the first slots stay full.
	for (int32 Index = Slots.Num() - 1; Index >= 0 && Remaining > 0; --Index)
	{
		FLIInventorySlot& Slot = Slots[Index];
		if (Slot.ItemId != ItemId)
		{
			continue;
		}

		const int32 ToRemove = FMath::Min(Remaining, Slot.Quantity);
		Slot.Quantity -= ToRemove;
		Remaining -= ToRemove;
		if (Slot.Quantity <= 0)
		{
			Slot.Clear();
		}
	}

	NotifyChanged();
	return true;
}

void ULIInventoryComponent::ClearInventory()
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	for (FLIInventorySlot& Slot : Slots)
	{
		Slot.Clear();
	}
	NotifyChanged();
}

void ULIInventoryComponent::SetSlots(const TArray<FLIInventorySlot>& NewSlots)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	const int32 Count = FMath::Max(DefaultSlotCount, NewSlots.Num());
	Slots.Reset();
	Slots.SetNum(Count);

	for (int32 Index = 0; Index < NewSlots.Num(); ++Index)
	{
		const FLIInventorySlot& Source = NewSlots[Index];
		const int32 MaxStack = GetMaxStack(Source.ItemId);
		if (Source.IsEmpty() || MaxStack <= 0)
		{
			continue;
		}
		Slots[Index].ItemId = Source.ItemId;
		Slots[Index].Quantity = FMath::Clamp(Source.Quantity, 1, MaxStack);
		const int32 MaxDurability = GetMaxDurability(Source.ItemId);
		// Older saves have no durability (0): treat as a fresh tool.
		Slots[Index].Durability = MaxDurability > 0
			? (Source.Durability > 0 ? FMath::Min(Source.Durability, MaxDurability) : MaxDurability)
			: 0;
	}

	NotifyChanged();
}

void ULIInventoryComponent::SetSlotCount(int32 NewSlotCount)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	Slots.SetNum(FMath::Clamp(NewSlotCount, 1, 200));
	NotifyChanged();
}

void ULIInventoryComponent::NotifyChanged()
{
	OnInventoryChanged.Broadcast();
}

void ULIInventoryComponent::OnRep_Slots()
{
	OnInventoryChanged.Broadcast();
}

// ---------------------------------------------------------------------------
// Notices (owning client)
// ---------------------------------------------------------------------------

void ULIInventoryComponent::ClientNotifyItemAdded_Implementation(FName ItemId, int32 Quantity)
{
	const ULIItemRegistrySubsystem* Registry = ULIItemRegistrySubsystem::Get(this);
	const FText Name = Registry ? Registry->GetDisplayName(ItemId) : FText::FromName(ItemId);
	PushNotice(FText::Format(LOCTEXT("Added", "+{0} {1}"), FText::AsNumber(Quantity), Name), false);
}

void ULIInventoryComponent::ClientNotifyInventoryFull_Implementation(FName ItemId)
{
	PushNotice(LOCTEXT("Full", "Inventory full"), true);
}

void ULIInventoryComponent::PushNotice(const FText& Text, bool bIsWarning)
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FLIInventoryNotice& Notice = Notices.AddDefaulted_GetRef();
	Notice.Text = Text;
	Notice.bIsWarning = bIsWarning;
	Notice.ExpireTime = World->GetTimeSeconds() + NoticeDuration;

	if (Notices.Num() > MaxNotices)
	{
		Notices.RemoveAt(0, Notices.Num() - MaxNotices);
	}
}

const TArray<FLIInventoryNotice>& ULIInventoryComponent::GetActiveNotices()
{
	if (const UWorld* World = GetWorld())
	{
		const double Now = World->GetTimeSeconds();
		Notices.RemoveAll([Now](const FLIInventoryNotice& Notice) { return Notice.ExpireTime <= Now; });
	}
	return Notices;
}

#undef LOCTEXT_NAMESPACE
