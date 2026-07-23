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

	// Grown height in tiles; starts at 1, +1 per lifetime rollover up to the def max.
	UPROPERTY() int32 HeightTiles = 1;
};

UCLASS()
class AWSIM_API UGridSubsystem : public USimPhase
{
	GENERATED_BODY()

public:
	static constexpr int32 GridWidth = 500;
	static constexpr int32 GridHeight = 500;

	// World units per tile; the single world<->tile mapping constant.
	static constexpr float TileSize = 100.f;

	// Centered on the world origin: tile (0,0)'s corner sits at (WorldMinX, WorldMinY).
	static constexpr float WorldMinX = -GridWidth * TileSize * 0.5f;
	static constexpr float WorldMinY = -GridHeight * TileSize * 0.5f;

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
	// Reference stays valid only until the next grid mutation; copy to keep.
	const FGridContent& GetContentAt(FGridCoord Tile) const;

	// Grown height of the building covering Tile; 0 for empty, roads, utilities.
	float GetHeightAt(FGridCoord Tile) const;

	// Sim steps per grown floor; a building's clock counts 1..StepsPerFloor.
	static constexpr uint8 StepsPerFloor = 100;

	// Growth clock for a building origin; 0 once fully grown or not a building.
	uint8 GetLifetime(FGridCoord Origin) const;

	// First building column the down-going ray hits, else the ground tile; false off-grid.
	bool PickTile(const FVector& RayOrigin, const FVector& RayDir, FGridCoord& OutTile) const;

	// Where the ray meets z = 0, ignoring buildings (marquee select); false off-grid.
	bool PickGroundTile(const FVector& RayOrigin, const FVector& RayDir, FGridCoord& OutTile) const;
	bool SetContent(FGridCoord Tile, FGridContent Content);

	// Def Dimensions rotated by Facing; the single footprint authority.
	static FIntPoint GetFootprintExtent(const FGridContent& Content);

	// Applies FIFO at the top of the grid phase; SetContent validates and charges.
	void QueuePlacement(FGridCoord Tile, FGridContent Content);
	int32 NumPendingPlacements() const { return PendingPlacements.Num(); }

	// Clamps to the authored range; false if no building covers Tile or the index is invalid.
	bool SetSliderValue(FGridCoord Tile, int32 SliderIndex, float Value);

	// Injectable for world-less specs; resolved from the owning world when unset.
	void SetFunds(UGamePlayerFundsSubsystem* InFunds) { Funds = InFunds; }

	// Building covering Tile, or null; pointer valid until the next grid mutation.
	const FPlacedBuilding* FindBuildingAt(FGridCoord Tile) const;

	const TArray<FPlacedBuilding>& GetBuildings() const { return Buildings; }
	const TMap<FGridCoord, FGridContent>& GetRoads() const { return Roads; }
	const TMap<FGridCoord, FGridContent>& GetUtilities() const { return Utilities; }

	// Bumped on placement/removal (not slider edits); polled by the render projection.
	uint64 GetContentRevision() const { return ContentRevision; }

	// Bumped on slider edits; with GetContentRevision, lets domain phases skip recomputes.
	uint64 GetSliderRevision() const { return SliderRevision; }

	// Buildings grouped into connected islands (by origin tile); rebuilt lazily.
	const TArray<TArray<FGridCoord>>& GetIslands() const;

private:
	void EnsureNetworks() const;
	void EnsureIslands() const;
	void RebuildIslands() const;
	void GrowBuildings();
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

	// Column heights, row-major GridWidth x GridHeight; allocated on the first building.
	TArray<float> HeightAt;

	// Growth clocks keyed by building origin; fully grown buildings drop out.
	UPROPERTY()
	TMap<FGridCoord, uint8> LifetimeAt;

	// Cached connector networks (tile -> network id); UtilDomain[id] is that network's domain.
	mutable TMap<FGridCoord, int32> RoadNet;
	mutable TMap<FGridCoord, int32> UtilNet;
	mutable TArray<EDomain> UtilDomain;
	mutable bool bRoadNetDirty = true;
	mutable bool bUtilNetDirty = true;

	mutable TArray<TArray<FGridCoord>> Islands;
	mutable bool bIslandsDirty = true;

	// Runtime change signals only (see the revision getters); never saved.
	uint64 ContentRevision = 0;
	uint64 SliderRevision = 0;
};
