#pragma once

#include "CoreMinimal.h"
#include "Simulation/GameSimPhase.h"
#include "Entities/GridCoord.h"
#include "Entities/GridContent.h"
#include "GameGridSubsystem.generated.h"

class UGamePlayerFundsSubsystem;

USTRUCT()
struct FPlacedBuilding
{
	GENERATED_BODY()

	UPROPERTY() FGridCoord Origin;
	UPROPERTY() FGridContent Content;
};

UCLASS()
class AWSIM_API UGridSubsystem : public USimPhase
{
	GENERATED_BODY()

public:
	static constexpr int32 GridWidth = 1000;
	static constexpr int32 GridHeight = 1000;

	virtual void Step(float StepSeconds) override;
	virtual int32 PhaseOrder() const override { return 200; }

	static constexpr int32 GetWidth() { return GridWidth; }
	static constexpr int32 GetHeight() { return GridHeight; }

	static bool IsInBounds(FGridCoord Tile)
	{
		return Tile.X >= 0 && Tile.X < GridWidth && Tile.Y >= 0 && Tile.Y < GridHeight;
	}

	bool IsTileOccupied(FGridCoord Tile) const;
	FGridContent GetContentAt(FGridCoord Tile) const;
	bool SetContent(FGridCoord Tile, FGridContent Content);

	// Player-intent entry point: placements queue here and apply FIFO at the
	// top of the grid phase, so a placement queued during tick N is live for
	// every domain phase of tick N+1. SetContent stays the immediate,
	// authoritative mutator (validation + charging happen there, at apply time).
	void QueuePlacement(FGridCoord Tile, FGridContent Content);
	int32 NumPendingPlacements() const { return PendingPlacements.Num(); }

	// Writes one placed building's per-instance slider value, clamped to the
	// slider's authored range. False if no building covers Tile or the index
	// is invalid. Never reshapes islands (producer checks are type-level).
	bool SetSliderValue(FGridCoord Tile, int32 SliderIndex, float Value);

	// Funds source for placement charging: an item's Cost is paid on placement,
	// and an unaffordable item is rejected. Resolved from the owning world when
	// unset; injectable so world-less specs can drive affordability.
	void SetFunds(UGamePlayerFundsSubsystem* InFunds) { Funds = InFunds; }

	// Read access for downstream domain phases.
	const TArray<FPlacedBuilding>& GetBuildings() const { return Buildings; }

	// Buildings grouped into connected islands (by origin tile). Rebuilt lazily
	// when placement changes; refreshed in Step (phase 200) before domains read.
	const TArray<TArray<FGridCoord>>& GetIslands() const;

private:
	void EnsureNetworks() const;
	void EnsureIslands() const;
	void RebuildIslands() const;
	void RemoveBuildingAt(int32 Index);
	UGamePlayerFundsSubsystem* ResolveFunds() const;

	UPROPERTY(Transient)
	TObjectPtr<UGamePlayerFundsSubsystem> Funds;

	UPROPERTY()
	TMap<FGridCoord, FGridContent> Roads;

	UPROPERTY()
	TMap<FGridCoord, FGridContent> Utilities;

	UPROPERTY()
	TArray<FPlacedBuilding> Buildings;

	// Queued player placements, drained FIFO by Step. Ephemeral player intent,
	// not authoritative state.
	UPROPERTY(Transient)
	TArray<FPlacedBuilding> PendingPlacements;

	// Reverse index: every covered tile -> index into Buildings. Derived from
	// Buildings; maintained on placement so coverage queries stay O(1).
	TMap<FGridCoord, int32> BuildingAt;

	// Cached connector networks (tile -> network id). Re-labelled only when the
	// matching connector type changes; UtilDomain[id] is each utility network's
	// domain. Building-only placements reuse these untouched.
	mutable TMap<FGridCoord, int32> RoadNet;
	mutable TMap<FGridCoord, int32> UtilNet;
	mutable TArray<EDomain> UtilDomain;
	mutable bool bRoadNetDirty = true;
	mutable bool bUtilNetDirty = true;

	mutable TArray<TArray<FGridCoord>> Islands;
	mutable bool bIslandsDirty = true;
};
