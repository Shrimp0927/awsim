#include "Misc/AutomationTest.h"
#include "Simulation/GameGridSubsystem.h"
#include "Simulation/GamePlayerFundsSubsystem.h"
#include "Entities/GridContent.h"
#include "UObject/StrongObjectPtr.h"

// Spec for UGridSubsystem: bounds, funds-gated footprint placement, the
// placement queue, per-instance sliders, and connected-island detection.

#if WITH_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FGridSpec, "awsim.Simulation.Grid",
	EAutomationTestFlags::ProductFilter | EAutomationTestFlags_ApplicationContextMask)

	TStrongObjectPtr<UGridSubsystem> Grid;
	TStrongObjectPtr<UGamePlayerFundsSubsystem> PlayerFunds;

	UPlaceableDef* MakeDef(EPlaceableType Type, FIntPoint Dims, EDomain ConnectorDomain)
	{
		UPlaceableDef* Def = NewObject<UPlaceableDef>();
		Def->Type = Type;
		Def->Dimensions = Dims;
		Def->ConnectorDomain = ConnectorDomain;
		return Def;
	}

	// A 1x1 building def with a placement price.
	UPlaceableDef* MakeCostedDef(float Cost)
	{
		UPlaceableDef* Def = MakeDef(EPlaceableType::Building, FIntPoint(1, 1), EDomain::None);
		Def->Cost = Cost;
		return Def;
	}

	// A building def that produces the given domain.
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

	// A ray through the center of GroundTile at the camera's -60 pitch, looking -Y
	// (default orientation: the camera sits at +Y of what it looks at).
	void MakeRay(FGridCoord GroundTile, FVector& OutOrigin, FVector& OutDir)
	{
		const FVector Target(
			UGridSubsystem::WorldMinX + (GroundTile.X + 0.5f) * UGridSubsystem::TileSize,
			UGridSubsystem::WorldMinY + (GroundTile.Y + 0.5f) * UGridSubsystem::TileSize,
			0.f);
		OutDir = FVector(0.f, -0.5f, -FMath::Sin(FMath::DegreesToRadians(60.f)));
		OutOrigin = Target - OutDir * 40000.f;
	}

	void StepGrid(int32 Steps)
	{
		for (int32 i = 0; i < Steps; ++i) Grid->Step(1.f / 30.f);
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
		It("is a 500 x 500 grid", [this]()
		{
			TestEqual(TEXT("width"), UGridSubsystem::GetWidth(), 500);
			TestEqual(TEXT("height"), UGridSubsystem::GetHeight(), 500);
		});

		It("treats coordinates outside 0..499 as out of bounds", [this]()
		{
			TestTrue(TEXT("origin"), UGridSubsystem::IsInBounds(FGridCoord(0, 0)));
			TestTrue(TEXT("far corner"), UGridSubsystem::IsInBounds(FGridCoord(499, 499)));
			TestFalse(TEXT("negative x"), UGridSubsystem::IsInBounds(FGridCoord(-1, 0)));
			TestFalse(TEXT("x == 500"), UGridSubsystem::IsInBounds(FGridCoord(500, 0)));
		});
	});

	Describe("Column heights", [this]()
	{
		It("stamps one floor across the footprint on placement and clears on removal", [this]()
		{
			UPlaceableDef* Def = MakeDef(EPlaceableType::Building, FIntPoint(2, 2), EDomain::None);
			Def->HeightTiles = 30.f;
			TestTrue(TEXT("placed"), Grid->SetContent(FGridCoord(5, 5), MakeContent(EPlaceableType::Building, Def, EPlaceableDirection::North)));
			TestEqual(TEXT("origin"), Grid->GetHeightAt(FGridCoord(5, 5)), 1.f);
			TestEqual(TEXT("far corner"), Grid->GetHeightAt(FGridCoord(6, 6)), 1.f);
			TestEqual(TEXT("outside footprint"), Grid->GetHeightAt(FGridCoord(7, 7)), 0.f);

			Grid->SetContent(FGridCoord(6, 6), FGridContent());
			TestEqual(TEXT("cleared"), Grid->GetHeightAt(FGridCoord(5, 5)), 0.f);
		});

		It("grows one floor per lifetime cycle, then leaves the clock", [this]()
		{
			UPlaceableDef* Def = MakeDef(EPlaceableType::Building, FIntPoint(1, 1), EDomain::None);
			Def->HeightTiles = 3.f;
			TestTrue(TEXT("placed"), Grid->SetContent(FGridCoord(5, 5), MakeContent(EPlaceableType::Building, Def, EPlaceableDirection::North)));
			TestEqual(TEXT("clock starts at 1"), static_cast<int32>(Grid->GetLifetime(FGridCoord(5, 5))), 1);

			StepGrid(UGridSubsystem::StepsPerFloor);
			TestEqual(TEXT("second floor"), Grid->GetHeightAt(FGridCoord(5, 5)), 2.f);
			StepGrid(UGridSubsystem::StepsPerFloor);
			TestEqual(TEXT("third floor"), Grid->GetHeightAt(FGridCoord(5, 5)), 3.f);
			TestEqual(TEXT("fully grown drops off the clock"), static_cast<int32>(Grid->GetLifetime(FGridCoord(5, 5))), 0);
			StepGrid(UGridSubsystem::StepsPerFloor);
			TestEqual(TEXT("stays at max"), Grid->GetHeightAt(FGridCoord(5, 5)), 3.f);
		});

		It("does not age on paused pumps (dt = 0)", [this]()
		{
			UPlaceableDef* Def = MakeDef(EPlaceableType::Building, FIntPoint(1, 1), EDomain::None);
			Def->HeightTiles = 2.f;
			TestTrue(TEXT("placed"), Grid->SetContent(FGridCoord(5, 5), MakeContent(EPlaceableType::Building, Def, EPlaceableDirection::North)));
			for (int32 i = 0; i < 3; ++i) Grid->Step(0.f);
			TestEqual(TEXT("clock unmoved"), static_cast<int32>(Grid->GetLifetime(FGridCoord(5, 5))), 1);
			TestEqual(TEXT("height unmoved"), Grid->GetHeightAt(FGridCoord(5, 5)), 1.f);
		});

		It("keeps roads and out-of-bounds tiles flat", [this]()
		{
			PlaceRoad(FGridCoord(3, 3));
			TestEqual(TEXT("road"), Grid->GetHeightAt(FGridCoord(3, 3)), 0.f);
			TestEqual(TEXT("out of bounds"), Grid->GetHeightAt(FGridCoord(-1, 0)), 0.f);
		});
	});

	Describe("Cursor picking", [this]()
	{
		It("picks the ground tile when nothing occludes the ray", [this]()
		{
			FVector Origin, Dir;
			MakeRay(FGridCoord(10, 13), Origin, Dir);
			FGridCoord Picked;
			TestTrue(TEXT("picked"), Grid->PickTile(Origin, Dir, Picked));
			TestEqual(TEXT("tile"), Picked, FGridCoord(10, 13));
		});

		It("picks a tall building that blocks the ray before the ground tile", [this]()
		{
			UPlaceableDef* Tall = MakeDef(EPlaceableType::Building, FIntPoint(1, 1), EDomain::None);
			Tall->HeightTiles = 5.f;
			TestTrue(TEXT("placed"), Grid->SetContent(FGridCoord(10, 15), MakeContent(EPlaceableType::Building, Tall, EPlaceableDirection::North))); // camera-side of (10, 13)
			StepGrid(4 * UGridSubsystem::StepsPerFloor); // grow to 5 tiles; the ray passes ~2.6 tiles up
			FVector Origin, Dir;
			MakeRay(FGridCoord(10, 13), Origin, Dir);
			FGridCoord Picked;
			TestTrue(TEXT("picked"), Grid->PickTile(Origin, Dir, Picked));
			TestEqual(TEXT("occluder wins"), Picked, FGridCoord(10, 15));
		});

		It("marches over a building too short to reach the ray", [this]()
		{
			UPlaceableDef* Short = MakeDef(EPlaceableType::Building, FIntPoint(1, 1), EDomain::None);
			Short->HeightTiles = 2.f; // even fully grown, the ray passes ~2.6 tiles up
			TestTrue(TEXT("placed"), Grid->SetContent(FGridCoord(10, 15), MakeContent(EPlaceableType::Building, Short, EPlaceableDirection::North)));
			StepGrid(UGridSubsystem::StepsPerFloor); // fully grown at 2 tiles
			FVector Origin, Dir;
			MakeRay(FGridCoord(10, 13), Origin, Dir);
			FGridCoord Picked;
			TestTrue(TEXT("picked"), Grid->PickTile(Origin, Dir, Picked));
			TestEqual(TEXT("ground wins"), Picked, FGridCoord(10, 13));
		});

		It("ground pick ignores occluding buildings", [this]()
		{
			UPlaceableDef* Tall = MakeDef(EPlaceableType::Building, FIntPoint(1, 1), EDomain::None);
			Tall->HeightTiles = 5.f;
			TestTrue(TEXT("placed"), Grid->SetContent(FGridCoord(10, 15), MakeContent(EPlaceableType::Building, Tall, EPlaceableDirection::North)));
			StepGrid(4 * UGridSubsystem::StepsPerFloor); // tall enough to occlude (10, 13) for PickTile
			FVector Origin, Dir;
			MakeRay(FGridCoord(10, 13), Origin, Dir);
			FGridCoord Picked;
			TestTrue(TEXT("picked"), Grid->PickGroundTile(Origin, Dir, Picked));
			TestEqual(TEXT("plane tile"), Picked, FGridCoord(10, 13));
		});

		It("returns false for a ray landing off-grid with no occluder", [this]()
		{
			FVector Origin, Dir;
			MakeRay(FGridCoord(10, -20), Origin, Dir);
			FGridCoord Picked;
			TestFalse(TEXT("off-grid"), Grid->PickTile(Origin, Dir, Picked));
		});
	});

	Describe("Placement", [this]()
	{
		It("places a building on an empty, in-bounds tile", [this]()
		{
			TestTrue(TEXT("placed"), PlaceBuilding(FGridCoord(5, 5)));
			TestTrue(TEXT("occupied"), Grid->IsTileOccupied(FGridCoord(5, 5)));
		});

		It("finds the covering building from any footprint tile", [this]()
		{
			UPlaceableDef* Def = MakeDef(EPlaceableType::Building, FIntPoint(2, 2), EDomain::None);
			TestTrue(TEXT("placed"), Grid->SetContent(FGridCoord(5, 5), MakeContent(EPlaceableType::Building, Def, EPlaceableDirection::North)));
			const FPlacedBuilding* Building = Grid->FindBuildingAt(FGridCoord(6, 6));
			TestNotNull(TEXT("found"), Building);
			if (Building)
			{
				TestEqual(TEXT("origin"), Building->Origin, FGridCoord(5, 5));
			}
			TestNull(TEXT("empty tile"), Grid->FindBuildingAt(FGridCoord(9, 9)));
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

	Describe("Placement queue — player intent applies when the grid phase steps", [this]()
	{
		It("does not occupy the tile until the grid steps", [this]()
		{
			Grid->QueuePlacement(FGridCoord(5, 5), MakeContent(EPlaceableType::Building, MakeDef(EPlaceableType::Building, FIntPoint(1, 1), EDomain::None), EPlaceableDirection::North));
			TestFalse(TEXT("queued, not applied"), Grid->IsTileOccupied(FGridCoord(5, 5)));
			TestEqual(TEXT("one pending"), Grid->NumPendingPlacements(), 1);

			Grid->Step(0.f);
			TestTrue(TEXT("applied"), Grid->IsTileOccupied(FGridCoord(5, 5)));
			TestEqual(TEXT("queue drained"), Grid->NumPendingPlacements(), 0);
		});

		It("applies queued placements FIFO — the first to claim a tile wins", [this]()
		{
			Grid->QueuePlacement(FGridCoord(3, 3), MakeContent(EPlaceableType::Road, MakeDef(EPlaceableType::Road, FIntPoint(1, 1), EDomain::None), EPlaceableDirection::None));
			Grid->QueuePlacement(FGridCoord(3, 3), MakeContent(EPlaceableType::Building, MakeDef(EPlaceableType::Building, FIntPoint(1, 1), EDomain::None), EPlaceableDirection::North));

			Grid->Step(0.f);
			TestTrue(TEXT("road won the tile"), Grid->GetContentAt(FGridCoord(3, 3)).Type == EPlaceableType::Road);
		});

		It("charges funds when the placement is processed, not when queued", [this]()
		{
			PlayerFunds = TStrongObjectPtr<UGamePlayerFundsSubsystem>(NewObject<UGamePlayerFundsSubsystem>());
			PlayerFunds->Deposit(100.f);
			PlayerFunds->CommitDeposits();
			Grid->SetFunds(PlayerFunds.Get());

			Grid->QueuePlacement(FGridCoord(5, 5), MakeContent(EPlaceableType::Building, MakeCostedDef(30.f), EPlaceableDirection::North));
			TestEqual(TEXT("not charged while queued"), PlayerFunds->GetBalance(), 100.f);

			Grid->Step(0.f);
			TestEqual(TEXT("charged at apply"), PlayerFunds->GetBalance(), 70.f);
			PlayerFunds.Reset();
		});
	});

	Describe("Per-instance slider values", [this]()
	{
		It("seeds instance values at each slider's range max on placement", [this]()
		{
			PlaceBuilding(FGridCoord(5, 5), EPlaceableDirection::North, MakeProducerDef(EDomain::Energy));
			const FGridContent Content = Grid->GetContentAt(FGridCoord(5, 5));
			TestEqual(TEXT("one value per slider"), Content.SliderValues.Num(), 1);
			TestEqual(TEXT("seeded at the range max"), Content.SliderValues[0], 1.f);
		});

		It("keeps a caller-supplied full set of values", [this]()
		{
			FGridContent Content = MakeContent(EPlaceableType::Building, MakeProducerDef(EDomain::Energy), EPlaceableDirection::North);
			Content.SliderValues = { 0.25f };
			TestTrue(TEXT("placed"), Grid->SetContent(FGridCoord(5, 5), Content));
			TestEqual(TEXT("supplied value kept"), Grid->GetContentAt(FGridCoord(5, 5)).SliderValues[0], 0.25f);
		});

		It("SetSliderValue writes the instance value, clamped to the slider's range", [this]()
		{
			PlaceBuilding(FGridCoord(5, 5), EPlaceableDirection::North, MakeProducerDef(EDomain::Energy));

			TestTrue(TEXT("written"), Grid->SetSliderValue(FGridCoord(5, 5), 0, 0.9f));
			TestEqual(TEXT("value"), Grid->GetContentAt(FGridCoord(5, 5)).SliderValues[0], 0.9f);

			TestTrue(TEXT("written (out of range)"), Grid->SetSliderValue(FGridCoord(5, 5), 0, 5.f));
			TestEqual(TEXT("clamped to range max"), Grid->GetContentAt(FGridCoord(5, 5)).SliderValues[0], 1.f);
		});

		It("rejects a slider write to an empty tile or an invalid index", [this]()
		{
			TestFalse(TEXT("empty tile"), Grid->SetSliderValue(FGridCoord(40, 40), 0, 0.5f));

			PlaceBuilding(FGridCoord(5, 5)); // def with no sliders
			TestFalse(TEXT("no such slider"), Grid->SetSliderValue(FGridCoord(5, 5), 0, 0.5f));
		});
	});

	Describe("Placement — affordability (player funds)", [this]()
	{
		BeforeEach([this]()
		{
			PlayerFunds = TStrongObjectPtr<UGamePlayerFundsSubsystem>(NewObject<UGamePlayerFundsSubsystem>());
			PlayerFunds->Deposit(100.f);
			PlayerFunds->CommitDeposits(); // deposits buffer until end-of-step
			Grid->SetFunds(PlayerFunds.Get());
		});
		AfterEach([this]() { PlayerFunds.Reset(); });

		It("charges the item's cost on successful placement", [this]()
		{
			TestTrue(TEXT("placed"), PlaceBuilding(FGridCoord(5, 5), EPlaceableDirection::North, MakeCostedDef(30.f)));
			TestEqual(TEXT("balance charged"), PlayerFunds->GetBalance(), 70.f);
		});

		It("rejects an item the player cannot afford, leaving the tile free and the balance untouched", [this]()
		{
			TestFalse(TEXT("rejected"), PlaceBuilding(FGridCoord(5, 5), EPlaceableDirection::North, MakeCostedDef(150.f)));
			TestFalse(TEXT("tile free"), Grid->IsTileOccupied(FGridCoord(5, 5)));
			TestEqual(TEXT("balance untouched"), PlayerFunds->GetBalance(), 100.f);
		});

		It("allows spending the balance down to exactly zero", [this]()
		{
			TestTrue(TEXT("placed"), PlaceBuilding(FGridCoord(5, 5), EPlaceableDirection::North, MakeCostedDef(100.f)));
			TestEqual(TEXT("balance empty"), PlayerFunds->GetBalance(), 0.f);
		});

		It("does not charge when placement fails for another reason (occupied tile)", [this]()
		{
			TestTrue(TEXT("first"), PlaceBuilding(FGridCoord(5, 5)));
			TestFalse(TEXT("overlap rejected"), PlaceBuilding(FGridCoord(5, 5), EPlaceableDirection::North, MakeCostedDef(30.f)));
			TestEqual(TEXT("balance untouched"), PlayerFunds->GetBalance(), 100.f);
		});

		It("places zero-cost items without touching the balance", [this]()
		{
			TestTrue(TEXT("placed"), PlaceBuilding(FGridCoord(5, 5)));
			TestEqual(TEXT("balance untouched"), PlayerFunds->GetBalance(), 100.f);
		});

		It("charges connectors (roads, utilities) too", [this]()
		{
			UPlaceableDef* RoadDef = MakeDef(EPlaceableType::Road, FIntPoint(1, 1), EDomain::None);
			RoadDef->Cost = 10.f;
			TestTrue(TEXT("road placed"), Grid->SetContent(FGridCoord(3, 3), MakeContent(EPlaceableType::Road, RoadDef, EPlaceableDirection::None)));
			TestEqual(TEXT("balance charged"), PlayerFunds->GetBalance(), 90.f);
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
			PlaceBuilding(FGridCoord(5, 7), EPlaceableDirection::South); // faces the road
			PlaceBuilding(FGridCoord(9, 7), EPlaceableDirection::South);
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
			PlaceBuilding(FGridCoord(10, 12), EPlaceableDirection::South);
			PlaceBuilding(FGridCoord(12, 12), EPlaceableDirection::South);
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
			// Both face away from the utility — utility ignores facing.
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
			PlaceRoad(FGridCoord(10, 10));
			PlaceBuilding(FGridCoord(10, 12), EPlaceableDirection::South); // X -> road
			PlaceBuilding(FGridCoord(10, 8), EPlaceableDirection::North, MakeProducerDef(EDomain::Energy)); // B -> road

			PlaceUtility(FGridCoord(10, 6), EDomain::Energy);
			PlaceBuilding(FGridCoord(10, 4), EPlaceableDirection::North); // Y -> utility

			// X --road-- B --utility-- Y  => one island.
			TestTrue(TEXT("bridged"), SameIsland(FGridCoord(10, 12), FGridCoord(10, 4)));
		});
	});
}

#endif // WITH_AUTOMATION_TESTS
