// Copyright LOST ISLAND Team. All Rights Reserved.

#include "World/LIDroppedLoot.h"

#include "LostIsland.h"
#include "Inventory/LIInventoryComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "LIDroppedLoot"

ALIDroppedLoot::ALIDroppedLoot()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetStaticMesh(SphereMesh.Object);
	Mesh->SetRelativeScale3D(FVector(0.6f, 0.6f, 0.45f));
	Mesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
}

void ALIDroppedLoot::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ALIDroppedLoot, Slots);
}

void ALIDroppedLoot::BeginPlay()
{
	Super::BeginPlay();
	LITintPlaceholder(Mesh, FLinearColor(0.45f, 0.30f, 0.15f)); // leather-brown bundle
	if (HasAuthority() && Lifetime > 0.f)
	{
		SetLifeSpan(Lifetime);
	}
}

void ALIDroppedLoot::SetContents(const TArray<FLIInventorySlot>& InSlots)
{
	Slots.Reset();
	for (const FLIInventorySlot& Slot : InSlots)
	{
		if (!Slot.IsEmpty())
		{
			Slots.Add(Slot);
		}
	}
}

bool ALIDroppedLoot::CanInteract_Implementation(APawn* InstigatorPawn) const
{
	return InstigatorPawn && Slots.Num() > 0;
}

FText ALIDroppedLoot::GetInteractionPrompt_Implementation(APawn* InstigatorPawn) const
{
	return FText::Format(LOCTEXT("TakeLoot", "Take Dropped Items ({0})"), FText::AsNumber(Slots.Num()));
}

void ALIDroppedLoot::Interact_Implementation(APawn* InstigatorPawn)
{
	ULIInventoryComponent* Inventory = InstigatorPawn ? InstigatorPawn->FindComponentByClass<ULIInventoryComponent>() : nullptr;
	if (!HasAuthority() || !Inventory)
	{
		return;
	}

	for (int32 Index = Slots.Num() - 1; Index >= 0; --Index)
	{
		FLIInventorySlot& Slot = Slots[Index];
		// Tools are single-item slots and keep their wear.
		const int32 Added = Inventory->AddItem(Slot.ItemId, Slot.Quantity, Slot.Durability);
		Slot.Quantity -= Added;
		if (Slot.Quantity <= 0)
		{
			Slots.RemoveAt(Index);
		}
	}

	if (Slots.Num() == 0)
	{
		Destroy();
	}
}

#undef LOCTEXT_NAMESPACE
