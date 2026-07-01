#pragma once

#include "CoreMinimal.h"
#include "Simulation/SimPhase.h"
#include "Entities/GridCoord.h"
#include "Entities/GridContent.h"
#include "GridSubsystem.generated.h"

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

	// Read access for downstream domain phases.
	const TArray<FPlacedBuilding>& GetBuildings() const { return Buildings; }

	// Buildings grouped into connected islands (by origin tile). Rebuilt lazily
	// when placement changes; refreshed in Step (phase 200) before domains read.
	const TArray<TArray<FGridCoord>>& GetIslands() const;

private:
	void EnsureIslands() const;
	void RebuildIslands() const;
	void RemoveBuildingAt(int32 Index);

	UPROPERTY()
	TMap<FGridCoord, FGridContent> Roads;

	UPROPERTY()
	TMap<FGridCoord, FGridContent> Utilities;

	UPROPERTY()
	TArray<FPlacedBuilding> Buildings;

	// Reverse index: every covered tile -> index into Buildings. Derived from
	// Buildings; maintained on placement so coverage queries stay O(1).
	TMap<FGridCoord, int32> BuildingAt;

	mutable TArray<TArray<FGridCoord>> Islands;
	mutable bool bIslandsDirty = true;
};
