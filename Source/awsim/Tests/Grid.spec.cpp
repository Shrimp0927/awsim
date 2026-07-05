#include "Misc/AutomationTest.h"
#include "Simulation/GridSubsystem.h"
#include "Entities/GridContent.h"
#include "UObject/StrongObjectPtr.h"

// Live spec for the GridSubsystem features that exist today: bounds, placement
// (footprint-aware), occupancy, and connected-island detection (proximity, road,
// utility with producer gate). Placement/island logic never calls GetWorld(), so
// a plain NewObject'd subsystem can be driven directly.
// (On UE < 5.5 the mask is EAutomationTestFlags::ApplicationContextMask.)

#if WITH_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FGridSpec, "awsim.Simulation.Grid",
	EAutomationTestFlags::ProductFilter | EAutomationTestFlags_ApplicationContextMask)

	TStrongObjectPtr<UGridSubsystem> Grid;

	UPlaceableDef* MakeDef(EPlaceableType Type, FIntPoint Dims, EDomain ConnectorDomain)
	{
		UPlaceableDef* Def = NewObject<UPlaceableDef>();
		Def->Type = Type;
		Def->Dimensions = Dims;
		Def->ConnectorDomain = ConnectorDomain;
		return Def;
	}

	// A building def that PRODUCES a domain (positive effect at max slider).
	UPlaceableDef* MakeProducerDef(EDomain Domain)
	{
		UPlaceableDef* Def = MakeDef(EPlaceableType::Building, FIntPoint(1, 1), EDomain::None);
		FDomainEffect Effect;
		Effect.Domain = Domain;
		Effect.AmountAtMax = 10.f;
		FSliderDef Slider;
		Slider.Effects.Add(Effect);
		Def->Sliders.Add(Slider);
		return Def;
	}

	FGridContent MakeContent(EPlaceableType Type, UPlaceableDef* Def, EPlaceableDirection Facing)
	{
		FGridContent Content;
		Content.Type = Type;
		Content.Facing = Facing;
		Content.Definition = Def;
		return Content;
	}

	bool PlaceBuilding(FGridCoord At, EPlaceableDirection Facing = EPlaceableDirection::North, UPlaceableDef* Def = nullptr)
	{
		if (!Def) Def = MakeDef(EPlaceableType::Building, FIntPoint(1, 1), EDomain::None);
		return Grid->SetContent(At, MakeContent(EPlaceableType::Building, Def, Facing));
	}

	bool PlaceRoad(FGridCoord At)
	{
		return Grid->SetContent(At, MakeContent(EPlaceableType::Road, MakeDef(EPlaceableType::Road, FIntPoint(1, 1), EDomain::None), EPlaceableDirection::None));
	}

	bool PlaceUtility(FGridCoord At, EDomain Domain)
	{
		return Grid->SetContent(At, MakeContent(EPlaceableType::Utility, MakeDef(EPlaceableType::Utility, FIntPoint(1, 1), Domain), EPlaceableDirection::None));
	}

	bool SameIsland(FGridCoord A, FGridCoord B)
	{
		for (const TArray<FGridCoord>& Island : Grid->GetIslands())
		{
			if (Island.Contains(A) && Island.Contains(B)) return true;
		}
		return false;
	}

END_DEFINE_SPEC(FGridSpec)

void FGridSpec::Define()
{
	BeforeEach([this]() { Grid = TStrongObjectPtr<UGridSubsystem>(NewObject<UGridSubsystem>()); });
	AfterEach([this]() { Grid.Reset(); });

	Describe("World dimensions", [this]()
	{
		It("is a 1000 x 1000 grid", [this]()
		{
			TestEqual(TEXT("width"), UGridSubsystem::GetWidth(), 1000);
			TestEqual(TEXT("height"), UGridSubsystem::GetHeight(), 1000);
		});

		It("treats coordinates outside 0..999 as out of bounds", [this]()
		{
			TestTrue(TEXT("origin"), UGridSubsystem::IsInBounds(FGridCoord(0, 0)));
			TestTrue(TEXT("far corner"), UGridSubsystem::IsInBounds(FGridCoord(999, 999)));
			TestFalse(TEXT("negative x"), UGridSubsystem::IsInBounds(FGridCoord(-1, 0)));
			TestFalse(TEXT("x == 1000"), UGridSubsystem::IsInBounds(FGridCoord(1000, 0)));
		});
	});

	Describe("Placement", [this]()
	{
		It("places a building on an empty, in-bounds tile", [this]()
		{
			TestTrue(TEXT("placed"), PlaceBuilding(FGridCoord(5, 5)));
			TestTrue(TEXT("occupied"), Grid->IsTileOccupied(FGridCoord(5, 5)));
		});

		It("rejects a building placed out of bounds", [this]()
		{
			TestFalse(TEXT("rejected"), PlaceBuilding(FGridCoord(-1, 0)));
			TestFalse(TEXT("not occupied"), Grid->IsTileOccupied(FGridCoord(-1, 0)));
		});

		It("rejects a building on an already-occupied tile", [this]()
		{
			TestTrue(TEXT("first"), PlaceBuilding(FGridCoord(5, 5)));
			TestFalse(TEXT("second"), PlaceBuilding(FGridCoord(5, 5)));
		});

		It("places roads, which occupy their tiles", [this]()
		{
			TestTrue(TEXT("placed"), PlaceRoad(FGridCoord(3, 3)));
			TestTrue(TEXT("occupied"), Grid->IsTileOccupied(FGridCoord(3, 3)));
			TestTrue(TEXT("content is road"), Grid->GetContentAt(FGridCoord(3, 3)).Type == EPlaceableType::Road);
		});

		It("removing a tile frees it", [this]()
		{
			PlaceBuilding(FGridCoord(5, 5));
			TestTrue(TEXT("removed"), Grid->SetContent(FGridCoord(5, 5), FGridContent()));
			TestFalse(TEXT("free"), Grid->IsTileOccupied(FGridCoord(5, 5)));
		});
	});

	Describe("Multi-tile footprint", [this]()
	{
		It("occupies every tile of a 2x3 footprint (facing North)", [this]()
		{
			UPlaceableDef* Def = MakeDef(EPlaceableType::Building, FIntPoint(2, 3), EDomain::None);
			TestTrue(TEXT("placed"), PlaceBuilding(FGridCoord(10, 10), EPlaceableDirection::North, Def));
			TestTrue(TEXT("covers (10,10)"), Grid->IsTileOccupied(FGridCoord(10, 10)));
			TestTrue(TEXT("covers (11,12)"), Grid->IsTileOccupied(FGridCoord(11, 12)));
			TestFalse(TEXT("excludes (12,10)"), Grid->IsTileOccupied(FGridCoord(12, 10)));
		});

		It("rejects placement overlapping an existing footprint", [this]()
		{
			UPlaceableDef* Def = MakeDef(EPlaceableType::Building, FIntPoint(2, 2), EDomain::None);
			TestTrue(TEXT("first"), PlaceBuilding(FGridCoord(10, 10), EPlaceableDirection::North, Def));
			TestFalse(TEXT("overlap rejected"), PlaceBuilding(FGridCoord(11, 11)));
		});

		It("rotates the footprint with facing (East swaps width/length)", [this]()
		{
			// 2x3 North -> 2 wide x 3 tall; East -> 3 wide x 2 tall.
			UPlaceableDef* Def = MakeDef(EPlaceableType::Building, FIntPoint(2, 3), EDomain::None);
			TestTrue(TEXT("placed"), PlaceBuilding(FGridCoord(20, 20), EPlaceableDirection::East, Def));
			TestTrue(TEXT("covers (22,21) — x extent 3"), Grid->IsTileOccupied(FGridCoord(22, 21)));
			TestFalse(TEXT("excludes (20,22) — y extent 2"), Grid->IsTileOccupied(FGridCoord(20, 22)));
		});
	});

	Describe("Islands — proximity (Chebyshev <= 2, facing ignored)", [this]()
	{
		It("puts two buildings within Chebyshev distance 2 into one island", [this]()
		{
			PlaceBuilding(FGridCoord(5, 5));
			PlaceBuilding(FGridCoord(7, 5)); // distance 2
			TestTrue(TEXT("same island"), SameIsland(FGridCoord(5, 5), FGridCoord(7, 5)));
			TestEqual(TEXT("one island"), Grid->GetIslands().Num(), 1);
		});

		It("treats a diagonal gap within 2 as proximity (Chebyshev, not Manhattan)", [this]()
		{
			PlaceBuilding(FGridCoord(5, 5));
			PlaceBuilding(FGridCoord(7, 7)); // Chebyshev 2, Manhattan 4
			TestTrue(TEXT("same island"), SameIsland(FGridCoord(5, 5), FGridCoord(7, 7)));
		});

		It("keeps two buildings more than 2 tiles apart in separate islands", [this]()
		{
			PlaceBuilding(FGridCoord(5, 5));
			PlaceBuilding(FGridCoord(8, 5)); // distance 3
			TestFalse(TEXT("different islands"), SameIsland(FGridCoord(5, 5), FGridCoord(8, 5)));
			TestEqual(TEXT("two islands"), Grid->GetIslands().Num(), 2);
		});
	});

	Describe("Islands — road connectivity (facing rule)", [this]()
	{
		It("puts two buildings that reach the same road network into one island", [this]()
		{
			for (int32 x = 5; x <= 9; ++x) PlaceRoad(FGridCoord(x, 5)); // road along y=5
			PlaceBuilding(FGridCoord(5, 7), EPlaceableDirection::South); // scans -Y to (5,5) at dist 2
			PlaceBuilding(FGridCoord(9, 7), EPlaceableDirection::South); // scans -Y to (9,5)
			TestTrue(TEXT("same island via road"), SameIsland(FGridCoord(5, 7), FGridCoord(9, 7)));
		});

		It("keeps buildings on two disconnected road networks in separate islands", [this]()
		{
			PlaceRoad(FGridCoord(5, 5));
			PlaceRoad(FGridCoord(20, 5)); // far, separate network
			PlaceBuilding(FGridCoord(5, 7), EPlaceableDirection::South);
			PlaceBuilding(FGridCoord(20, 7), EPlaceableDirection::South);
			TestFalse(TEXT("different islands"), SameIsland(FGridCoord(5, 7), FGridCoord(20, 7)));
		});

		It("does not connect a building facing away from the road", [this]()
		{
			for (int32 x = 10; x <= 12; ++x) PlaceRoad(FGridCoord(x, 10));
			PlaceBuilding(FGridCoord(10, 12), EPlaceableDirection::South); // reaches (10,10)
			PlaceBuilding(FGridCoord(12, 12), EPlaceableDirection::South); // reaches (12,10)
			PlaceBuilding(FGridCoord(12, 8), EPlaceableDirection::South);  // road is +Y, faces -Y -> no reach
			TestTrue(TEXT("facers connect"), SameIsland(FGridCoord(10, 12), FGridCoord(12, 12)));
			TestFalse(TEXT("away-facer isolated"), SameIsland(FGridCoord(10, 12), FGridCoord(12, 8)));
		});
	});

	Describe("Islands — utility connectivity (producer-gated, facing ignored)", [this]()
	{
		It("connects a producer and a consumer on one utility network (facing ignored)", [this]()
		{
			for (int32 x = 10; x <= 14; ++x) PlaceUtility(FGridCoord(x, 10), EDomain::Energy);
			// Both face North (AWAY from the utility to their South) — utility ignores facing.
			PlaceBuilding(FGridCoord(10, 12), EPlaceableDirection::North, MakeProducerDef(EDomain::Energy));
			PlaceBuilding(FGridCoord(14, 12), EPlaceableDirection::North); // consumer
			TestTrue(TEXT("same island"), SameIsland(FGridCoord(10, 12), FGridCoord(14, 12)));
		});

		It("links multiple consumers through the producer's network (transitive)", [this]()
		{
			for (int32 x = 10; x <= 16; ++x) PlaceUtility(FGridCoord(x, 10), EDomain::Energy);
			PlaceBuilding(FGridCoord(10, 12), EPlaceableDirection::North, MakeProducerDef(EDomain::Energy));
			PlaceBuilding(FGridCoord(13, 12), EPlaceableDirection::North); // consumer
			PlaceBuilding(FGridCoord(16, 12), EPlaceableDirection::North); // consumer
			TestTrue(TEXT("consumers linked via producer"), SameIsland(FGridCoord(13, 12), FGridCoord(16, 12)));
		});

		It("does NOT connect two consumers on a producer-less utility network", [this]()
		{
			for (int32 x = 10; x <= 14; ++x) PlaceUtility(FGridCoord(x, 10), EDomain::Energy);
			PlaceBuilding(FGridCoord(10, 12), EPlaceableDirection::North); // consumer
			PlaceBuilding(FGridCoord(14, 12), EPlaceableDirection::North); // consumer
			TestFalse(TEXT("no source -> no link"), SameIsland(FGridCoord(10, 12), FGridCoord(14, 12)));
		});

		It("requires the connector's matching domain (Water pipe ignores an Energy producer)", [this]()
		{
			for (int32 x = 10; x <= 14; ++x) PlaceUtility(FGridCoord(x, 10), EDomain::Water); // pipe
			PlaceBuilding(FGridCoord(10, 12), EPlaceableDirection::North, MakeProducerDef(EDomain::Energy)); // wrong domain
			PlaceBuilding(FGridCoord(14, 12), EPlaceableDirection::North);
			TestFalse(TEXT("domain mismatch -> no link"), SameIsland(FGridCoord(10, 12), FGridCoord(14, 12)));
		});
	});

	Describe("Islands — bridging", [this]()
	{
		It("merges a road group and a utility group through one bridging building", [this]()
		{
			// Road at (10,10); X reaches it from +Y, B (producer) reaches it from -Y.
			PlaceRoad(FGridCoord(10, 10));
			PlaceBuilding(FGridCoord(10, 12), EPlaceableDirection::South); // X -> road
			PlaceBuilding(FGridCoord(10, 8), EPlaceableDirection::North, MakeProducerDef(EDomain::Energy)); // B -> road

			// Utility at (10,6); B reaches it (any side), Y reaches it from -Y.
			PlaceUtility(FGridCoord(10, 6), EDomain::Energy);
			PlaceBuilding(FGridCoord(10, 4), EPlaceableDirection::North); // Y -> utility

			// X --road-- B --utility-- Y  => all one island.
			TestTrue(TEXT("bridged"), SameIsland(FGridCoord(10, 12), FGridCoord(10, 4)));
		});
	});
}

#endif // WITH_AUTOMATION_TESTS
