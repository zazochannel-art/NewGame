// Copyright LOST ISLAND Team. All Rights Reserved.

#include "World/LIDroppedLoot.h"

#include "Math/RandomStream.h"
#include "ProceduralMeshComponent.h"
#include "Visuals/LILowPolyMesh.h"
#include "Visuals/LIVisualAssets.h"
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

	// Detailed model (cosmetic): a tied leather sack. The root sphere is scaled, so the model uses absolute scale.
	{
		const FLinearColor Leather(0.42f, 0.27f, 0.13f);
		const FLinearColor Cord(0.20f, 0.14f, 0.08f);
		FRandomStream Random(LIVisualSeed(GetActorLocation(), 0x10A7));
		FLILowPolyMesh Model;
		Model.AddBlob(FTransform(FVector(0.f, 0.f, -2.f)), FVector(30.f, 27.f, 21.f), 0.12f, Random, Leather);
		Model.AddFrustum(FTransform(FVector(0.f, 0.f, 15.f)), 8, 11.f, 6.f, 13.f, Leather, 0.08f, &Random);
		Model.AddFrustum(FTransform(FVector(0.f, 0.f, 22.f)), 8, 7.5f, 7.5f, 3.f, Cord);
		Model.AddBlob(FTransform(FVector(0.f, 0.f, 30.f)), FVector(8.f, 8.f, 5.f), 0.15f, Random, Leather, nullptr, 0.7f, 0);
		if (UProceduralMeshComponent* Visual = LICreateVisualComponent(this))
		{
			Visual->SetUsingAbsoluteScale(true);
			Visual->SetWorldScale3D(FVector::OneVector);
			if (Model.ApplyTo(Visual))
			{
				Mesh->SetHiddenInGame(true);
			}
		}
	}
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
