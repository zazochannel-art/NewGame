// Copyright LOST ISLAND Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/LIInteractable.h"
#include "LIBuildingPiece.generated.h"

class UStaticMeshComponent;

UENUM(BlueprintType)
enum class ELIBuildingPieceType : uint8
{
	Floor,
	Wall,
	Roof
};

/**
 * ALIBuildingPiece
 *
 * Wooden shelter parts on a 300 cm grid. Floors go on the ground (or snap next to another floor),
 * walls snap to a floor edge, roofs sit one wall-height above a floor cell. Placement rules live in
 * ComputePlacement (server, called by ULICraftingComponent when a building item is placed with G).
 *
 * Standing under a roof counts as sheltered: no night cold (see ULISurvivalComponent).
 * Persisted in the save game with the other structures. Interact (E) dismantles a piece and gives
 * its item back. Build mode (B) shows a local, non-replicated preview copy (IsPreview()).
 */
UCLASS(Abstract)
class LOSTISLAND_API ALIBuildingPiece : public AActor, public ILIInteractable
{
	GENERATED_BODY()

public:
	ALIBuildingPiece();

	// ILIInteractable: dismantle
	virtual bool CanInteract_Implementation(APawn* InstigatorPawn) const override;
	virtual void Interact_Implementation(APawn* InstigatorPawn) override;
	virtual FText GetInteractionPrompt_Implementation(APawn* InstigatorPawn) const override;

	/** Local-only ghost of PieceClass for build mode: no collision, not replicated, tinted green. */
	static ALIBuildingPiece* SpawnPreview(UWorld* World, TSubclassOf<ALIBuildingPiece> PieceClass);

	bool IsPreview() const { return bIsPreview; }

	static constexpr float GridSize = 300.f;
	static constexpr float WallHeight = 300.f;
	static constexpr float FloorThickness = 20.f;

	ELIBuildingPieceType GetPieceType() const { return PieceType; }

	/**
	 * Where a piece of this class would go for a builder at BuilderLocation looking along BuilderForward.
	 * Returns false if there is no valid spot.
	 */
	static bool ComputePlacement(const UWorld* World, TSubclassOf<ALIBuildingPiece> PieceClass,
		const FVector& BuilderLocation, const FVector& BuilderForward, const AActor* Builder, FTransform& OutTransform);

	/** True if a roof piece is directly above Location (within two wall heights). */
	static bool IsSheltered(const UWorld* World, const FVector& Location);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(EditDefaultsOnly, Category = "LostIsland|Building")
	ELIBuildingPieceType PieceType = ELIBuildingPieceType::Floor;

	UPROPERTY(EditDefaultsOnly, Category = "LostIsland|Building")
	FLinearColor WoodColor = FLinearColor(0.42f, 0.27f, 0.13f);

	/** Item given back when the piece is dismantled. */
	UPROPERTY(EditDefaultsOnly, Category = "LostIsland|Building")
	FName ItemId = NAME_None;

	/** Extra mesh parts (doorway posts); tinted like Mesh. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> ExtraParts;

	bool bIsPreview = false;
};

UCLASS()
class LOSTISLAND_API ALIWoodenFloor : public ALIBuildingPiece
{
	GENERATED_BODY()
public:
	ALIWoodenFloor();
};

UCLASS()
class LOSTISLAND_API ALIWoodenWall : public ALIBuildingPiece
{
	GENERATED_BODY()
public:
	ALIWoodenWall();
};

/** A wall with a 100 x 220 cm opening. Snaps like a wall (and takes a wall's place). */
UCLASS()
class LOSTISLAND_API ALIWoodenDoorway : public ALIBuildingPiece
{
	GENERATED_BODY()
public:
	ALIWoodenDoorway();

protected:
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> RightPost;
	UPROPERTY(VisibleAnywhere, Category = "Components") TObjectPtr<UStaticMeshComponent> Lintel;
};

UCLASS()
class LOSTISLAND_API ALIWoodenRoof : public ALIBuildingPiece
{
	GENERATED_BODY()
public:
	ALIWoodenRoof();
};
