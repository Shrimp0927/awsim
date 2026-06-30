#pragma once

#include "CoreMinimal.h"
#include "Simulation/SimPhase.h"
#include "Entities/GridCoord.h"
#include "Entities/GridContent.h"
#include "GridSubsystem.generated.h"

UCLASS()
class AWSIM_API UGridSubsystem : public USimPhase
{
	GENERATED_BODY()

public:
	virtual void Step(float StepSeconds) override;
	virtual int32 PhaseOrder() const override { return 200; }

	bool IsTileOccupied(FGridCoord Tile) const;

	FGridContent GetContentAt(FGridCoord Tile) const;

	bool SetContent(FGridCoord Tile, FGridContent Content);

	// Read access for downstream domain phases.
	const TMap<FGridCoord, FGridContent>& GetPlacements() const { return Occupancy; }

	// Buildings grouped into connected islands (by origin tile). Rebuilt lazily
	// when placement changes; refreshed in Step (phase 200) before domains read.
	const TArray<TArray<FGridCoord>>& GetIslands() const;

private:
	const FGridContent* FindCovering(FGridCoord Tile) const;

	void EnsureIslands() const;
	void RebuildIslands() const;

	UPROPERTY()
	TMap<FGridCoord, FGridContent> Occupancy;

	mutable TArray<TArray<FGridCoord>> Islands;
	mutable bool bIslandsDirty = true;
};
