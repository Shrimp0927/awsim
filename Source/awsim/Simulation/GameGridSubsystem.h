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

	// World units per tile; the single world<->tile mapping constant.
	static constexpr float TileSize = 100.f;

	virtual void Step(float StepSeconds) override;
	virtual int32 PhaseOrder() const override { return 200; }

	// Input band: placements keep applying (and charging funds) while paused.
	virtual bool StepsWhilePaused() const override { return true; }

	static constexpr int32 GetWidth() { return GridWidth; }
	static constexpr int32 GetHeight() { return GridHeight; }

	static bool IsInBounds(FGridCoord Tile)
	{
		return Tile.X >= 0 && Tile.X < GridWidth && Tile.Y >= 0 && Tile.Y < GridHeight;
	}

	bool IsTileOccupied(FGridCoord Tile) const;
	FGridContent GetContentAt(FGridCoord Tile) const;
	bool SetContent(FGridCoord Tile, FGridContent Content);

	// Rotated tile footprint (def Dimensions rotated by Facing); the single
	// footprint authority for occupancy, rendering, and picking.
	static FIntPoint GetFootprintExtent(const FGridContent& Content);

	// Placements queue here and apply FIFO at the top of the grid phase;
	// validation and charging happen in SetContent at apply time.
	void QueuePlacement(FGridCoord Tile, FGridContent Content);
	int32 NumPendingPlacements() const { return PendingPlacements.Num(); }

	// Clamps to the slider's authored range; false if no building covers Tile
	// or the index is invalid. Never reshapes islands.
	bool SetSliderValue(FGridCoord Tile, int32 SliderIndex, float Value);

	// Injectable for world-less specs; resolved from the owning world when unset.
	void SetFunds(UGamePlayerFundsSubsystem* InFunds) { Funds = InFunds; }

	const TArray<FPlacedBuilding>& GetBuildings() const { return Buildings; }
	const TMap<FGridCoord, FGridContent>& GetRoads() const { return Roads; }
	const TMap<FGridCoord, FGridContent>& GetUtilities() const { return Utilities; }

	// Bumped on every successful placement or removal (not slider edits);
	// polled by the render projection to know when to rebuild.
	uint64 GetContentRevision() const { return ContentRevision; }

	// Buildings grouped into connected islands (by origin tile); rebuilt lazily,
	// refreshed in Step before domain phases read.
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

	// Queued player placements, drained FIFO by Step.
	UPROPERTY(Transient)
	TArray<FPlacedBuilding> PendingPlacements;

	// Reverse index: every covered tile -> index into Buildings.
	TMap<FGridCoord, int32> BuildingAt;

	// Cached connector networks (tile -> network id), re-labelled only when the
	// matching connector type changes; UtilDomain[id] is that network's domain.
	mutable TMap<FGridCoord, int32> RoadNet;
	mutable TMap<FGridCoord, int32> UtilNet;
	mutable TArray<EDomain> UtilDomain;
	mutable bool bRoadNetDirty = true;
	mutable bool bUtilNetDirty = true;

	mutable TArray<TArray<FGridCoord>> Islands;
	mutable bool bIslandsDirty = true;

	// Runtime change signal only (see GetContentRevision); never saved.
	uint64 ContentRevision = 0;
};
