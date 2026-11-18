// Copyright LOST ISLAND Team. All Rights Reserved.

#include "Inventory/LIInventoryComponent.h"

#include "Items/LIItemRegistrySubsystem.h"
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
	return Registry ? Registry->GetMaxStackSize(ItemId) : 0;
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
// Mutation (server)
// ---------------------------------------------------------------------------

int32 ULIInventoryComponent::AddItem(FName ItemId, int32 Quantity)
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
