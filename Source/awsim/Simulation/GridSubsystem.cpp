#include "Simulation/GridSubsystem.h"

namespace
{
	FIntPoint FootprintExtent(const FGridContent& Content)
	{
		FIntPoint Dim(1, 1);
		if (Content.Definition)
		{
			Dim = Content.Definition->Dimensions;
		}
		if (Content.Facing == EDirection::East || Content.Facing == EDirection::West)
		{
			return FIntPoint(Dim.Y, Dim.X);
		}
		return Dim;
	}

	bool FootprintCovers(const FGridCoord& Origin, const FGridContent& Content, const FGridCoord& Tile)
	{
		const FIntPoint Extent = FootprintExtent(Content);
		return Tile.X >= Origin.X && Tile.X < Origin.X + Extent.X
			&& Tile.Y >= Origin.Y && Tile.Y < Origin.Y + Extent.Y;
	}
}

void UGridSubsystem::Step(float StepSeconds)
{
}

const FGridContent* UGridSubsystem::FindCovering(FGridCoord Tile) const
{
	if (const FGridContent* Found = Occupancy.Find(Tile))
	{
		if (Found->Type != EPlaceableType::None)
		{
			return Found;
		}
	}

	for (const TPair<FGridCoord, FGridContent>& Pair : Occupancy)
	{
		if (Pair.Value.Type == EPlaceableType::None || Pair.Key == Tile)
		{
			continue;
		}
		if (FootprintCovers(Pair.Key, Pair.Value, Tile))
		{
			return &Pair.Value;
		}
	}

	return nullptr;
}

bool UGridSubsystem::IsTileOccupied(FGridCoord Tile) const
{
	return FindCovering(Tile) != nullptr;
}

FGridContent UGridSubsystem::GetContentAt(FGridCoord Tile) const
{
	const FGridContent* Found = FindCovering(Tile);
	return Found ? *Found : FGridContent();
}

bool UGridSubsystem::SetContent(FGridCoord Tile, FGridContent Content)
{
	if (Content.Type == EPlaceableType::None)
	{
		return Occupancy.Remove(Tile) > 0;
	}

	Occupancy.Add(Tile, Content);
	return true;
}
