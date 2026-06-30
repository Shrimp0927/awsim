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

	bool FootprintCovers(const FGridCoord& Origin, const FGridContent& Content, const FGridCoord& Tile)
	{
		const FIntPoint Extent = FootprintExtent(Content);
		return Tile.X >= Origin.X && Tile.X < Origin.X + Extent.X
			&& Tile.Y >= Origin.Y && Tile.Y < Origin.Y + Extent.Y;
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
	// ray, a connector of NetMap ends it (its network is recorded), empty continues.
	void ScanForNetworks(const TArray<FGridCoord>& Edge, FGridCoord Step,
		const TMap<FGridCoord, int32>& NetMap, const TMap<FGridCoord, int32>& TileToBuilding,
		TSet<int32>& OutNets)
	{
		for (const FGridCoord& E : Edge)
		{
			for (int32 k = 1; k <= ConnectorReach; ++k)
			{
				const FGridCoord T(E.X + Step.X * k, E.Y + Step.Y * k);
				if (TileToBuilding.Contains(T)) break;                  // blocked by a building
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

	// Flood-fill connector tiles of TargetType into networks (8-connectivity).
	// For utilities, only same-domain neighbours join; OutDomains[id] records the
	// network's domain (left empty for roads).
	void LabelNetworks(const TMap<FGridCoord, FGridContent>& Occupancy, EPlaceableType TargetType,
		TMap<FGridCoord, int32>& OutNet, TArray<EDomain>* OutDomains)
	{
		auto DomainOf = [](const FGridContent& C)
		{
			return C.Definition ? C.Definition->ConnectorDomain : EDomain::None;
		};

		for (const TPair<FGridCoord, FGridContent>& Pair : Occupancy)
		{
			if (Pair.Value.Type != TargetType || OutNet.Contains(Pair.Key)) continue;

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

						const FGridContent* NC = Occupancy.Find(Nb);
						if (!NC || NC->Type != TargetType) continue;
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
		const bool bRemoved = Occupancy.Remove(Tile) > 0;
		bIslandsDirty |= bRemoved;
		return bRemoved;
	}

	Occupancy.Add(Tile, Content);
	bIslandsDirty = true;
	return true;
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

	// 1. Collect buildings and a tile -> building index for coverage/obstruction.
	TArray<FGridCoord> Origins;
	TArray<const FGridContent*> Contents;
	TMap<FGridCoord, int32> TileToBuilding;

	for (const TPair<FGridCoord, FGridContent>& Pair : Occupancy)
	{
		if (Pair.Value.Type != EPlaceableType::Building) continue;

		const int32 Idx = Origins.Add(Pair.Key);
		Contents.Add(&Pair.Value);

		const FIntPoint Ext = FootprintExtent(Pair.Value);
		for (int32 dx = 0; dx < Ext.X; ++dx)
		{
			for (int32 dy = 0; dy < Ext.Y; ++dy)
			{
				TileToBuilding.Add(FGridCoord(Pair.Key.X + dx, Pair.Key.Y + dy), Idx);
			}
		}
	}

	const int32 N = Origins.Num();
	if (N == 0) return;

	// 2. Label connector networks.
	TMap<FGridCoord, int32> RoadNet;
	LabelNetworks(Occupancy, EPlaceableType::Road, RoadNet, nullptr);

	TMap<FGridCoord, int32> UtilNet;
	TArray<EDomain> UtilDomain;
	LabelNetworks(Occupancy, EPlaceableType::Utility, UtilNet, &UtilDomain);
	TArray<bool> UtilHasProducer;
	UtilHasProducer.Init(false, UtilDomain.Num());

	// 3. Union-Find over buildings.
	TArray<int32> Parent;
	Parent.SetNum(N);
	for (int32 i = 0; i < N; ++i) Parent[i] = i;

	// (a) Proximity — Chebyshev <= 2, facing ignored.
	for (int32 i = 0; i < N; ++i)
	{
		const FIntPoint Ext = FootprintExtent(*Contents[i]);
		for (int32 dx = 0; dx < Ext.X; ++dx)
		{
			for (int32 dy = 0; dy < Ext.Y; ++dy)
			{
				const FGridCoord T(Origins[i].X + dx, Origins[i].Y + dy);
				for (int32 ox = -ConnectorReach; ox <= ConnectorReach; ++ox)
				{
					for (int32 oy = -ConnectorReach; oy <= ConnectorReach; ++oy)
					{
						const int32* J = TileToBuilding.Find(FGridCoord(T.X + ox, T.Y + oy));
						if (J && *J != i) DsuUnion(Parent, i, *J);
					}
				}
			}
		}
	}

	// (b) Road — facing edge only; any two buildings on the same network join.
	TMap<int32, TArray<int32>> RoadGroups;
	for (int32 i = 0; i < N; ++i)
	{
		const FIntPoint Ext = FootprintExtent(*Contents[i]);
		TArray<FGridCoord> Edge;
		FGridCoord Step;
		EdgeRay(Origins[i], Ext, Contents[i]->Facing, Edge, Step);

		TSet<int32> Nets;
		ScanForNetworks(Edge, Step, RoadNet, TileToBuilding, Nets);
		for (int32 Net : Nets) RoadGroups.FindOrAdd(Net).Add(i);
	}
	for (const TPair<int32, TArray<int32>>& Group : RoadGroups)
	{
		for (int32 k = 1; k < Group.Value.Num(); ++k) DsuUnion(Parent, Group.Value[0], Group.Value[k]);
	}

	// (c) Utility — all four sides (facing ignored); only joins when the network
	//     has a matching-domain producer on it.
	const EPlaceableDirection Sides[] = {
		EPlaceableDirection::North, EPlaceableDirection::East,
		EPlaceableDirection::South, EPlaceableDirection::West };

	TMap<int32, TArray<int32>> UtilGroups;
	for (int32 i = 0; i < N; ++i)
	{
		const FIntPoint Ext = FootprintExtent(*Contents[i]);
		TSet<int32> Nets;
		for (EPlaceableDirection Side : Sides)
		{
			TArray<FGridCoord> Edge;
			FGridCoord Step;
			EdgeRay(Origins[i], Ext, Side, Edge, Step);
			ScanForNetworks(Edge, Step, UtilNet, TileToBuilding, Nets);
		}
		for (int32 Net : Nets)
		{
			UtilGroups.FindOrAdd(Net).Add(i);
			if (DefProducesDomain(Contents[i]->Definition, UtilDomain[Net]))
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

	// 4. Gather components into islands (by building origin).
	TMap<int32, int32> RootToIsland;
	for (int32 i = 0; i < N; ++i)
	{
		const int32 Root = DsuFind(Parent, i);
		int32* IslandIdx = RootToIsland.Find(Root);
		if (!IslandIdx)
		{
			IslandIdx = &RootToIsland.Add(Root, Islands.AddDefaulted());
		}
		Islands[*IslandIdx].Add(Origins[i]);
	}
}
