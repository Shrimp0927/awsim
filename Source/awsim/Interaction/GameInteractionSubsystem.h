#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "Entities/GridCoord.h"
#include "GameInteractionSubsystem.generated.h"

class APlayerController;
class UInputAction;
struct FInputActionValue;
enum class EDomain : uint8;
enum class EPlaceableType : uint8;

// View-layer cursor state: per-frame mouse picks plus click/key intents on the
// hovered tile. Mutates the sim only through the grid's queue APIs.
UCLASS()
class AWSIM_API UGameInteractionSubsystem : public UWorldSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaSeconds) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;

	// False when the cursor is off the viewport or past the grid with no occluder.
	bool HasHoveredTile() const { return bHasHover; }
	FGridCoord GetHoveredTile() const { return HoveredTile; }

	// Marquee: anchored at mouse-down, tracks the cursor while held, clears on
	// release; both ends are ground-plane picks so buildings never bend it.
	bool IsDragSelecting() const { return bDragging; }
	FIntRect GetDragRect() const; // [Min, Max) tile rect spanning anchor..cursor

private:
	// Bound lazily once the player controller's input component exists.
	void EnsureInputBound(APlayerController* PC);
	// Single hovered tile, or everything in the drag rect while selecting.
	void OnDemolish(const FInputActionValue& Value);
	void OnSelectPressed(const FInputActionValue& Value);
	void OnSelectReleased(const FInputActionValue& Value);
	void OnBuildEnergy(const FInputActionValue& Value);
	void OnBuildWater(const FInputActionValue& Value);
	void OnBuildHome(const FInputActionValue& Value);
	void OnBuildBusiness(const FInputActionValue& Value);
	void OnBuildRoad(const FInputActionValue& Value);
	void OnBuildPowerLine(const FInputActionValue& Value);
	void OnBuildPipe(const FInputActionValue& Value);
	void OnToggleMenu(const FInputActionValue& Value);
	// Queue a rect-sized building of Domain; rejected unless every tile is empty.
	void QueueBuildInRect(EDomain Domain);
	// Queue 1x1 connectors (road/utility) filling the rect; same all-empty rule.
	void QueueConnectorsInRect(EPlaceableType Type, EDomain Domain);

	bool bHasHover = false;
	FGridCoord HoveredTile;

	bool bHasGroundHover = false;
	FGridCoord GroundHoveredTile; // plane-only pick, for the marquee

	bool bDragging = false;
	FGridCoord AnchorTile;  // ground tile under the cursor at mouse-down
	FGridCoord DragEndTile; // last valid ground tile while dragging

	bool bInputBound = false;
	UPROPERTY(Transient) TObjectPtr<UInputAction> DemolishAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> SelectAction;
	UPROPERTY(Transient) TArray<TObjectPtr<UInputAction>> BuildActions; // keys 1-7
	UPROPERTY(Transient) TObjectPtr<UInputAction> ToggleMenuAction;
};
