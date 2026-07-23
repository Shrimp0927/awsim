#include "Misc/AutomationTest.h"
#include "Simulation/GameSimulationSubsystem.h"
#include "Simulation/GameSimPhase.h"
#include "Simulation/GameGridSubsystem.h"
#include "Simulation/GameEditSubsystem.h"
#include "Simulation/GameEnergySubsystem.h"
#include "Simulation/GamePlayerFundsSubsystem.h"
#include "Entities/GridContent.h"
#include "UObject/StrongObjectPtr.h"

// Spec for the orchestrator's pause contract: paused ticks still pump the
// input band with dt = 0, while domain phases and the day clock stay frozen.

#if WITH_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FSimulationSpec, "awsim.Simulation.Orchestrator",
	EAutomationTestFlags::ProductFilter | EAutomationTestFlags_ApplicationContextMask)

	TStrongObjectPtr<USimulationSubsystem> Sim;
	TStrongObjectPtr<UGridSubsystem> Grid;
	TStrongObjectPtr<UEditSubsystem> Edit;
	TStrongObjectPtr<UEnergySubsystem> Energy;
	TStrongObjectPtr<UGamePlayerFundsSubsystem> Funds;

	UPlaceableDef* MakeDef(EDomain Domain, float Amount, float Cost)
	{
		UPlaceableDef* Def = NewObject<UPlaceableDef>();
		Def->Type = EPlaceableType::Building;
		Def->Dimensions = FIntPoint(1, 1);
		Def->Cost = Cost;
		FSliderDef Slider;
		FDomainEffect Effect;
		Effect.Domain = Domain;
		Effect.AmountAtMin = Amount;
		Effect.AmountAtMax = Amount;
		Slider.Effects.Add(Effect);
		Def->Sliders.Add(Slider);
		return Def;
	}

	void QueueBuilding(FGridCoord At, UPlaceableDef* Def)
	{
		FGridContent Content;
		Content.Type = EPlaceableType::Building;
		Content.Facing = EPlaceableDirection::North;
		Content.Definition = Def;
		Grid->QueuePlacement(At, Content);
	}

END_DEFINE_SPEC(FSimulationSpec)

void FSimulationSpec::Define()
{
	BeforeEach([this]()
	{
		Sim = TStrongObjectPtr<USimulationSubsystem>(NewObject<USimulationSubsystem>());
		Grid = TStrongObjectPtr<UGridSubsystem>(NewObject<UGridSubsystem>());
		Edit = TStrongObjectPtr<UEditSubsystem>(NewObject<UEditSubsystem>());
		Energy = TStrongObjectPtr<UEnergySubsystem>(NewObject<UEnergySubsystem>());
		Funds = TStrongObjectPtr<UGamePlayerFundsSubsystem>(NewObject<UGamePlayerFundsSubsystem>());
		Grid->SetFunds(Funds.Get());
		Edit->SetGrid(Grid.Get());
		Energy->SetGrid(Grid.Get());
		Energy->SetFunds(Funds.Get());
		Sim->SetPhases({ Edit.Get(), Grid.Get(), Energy.Get() });
		Sim->SetPaused(false); // worldless: OnWorldBeginPlay never ran
		Funds->Deposit(1000.f);
		Funds->CommitDeposits();
	});
	AfterEach([this]()
	{
		Funds.Reset(); Energy.Reset(); Edit.Reset(); Grid.Reset(); Sim.Reset();
	});

	Describe("Paused — input band stays live", [this]()
	{
		It("applies a queued placement to the grid and charges funds while paused", [this]()
		{
			Sim->SetPaused(true);
			QueueBuilding(FGridCoord(5, 5), MakeDef(EDomain::Energy, 100.f, 300.f));
			const uint64 RevBefore = Grid->GetContentRevision();

			Sim->Tick(1.f);

			TestEqual(TEXT("placed"), Grid->GetBuildings().Num(), 1);
			TestEqual(TEXT("charged"), Funds->GetBalance(), 700.f);
			TestTrue(TEXT("revision bumped for the renderer"), Grid->GetContentRevision() > RevBefore);
		});

		It("applies a queued slider edit while paused", [this]()
		{
			QueueBuilding(FGridCoord(5, 5), MakeDef(EDomain::Energy, 100.f, 0.f));
			Sim->Tick(1.f / 30.f); // place while running

			Sim->SetPaused(true);
			Edit->QueueSliderEdit(FGridCoord(5, 5), 0, 0.25f); // below the placement default so the edit is observable
			Sim->Tick(1.f);

			TestEqual(TEXT("edit drained"), Edit->NumPendingEdits(), 0);
			TestEqual(TEXT("value applied"), Grid->GetContentAt(FGridCoord(5, 5)).SliderValues[0], 0.25f);
		});
	});

	Describe("Paused — sim clock is gated", [this]()
	{
		It("does not step domain phases while paused; they read the enlarged grid on resume", [this]()
		{
			Sim->SetPaused(true);
			QueueBuilding(FGridCoord(5, 5), MakeDef(EDomain::Energy, 100.f, 0.f));
			Sim->Tick(1.f);

			TestEqual(TEXT("placed while paused"), Grid->GetBuildings().Num(), 1);
			TestEqual(TEXT("energy still frozen"), Energy->GetCapacity(), 0.f);

			Sim->SetPaused(false);
			Sim->Tick(1.f / 30.f);
			TestEqual(TEXT("recomputed on resume"), Energy->GetCapacity(), 100.f);
		});

		It("does not advance the day while paused", [this]()
		{
			Sim->SetPaused(true);
			for (int32 i = 0; i < 500; ++i) Sim->Tick(1.f);
			TestEqual(TEXT("day frozen"), Sim->GetDay(), 0);

			Sim->SetPaused(false);
			for (int32 i = 0; i < 500; ++i) Sim->Tick(1.f);
			TestTrue(TEXT("days roll again once resumed"), Sim->GetDay() > 0);
		});

		It("accrues no time debt during pause — resume does not burst catch-up steps", [this]()
		{
			QueueBuilding(FGridCoord(5, 5), MakeDef(EDomain::Energy, 100.f, 0.f));
			Sim->SetPaused(true);
			for (int32 i = 0; i < 100; ++i) Sim->Tick(1.f); // 100s of paused wall time

			Sim->SetPaused(false);
			Sim->Tick(0.001f); // less than one FixedStep of real time owed
			TestEqual(TEXT("no step ran — pause left no debt"), Energy->GetCapacity(), 0.f);

			Sim->Tick(1.f / 30.f);
			TestEqual(TEXT("first step arrives on time"), Energy->GetCapacity(), 100.f);
		});
	});
}

#endif // WITH_AUTOMATION_TESTS
