#include "Misc/AutomationTest.h"
#include "Simulation/GameGridSubsystem.h"
#include "Simulation/GameEnergySubsystem.h"
#include "Simulation/GameWaterSubsystem.h"
#include "Simulation/GameHousingSubsystem.h"
#include "Simulation/GamePlayerFundsSubsystem.h"
#include "Entities/GridContent.h"
#include "UObject/StrongObjectPtr.h"

// Spec for the Housing domain phase: raw capacity counts everywhere; only
// islands serviced by both energy and water count as serviced and pay tax.

#if WITH_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FHousingSpec, "awsim.Simulation.Housing",
	EAutomationTestFlags::ProductFilter | EAutomationTestFlags_ApplicationContextMask)

	TStrongObjectPtr<UGridSubsystem> Grid;
	TStrongObjectPtr<UEnergySubsystem> Energy;
	TStrongObjectPtr<UGameWaterSubsystem> Water;
	TStrongObjectPtr<UHousingSubsystem> Housing;
	TStrongObjectPtr<UGamePlayerFundsSubsystem> PlayerFunds;

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

	// A home that needs both utilities; plants that supply them.
	UPlaceableDef* HomeDef()       { return MakeBuildingDef({{EDomain::Housing, 10.f}, {EDomain::Energy, -5.f}, {EDomain::Water, -5.f}}); }
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

	void StepAll()
	{
		Grid->Step(0.f);
		Energy->Step(0.f);
		Water->Step(0.f);
		Housing->Step(0.f);
	}

END_DEFINE_SPEC(FHousingSpec)

void FHousingSpec::Define()
{
	BeforeEach([this]()
	{
		Grid = TStrongObjectPtr<UGridSubsystem>(NewObject<UGridSubsystem>());
		Energy = TStrongObjectPtr<UEnergySubsystem>(NewObject<UEnergySubsystem>());
		Water = TStrongObjectPtr<UGameWaterSubsystem>(NewObject<UGameWaterSubsystem>());
		Housing = TStrongObjectPtr<UHousingSubsystem>(NewObject<UHousingSubsystem>());
		Energy->SetGrid(Grid.Get());
		Water->SetGrid(Grid.Get());
		Housing->SetGrid(Grid.Get());
		Housing->SetEnergy(Energy.Get());
		Housing->SetWater(Water.Get());
	});
	AfterEach([this]() { PlayerFunds.Reset(); Housing.Reset(); Water.Reset(); Energy.Reset(); Grid.Reset(); });

	Describe("Starting state", [this]()
	{
		It("starts with housing capacity 0", [this]()
		{
			StepAll();
			TestEqual(TEXT("capacity"), Housing->GetCapacity(), 0.f);
			TestEqual(TEXT("serviced"), Housing->GetServicedCapacity(), 0.f);
		});
	});

	Describe("Building contributions", [this]()
	{
		It("raises housing capacity for a building with positive housing", [this]()
		{
			Place(FGridCoord(5, 5), HomeDef());
			StepAll();
			TestEqual(TEXT("raw capacity counts regardless of service"), Housing->GetCapacity(), 10.f);
		});

		It("lowers housing capacity for a building with negative housing", [this]()
		{
			Place(FGridCoord(5, 5), HomeDef());
			Place(FGridCoord(6, 5), MakeBuildingDef({{EDomain::Housing, -4.f}}));
			StepAll();
			TestEqual(TEXT("signed sum"), Housing->GetCapacity(), 6.f);
		});
	});

	Describe("Island servicing — a home only functions with energy AND water", [this]()
	{
		It("counts a home's capacity when its island has both energy and water", [this]()
		{
			Place(FGridCoord(5, 5), HomeDef());
			Place(FGridCoord(6, 5), PowerDef());
			Place(FGridCoord(7, 5), WaterPlantDef());
			StepAll();
			TestEqual(TEXT("serviced capacity"), Housing->GetServicedCapacity(), 10.f);
		});

		It("excludes a home on an island missing water", [this]()
		{
			Place(FGridCoord(5, 5), HomeDef());
			Place(FGridCoord(6, 5), PowerDef()); // power, no water
			StepAll();
			TestEqual(TEXT("raw capacity still counts"), Housing->GetCapacity(), 10.f);
			TestEqual(TEXT("serviced capacity does not"), Housing->GetServicedCapacity(), 0.f);
		});

		It("excludes a home on an island missing energy", [this]()
		{
			Place(FGridCoord(5, 5), HomeDef());
			Place(FGridCoord(6, 5), WaterPlantDef()); // water, no power
			StepAll();
			TestEqual(TEXT("serviced capacity"), Housing->GetServicedCapacity(), 0.f);
		});
	});

	Describe("Tax — each serviced home pays a flat daily tax", [this]()
	{
		It("collects tax only from homes on serviced islands", [this]()
		{
			// Serviced island: two homes + both plants.
			Place(FGridCoord(5, 5), HomeDef());
			Place(FGridCoord(6, 5), HomeDef());
			Place(FGridCoord(6, 6), PowerDef());
			Place(FGridCoord(5, 6), WaterPlantDef());
			Place(FGridCoord(50, 50), HomeDef()); // unserviced island: lone home far away
			StepAll();
			TestEqual(TEXT("two serviced homes x 10"), Housing->GetTaxRevenue(), 20.f);
		});

		It("SettleDay queues the tax as a deposit for end of step", [this]()
		{
			PlayerFunds = TStrongObjectPtr<UGamePlayerFundsSubsystem>(NewObject<UGamePlayerFundsSubsystem>());
			Housing->SetFunds(PlayerFunds.Get());
			Place(FGridCoord(5, 5), HomeDef());
			Place(FGridCoord(6, 5), PowerDef());
			Place(FGridCoord(7, 5), WaterPlantDef());
			StepAll();

			Housing->SettleDay();
			TestEqual(TEXT("tax buffered, not landed"), PlayerFunds->GetPendingDeposits(), 10.f);
			TestEqual(TEXT("balance untouched mid-step"), PlayerFunds->GetBalance(), 0.f);

			PlayerFunds->CommitDeposits();
			TestEqual(TEXT("tax landed"), PlayerFunds->GetBalance(), 10.f);
		});
	});
}

#endif // WITH_AUTOMATION_TESTS
