// Copyright LOST ISLAND Team. All Rights Reserved.

#include "Interaction/LIInteractableActor.h"

#include "LostIsland.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "LIInteractableActor"

ALIInteractableActor::ALIInteractableActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

	// Engine placeholder so the actor is visible without any project content.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMesh.Object);
	}

	PromptText = LOCTEXT("DefaultPrompt", "Inspect");
}

void ALIInteractableActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ALIInteractableActor, UseCount);
	DOREPLIFETIME_CONDITION(ALIInteractableActor, PromptText, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ALIInteractableActor, MaxUses, COND_InitialOnly);
}

bool ALIInteractableActor::CanInteract_Implementation(APawn* InstigatorPawn) const
{
	return MaxUses == 0 || UseCount < MaxUses;
}

void ALIInteractableActor::Interact_Implementation(APawn* InstigatorPawn)
{
	++UseCount;

	UE_LOG(LogLostIsland, Log, TEXT("%s interacted with %s (use %d)."),
		*GetNameSafe(InstigatorPawn), *GetName(), UseCount);

#if !UE_BUILD_SHIPPING
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Cyan,
			FString::Printf(TEXT("%s: %s (use %d)"), *GetName(), *PromptText.ToString(), UseCount));
	}
#endif

	BP_OnInteracted(InstigatorPawn, UseCount);
}

FText ALIInteractableActor::GetInteractionPrompt_Implementation(APawn* InstigatorPawn) const
{
	return PromptText;
}

void ALIInteractableActor::OnFocusBegin_Implementation(APawn* InstigatorPawn)
{
	// Custom depth lets a post-process outline material highlight the object later.
	Mesh->SetRenderCustomDepth(true);
}

void ALIInteractableActor::OnFocusEnd_Implementation(APawn* InstigatorPawn)
{
	Mesh->SetRenderCustomDepth(false);
}

#undef LOCTEXT_NAMESPACE
