#pragma once

#include "CoreMinimal.h"
#include "GridCoord.generated.h"

/**
 * An integer tile coordinate on the city grid.
 *
 * Plain value type shared by everything the grid places (roads today; buildings,
 * airstrips, ... later — see GridSubsystem). Kept reflection-friendly and
 * hashable so it can key the grid's occupancy map.
 *
 * NOTE: the grid model (square tiles vs. freeform) is still an open question
 * (ARCHITECTURE.md §8). Tiles are the loose first pass.
 */
USTRUCT()
struct FGridCoord
{
	GENERATED_BODY()

	UPROPERTY() int32 X = 0;
	UPROPERTY() int32 Y = 0;

	FGridCoord() = default;
	FGridCoord(int32 InX, int32 InY) : X(InX), Y(InY) {}

	bool operator==(const FGridCoord& Other) const
	{
		return X == Other.X && Y == Other.Y;
	}
};

FORCEINLINE uint32 GetTypeHash(const FGridCoord& Coord)
{
	return HashCombine(GetTypeHash(Coord.X), GetTypeHash(Coord.Y));
}
