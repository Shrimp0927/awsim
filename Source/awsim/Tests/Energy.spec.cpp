#include "Misc/AutomationTest.h"
#include "Simulation/GameGridSubsystem.h"
#include "Simulation/GameEnergySubsystem.h"
#include "Simulation/GamePlayerFundsSubsystem.h"
#include "Entities/GridContent.h"
#include "UObject/StrongObjectPtr.h"

// Live spec for the Energy domain phase: global capacity/consumption sums,
// island-scoped service, maintenance, and the daily money settlement. Driven
// world-less via SetGrid/SetFunds injection. Effects use AmountAtMin ==
// AmountAtMax so slider position never muddies the arithmetic.

#if WITH_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FEnergySpec, "awsim.Simulation.Energy",
	EAutomationTestFlags::ProductFilter | EAutomationTestFlags_ApplicationContextMask)

	TStrongObjectPtr<UGridSubsystem> Grid;
	TStrongObjectPtr<UEnergySubsystem> Energy;
	TStrongObjectPtr<UGamePlayerFundsSubsystem> PlayerFunds;

	UPlaceableDef* MakeBuildingDef(const TArray<TPair<EDomain, float>>& Effects, float DailyMaintenance = 0.f)
	{
		UPlaceableDef* Def = NewObject<UPlaceableDef>();
		Def->Type = EPlaceableType::Building;
		Def->Dimensions = FIntPoint(1, 1);
		Def->DailyMaintenanceCost = DailyMaintenance;
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

	void Place(FGridCoord At, UPlaceableDef* Def)
	{
		FGridContent Content;
		Content.Type = EPlaceableType::Building;
		Content.Facing = EPlaceableDirection::North;
		Content.Definition = Def;
		Grid->SetContent(At, Content);
	}

	int32 IslandIndexOf(FGridCoord At)
	{
		const TArray<TArray<FGridCoord>>& Islands = Grid->GetIslands();
		for (int32 i = 0; i < Islands.Num(); ++i)
		{
			if (Islands[i].Contains(At)) return i;
		}
		return INDEX_NONE;
	}

END_DEFINE_SPEC(FEnergySpec)

void FEnergySpec::Define()
{
	BeforeEach([this]()
	{
		Grid = TStrongObjectPtr<UGridSubsystem>(NewObject<UGridSubsystem>());
		Energy = TStrongObjectPtr<UEnergySubsystem>(NewObject<UEnergySubsystem>());
		Energy->SetGrid(Grid.Get());
	});
	AfterEach([this]() { PlayerFunds.Reset(); Energy.Reset(); Grid.Reset(); });

	Describe("Starting state", [this]()
	{
		It("starts with energy capacity 0", [this]()
		{
			Energy->Step(0.f);
			TestEqual(TEXT("capacity"), Energy->GetCapacity(), 0.f);
			TestEqual(TEXT("consumption"), Energy->GetConsumption(), 0.f);
		});
	});

	Describe("Capacity — counts regardless of connectivity", [this]()
	{
		It("raises total capacity for any energy building, even one alone on its own island", [this]()
		{
			Place(FGridCoord(5, 5), MakeBuildingDef({{EDomain::Energy, 100.f}}));
			Energy->Step(0.f);
			TestEqual(TEXT("capacity is the global raw sum"), Energy->GetCapacity(), 100.f);
		});

		It("changes total capacity by the building's signed energy contribution", [this]()
		{
			Place(FGridCoord(5, 5), MakeBuildingDef({{EDomain::Energy, 100.f}}));
			Place(FGridCoord(6, 5), MakeBuildingDef({{EDomain::Energy, -30.f}}));
			Energy->Step(0.f);
			TestEqual(TEXT("positive -> capacity"), Energy->GetCapacity(), 100.f);
			TestEqual(TEXT("negative -> consumption (absolute)"), Energy->GetConsumption(), 30.f);
		});
	});

	Describe("Service is island-scoped — a plant only serves its own island", [this]()
	{
		It("meets demand for consumers in the SAME island when local supply >= local demand", [this]()
		{
			Place(FGridCoord(5, 5), MakeBuildingDef({{EDomain::Energy, 100.f}}));
			Place(FGridCoord(6, 5), MakeBuildingDef({{EDomain::Energy, -30.f}}));
			Energy->Step(0.f);
			TestTrue(TEXT("island serviced"), Energy->IsIslandServiced(IslandIndexOf(FGridCoord(5, 5))));
		});

		It("does NOT power consumers in a DIFFERENT island from the plant", [this]()
		{
			Place(FGridCoord(5, 5), MakeBuildingDef({{EDomain::Energy, 100.f}}));
			Place(FGridCoord(50, 50), MakeBuildingDef({{EDomain::Energy, -30.f}})); // far: own island
			Energy->Step(0.f);
			TestTrue(TEXT("plant island serviced"), Energy->IsIslandServiced(IslandIndexOf(FGridCoord(5, 5))));
			TestFalse(TEXT("consumer island dark"), Energy->IsIslandServiced(IslandIndexOf(FGridCoord(50, 50))));
		});

		It("leaves an island under-supplied even when another island has surplus", [this]()
		{
			Place(FGridCoord(5, 5), MakeBuildingDef({{EDomain::Energy, 100.f}}));
			Place(FGridCoord(50, 50), MakeBuildingDef({{EDomain::Energy, -30.f}}));
			Energy->Step(0.f);
			TestTrue(TEXT("global capacity exceeds global demand"), Energy->GetCapacity() > Energy->GetConsumption());
			TestFalse(TEXT("island still dark — supply doesn't cross islands"), Energy->IsIslandServiced(IslandIndexOf(FGridCoord(50, 50))));
		});
	});

	Describe("Money — maintenance and revenue", [this]()
	{
		It("sums DailyMaintenanceCost over buildings that touch the energy domain only", [this]()
		{
			Place(FGridCoord(5, 5), MakeBuildingDef({{EDomain::Energy, -30.f}}, 7.f));
			Place(FGridCoord(6, 5), MakeBuildingDef({{EDomain::Housing, 10.f}}, 99.f)); // no energy effect
			Energy->Step(0.f);
			TestEqual(TEXT("only the energy building pays here"), Energy->GetMaintenanceCost(), 7.f);
		});

		It("computes revenue from consumption (placeholder: consumption * 10)", [this]()
		{
			Place(FGridCoord(5, 5), MakeBuildingDef({{EDomain::Energy, -30.f}}));
			Energy->Step(0.f);
			TestEqual(TEXT("revenue"), Energy->GetRevenue(), 300.f);
		});

		It("SettleDay forces maintenance out immediately and queues revenue for end of step", [this]()
		{
			PlayerFunds = TStrongObjectPtr<UGamePlayerFundsSubsystem>(NewObject<UGamePlayerFundsSubsystem>());
			Energy->SetFunds(PlayerFunds.Get());
			Place(FGridCoord(5, 5), MakeBuildingDef({{EDomain::Energy, -30.f}}, 7.f));
			Energy->Step(0.f);

			Energy->SettleDay();
			TestEqual(TEXT("maintenance spent (override -> can go negative)"), PlayerFunds->GetBalance(), -7.f);
			TestEqual(TEXT("revenue buffered"), PlayerFunds->GetPendingDeposits(), 300.f);

			PlayerFunds->CommitDeposits();
			TestEqual(TEXT("revenue landed"), PlayerFunds->GetBalance(), 293.f);
		});
	});
}

#endif // WITH_AUTOMATION_TESTS
