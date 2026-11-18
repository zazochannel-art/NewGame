// Copyright LOST ISLAND Team. All Rights Reserved.

#include "Interaction/LIInteractionComponent.h"

#include "Interaction/LIInteractable.h"
#include "LostIsland.h"
#include "CollisionQueryParams.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

ULIInteractionComponent::ULIInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	// Required so ServerInteract can be routed through the owning connection.
	SetIsReplicatedByDefault(true);
}

void ULIInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetScanningEnabled(false);
	Super::EndPlay(EndPlayReason);
}

APawn* ULIInteractionComponent::GetOwnerPawn() const
{
	return Cast<APawn>(GetOwner());
}

void ULIInteractionComponent::SetScanningEnabled(bool bEnabled)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (bEnabled)
	{
		if (!World->GetTimerManager().IsTimerActive(ScanTimerHandle))
		{
			// Small random first delay so several local players don't all scan on the same frame.
			const float FirstDelay = FMath::FRandRange(0.f, ScanInterval);
			World->GetTimerManager().SetTimer(ScanTimerHandle, this, &ULIInteractionComponent::ScanForInteractable, ScanInterval, true, FirstDelay);
		}
	}
	else
	{
		World->GetTimerManager().ClearTimer(ScanTimerHandle);
		SetFocusedActor(nullptr);
	}
}

void ULIInteractionComponent::ScanForInteractable()
{
	APawn* Pawn = GetOwnerPawn();
	AController* Controller = Pawn ? Pawn->GetController() : nullptr;
	UWorld* World = GetWorld();
	if (!Pawn || !Controller || !World)
	{
		SetFocusedActor(nullptr);
		return;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
	const FVector ViewDir = ViewRotation.Vector();

	// Third-person camera sits behind the pawn. Start the sweep at the point on the
	// view ray closest to the pawn so reach is independent of camera boom length.
	const FVector PawnLocation = Pawn->GetActorLocation();
	const float DistanceAlongRay = FMath::Max(0.f, FVector::DotProduct(PawnLocation - ViewLocation, ViewDir));
	const FVector TraceStart = ViewLocation + ViewDir * DistanceAlongRay;
	const FVector TraceEnd = TraceStart + ViewDir * InteractionDistance;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(LIInteractionScan), /*bTraceComplex*/ false, Pawn);
	FHitResult Hit;
	const bool bHit = World->SweepSingleByChannel(Hit, TraceStart, TraceEnd, FQuat::Identity, TraceChannel,
		FCollisionShape::MakeSphere(TraceRadius), Params);

#if ENABLE_DRAW_DEBUG
	if (bDrawDebugTrace)
	{
		DrawDebugLine(World, TraceStart, bHit ? Hit.ImpactPoint : TraceEnd, bHit ? FColor::Green : FColor::Red, false, ScanInterval);
	}
#endif

	AActor* NewFocus = nullptr;
	if (bHit)
	{
		AActor* HitActor = Hit.GetActor();
		if (CanInteractWith(HitActor, 0.f))
		{
			NewFocus = HitActor;
		}
	}

	SetFocusedActor(NewFocus);
}

void ULIInteractionComponent::SetFocusedActor(AActor* NewFocus)
{
	APawn* Pawn = GetOwnerPawn();
	AActor* OldFocus = FocusedActor.Get();

	const FText NewPrompt = NewFocus ? ILIInteractable::Execute_GetInteractionPrompt(NewFocus, Pawn) : FText::GetEmpty();

	if (OldFocus == NewFocus)
	{
		// Same target; only notify if the prompt text changed (e.g. "Chop" -> "Depleted").
		if (!NewPrompt.EqualTo(FocusedPrompt))
		{
			FocusedPrompt = NewPrompt;
			OnFocusChanged.Broadcast(NewFocus, FocusedPrompt);
		}
		return;
	}

	if (OldFocus && OldFocus->Implements<ULIInteractable>())
	{
		ILIInteractable::Execute_OnFocusEnd(OldFocus, Pawn);
	}

	FocusedActor = NewFocus;
	FocusedPrompt = NewPrompt;

	if (NewFocus)
	{
		ILIInteractable::Execute_OnFocusBegin(NewFocus, Pawn);
	}

	OnFocusChanged.Broadcast(NewFocus, FocusedPrompt);
}

bool ULIInteractionComponent::IsWithinReach(const AActor* Target, float ExtraTolerance) const
{
	const APawn* Pawn = GetOwnerPawn();
	if (!Pawn || !Target)
	{
		return false;
	}

	const FVector PawnLocation = Pawn->GetActorLocation();
	const FBox Bounds = Target->GetComponentsBoundingBox(/*bNonColliding*/ false);
	const double DistSq = Bounds.IsValid
		? Bounds.ComputeSquaredDistanceToPoint(PawnLocation)
		: FVector::DistSquared(PawnLocation, Target->GetActorLocation());

	return DistSq <= FMath::Square(InteractionDistance + ExtraTolerance);
}

bool ULIInteractionComponent::CanInteractWith(AActor* Target, float ExtraTolerance) const
{
	return IsValid(Target)
		&& Target->Implements<ULIInteractable>()
		&& IsWithinReach(Target, ExtraTolerance)
		&& ILIInteractable::Execute_CanInteract(Target, GetOwnerPawn());
}

void ULIInteractionComponent::TryInteract()
{
	AActor* Target = FocusedActor.Get();
	if (!Target)
	{
		return;
	}

	if (GetOwnerRole() == ROLE_Authority)
	{
		PerformInteract(Target);
	}
	else
	{
		ServerInteract(Target);
	}
}

void ULIInteractionComponent::ServerInteract_Implementation(AActor* Target)
{
	PerformInteract(Target);
}

void ULIInteractionComponent::PerformInteract(AActor* Target)
{
	check(GetOwnerRole() == ROLE_Authority);

	const double Now = GetWorld()->GetTimeSeconds();
	if (Now - LastServerInteractTime < MinInteractInterval)
	{
		return;
	}

	if (!CanInteractWith(Target, ServerDistanceTolerance))
	{
		UE_LOG(LogLostIsland, Verbose, TEXT("Interaction with %s rejected by server validation."), *GetNameSafe(Target));
		return;
	}

	LastServerInteractTime = Now;
	ILIInteractable::Execute_Interact(Target, GetOwnerPawn());
	OnInteracted.Broadcast(Target);
}
