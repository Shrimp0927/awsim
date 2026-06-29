#pragma once

#include "CoreMinimal.h"
#include "GridCoord.generated.h"

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
