#include "Misc/AutomationTest.h"
#include "Simulation/GameGridSubsystem.h"
#include "Simulation/GameWaterSubsystem.h"
#include "Entities/GridContent.h"
#include "UObject/StrongObjectPtr.h"

// Spec for the Water domain phase (mirrors Energy): capacity/consumption sums
// and island-scoped service.

#if WITH_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FWaterSpec, "awsim.Simulation.Water",
	EAutomationTestFlags::ProductFilter | EAutomationTestFlags_ApplicationContextMask)

	TStrongObjectPtr<UGridSubsystem> Grid;
	TStrongObjectPtr<UGameWaterSubsystem> Water;

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

	int32 IslandIndexOf(FGridCoord At)
	{
		const TArray<TArray<FGridCoord>>& Islands = Grid->GetIslands();
		for (int32 i = 0; i < Islands.Num(); ++i)
		{
			if (Islands[i].Contains(At)) return i;
		}
		return INDEX_NONE;
	}

END_DEFINE_SPEC(FWaterSpec)

void FWaterSpec::Define()
{
	BeforeEach([this]()
	{
		Grid = TStrongObjectPtr<UGridSubsystem>(NewObject<UGridSubsystem>());
		Water = TStrongObjectPtr<UGameWaterSubsystem>(NewObject<UGameWaterSubsystem>());
		Water->SetGrid(Grid.Get());
	});
	AfterEach([this]() { Water.Reset(); Grid.Reset(); });

	Describe("Starting state", [this]()
	{
		It("starts with water capacity 0", [this]()
		{
			Water->Step(0.f);
			TestEqual(TEXT("capacity"), Water->GetCapacity(), 0.f);
			TestEqual(TEXT("consumption"), Water->GetConsumption(), 0.f);
		});
	});

	Describe("Capacity — counts regardless of connectivity", [this]()
	{
		It("raises total capacity for any water building, even one alone on its own island", [this]()
		{
			Place(FGridCoord(5, 5), MakeBuildingDef({{EDomain::Water, 100.f}}));
			Water->Step(0.f);
			TestEqual(TEXT("capacity is the global raw sum"), Water->GetCapacity(), 100.f);
		});

		It("changes total capacity by the building's signed water contribution", [this]()
		{
			Place(FGridCoord(5, 5), MakeBuildingDef({{EDomain::Water, 100.f}}));
			Place(FGridCoord(6, 5), MakeBuildingDef({{EDomain::Water, -30.f}}));
			Water->Step(0.f);
			TestEqual(TEXT("positive -> capacity"), Water->GetCapacity(), 100.f);
			TestEqual(TEXT("negative -> consumption (absolute)"), Water->GetConsumption(), 30.f);
		});
	});

	Describe("Service is island-scoped — a water plant only serves its own island", [this]()
	{
		It("meets demand for consumers in the SAME island when local supply >= local demand", [this]()
		{
			Place(FGridCoord(5, 5), MakeBuildingDef({{EDomain::Water, 100.f}}));
			Place(FGridCoord(6, 5), MakeBuildingDef({{EDomain::Water, -30.f}}));
			Water->Step(0.f);
			TestTrue(TEXT("island serviced"), Water->IsIslandServiced(IslandIndexOf(FGridCoord(5, 5))));
		});

		It("does NOT supply consumers in a DIFFERENT island from the plant", [this]()
		{
			Place(FGridCoord(5, 5), MakeBuildingDef({{EDomain::Water, 100.f}}));
			Place(FGridCoord(50, 50), MakeBuildingDef({{EDomain::Water, -30.f}}));
			Water->Step(0.f);
			TestTrue(TEXT("plant island serviced"), Water->IsIslandServiced(IslandIndexOf(FGridCoord(5, 5))));
			TestFalse(TEXT("consumer island dry"), Water->IsIslandServiced(IslandIndexOf(FGridCoord(50, 50))));
		});

		It("leaves an island under-supplied even when another island has surplus", [this]()
		{
			Place(FGridCoord(5, 5), MakeBuildingDef({{EDomain::Water, 100.f}}));
			Place(FGridCoord(50, 50), MakeBuildingDef({{EDomain::Water, -30.f}}));
			Water->Step(0.f);
			TestTrue(TEXT("global capacity exceeds global demand"), Water->GetCapacity() > Water->GetConsumption());
			TestFalse(TEXT("island still dry — supply doesn't cross islands"), Water->IsIslandServiced(IslandIndexOf(FGridCoord(50, 50))));
		});
	});
}

#endif // WITH_AUTOMATION_TESTS
