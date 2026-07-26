#include "Misc/AutomationTest.h"
#include "Core/GameFlowSubsystem.h"
#include "Core/GameSaveSubsystem.h"
#include "Simulation/GameGridSubsystem.h"
#include "Simulation/GamePlayerFundsSubsystem.h"
#include "Simulation/GameSimulationSubsystem.h"
#include "Entities/GridContent.h"
#include "HAL/FileManager.h"
#include "UObject/StrongObjectPtr.h"

// Spec for the app flow state machine: boot in the menu with the clock gated,
// transitions drive the sim and save APIs.

#if WITH_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FFlowSpec, "awsim.Simulation.Flow",
	EAutomationTestFlags::ProductFilter | EAutomationTestFlags_ApplicationContextMask)

	TStrongObjectPtr<UGameFlowSubsystem> Flow;
	TStrongObjectPtr<USimulationSubsystem> Sim;
	TStrongObjectPtr<UGridSubsystem> Grid;
	TStrongObjectPtr<UGamePlayerFundsSubsystem> PlayerFunds;
	TStrongObjectPtr<UGameSaveSubsystem> Save;

	bool PlaceBuilding(FGridCoord At)
	{
		UPlaceableDef* Def = NewObject<UPlaceableDef>();
		Def->Type = EPlaceableType::Building;
		FGridContent Content;
		Content.Type = EPlaceableType::Building;
		Content.Facing = EPlaceableDirection::North;
		Content.Definition = Def;
		return Grid->SetContent(At, Content);
	}

END_DEFINE_SPEC(FFlowSpec)

void FFlowSpec::Define()
{
	BeforeEach([this]()
	{
		Flow = TStrongObjectPtr<UGameFlowSubsystem>(NewObject<UGameFlowSubsystem>());
		Sim = TStrongObjectPtr<USimulationSubsystem>(NewObject<USimulationSubsystem>());
		Grid = TStrongObjectPtr<UGridSubsystem>(NewObject<UGridSubsystem>());
		PlayerFunds = TStrongObjectPtr<UGamePlayerFundsSubsystem>(NewObject<UGamePlayerFundsSubsystem>());
		Save = TStrongObjectPtr<UGameSaveSubsystem>(NewObject<UGameSaveSubsystem>());
		Grid->SetFunds(PlayerFunds.Get());
		Save->SetGrid(Grid.Get());
		Save->SetFunds(PlayerFunds.Get());
		Flow->SetSim(Sim.Get());
		Flow->SetGrid(Grid.Get());
		Flow->SetFunds(PlayerFunds.Get());
		Flow->SetSave(Save.Get());
		Flow->SlotPrefix = TEXT("SpecFlowSlot");
	});

	AfterEach([this]()
	{
		for (int32 i = 0; i < UGameFlowSubsystem::NumSaveSlots; ++i)
		{
			IFileManager::Get().Delete(*UGameSaveSubsystem::SlotToPath(Flow->SlotNameFor(i)), /*RequireExists*/ false);
		}
		Save.Reset();
		PlayerFunds.Reset();
		Grid.Reset();
		Sim.Reset();
		Flow.Reset();
	});

	It("boots into the menu with the sim clock gated", [this]()
	{
		TestTrue(TEXT("menu"), Flow->GetState() == EGameFlowState::Menu);
		TestFalse(TEXT("clock gated"), Sim->IsRunning());
	});

	It("Resume before any game starts is a no-op", [this]()
	{
		Flow->Resume();
		TestTrue(TEXT("still menu"), Flow->GetState() == EGameFlowState::Menu);
		TestFalse(TEXT("still gated"), Sim->IsRunning());
		Flow->ToggleMenu();
		TestTrue(TEXT("toggle cannot leave the menu either"), Flow->GetState() == EGameFlowState::Menu);
	});

	It("Resume enters Playing and starts the clock; OpenMenu gates it again", [this]()
	{
		Flow->StartNewGame(); // activates the session
		Flow->OpenMenu();
		Flow->Resume();
		TestTrue(TEXT("playing"), Flow->GetState() == EGameFlowState::Playing);
		TestTrue(TEXT("running"), Sim->IsRunning());

		Flow->OpenMenu();
		TestTrue(TEXT("menu"), Flow->GetState() == EGameFlowState::Menu);
		TestFalse(TEXT("gated"), Sim->IsRunning());

		Flow->ToggleMenu();
		TestTrue(TEXT("toggled back"), Flow->GetState() == EGameFlowState::Playing);
	});

	It("StartNewGame wipes the city, grants starting funds, and plays", [this]()
	{
		PlayerFunds->Deposit(500.f);
		PlayerFunds->CommitDeposits();
		TestTrue(TEXT("placed"), PlaceBuilding(FGridCoord(5, 5)));

		Flow->StartNewGame();
		TestEqual(TEXT("city wiped"), Grid->GetBuildings().Num(), 0);
		TestEqual(TEXT("starting funds"), PlayerFunds->GetBalance(), UGameFlowSubsystem::StartingFunds);
		TestEqual(TEXT("day zero"), Sim->GetDay(), 0);
		TestTrue(TEXT("playing"), Flow->GetState() == EGameFlowState::Playing);
		TestTrue(TEXT("running"), Sim->IsRunning());
	});

	It("SaveGame arms an N + 1 save request; out-of-range slots are ignored", [this]()
	{
		Flow->SaveGame(UGameFlowSubsystem::NumSaveSlots);
		TestFalse(TEXT("bad slot ignored"), Save->HasPendingSave());
		Flow->SaveGame(0);
		TestTrue(TEXT("armed"), Save->HasPendingSave());
	});

	It("LoadGame with no save file stays in the menu", [this]()
	{
		AddExpectedError(TEXT("cannot read"), EAutomationExpectedErrorFlags::Contains, 1);
		TestFalse(TEXT("slot reads empty"), Flow->SlotExists(0));
		Flow->LoadGame(0);
		TestTrue(TEXT("still menu"), Flow->GetState() == EGameFlowState::Menu);
		TestFalse(TEXT("still gated"), Sim->IsRunning());
	});

	It("a saved slot exists and loads back into Playing", [this]()
	{
		Flow->StartNewGame();
		TestTrue(TEXT("written"), Save->SaveNow(Flow->SlotNameFor(2)));
		TestTrue(TEXT("slot exists"), Flow->SlotExists(2));

		Flow->OpenMenu();
		Flow->LoadGame(2);
		TestTrue(TEXT("playing"), Flow->GetState() == EGameFlowState::Playing);
		TestTrue(TEXT("running"), Sim->IsRunning());
	});
}

#endif // WITH_AUTOMATION_TESTS
