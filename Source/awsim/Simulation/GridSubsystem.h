#pragma once

#include "CoreMinimal.h"
#include "Simulation/SimPhase.h"
#include "Entities/GridCoord.h"
#include "GridSubsystem.generated.h"

/**
 * What occupies a grid tile. Concrete content types are being reworked from the
 * ground up; Building is a reserved placeholder for now.
 */
UENUM()
enum class EGridContent : uint8
{
	Empty,
	Building, // reserved — not built yet
};

/**
 * The city grid: placement authority for everything placed on the map.
 *
 * Runs as the Grid phase (PhaseOrder 200) so it's settled before later phases
 * read it. Concrete content types (buildings, transport, ...) are being reworked;
 * this is a skeleton that owns a generic tile -> content map and the queries over
 * it.
 */
UCLASS()
class AWSIM_API UGridSubsystem : public USimPhase
{
	GENERATED_BODY()

public:
	virtual void Step(float StepSeconds) override;
	virtual int32 PhaseOrder() const override { return 200; }

	// --- Tile queries ---
	bool IsTileOccupied(FGridCoord Tile) const;
	EGridContent GetContentAt(FGridCoord Tile) const;

	/** Set what occupies a tile (Empty clears it). Returns false on a no-op. */
	bool SetContent(FGridCoord Tile, EGridContent Content);

private:
	/** Tile -> what sits on it. Authoritative placement state — NOT Transient,
	 *  so the save/load layer can persist it. */
	UPROPERTY()
	TMap<FGridCoord, EGridContent> Occupancy;
};
