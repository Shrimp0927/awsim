#include "Misc/AutomationTest.h"
#include "Simulation/GameGridSubsystem.h"
#include "Simulation/GameEnergySubsystem.h"
#include "Simulation/GameWaterSubsystem.h"
#include "Simulation/GameHousingSubsystem.h"
#include "Simulation/GameEconomySubsystem.h"
#include "Simulation/GamePopulationSubsystem.h"
#include "Entities/GridContent.h"
#include "UObject/StrongObjectPtr.h"

// Spec for the Population domain phase: the count grows toward serviced
// housing capacity (islands with both energy and water), boosted by the economy.

#if WITH_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FPopulationSpec, "awsim.Simulation.Population",
	EAutomationTestFlags::ProductFilter | EAutomationTestFlags_ApplicationContextMask)

	TStrongObjectPtr<UGridSubsystem> Grid;
	TStrongObjectPtr<UEnergySubsystem> Energy;
	TStrongObjectPtr<UGameWaterSubsystem> Water;
	TStrongObjectPtr<UHousingSubsystem> Housing;
	TStrongObjectPtr<UEconomySubsystem> Economy;
	TStrongObjectPtr<UPopulationSubsystem> Population;

	UPlaceableDef* MakeBuildingDef(const TArray<TPair<EDomain, float>>& Effects)
	{
		UPlaceableDef* Def = NewObject<UPlaceableDef>();
		Def->Type = EPlaceableType::Building;
		Def->Dimensions = FIntPoint(1, 1);
		FSliderDef Slider;
		for (const TPair<EDomain, float>& E : Effects)
		{
			FDomainEffect Effect;
			Effect.Domain = E.Key;
			Effect.AmountAtMin = E.Value;
			Effect.AmountAtMax = E.Value;
			Slider.Effects.Add(Effect);
		}
		Def->Sliders.Add(Slider);
		return Def;
	}

	UPlaceableDef* HomeDef(float HousingAmount) { return MakeBuildingDef({{EDomain::Housing, HousingAmount}, {EDomain::Energy, -5.f}, {EDomain::Water, -5.f}}); }
	UPlaceableDef* PowerDef()      { return MakeBuildingDef({{EDomain::Energy, 100.f}}); }
	UPlaceableDef* WaterPlantDef() { return MakeBuildingDef({{EDomain::Water, 100.f}}); }

	void Place(FGridCoord At, UPlaceableDef* Def)
	{
		FGridContent Content;
		Content.Type = EPlaceableType::Building;
		Content.Facing = EPlaceableDirection::North;
		Content.Definition = Def;
		Grid->SetContent(At, Content);
	}

	// A fully serviced island: one home + both plants, clustered at Origin.
	void PlaceServicedCluster(FGridCoord Origin, float HousingAmount)
	{
		Place(Origin, HomeDef(HousingAmount));
		Place(FGridCoord(Origin.X + 1, Origin.Y), PowerDef());
		Place(FGridCoord(Origin.X + 2, Origin.Y), WaterPlantDef());
	}

	void StepAll(float Dt)
	{
		Grid->Step(Dt);
		Energy->Step(Dt);
		Water->Step(Dt);
		Housing->Step(Dt);
		Economy->Step(Dt);
		Population->Step(Dt);
	}

END_DEFINE_SPEC(FPopulationSpec)

void FPopulationSpec::Define()
{
	BeforeEach([this]()
	{
		Grid = TStrongObjectPtr<UGridSubsystem>(NewObject<UGridSubsystem>());
		Energy = TStrongObjectPtr<UEnergySubsystem>(NewObject<UEnergySubsystem>());
		Water = TStrongObjectPtr<UGameWaterSubsystem>(NewObject<UGameWaterSubsystem>());
		Housing = TStrongObjectPtr<UHousingSubsystem>(NewObject<UHousingSubsystem>());
		Economy = TStrongObjectPtr<UEconomySubsystem>(NewObject<UEconomySubsystem>());
		Population = TStrongObjectPtr<UPopulationSubsystem>(NewObject<UPopulationSubsystem>());
		Energy->SetGrid(Grid.Get());
		Water->SetGrid(Grid.Get());
		Housing->SetGrid(Grid.Get());
		Housing->SetEnergy(Energy.Get());
		Housing->SetWater(Water.Get());
		Economy->SetGrid(Grid.Get());
		Economy->SetEnergy(Energy.Get());
		Population->SetHousing(Housing.Get());
		Population->SetEconomy(Economy.Get());
	});
	AfterEach([this]()
	{
		Population.Reset(); Economy.Reset(); Housing.Reset();
		Water.Reset(); Energy.Reset(); Grid.Reset();
	});

	Describe("Starting state", [this]()
	{
		It("starts empty — a new city has zero population", [this]()
		{
			TestEqual(TEXT("empty start"), Population->GetCount(), 0);
		});
	});

	Describe("Capacity — bounded by serviced housing", [this]()
	{
		It("never exceeds total housing capacity", [this]()
		{
			PlaceServicedCluster(FGridCoord(5, 5), 200.f);
			for (int32 i = 0; i < 200; ++i) StepAll(1.f);
			TestTrue(TEXT("approached capacity"), Population->GetCount() > 190);
			TestTrue(TEXT("never exceeded it"), Population->GetCount() <= 200);
		});

		It("counts a home toward capacity only when its island has energy AND water", [this]()
		{
			PlaceServicedCluster(FGridCoord(5, 5), 200.f);
			StepAll(1.f);
			TestTrue(TEXT("grows toward the serviced home"), Population->GetCount() > 0);
		});

		It("excludes housing on an island missing energy", [this]()
		{
			Population->SetCount(100.f); // established residents
			Place(FGridCoord(5, 5), HomeDef(200.f));
			Place(FGridCoord(6, 5), WaterPlantDef()); // water only
			StepAll(1.f);
			TestTrue(TEXT("unserviced home holds nobody — declines"), Population->GetCount() < 100);
		});

		It("excludes housing on an island missing water", [this]()
		{
			Population->SetCount(100.f); // established residents
			Place(FGridCoord(5, 5), HomeDef(200.f));
			Place(FGridCoord(6, 5), PowerDef()); // power only
			StepAll(1.f);
			TestTrue(TEXT("unserviced home holds nobody — declines"), Population->GetCount() < 100);
		});

		It("treats two serviced islands' housing as additive capacity", [this]()
		{
			PlaceServicedCluster(FGridCoord(5, 5), 200.f);
			PlaceServicedCluster(FGridCoord(50, 50), 200.f);
			for (int32 i = 0; i < 200; ++i) StepAll(1.f);
			TestTrue(TEXT("approaches the sum of both islands"), Population->GetCount() > 350);
		});
	});

	Describe("Growth dynamics — moves toward effective capacity over time", [this]()
	{
		It("grows toward effective capacity rather than jumping instantly", [this]()
		{
			PlaceServicedCluster(FGridCoord(5, 5), 200.f);
			StepAll(1.f);
			TestTrue(TEXT("moved up"), Population->GetCount() > 0);
			TestTrue(TEXT("only PART-way"), Population->GetCount() < 200);
		});

		It("declines when effective capacity drops below the current count", [this]()
		{
			PlaceServicedCluster(FGridCoord(5, 5), 200.f);
			for (int32 i = 0; i < 10; ++i) StepAll(1.f);
			const int32 Before = Population->GetCount();
			TestTrue(TEXT("grew first"), Before > 0);

			// Blackout: remove the power plant.
			Grid->SetContent(FGridCoord(6, 5), FGridContent());
			StepAll(1.f);
			TestTrue(TEXT("trending down"), Population->GetCount() < Before);
		});

		It("holds steady when count already equals effective capacity", [this]()
		{
			Population->SetCount(100.f);
			PlaceServicedCluster(FGridCoord(5, 5), 100.f); // capacity == count
			StepAll(1.f);
			TestEqual(TEXT("steady"), Population->GetCount(), 100);
		});

		It("trends toward zero when there is no serviced housing", [this]()
		{
			Population->SetCount(100.f); // residents with nothing to live in
			for (int32 i = 0; i < 60; ++i) StepAll(1.f);
			TestTrue(TEXT("nearly empty"), Population->GetCount() < 5);
		});

		It("stays non-negative", [this]()
		{
			Population->SetCount(100.f);
			for (int32 i = 0; i < 300; ++i) StepAll(1.f);
			TestTrue(TEXT("never below zero"), Population->GetCount() >= 0);
		});
	});

	Describe("Economy influence", [this]()
	{
		It("grows faster when the economy is producing", [this]()
		{
			PlaceServicedCluster(FGridCoord(5, 5), 200.f);
			StepAll(1.f);
			const float C1 = static_cast<float>(Population->GetCount());
			const float FractionNoGDP = C1 / 200.f;

			// A big powered business maxes the GDP growth boost.
			Place(FGridCoord(6, 6), MakeBuildingDef({{EDomain::Economy, 2000.f}}));
			StepAll(1.f);
			const float C2 = static_cast<float>(Population->GetCount());
			const float FractionWithGDP = (C2 - C1) / (200.f - C1);

			TestTrue(TEXT("gap closes faster with GDP"), FractionWithGDP > FractionNoGDP * 1.5f);
		});
	});

	Describe("Deterministic pipeline", [this]()
	{
		It("reads capacity produced by phases that run earlier in the same tick (fixed order)", [this]()
		{
			TestTrue(TEXT("energy before housing"), Energy->PhaseOrder() < Housing->PhaseOrder());
			TestTrue(TEXT("water before housing"), Water->PhaseOrder() < Housing->PhaseOrder());
			TestTrue(TEXT("housing before economy"), Housing->PhaseOrder() < Economy->PhaseOrder());
			TestTrue(TEXT("economy before population"), Economy->PhaseOrder() < Population->PhaseOrder());
		});
	});

	Describe("Feeds downstream consumers", [this]()
	{
		It("exposes an integer count for CityStats to aggregate and the agent crowd to scale from", [this]()
		{
			PlaceServicedCluster(FGridCoord(5, 5), 200.f);
			StepAll(1.f);
			TestTrue(TEXT("count is exposed and sane"), Population->GetCount() > 0);
		});
	});
}

#endif // WITH_AUTOMATION_TESTS
