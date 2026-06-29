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

private:
	const FGridContent* FindCovering(FGridCoord Tile) const;

	UPROPERTY()
	TMap<FGridCoord, FGridContent> Occupancy;
};
