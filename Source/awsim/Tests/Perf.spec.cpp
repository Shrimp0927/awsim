#include "Misc/AutomationTest.h"
#include "awsim.h"
#include "Core/GameSaveSubsystem.h"
#include "Simulation/GameGridSubsystem.h"
#include "Simulation/GameEnergySubsystem.h"
#include "Entities/GridContent.h"
#include "HAL/FileManager.h"
#include "UObject/StrongObjectPtr.h"

// Timing benchmarks, not correctness specs. Run with: make test TEST_FILTER=awsim.Perf

#if WITH_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FPerfSpec, "awsim.Perf",
	EAutomationTestFlags::ProductFilter | EAutomationTestFlags_ApplicationContextMask)

	static constexpr float Dt = 1.f / 30.f;
	static constexpr int32 Side = 6;

	TStrongObjectPtr<UGridSubsystem> Grid;
	TStrongObjectPtr<UEnergySubsystem> Energy;
	TStrongObjectPtr<UGameSaveSubsystem> Save;
	TStrongObjectPtr<UPlaceableDef> Power, Home, Road;

	UPlaceableDef* MakeDef(EPlaceableType Type, FIntPoint Dims, const TArray<TPair<EDomain, float>>& Effects)
	{
		UPlaceableDef* Def = NewObject<UPlaceableDef>();
		Def->Type = Type;
		Def->Dimensions = Dims;
		Def->HeightTiles = 20.f;
		FSliderDef Slider;
		for (const TPair<EDomain, float>& E : Effects)
		{
			FDomainEffect Effect;
			Effect.Domain = E.Key;
			Effect.AmountAtMin = E.Value * 0.5f;
			Effect.AmountAtMax = E.Value;
			Slider.Effects.Add(Effect);
		}
		Def->Sliders.Add(Slider);
		return Def;
	}

	// Rows of 6x6 buildings, each row facing a full-width road.
	void QueueCity(int32 NumBuildings)
	{
		int32 Placed = 0;
		for (int32 y = 0; y + Side < UGridSubsystem::GridHeight && Placed < NumBuildings; y += Side + 1)
		{
			for (int32 x = 0; x + Side <= UGridSubsystem::GridWidth - 2 && Placed < NumBuildings; x += Side, ++Placed)
			{
				FGridContent Content;
				Content.Type = EPlaceableType::Building;
				Content.Facing = EPlaceableDirection::North;
				Content.Definition = Placed % 8 == 0 ? Power.Get() : Home.Get();
				Grid->QueuePlacement(FGridCoord(x, y), Content);
			}
			for (int32 x = 0; x < UGridSubsystem::GridWidth - 2; ++x)
			{
				FGridContent Content;
				Content.Type = EPlaceableType::Road;
				Content.Definition = Road.Get();
				Grid->QueuePlacement(FGridCoord(x, y + Side), Content);
			}
		}
	}

	void BuildCity(int32 NumBuildings)
	{
		Grid->ResetForLoad();
		QueueCity(NumBuildings);
		Grid->Step(0.f);
	}

	double MedianMs(int32 Runs, TFunctionRef<void()> Setup, TFunctionRef<void()> Timed)
	{
		TArray<double> Samples;
		for (int32 r = 0; r < Runs; ++r)
		{
			Setup();
			const uint64 Start = FPlatformTime::Cycles64();
			Timed();
			Samples.Add(FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64() - Start));
		}
		Samples.Sort();
		return Samples[Runs / 2];
	}

	void Report(int32 N, const TCHAR* Name, double Value, const TCHAR* Unit)
	{
		const FString Line = FString::Printf(TEXT("PERF buildings=%d %s = %.4f %s"), N, Name, Value, Unit);
		UE_LOG(LogAwsim, Display, TEXT("%s"), *Line);
		AddInfo(Line);
	}

	void RunAll(int32 N)
	{
		const auto None = []() {};
		BuildCity(N);
		TestEqual(TEXT("city size"), Grid->GetBuildings().Num(), N);

		// Pick
		constexpr int32 NumRays = 10000;
		TArray<FVector> Origins;
		const FVector Dir(0.f, -0.5f, -FMath::Sin(FMath::DegreesToRadians(60.f)));
		FRandomStream Rng(1);
		for (int32 i = 0; i < NumRays; ++i)
		{
			const FVector Target(
				UGridSubsystem::WorldMinX + Rng.FRandRange(0.f, UGridSubsystem::GridWidth) * UGridSubsystem::TileSize,
				UGridSubsystem::WorldMinY + Rng.FRandRange(0.f, UGridSubsystem::GridHeight) * UGridSubsystem::TileSize,
				0.f);
			Origins.Add(Target - Dir * 40000.f);
		}
		int32 Hits = 0;
		const double PickMs = MedianMs(9, None, [&]()
		{
			FGridCoord Tile;
			for (const FVector& Origin : Origins) Hits += Grid->PickTile(Origin, Dir, Tile);
		});
		Report(N, TEXT("PickTile"), PickMs * 1e6 / NumRays, TEXT("ns/call"));

		// Grid step with nothing to do
		Report(N, TEXT("GridStep idle"), MedianMs(9, None, [&]() { Grid->Step(Dt); }), TEXT("ms"));

		// Island rebuild: toggling one free road tile dirties roads and islands.
		const FGridCoord Spare(UGridSubsystem::GridWidth - 1, UGridSubsystem::GridHeight - 1);
		FGridContent SpareRoad;
		SpareRoad.Type = EPlaceableType::Road;
		SpareRoad.Definition = Road.Get();
		const double IslandMs = MedianMs(9,
			[&]() { Grid->SetContent(Spare, Grid->IsTileOccupied(Spare) ? FGridContent() : SpareRoad); },
			[&]() { Grid->GetIslands(); });
		Report(N, TEXT("RebuildIslands"), IslandMs, TEXT("ms"));

		// Energy: cached step vs forced recompute
		Energy->Step(Dt);
		Report(N, TEXT("EnergyStep cached"), MedianMs(9, None, [&]() { Energy->Step(Dt); }), TEXT("ms"));
		float Slider = 0.f;
		const double RecomputeMs = MedianMs(9,
			[&]() { Slider = 1.f - Slider; Grid->SetSliderValue(FGridCoord(0, 0), 0, Slider); },
			[&]() { Energy->Step(Dt); });
		Report(N, TEXT("EnergyStep recompute"), RecomputeMs, TEXT("ms"));

		// Save / load
		const FString Slot = TEXT("PerfSlot");
		Report(N, TEXT("SaveNow"), MedianMs(5, None, [&]() { Save->SaveNow(Slot); }), TEXT("ms"));
		Report(N, TEXT("LoadNow"), MedianMs(5, None, [&]() { Save->LoadNow(Slot); }), TEXT("ms"));
		IFileManager::Get().Delete(*UGameSaveSubsystem::SlotToPath(Slot), /*RequireExists*/ false);

		// Placement drain: one step applying the whole queue
		const double DrainMs = MedianMs(5,
			[&]() { Grid->ResetForLoad(); QueueCity(N); },
			[&]() { Grid->Step(0.f); });
		Report(N, TEXT("Drain step"), DrainMs, TEXT("ms"));

		// Growth: the step where every building gains a floor
		const double GrowMs = MedianMs(5,
			[&]() { BuildCity(N); for (int32 i = 0; i < UGridSubsystem::StepsPerFloor - 1; ++i) Grid->Step(Dt); },
			[&]() { Grid->Step(Dt); });
		Report(N, TEXT("Growth step"), GrowMs, TEXT("ms"));

		// Demolish every building one by one
		TArray<FGridCoord> Targets;
		const double DemolishMs = MedianMs(3,
			[&]()
			{
				BuildCity(N);
				Targets.Reset();
				for (const FPlacedBuilding& B : Grid->GetBuildings()) Targets.Add(B.Origin);
			},
			[&]() { for (const FGridCoord& T : Targets) Grid->SetContent(T, FGridContent()); });
		Report(N, TEXT("Demolish"), DemolishMs * 1e3 / N, TEXT("us/building"));
	}

END_DEFINE_SPEC(FPerfSpec)

void FPerfSpec::Define()
{
	BeforeEach([this]()
	{
		Grid = TStrongObjectPtr<UGridSubsystem>(NewObject<UGridSubsystem>());
		Energy = TStrongObjectPtr<UEnergySubsystem>(NewObject<UEnergySubsystem>());
		Save = TStrongObjectPtr<UGameSaveSubsystem>(NewObject<UGameSaveSubsystem>());
		Energy->SetGrid(Grid.Get());
		Save->SetGrid(Grid.Get());
		Power = TStrongObjectPtr<UPlaceableDef>(MakeDef(EPlaceableType::Building, FIntPoint(Side, Side), {{EDomain::Energy, 2.f}}));
		Home = TStrongObjectPtr<UPlaceableDef>(MakeDef(EPlaceableType::Building, FIntPoint(Side, Side), {{EDomain::Housing, 0.5f}, {EDomain::Energy, -0.1f}}));
		Road = TStrongObjectPtr<UPlaceableDef>(MakeDef(EPlaceableType::Road, FIntPoint(1, 1), {}));
	});
	AfterEach([this]() { Road.Reset(); Home.Reset(); Power.Reset(); Save.Reset(); Energy.Reset(); Grid.Reset(); });

	It("100 buildings", [this]() { RunAll(100); });
	It("1000 buildings", [this]() { RunAll(1000); });
	It("5000 buildings", [this]() { RunAll(5000); });
}

#endif // WITH_AUTOMATION_TESTS
