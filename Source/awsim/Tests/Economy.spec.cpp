#include "Misc/AutomationTest.h"
#include "Simulation/GameGridSubsystem.h"
#include "Simulation/GameEnergySubsystem.h"
#include "Simulation/GameEconomySubsystem.h"
#include "Entities/GridContent.h"
#include "UObject/StrongObjectPtr.h"

// Live spec for the Economy domain phase. The economy is pegged to ENERGY
// only: an island without power produces no GDP; water is irrelevant here.
// Tax is intentionally NOT modelled yet — buildings affect the economy at a
// macro level only. Driven world-less via injection.

#if WITH_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FEconomySpec, "awsim.Simulation.Economy",
	EAutomationTestFlags::ProductFilter | EAutomationTestFlags_ApplicationContextMask)

	TStrongObjectPtr<UGridSubsystem> Grid;
	TStrongObjectPtr<UEnergySubsystem> Energy;
	TStrongObjectPtr<UEconomySubsystem> Economy;

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
		Economy->Step(0.f);
	}

END_DEFINE_SPEC(FEconomySpec)

void FEconomySpec::Define()
{
	BeforeEach([this]()
	{
		Grid = TStrongObjectPtr<UGridSubsystem>(NewObject<UGridSubsystem>());
		Energy = TStrongObjectPtr<UEnergySubsystem>(NewObject<UEnergySubsystem>());
		Economy = TStrongObjectPtr<UEconomySubsystem>(NewObject<UEconomySubsystem>());
		Energy->SetGrid(Grid.Get());
		Economy->SetGrid(Grid.Get());
		Economy->SetEnergy(Energy.Get());
	});
	AfterEach([this]() { Economy.Reset(); Energy.Reset(); Grid.Reset(); });

	Describe("Starting state", [this]()
	{
		It("starts with GDP 0", [this]()
		{
			StepAll();
			TestEqual(TEXT("GDP"), Economy->GetGDP(), 0.f);
		});
	});

	Describe("Building contributions", [this]()
	{
		It("raises GDP for a building with positive economy", [this]()
		{
			Place(FGridCoord(5, 5), MakeBuildingDef({{EDomain::Economy, 50.f}})); // no energy demand -> trivially serviced
			StepAll();
			TestEqual(TEXT("GDP"), Economy->GetGDP(), 50.f);
		});

		It("lowers GDP for a building with negative economy", [this]()
		{
			Place(FGridCoord(5, 5), MakeBuildingDef({{EDomain::Economy, 50.f}}));
			Place(FGridCoord(6, 5), MakeBuildingDef({{EDomain::Economy, -20.f}}));
			StepAll();
			TestEqual(TEXT("signed sum"), Economy->GetGDP(), 30.f);
		});
	});

	Describe("Island servicing — a business only produces when its island has energy", [this]()
	{
		It("produces no GDP on an island without power", [this]()
		{
			Place(FGridCoord(5, 5), MakeBuildingDef({{EDomain::Economy, 50.f}, {EDomain::Energy, -5.f}}));
			StepAll();
			TestEqual(TEXT("blacked-out island produces nothing"), Economy->GetGDP(), 0.f);
		});

		It("produces GDP once the island is powered", [this]()
		{
			Place(FGridCoord(5, 5), MakeBuildingDef({{EDomain::Economy, 50.f}, {EDomain::Energy, -5.f}}));
			Place(FGridCoord(6, 5), MakeBuildingDef({{EDomain::Energy, 100.f}}));
			StepAll();
			TestEqual(TEXT("powered island produces"), Economy->GetGDP(), 50.f);
		});

		It("does not require water — energy alone gates the economy", [this]()
		{
			// Business demands water too, but there is no water plant anywhere.
			Place(FGridCoord(5, 5), MakeBuildingDef({{EDomain::Economy, 50.f}, {EDomain::Energy, -5.f}, {EDomain::Water, -5.f}}));
			Place(FGridCoord(6, 5), MakeBuildingDef({{EDomain::Energy, 100.f}}));
			StepAll();
			TestEqual(TEXT("water shortage does not stop GDP"), Economy->GetGDP(), 50.f);
		});
	});
}

#endif // WITH_AUTOMATION_TESTS
