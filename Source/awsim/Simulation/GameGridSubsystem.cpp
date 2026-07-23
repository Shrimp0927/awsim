#include "Simulation/GameGridSubsystem.h"
#include "Simulation/GamePlayerFundsSubsystem.h"
#include "Entities/GridContent.h"
#include "Engine/World.h"

namespace
{
	constexpr int32 ConnectorReach = 2; // tiles a building can reach a connector across

	TArray<FGridCoord> FootprintTiles(const FGridCoord& Origin, const FGridContent& Content)
	{
		const FIntPoint Ext = UGridSubsystem::GetFootprintExtent(Content);
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

	// Scan outward up to ConnectorReach: a building blocks the ray, a connector ends it.
	void ScanForNetworks(const TArray<FGridCoord>& Edge, FGridCoord Step,
		const TMap<FGridCoord, int32>& NetMap, const TMap<FGridCoord, int32>& BuildingAt,
		TSet<int32>& OutNets)
	{
		for (const FGridCoord& E : Edge)
		{
			for (int32 k = 1; k <= ConnectorReach; ++k)
			{
				const FGridCoord T(E.X + Step.X * k, E.Y + Step.Y * k);
				if (BuildingAt.Contains(T)) break;
				if (const int32* Net = NetMap.Find(T)) { OutNets.Add(*Net); break; }
			}
		}
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

	// Flood-fill connectors into networks (8-connectivity); utilities join same-domain only.
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
	// Apply queued placements before domain phases read; invalid ones drop in SetContent.
	for (FPlacedBuilding& Pending : PendingPlacements)
	{
		SetContent(Pending.Origin, MoveTemp(Pending.Content));
	}
	PendingPlacements.Reset();

	if (StepSeconds > 0.f)
	{
		GrowBuildings(); // paused pumps (dt = 0) don't age buildings
	}

	EnsureIslands();
}

void UGridSubsystem::GrowBuildings()
{
	TArray<FGridCoord> FullyGrown;
	for (TPair<FGridCoord, uint8>& Clock : LifetimeAt)
	{
		if (++Clock.Value <= StepsPerFloor)
		{
			continue;
		}
		Clock.Value = 1; // rolled over: add a floor and restart

		const int32* Idx = BuildingAt.Find(Clock.Key);
		if (!Idx)
		{
			FullyGrown.Add(Clock.Key);
			continue;
		}
		FPlacedBuilding& Building = Buildings[*Idx];
		const UPlaceableDef* Def = Building.Content.Definition;
		const int32 MaxHeight = Def
			? static_cast<int32>(FMath::Clamp(Def->HeightTiles, 1.f, UPlaceableDef::MaxHeightTiles)) : 1;
		Building.HeightTiles = FMath::Min(Building.HeightTiles + 1, MaxHeight);
		for (const FGridCoord& T : FootprintTiles(Building.Origin, Building.Content))
		{
			HeightAt[T.Y * GridWidth + T.X] = Building.HeightTiles;
		}
		++ContentRevision; // taller now: rendering and picking see the new column
		if (Building.HeightTiles >= MaxHeight)
		{
			FullyGrown.Add(Clock.Key);
		}
	}
	for (const FGridCoord& Key : FullyGrown)
	{
		LifetimeAt.Remove(Key);
	}
}

uint8 UGridSubsystem::GetLifetime(FGridCoord Origin) const
{
	const uint8* Clock = LifetimeAt.Find(Origin);
	return Clock ? *Clock : 0;
}

void UGridSubsystem::QueuePlacement(FGridCoord Tile, FGridContent Content)
{
	FPlacedBuilding Pending;
	Pending.Origin = Tile;
	Pending.Content = MoveTemp(Content);
	PendingPlacements.Add(MoveTemp(Pending));
}

FIntPoint UGridSubsystem::GetFootprintExtent(const FGridContent& Content)
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

bool UGridSubsystem::SetSliderValue(FGridCoord Tile, int32 SliderIndex, float Value)
{
	const int32* Idx = BuildingAt.Find(Tile);
	if (!Idx)
	{
		return false;
	}

	FGridContent& Content = Buildings[*Idx].Content;
	if (!Content.Definition || !Content.SliderValues.IsValidIndex(SliderIndex))
	{
		return false;
	}

	const FFloatInterval& Range = Content.Definition->Sliders[SliderIndex].Range;
	Content.SliderValues[SliderIndex] = FMath::Clamp(Value, Range.Min, Range.Max);
	++SliderRevision;
	return true;
}

bool UGridSubsystem::IsTileOccupied(FGridCoord Tile) const
{
	return Roads.Contains(Tile) || Utilities.Contains(Tile) || BuildingAt.Contains(Tile);
}

bool UGridSubsystem::PickGroundTile(const FVector& RayOrigin, const FVector& RayDir, FGridCoord& OutTile) const
{
	if (RayDir.Z >= 0.f)
	{
		return false;
	}
	const FVector Ground = RayOrigin - RayDir * (RayOrigin.Z / RayDir.Z);
	OutTile = FGridCoord(
		FMath::FloorToInt32((Ground.X - WorldMinX) / TileSize),
		FMath::FloorToInt32((Ground.Y - WorldMinY) / TileSize));
	return IsInBounds(OutTile);
}

bool UGridSubsystem::PickTile(const FVector& RayOrigin, const FVector& RayDir, FGridCoord& OutTile) const
{
	if (RayDir.Z >= 0.f)
	{
		return false;
	}

	const auto TileOf = [](const FVector& P)
	{
		return FGridCoord(
			FMath::FloorToInt32((P.X - WorldMinX) / TileSize),
			FMath::FloorToInt32((P.Y - WorldMinY) / TileSize));
	};

	const float GroundT = -RayOrigin.Z / RayDir.Z;
	const FGridCoord GroundTile = TileOf(RayOrigin + RayDir * GroundT);

	if (HeightAt.Num() > 0)
	{
		// 2D DDA from where the ray first dips under MaxHeightTiles down to the ground point.
		const float MaxZ = UPlaceableDef::MaxHeightTiles * TileSize;
		const float StartT = FMath::Max((MaxZ - RayOrigin.Z) / RayDir.Z, 0.f);
		FGridCoord Tile = TileOf(RayOrigin + RayDir * StartT);

		const int32 StepX = RayDir.X > KINDA_SMALL_NUMBER ? 1 : (RayDir.X < -KINDA_SMALL_NUMBER ? -1 : 0);
		const int32 StepY = RayDir.Y > KINDA_SMALL_NUMBER ? 1 : (RayDir.Y < -KINDA_SMALL_NUMBER ? -1 : 0);
		const auto BoundaryT = [&](float Origin, float Dir, float Min, int32 Cell, int32 Step)
		{
			if (Step == 0) return UE_BIG_NUMBER;
			const float Edge = Min + (Cell + (Step > 0 ? 1 : 0)) * TileSize;
			return (Edge - Origin) / Dir;
		};
		float NextX = BoundaryT(RayOrigin.X, RayDir.X, WorldMinX, Tile.X, StepX);
		float NextY = BoundaryT(RayOrigin.Y, RayDir.Y, WorldMinY, Tile.Y, StepY);

		while (true)
		{
			// A column catches the ray iff its top reaches the ray's exit altitude.
			const float ExitT = FMath::Min3(NextX, NextY, GroundT);
			const float ExitZ = RayOrigin.Z + RayDir.Z * ExitT;
			const float Height = GetHeightAt(Tile) * TileSize;
			if (Height > 0.f && Height >= ExitZ)
			{
				OutTile = Tile;
				return true;
			}
			if (ExitT >= GroundT)
			{
				break;
			}
			if (NextX < NextY)
			{
				Tile.X += StepX;
				NextX += TileSize / FMath::Abs(RayDir.X);
			}
			else
			{
				Tile.Y += StepY;
				NextY += TileSize / FMath::Abs(RayDir.Y);
			}
		}
	}

	OutTile = GroundTile;
	return IsInBounds(GroundTile);
}

const FPlacedBuilding* UGridSubsystem::FindBuildingAt(FGridCoord Tile) const
{
	const int32* Idx = BuildingAt.Find(Tile);
	return Idx ? &Buildings[*Idx] : nullptr;
}

float UGridSubsystem::GetHeightAt(FGridCoord Tile) const
{
	const int32 Index = Tile.Y * GridWidth + Tile.X;
	return IsInBounds(Tile) && HeightAt.IsValidIndex(Index) ? HeightAt[Index] : 0.f;
}

const FGridContent& UGridSubsystem::GetContentAt(FGridCoord Tile) const
{
	static const FGridContent Empty;
	if (const FGridContent* R = Roads.Find(Tile)) return *R;
	if (const FGridContent* U = Utilities.Find(Tile)) return *U;
	if (const int32* Idx = BuildingAt.Find(Tile)) return Buildings[*Idx].Content;
	return Empty;
}

bool UGridSubsystem::SetContent(FGridCoord Tile, FGridContent Content)
{
	if (Content.Type == EPlaceableType::None)
	{
		if (Roads.Remove(Tile) > 0) { bRoadNetDirty = true; bIslandsDirty = true; ++ContentRevision; return true; }
		if (Utilities.Remove(Tile) > 0) { bUtilNetDirty = true; bIslandsDirty = true; ++ContentRevision; return true; }
		if (const int32* Idx = BuildingAt.Find(Tile))
		{
			RemoveBuildingAt(*Idx);
			bIslandsDirty = true;
			++ContentRevision;
			return true;
		}
		return false;
	}

	const TArray<FGridCoord> Tiles = FootprintTiles(Tile, Content);
	for (const FGridCoord& T : Tiles)
	{
		if (!IsInBounds(T) || IsTileOccupied(T))
		{
			return false;
		}
	}

	// Charge cost only after validation so a rejected placement never spends.
	const float Cost = Content.Definition ? Content.Definition->Cost : 0.f;
	if (Cost > 0.f)
	{
		UGamePlayerFundsSubsystem* PlayerFunds = ResolveFunds();
		if (!PlayerFunds || !PlayerFunds->TrySpend(Cost))
		{
			return false;
		}
	}

	// Placements start at full operation unless the caller supplied a full set.
	if (Content.Definition && Content.SliderValues.Num() != Content.Definition->Sliders.Num())
	{
		Content.SliderValues.Reset(Content.Definition->Sliders.Num());
		for (const FSliderDef& Slider : Content.Definition->Sliders)
		{
			Content.SliderValues.Add(Slider.Range.Max);
		}
	}

	switch (Content.Type)
	{
	case EPlaceableType::Road:
		Roads.Add(Tile, Content);
		bRoadNetDirty = true;
		break;
	case EPlaceableType::Utility:
		Utilities.Add(Tile, Content);
		bUtilNetDirty = true;
		break;
	default: // Building (and any other footprint occupant)
	{
		FPlacedBuilding Placed;
		Placed.Origin = Tile;
		Placed.Content = Content;
		const int32 Idx = Buildings.Add(MoveTemp(Placed));
		for (const FGridCoord& T : Tiles) BuildingAt.Add(T, Idx);

		if (HeightAt.Num() == 0) HeightAt.SetNumZeroed(GridWidth * GridHeight);
		for (const FGridCoord& T : Tiles) HeightAt[T.Y * GridWidth + T.X] = 1.f; // one floor at placement

		const UPlaceableDef* Def = Buildings[Idx].Content.Definition;
		const int32 MaxHeight = Def
			? static_cast<int32>(FMath::Clamp(Def->HeightTiles, 1.f, UPlaceableDef::MaxHeightTiles)) : 1;
		if (MaxHeight > 1)
		{
			LifetimeAt.Add(Tile, 1); // growth clock starts ticking
		}
		break;
	}
	}

	bIslandsDirty = true;
	++ContentRevision;
	return true;
}

void UGridSubsystem::RemoveBuildingAt(int32 Index)
{
	LifetimeAt.Remove(Buildings[Index].Origin);
	for (const FGridCoord& T : FootprintTiles(Buildings[Index].Origin, Buildings[Index].Content))
	{
		BuildingAt.Remove(T);
		if (HeightAt.Num() > 0) HeightAt[T.Y * GridWidth + T.X] = 0.f;
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

UGamePlayerFundsSubsystem* UGridSubsystem::ResolveFunds() const
{
	if (Funds)
	{
		return Funds;
	}
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UGamePlayerFundsSubsystem>() : nullptr;
}

const TArray<TArray<FGridCoord>>& UGridSubsystem::GetIslands() const
{
	EnsureIslands();
	return Islands;
}

void UGridSubsystem::EnsureNetworks() const
{
	if (bRoadNetDirty)
	{
		RoadNet.Reset();
		LabelNetworks(Roads, RoadNet, nullptr);
		bRoadNetDirty = false;
	}
	if (bUtilNetDirty)
	{
		UtilNet.Reset();
		UtilDomain.Reset();
		LabelNetworks(Utilities, UtilNet, &UtilDomain);
		bUtilNetDirty = false;
	}
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

	// Networks are cached, but producer flags depend on buildings so rebuild every time.
	EnsureNetworks();
	TArray<bool> UtilHasProducer;
	UtilHasProducer.Init(false, UtilDomain.Num());

	TArray<int32> Parent;
	Parent.SetNum(N);
	for (int32 i = 0; i < N; ++i) Parent[i] = i;

	// Proximity — Chebyshev distance <= ConnectorReach, facing ignored.
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

	// Road — facing edge only; any two buildings on the same network join.
	TMap<int32, TArray<int32>> RoadGroups;
	for (int32 i = 0; i < N; ++i)
	{
		const FIntPoint Ext = GetFootprintExtent(Buildings[i].Content);
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

	// Utility — all four sides; joins only when the network has a matching-domain producer.
	const EPlaceableDirection Sides[] = {
		EPlaceableDirection::North, EPlaceableDirection::East,
		EPlaceableDirection::South, EPlaceableDirection::West };

	TMap<int32, TArray<int32>> UtilGroups;
	for (int32 i = 0; i < N; ++i)
	{
		const FIntPoint Ext = GetFootprintExtent(Buildings[i].Content);
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
