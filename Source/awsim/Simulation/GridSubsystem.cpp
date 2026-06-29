#include "Simulation/GridSubsystem.h"

void UGridSubsystem::Step(float StepSeconds)
{
	// Nothing to advance yet — placement is event-driven, and concrete content
	// types are being reworked. The per-step slot stays reserved for any derived
	// rebuild (e.g. a transport-network graph) once content returns.
}

bool UGridSubsystem::IsTileOccupied(FGridCoord Tile) const
{
	const EGridContent* Found = Occupancy.Find(Tile);
	return Found && *Found != EGridContent::Empty;
}

EGridContent UGridSubsystem::GetContentAt(FGridCoord Tile) const
{
	const EGridContent* Found = Occupancy.Find(Tile);
	return Found ? *Found : EGridContent::Empty;
}

bool UGridSubsystem::SetContent(FGridCoord Tile, EGridContent Content)
{
	if (Content == EGridContent::Empty)
	{
		return Occupancy.Remove(Tile) > 0;
	}

	EGridContent& Slot = Occupancy.FindOrAdd(Tile);
	if (Slot == Content)
	{
		return false;
	}
	Slot = Content;
	return true;
}
