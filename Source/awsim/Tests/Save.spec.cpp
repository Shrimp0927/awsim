#include "Misc/AutomationTest.h"
#include "Core/GameSaveSubsystem.h"
#include "Simulation/GameGridSubsystem.h"
#include "Simulation/GamePlayerFundsSubsystem.h"
#include "Entities/GridContent.h"
#include "HAL/FileManager.h"
#include "UObject/StrongObjectPtr.h"

// Spec for save/load: authoritative grid content and stats round-trip through
// disk, derived state rebuilds, and saves respect the empty-queue contract.

#if WITH_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FSaveSpec, "awsim.Simulation.Save",
	EAutomationTestFlags::ProductFilter | EAutomationTestFlags_ApplicationContextMask)

	TStrongObjectPtr<UGridSubsystem> Grid;
	TStrongObjectPtr<UGamePlayerFundsSubsystem> PlayerFunds;
	TStrongObjectPtr<UGameSaveSubsystem> Save;
	FString Slot;

	UPlaceableDef* MakeHomeDef()
	{
		UPlaceableDef* Def = NewObject<UPlaceableDef>();
		Def->Type = EPlaceableType::Building;
		Def->Dimensions = FIntPoint(2, 2);
		Def->HeightTiles = 3.f;
		Def->Cost = 100.f;
		FDomainEffect Effect;
		Effect.Domain = EDomain::Housing;
		Effect.AmountAtMin = 0.25f;
		Effect.AmountAtMax = 0.5f;
		FSliderDef Slider;
		Slider.Effects.Add(Effect);
		Def->Sliders.Add(Slider);
		return Def;
	}

	bool PlaceBuilding(FGridCoord At, UPlaceableDef* Def)
	{
		FGridContent Content;
		Content.Type = EPlaceableType::Building;
		Content.Facing = EPlaceableDirection::North;
		Content.Definition = Def;
		return Grid->SetContent(At, Content);
	}

	bool PlaceRoad(FGridCoord At)
	{
		UPlaceableDef* Def = NewObject<UPlaceableDef>();
		Def->Type = EPlaceableType::Road;
		FGridContent Content;
		Content.Type = EPlaceableType::Road;
		Content.Definition = Def;
		return Grid->SetContent(At, Content);
	}

	void StepGrid(int32 Steps)
	{
		for (int32 i = 0; i < Steps; ++i) Grid->Step(1.f / 30.f);
	}

END_DEFINE_SPEC(FSaveSpec)

void FSaveSpec::Define()
{
	BeforeEach([this]()
	{
		Grid = TStrongObjectPtr<UGridSubsystem>(NewObject<UGridSubsystem>());
		PlayerFunds = TStrongObjectPtr<UGamePlayerFundsSubsystem>(NewObject<UGamePlayerFundsSubsystem>());
		Save = TStrongObjectPtr<UGameSaveSubsystem>(NewObject<UGameSaveSubsystem>());
		Grid->SetFunds(PlayerFunds.Get());
		Save->SetGrid(Grid.Get());
		Save->SetFunds(PlayerFunds.Get());
		Slot = TEXT("SpecSave");
	});

	AfterEach([this]()
	{
		IFileManager::Get().Delete(*UGameSaveSubsystem::SlotToPath(Slot), /*RequireExists*/ false);
		Save.Reset();
		PlayerFunds.Reset();
		Grid.Reset();
	});

	It("round-trips grid content, growth state, and funds", [this]()
	{
		PlayerFunds->Deposit(1000.f);
		PlayerFunds->CommitDeposits();
		TestTrue(TEXT("placed"), PlaceBuilding(FGridCoord(5, 5), MakeHomeDef()));
		TestTrue(TEXT("road placed"), PlaceRoad(FGridCoord(10, 10)));
		Grid->SetSliderValue(FGridCoord(5, 5), 0, 0.4f);
		StepGrid(UGridSubsystem::StepsPerFloor + 5); // one grown floor, clock at 6

		TestTrue(TEXT("saved"), Save->SaveNow(Slot));

		// Wreck the live state so load has to do real work.
		Grid->SetContent(FGridCoord(5, 5), FGridContent());
		Grid->SetContent(FGridCoord(10, 10), FGridContent());
		PlayerFunds->RestoreBalance(0.f);

		TestTrue(TEXT("loaded"), Save->LoadNow(Slot));

		TestEqual(TEXT("one building"), Grid->GetBuildings().Num(), 1);
		const FPlacedBuilding* Building = Grid->FindBuildingAt(FGridCoord(6, 6)); // reverse index rebuilt
		TestNotNull(TEXT("footprint indexed"), Building);
		if (Building)
		{
			TestEqual(TEXT("origin"), Building->Origin, FGridCoord(5, 5));
			TestEqual(TEXT("def dims"), Building->Content.Definition->Dimensions, FIntPoint(2, 2));
			TestEqual(TEXT("slider value"), Building->Content.SliderValues[0], 0.4f);
		}
		TestEqual(TEXT("grown height restored"), Grid->GetHeightAt(FGridCoord(5, 5)), 2.f);
		TestEqual(TEXT("growth clock restored"), static_cast<int32>(Grid->GetLifetime(FGridCoord(5, 5))), 6);
		TestTrue(TEXT("road restored"), Grid->GetContentAt(FGridCoord(10, 10)).Type == EPlaceableType::Road);
		TestEqual(TEXT("balance restored"), PlayerFunds->GetBalance(), 900.f);
		TestEqual(TEXT("islands rebuilt"), Grid->GetIslands().Num(), 1);
	});

	It("refuses SaveNow while intents are queued", [this]()
	{
		Grid->QueuePlacement(FGridCoord(5, 5), FGridContent());
		TestFalse(TEXT("refused"), Save->SaveNow(Slot));
		Grid->Step(1.f / 30.f); // queue drains
		TestTrue(TEXT("accepted after drain"), Save->SaveNow(Slot));
	});

	It("RequestSave fires only on a completed step with drained queues", [this]()
	{
		Save->RequestSave(Slot);
		PlayerFunds->Deposit(100.f);
		PlayerFunds->CommitDeposits();
		Grid->QueuePlacement(FGridCoord(5, 5), FGridContent()); // pending intent
		Save->NotifyStepCompleted();
		TestTrue(TEXT("still armed"), Save->HasPendingSave());
		TestFalse(TEXT("no file yet"), IFileManager::Get().FileExists(*UGameSaveSubsystem::SlotToPath(Slot)));

		Grid->Step(1.f / 30.f); // N + 1: the queue drains
		Save->NotifyStepCompleted();
		TestFalse(TEXT("request consumed"), Save->HasPendingSave());
		TestTrue(TEXT("file written"), IFileManager::Get().FileExists(*UGameSaveSubsystem::SlotToPath(Slot)));
	});
}

#endif // WITH_AUTOMATION_TESTS
