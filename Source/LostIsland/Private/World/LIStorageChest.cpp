// Copyright LOST ISLAND Team. All Rights Reserved.

#include "World/LIStorageChest.h"

#include "LostIsland.h"
#include "Inventory/LIInventoryComponent.h"
#include "Items/LIItemDefinition.h"
#include "Items/LIItemRegistrySubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "LIStorageChest"

ALIStorageChest::ALIStorageChest()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Box = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Box"));
	Box->SetupAttachment(Root);
	Box->SetStaticMesh(CubeMesh.Object);
	Box->SetRelativeScale3D(FVector(1.0f, 0.6f, 0.55f));
	Box->SetRelativeLocation(FVector(0.f, 0.f, 27.5f));
	Box->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

	Lid = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Lid"));
	Lid->SetupAttachment(Root);
	Lid->SetStaticMesh(CubeMesh.Object);
	Lid->SetRelativeScale3D(FVector(1.05f, 0.65f, 0.1f));
	Lid->SetRelativeLocation(FVector(0.f, 0.f, 60.f));
	Lid->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
}

void ALIStorageChest::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ALIStorageChest, Slots);
}

void ALIStorageChest::BeginPlay()
{
	Super::BeginPlay();
	LITintPlaceholder(Box, FLinearColor(0.40f, 0.26f, 0.12f));
	LITintPlaceholder(Lid, FLinearColor(0.30f, 0.19f, 0.09f));
}

int32 ALIStorageChest::GetUsedSlots() const
{
	int32 Used = 0;
	for (const FLIInventorySlot& Slot : Slots)
	{
		Used += Slot.IsEmpty() ? 0 : 1;
	}
	return Used;
}

void ALIStorageChest::SetSlots(const TArray<FLIInventorySlot>& InSlots)
{
	if (!HasAuthority())
	{
		return;
	}
	Slots.Reset();
	for (const FLIInventorySlot& Slot : InSlots)
	{
		if (!Slot.IsEmpty() && Slots.Num() < Capacity)
		{
			Slots.Add(Slot);
		}
	}
}

bool ALIStorageChest::CanInteract_Implementation(APawn* InstigatorPawn) const
{
	return InstigatorPawn != nullptr;
}

FText ALIStorageChest::GetInteractionPrompt_Implementation(APawn* InstigatorPawn) const
{
	return FText::Format(LOCTEXT("ChestPrompt", "Store materials ({0}/{1})   [R] Take all"),
		FText::AsNumber(GetUsedSlots()), FText::AsNumber(Capacity));
}

void ALIStorageChest::Interact_Implementation(APawn* InstigatorPawn)
{
	if (HasAuthority() && InstigatorPawn)
	{
		StoreFrom(InstigatorPawn->FindComponentByClass<ULIInventoryComponent>());
	}
}

int32 ALIStorageChest::AddToChest(FName ItemId, int32 Quantity, int32 Durability)
{
	const ULIItemRegistrySubsystem* Registry = ULIItemRegistrySubsystem::Get(this);
	const ULIItemDefinition* Def = Registry ? Registry->FindItem(ItemId) : nullptr;
	const int32 MaxStack = Def ? FMath::Max(1, Def->MaxStackSize) : 1;
	int32 Remaining = Quantity;

	if (Durability <= 0)
	{
		for (FLIInventorySlot& Slot : Slots)
		{
			if (Remaining > 0 && Slot.ItemId == ItemId && Slot.Quantity < MaxStack)
			{
				const int32 ToAdd = FMath::Min(Remaining, MaxStack - Slot.Quantity);
				Slot.Quantity += ToAdd;
				Remaining -= ToAdd;
			}
		}
	}
	while (Remaining > 0 && Slots.Num() < Capacity)
	{
		FLIInventorySlot& NewSlot = Slots.AddDefaulted_GetRef();
		NewSlot.ItemId = ItemId;
		NewSlot.Quantity = FMath::Min(Remaining, MaxStack);
		NewSlot.Durability = Durability;
		Remaining -= NewSlot.Quantity;
	}
	return Quantity - Remaining;
}

int32 ALIStorageChest::StoreFrom(ULIInventoryComponent* Inventory)
{
	const ULIItemRegistrySubsystem* Registry = ULIItemRegistrySubsystem::Get(this);
	if (!HasAuthority() || !Inventory || !Registry)
	{
		return 0;
	}

	// Copy first: RemoveItem reshapes the inventory while we iterate.
	const TArray<FLIInventorySlot> Carried = Inventory->GetSlots();
	int32 Moved = 0;
	for (const FLIInventorySlot& Slot : Carried)
	{
		const ULIItemDefinition* Def = Slot.IsEmpty() ? nullptr : Registry->FindItem(Slot.ItemId);
		const bool bStorable = Def && !Def->PlaceableActorClass
			&& (Def->Category == ELIItemCategory::Resource || Def->Category == ELIItemCategory::Food);
		if (!bStorable)
		{
			continue;
		}
		const int32 Added = AddToChest(Slot.ItemId, Slot.Quantity, 0);
		if (Added > 0)
		{
			Inventory->RemoveItem(Slot.ItemId, Added);
			Moved += Added;
		}
	}
	UE_LOG(LogLostIsland, Log, TEXT("%s: stored %d items (%d/%d stacks)."), *GetName(), Moved, GetUsedSlots(), Capacity);
	return Moved;
}

int32 ALIStorageChest::TakeAllInto(ULIInventoryComponent* Inventory)
{
	if (!HasAuthority() || !Inventory)
	{
		return 0;
	}
	int32 Moved = 0;
	for (int32 Index = Slots.Num() - 1; Index >= 0; --Index)
	{
		FLIInventorySlot& Slot = Slots[Index];
		const int32 Added = Inventory->AddItem(Slot.ItemId, Slot.Quantity, Slot.Durability);
		Slot.Quantity -= Added;
		Moved += Added;
		if (Slot.Quantity <= 0)
		{
			Slots.RemoveAt(Index);
		}
	}
	UE_LOG(LogLostIsland, Log, TEXT("%s: took %d items (%d stacks left)."), *GetName(), Moved, GetUsedSlots());
	return Moved;
}

#undef LOCTEXT_NAMESPACE
