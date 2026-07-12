#include "Misc/AutomationTest.h"
#include "Simulation/GameGridSubsystem.h"
#include "Simulation/GameEditSubsystem.h"
#include "Entities/GridContent.h"
#include "UObject/StrongObjectPtr.h"

// Live spec for the player-edit queue: slider edits queue at any time and
// apply FIFO when the edit phase steps (order 150 — before the grid at 200).
// The grid is injected via SetGrid so a plain NewObject'd pair can be driven
// world-less, matching the Grid spec.

#if WITH_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FEditSpec, "awsim.Simulation.Edit",
	EAutomationTestFlags::ProductFilter | EAutomationTestFlags_ApplicationContextMask)

	TStrongObjectPtr<UGridSubsystem> Grid;
	TStrongObjectPtr<UEditSubsystem> Edit;

	// A 1x1 building with one slider (default Value 0.5, Range 0..1) that
	// consumes Energy — the shape the domain subsystems read.
	UPlaceableDef* MakeSliderDef()
	{
		UPlaceableDef* Def = NewObject<UPlaceableDef>();
		Def->Type = EPlaceableType::Building;
		Def->Dimensions = FIntPoint(1, 1);
		FDomainEffect Effect;
		Effect.Domain = EDomain::Energy;
		Effect.AmountAtMin = -2.f;
		Effect.AmountAtMax = -10.f;
		FSliderDef Slider;
		Slider.Effects.Add(Effect);
		Def->Sliders.Add(Slider);
		return Def;
	}

	void PlaceSliderBuilding(FGridCoord At)
	{
		FGridContent Content;
		Content.Type = EPlaceableType::Building;
		Content.Facing = EPlaceableDirection::North;
		Content.Definition = MakeSliderDef();
		Grid->SetContent(At, Content);
	}

	float SliderValueAt(FGridCoord At)
	{
		return Grid->GetContentAt(At).SliderValues[0];
	}

END_DEFINE_SPEC(FEditSpec)

void FEditSpec::Define()
{
	BeforeEach([this]()
	{
		Grid = TStrongObjectPtr<UGridSubsystem>(NewObject<UGridSubsystem>());
		Edit = TStrongObjectPtr<UEditSubsystem>(NewObject<UEditSubsystem>());
		Edit->SetGrid(Grid.Get());
	});
	AfterEach([this]() { Edit.Reset(); Grid.Reset(); });

	Describe("Phase ordering", [this]()
	{
		It("runs in the input band, before the grid phase", [this]()
		{
			TestTrue(TEXT("edit < grid"), Edit->PhaseOrder() < Grid->PhaseOrder());
		});
	});

	Describe("Queued slider edits", [this]()
	{
		It("does not change the building until the edit phase steps", [this]()
		{
			PlaceSliderBuilding(FGridCoord(5, 5));
			Edit->QueueSliderEdit(FGridCoord(5, 5), 0, 0.9f);

			TestEqual(TEXT("still the authored default"), SliderValueAt(FGridCoord(5, 5)), 0.5f);
			TestEqual(TEXT("one pending"), Edit->NumPendingEdits(), 1);

			Edit->Step(0.f);
			TestEqual(TEXT("applied"), SliderValueAt(FGridCoord(5, 5)), 0.9f);
			TestEqual(TEXT("queue drained"), Edit->NumPendingEdits(), 0);
		});

		It("applies edits FIFO — the last queued edit wins", [this]()
		{
			PlaceSliderBuilding(FGridCoord(5, 5));
			Edit->QueueSliderEdit(FGridCoord(5, 5), 0, 0.2f);
			Edit->QueueSliderEdit(FGridCoord(5, 5), 0, 0.9f);

			Edit->Step(0.f);
			TestEqual(TEXT("last edit wins"), SliderValueAt(FGridCoord(5, 5)), 0.9f);
		});

		It("clamps the edited value to the slider's authored range", [this]()
		{
			PlaceSliderBuilding(FGridCoord(5, 5));
			Edit->QueueSliderEdit(FGridCoord(5, 5), 0, 5.f);

			Edit->Step(0.f);
			TestEqual(TEXT("clamped to range max"), SliderValueAt(FGridCoord(5, 5)), 1.f);
		});

		It("drops an edit whose target has no building", [this]()
		{
			Edit->QueueSliderEdit(FGridCoord(40, 40), 0, 0.9f);

			Edit->Step(0.f);
			TestEqual(TEXT("queue drained, nothing placed"), Edit->NumPendingEdits(), 0);
			TestFalse(TEXT("tile still empty"), Grid->IsTileOccupied(FGridCoord(40, 40)));
		});

		It("edits survive an unrelated placement in the same tick (edit phase then grid phase)", [this]()
		{
			PlaceSliderBuilding(FGridCoord(5, 5));
			Edit->QueueSliderEdit(FGridCoord(5, 5), 0, 0.9f);
			Grid->QueuePlacement(FGridCoord(20, 20), [this]()
			{
				FGridContent Content;
				Content.Type = EPlaceableType::Building;
				Content.Facing = EPlaceableDirection::North;
				Content.Definition = MakeSliderDef();
				return Content;
			}());

			// Same order the orchestrator uses: 150 then 200.
			Edit->Step(0.f);
			Grid->Step(0.f);

			TestEqual(TEXT("edit applied"), SliderValueAt(FGridCoord(5, 5)), 0.9f);
			TestTrue(TEXT("placement applied"), Grid->IsTileOccupied(FGridCoord(20, 20)));
		});
	});
}

#endif // WITH_AUTOMATION_TESTS
