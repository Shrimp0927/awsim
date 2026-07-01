#include "Simulation/GridSubsystem.h"
#include "Entities/GridContent.h"

namespace
{
	constexpr int32 ConnectorReach = 2; // tiles a building can reach a connector across

	FIntPoint FootprintExtent(const FGridContent& Content)
	{
		FIntPoint Dim(1, 1);
		if (Content.Definition)
		{
			Dim = Content.Definition->Dimensions;
		}
		if (Content.Facing == EPlaceableDirection::East || Content.Facing == EPlaceableDirection::West)
		{
			return FIntPoint(Dim.Y, Dim.X);
		}
		return Dim;
	}

	TArray<FGridCoord> FootprintTiles(const FGridCoord& Origin, const FGridContent& Content)
	{
		const FIntPoint Ext = FootprintExtent(Content);
		TArray<FGridCoord> Tiles;
		Tiles.Reserve(Ext.X * Ext.Y);
		for (int32 dx = 0; dx < Ext.X; ++dx)
		{
			for (int32 dy = 0; dy < Ext.Y; ++dy)
			{
				Tiles.Add(FGridCoord(Origin.X + dx, Origin.Y + dy));
			}
		}
		return Tiles;
	}

	FGridCoord StepFor(EPlaceableDirection Dir)
	{
		switch (Dir)
		{
		case EPlaceableDirection::North: return FGridCoord(0, 1);
		case EPlaceableDirection::South: return FGridCoord(0, -1);
		case EPlaceableDirection::East:  return FGridCoord(1, 0);
		case EPlaceableDirection::West:  return FGridCoord(-1, 0);
		default:                         return FGridCoord(0, 0);
		}
	}

	// The tiles forming one side of a footprint, plus the outward step from that side.
	void EdgeRay(const FGridCoord& Origin, const FIntPoint& Extent, EPlaceableDirection Side,
		TArray<FGridCoord>& OutEdge, FGridCoord& OutStep)
	{
		OutEdge.Reset();
		OutStep = StepFor(Side);
		switch (Side)
		{
		case EPlaceableDirection::North:
			for (int32 x = Origin.X; x < Origin.X + Extent.X; ++x) OutEdge.Add(FGridCoord(x, Origin.Y + Extent.Y - 1));
			break;
		case EPlaceableDirection::South:
			for (int32 x = Origin.X; x < Origin.X + Extent.X; ++x) OutEdge.Add(FGridCoord(x, Origin.Y));
			break;
		case EPlaceableDirection::East:
			for (int32 y = Origin.Y; y < Origin.Y + Extent.Y; ++y) OutEdge.Add(FGridCoord(Origin.X + Extent.X - 1, y));
			break;
		case EPlaceableDirection::West:
			for (int32 y = Origin.Y; y < Origin.Y + Extent.Y; ++y) OutEdge.Add(FGridCoord(Origin.X, y));
			break;
		default:
			break;
		}
	}

	// Scan outward from each edge tile up to ConnectorReach; a building blocks the
	// ray, a connector in NetMap ends it (its network is recorded), empty continues.
	void ScanForNetworks(const TArray<FGridCoord>& Edge, FGridCoord Step,
		const TMap<FGridCoord, int32>& NetMap, const TMap<FGridCoord, int32>& BuildingAt,
		TSet<int32>& OutNets)
	{
		for (const FGridCoord& E : Edge)
		{
			for (int32 k = 1; k <= ConnectorReach; ++k)
			{
				const FGridCoord T(E.X + Step.X * k, E.Y + Step.Y * k);
				if (BuildingAt.Contains(T)) break;                      // blocked by a building
				if (const int32* Net = NetMap.Find(T)) { OutNets.Add(*Net); break; }
				// empty tile: keep scanning out to the reach limit
			}
		}
	}

	// A building "produces" a domain if its type has any positive effect for it.
	// Type-level (not slider-level) so tuning a slider never changes island shape.
	bool DefProducesDomain(const UPlaceableDef* Def, EDomain Domain)
	{
		if (!Def || Domain == EDomain::None) return false;
		for (const FSliderDef& Slider : Def->Sliders)
		{
			for (const FDomainEffect& Effect : Slider.Effects)
			{
				if (Effect.Domain == Domain && (Effect.AmountAtMin > 0.f || Effect.AmountAtMax > 0.f))
				{
					return true;
				}
			}
		}
		return false;
	}

	int32 DsuFind(TArray<int32>& Parent, int32 I)
	{
		while (Parent[I] != I) { Parent[I] = Parent[Parent[I]]; I = Parent[I]; }
		return I;
	}

	void DsuUnion(TArray<int32>& Parent, int32 A, int32 B)
	{
		Parent[DsuFind(Parent, A)] = DsuFind(Parent, B);
	}

	// Flood-fill a per-type connector map into networks (8-connectivity). The map
	// holds only one connector kind, so any neighbour in it is a candidate. For
	// utilities, only same-domain neighbours join; OutDomains[id] records the
	// network's domain (pass nullptr for roads).
	void LabelNetworks(const TMap<FGridCoord, FGridContent>& Tiles,
		TMap<FGridCoord, int32>& OutNet, TArray<EDomain>* OutDomains)
	{
		auto DomainOf = [](const FGridContent& C)
		{
			return C.Definition ? C.Definition->ConnectorDomain : EDomain::None;
		};

		for (const TPair<FGridCoord, FGridContent>& Pair : Tiles)
		{
			if (OutNet.Contains(Pair.Key)) continue;

			const EDomain NetDomain = DomainOf(Pair.Value);
			const int32 Id = OutDomains ? OutDomains->Add(NetDomain) : OutNet.Num();

			TArray<FGridCoord> Stack;
			Stack.Push(Pair.Key);
			OutNet.Add(Pair.Key, Id);

			while (Stack.Num() > 0)
			{
				const FGridCoord C = Stack.Pop();
				for (int32 dx = -1; dx <= 1; ++dx)
				{
					for (int32 dy = -1; dy <= 1; ++dy)
					{
						if (dx == 0 && dy == 0) continue;
						const FGridCoord Nb(C.X + dx, C.Y + dy);
						if (OutNet.Contains(Nb)) continue;

						const FGridContent* NC = Tiles.Find(Nb);
						if (!NC) continue;
						if (OutDomains && DomainOf(*NC) != NetDomain) continue; // utilities split by domain

						OutNet.Add(Nb, Id);
						Stack.Push(Nb);
					}
				}
			}
		}
	}
}

void UGridSubsystem::Step(float StepSeconds)
{
	EnsureIslands();
}

bool UGridSubsystem::IsTileOccupied(FGridCoord Tile) const
{
	return Roads.Contains(Tile) || Utilities.Contains(Tile) || BuildingAt.Contains(Tile);
}

FGridContent UGridSubsystem::GetContentAt(FGridCoord Tile) const
{
	if (const FGridContent* R = Roads.Find(Tile)) return *R;
	if (const FGridContent* U = Utilities.Find(Tile)) return *U;
	if (const int32* Idx = BuildingAt.Find(Tile)) return Buildings[*Idx].Content;
	return FGridContent();
}

bool UGridSubsystem::SetContent(FGridCoord Tile, FGridContent Content)
{
	if (Content.Type == EPlaceableType::None)
	{
		if (Roads.Remove(Tile) > 0) { bIslandsDirty = true; return true; }
		if (Utilities.Remove(Tile) > 0) { bIslandsDirty = true; return true; }
		if (const int32* Idx = BuildingAt.Find(Tile))
		{
			RemoveBuildingAt(*Idx);
			bIslandsDirty = true;
			return true;
		}
		return false;
	}

	// Reject if any tile of the new footprint is out of bounds or already occupied.
	const TArray<FGridCoord> Tiles = FootprintTiles(Tile, Content);
	for (const FGridCoord& T : Tiles)
	{
		if (!IsInBounds(T) || IsTileOccupied(T))
		{
			return false;
		}
	}

	switch (Content.Type)
	{
	case EPlaceableType::Road:
		Roads.Add(Tile, Content);
		break;
	case EPlaceableType::Utility:
		Utilities.Add(Tile, Content);
		break;
	default: // Building (and any other footprint occupant)
	{
		FPlacedBuilding Placed;
		Placed.Origin = Tile;
		Placed.Content = Content;
		const int32 Idx = Buildings.Add(MoveTemp(Placed));
		for (const FGridCoord& T : Tiles) BuildingAt.Add(T, Idx);
		break;
	}
	}

	bIslandsDirty = true;
	return true;
}

void UGridSubsystem::RemoveBuildingAt(int32 Index)
{
	// Drop the removed building's tiles from the reverse index.
	for (const FGridCoord& T : FootprintTiles(Buildings[Index].Origin, Buildings[Index].Content))
	{
		BuildingAt.Remove(T);
	}

	Buildings.RemoveAtSwap(Index);

	// RemoveAtSwap moved the last element into Index; remap its tiles.
	if (Index < Buildings.Num())
	{
		for (const FGridCoord& T : FootprintTiles(Buildings[Index].Origin, Buildings[Index].Content))
		{
			BuildingAt[T] = Index;
		}
	}
}

const TArray<TArray<FGridCoord>>& UGridSubsystem::GetIslands() const
{
	EnsureIslands();
	return Islands;
}

void UGridSubsystem::EnsureIslands() const
{
	if (bIslandsDirty)
	{
		RebuildIslands();
		bIslandsDirty = false;
	}
}

void UGridSubsystem::RebuildIslands() const
{
	Islands.Reset();

	const int32 N = Buildings.Num();
	if (N == 0) return;

	// 1. Label connector networks (per-type maps -> network ids).
	TMap<FGridCoord, int32> RoadNet;
	LabelNetworks(Roads, RoadNet, nullptr);

	TMap<FGridCoord, int32> UtilNet;
	TArray<EDomain> UtilDomain;
	LabelNetworks(Utilities, UtilNet, &UtilDomain);
	TArray<bool> UtilHasProducer;
	UtilHasProducer.Init(false, UtilDomain.Num());

	// 2. Union-Find over buildings.
	TArray<int32> Parent;
	Parent.SetNum(N);
	for (int32 i = 0; i < N; ++i) Parent[i] = i;

	// (a) Proximity — Chebyshev <= 2, facing ignored (BuildingAt is the tile index).
	for (int32 i = 0; i < N; ++i)
	{
		for (const FGridCoord& T : FootprintTiles(Buildings[i].Origin, Buildings[i].Content))
		{
			for (int32 ox = -ConnectorReach; ox <= ConnectorReach; ++ox)
			{
				for (int32 oy = -ConnectorReach; oy <= ConnectorReach; ++oy)
				{
					const int32* J = BuildingAt.Find(FGridCoord(T.X + ox, T.Y + oy));
					if (J && *J != i) DsuUnion(Parent, i, *J);
				}
			}
		}
	}

	// (b) Road — facing edge only; any two buildings on the same network join.
	TMap<int32, TArray<int32>> RoadGroups;
	for (int32 i = 0; i < N; ++i)
	{
		const FIntPoint Ext = FootprintExtent(Buildings[i].Content);
		TArray<FGridCoord> Edge;
		FGridCoord Step;
		EdgeRay(Buildings[i].Origin, Ext, Buildings[i].Content.Facing, Edge, Step);

		TSet<int32> Nets;
		ScanForNetworks(Edge, Step, RoadNet, BuildingAt, Nets);
		for (int32 Net : Nets) RoadGroups.FindOrAdd(Net).Add(i);
	}
	for (const TPair<int32, TArray<int32>>& Group : RoadGroups)
	{
		for (int32 k = 1; k < Group.Value.Num(); ++k) DsuUnion(Parent, Group.Value[0], Group.Value[k]);
	}

	// (c) Utility — all four sides (facing ignored); joins only when the network
	//     has a matching-domain producer on it.
	const EPlaceableDirection Sides[] = {
		EPlaceableDirection::North, EPlaceableDirection::East,
		EPlaceableDirection::South, EPlaceableDirection::West };

	TMap<int32, TArray<int32>> UtilGroups;
	for (int32 i = 0; i < N; ++i)
	{
		const FIntPoint Ext = FootprintExtent(Buildings[i].Content);
		TSet<int32> Nets;
		for (EPlaceableDirection Side : Sides)
		{
			TArray<FGridCoord> Edge;
			FGridCoord Step;
			EdgeRay(Buildings[i].Origin, Ext, Side, Edge, Step);
			ScanForNetworks(Edge, Step, UtilNet, BuildingAt, Nets);
		}
		for (int32 Net : Nets)
		{
			UtilGroups.FindOrAdd(Net).Add(i);
			if (DefProducesDomain(Buildings[i].Content.Definition, UtilDomain[Net]))
			{
				UtilHasProducer[Net] = true;
			}
		}
	}
	for (const TPair<int32, TArray<int32>>& Group : UtilGroups)
	{
		if (!UtilHasProducer[Group.Key]) continue; // no source -> connects nothing
		for (int32 k = 1; k < Group.Value.Num(); ++k) DsuUnion(Parent, Group.Value[0], Group.Value[k]);
	}

	// 3. Gather components into islands (by building origin).
	TMap<int32, int32> RootToIsland;
	for (int32 i = 0; i < N; ++i)
	{
		const int32 Root = DsuFind(Parent, i);
		int32* IslandIdx = RootToIsland.Find(Root);
		if (!IslandIdx)
		{
			IslandIdx = &RootToIsland.Add(Root, Islands.AddDefaulted());
		}
		Islands[*IslandIdx].Add(Buildings[i].Origin);
	}
}
