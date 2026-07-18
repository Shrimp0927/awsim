// Dev-only console commands that wrap the same player-intent APIs the real UI will call,
// so they exercise the production input path. Compiled out of shipping builds.

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "HAL/IConsoleManager.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "awsim.h"
#include "Entities/GridContent.h"
#include "Simulation/GameGridSubsystem.h"
#include "Simulation/GameEditSubsystem.h"
#include "Simulation/GamePlayerFundsSubsystem.h"
#include "Simulation/GameSimulationSubsystem.h"

namespace
{
	// Effects run AmountAtMin = 0.5x .. AmountAtMax = 1x so slider edits are visible in the stats overlay.
	UPlaceableDef* MakeDevDef(const TCHAR* Name, const TArray<TPair<EDomain, float>>& Effects, float Cost, float DailyMaintenance, FIntPoint Dimensions)
	{
		UPlaceableDef* Def = NewObject<UPlaceableDef>(GetTransientPackage(),
			MakeUniqueObjectName(GetTransientPackage(), UPlaceableDef::StaticClass(), Name));
		Def->Type = EPlaceableType::Building;
		Def->Dimensions = Dimensions;
		Def->Cost = Cost;
		Def->DailyMaintenanceCost = DailyMaintenance;
		FSliderDef Slider;
		Slider.Name = TEXT("Operation");
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

	// Multi-tile footprints so buildings are visible from the whole-grid camera.
	UPlaceableDef* HomeDef()       { return MakeDevDef(TEXT("Home"), {{EDomain::Housing, 10.f}, {EDomain::Energy, -5.f}, {EDomain::Water, -5.f}}, 100.f, 1.f, FIntPoint(6, 6)); }
	UPlaceableDef* PowerDef()      { return MakeDevDef(TEXT("PowerPlant"), {{EDomain::Energy, 100.f}}, 500.f, 20.f, FIntPoint(10, 10)); }
	UPlaceableDef* WaterPlantDef() { return MakeDevDef(TEXT("WaterPlant"), {{EDomain::Water, 100.f}}, 500.f, 15.f, FIntPoint(10, 10)); }
	UPlaceableDef* BusinessDef()   { return MakeDevDef(TEXT("Business"), {{EDomain::Economy, 50.f}, {EDomain::Energy, -5.f}}, 200.f, 5.f, FIntPoint(8, 8)); }

	bool ParseCoord(const TArray<FString>& Args, FGridCoord& Out)
	{
		if (Args.Num() < 2)
		{
			UE_LOG(LogAwsim, Warning, TEXT("Expected: <X> <Y>"));
			return false;
		}
		Out = FGridCoord(FCString::Atoi(*Args[0]), FCString::Atoi(*Args[1]));
		return true;
	}

	void QueuePlace(UWorld* World, FGridCoord At, UPlaceableDef* Def, const TCHAR* Label)
	{
		UGridSubsystem* Grid = World ? World->GetSubsystem<UGridSubsystem>() : nullptr;
		if (!Grid)
		{
			UE_LOG(LogAwsim, Warning, TEXT("No GridSubsystem in this world."));
			return;
		}
		if (!UGridSubsystem::IsInBounds(At))
		{
			UE_LOG(LogAwsim, Warning, TEXT("(%d, %d) is out of bounds — the grid is %d x %d (tiles 0..%d)."),
				At.X, At.Y, UGridSubsystem::GetWidth(), UGridSubsystem::GetHeight(), UGridSubsystem::GetWidth() - 1);
			return;
		}
		FGridContent Content;
		Content.Type = Def ? EPlaceableType::Building : EPlaceableType::None;
		Content.Facing = Def ? EPlaceableDirection::North : EPlaceableDirection::None;
		Content.Definition = Def;
		Grid->QueuePlacement(At, Content);
		UE_LOG(LogAwsim, Log, TEXT("Queued %s at (%d, %d) — applies on the next sim step."), Label, At.X, At.Y);
	}
}

static FAutoConsoleCommandWithWorldAndArgs GCmdPlaceHome(
	TEXT("awsim.PlaceHome"),
	TEXT("awsim.PlaceHome <X> <Y> — queue a home (housing +10, needs energy + water)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		FGridCoord At;
		if (ParseCoord(Args, At)) QueuePlace(World, At, HomeDef(), TEXT("home"));
	}));

static FAutoConsoleCommandWithWorldAndArgs GCmdPlacePower(
	TEXT("awsim.PlacePower"),
	TEXT("awsim.PlacePower <X> <Y> — queue a power plant (energy +100)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		FGridCoord At;
		if (ParseCoord(Args, At)) QueuePlace(World, At, PowerDef(), TEXT("power plant"));
	}));

static FAutoConsoleCommandWithWorldAndArgs GCmdPlaceWaterPlant(
	TEXT("awsim.PlaceWaterPlant"),
	TEXT("awsim.PlaceWaterPlant <X> <Y> — queue a water plant (water +100)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		FGridCoord At;
		if (ParseCoord(Args, At)) QueuePlace(World, At, WaterPlantDef(), TEXT("water plant"));
	}));

static FAutoConsoleCommandWithWorldAndArgs GCmdPlaceBusiness(
	TEXT("awsim.PlaceBusiness"),
	TEXT("awsim.PlaceBusiness <X> <Y> — queue a business (economy +50, needs energy)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		FGridCoord At;
		if (ParseCoord(Args, At)) QueuePlace(World, At, BusinessDef(), TEXT("business"));
	}));

static FAutoConsoleCommandWithWorldAndArgs GCmdDemolish(
	TEXT("awsim.Demolish"),
	TEXT("awsim.Demolish <X> <Y> — queue removal of whatever occupies the tile."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		FGridCoord At;
		if (ParseCoord(Args, At)) QueuePlace(World, At, nullptr, TEXT("demolition"));
	}));

static FAutoConsoleCommandWithWorldAndArgs GCmdListBuildings(
	TEXT("awsim.ListBuildings"),
	TEXT("awsim.ListBuildings — dump every placed building: origin, live domain amounts, and each slider's index/value/range (feed the index to awsim.SetSlider)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		const UGridSubsystem* Grid = World ? World->GetSubsystem<UGridSubsystem>() : nullptr;
		if (!Grid)
		{
			UE_LOG(LogAwsim, Warning, TEXT("No GridSubsystem in this world."));
			return;
		}
		const TArray<FPlacedBuilding>& Buildings = Grid->GetBuildings();
		UE_LOG(LogAwsim, Log, TEXT("%d building(s):"), Buildings.Num());
		for (const FPlacedBuilding& Building : Buildings)
		{
			const UPlaceableDef* Def = Building.Content.Definition;
			FString DomainSummary;
			for (const EDomain Domain : {EDomain::Housing, EDomain::Economy, EDomain::Energy, EDomain::Water})
			{
				const float Amount = DomainAmount(Building.Content, Domain);
				if (!FMath::IsNearlyZero(Amount))
				{
					DomainSummary += FString::Printf(TEXT("  %s %+.1f"),
						*UEnum::GetDisplayValueAsText(Domain).ToString(), Amount);
				}
			}
			UE_LOG(LogAwsim, Log, TEXT("(%d, %d)  %s%s"),
				Building.Origin.X, Building.Origin.Y,
				Def ? *Def->GetName() : TEXT("<no def>"), *DomainSummary);
			if (!Def)
			{
				continue;
			}
			for (int32 i = 0; i < Def->Sliders.Num(); ++i)
			{
				const FSliderDef& Slider = Def->Sliders[i];
				const float Value = Building.Content.SliderValues.IsValidIndex(i)
					? Building.Content.SliderValues[i] : Slider.Value;
				UE_LOG(LogAwsim, Log, TEXT("    slider %d  '%s' = %.2f  [%.2f .. %.2f]"),
					i, *Slider.Name.ToString(), Value, Slider.Range.Min, Slider.Range.Max);
			}
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GCmdSetSlider(
	TEXT("awsim.SetSlider"),
	TEXT("awsim.SetSlider <X> <Y> <Index> <Value> — queue a slider edit on a placed building."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		FGridCoord At;
		if (!ParseCoord(Args, At) || Args.Num() < 4)
		{
			UE_LOG(LogAwsim, Warning, TEXT("Expected: <X> <Y> <SliderIndex> <Value>"));
			return;
		}
		UEditSubsystem* Edit = World ? World->GetSubsystem<UEditSubsystem>() : nullptr;
		if (!Edit)
		{
			UE_LOG(LogAwsim, Warning, TEXT("No EditSubsystem in this world."));
			return;
		}
		Edit->QueueSliderEdit(At, FCString::Atoi(*Args[2]), FCString::Atof(*Args[3]));
		UE_LOG(LogAwsim, Log, TEXT("Queued slider edit at (%d, %d) — applies on the next sim step."), At.X, At.Y);
	}));

static FAutoConsoleCommandWithWorldAndArgs GCmdGiveFunds(
	TEXT("awsim.GiveFunds"),
	TEXT("awsim.GiveFunds <Amount> — dev cheat: add funds immediately (skips the end-of-step deposit buffer)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		UGamePlayerFundsSubsystem* Funds = World ? World->GetSubsystem<UGamePlayerFundsSubsystem>() : nullptr;
		if (!Funds || Args.Num() < 1)
		{
			UE_LOG(LogAwsim, Warning, TEXT("Expected: <Amount> (and a world with a funds subsystem)."));
			return;
		}
		Funds->Deposit(FCString::Atof(*Args[0]));
		Funds->CommitDeposits(); // dev-immediate: don't wait for end of step
		UE_LOG(LogAwsim, Log, TEXT("Balance: %.0f"), Funds->GetBalance());
	}));

static FAutoConsoleCommandWithWorldAndArgs GCmdPause(
	TEXT("awsim.Pause"),
	TEXT("awsim.Pause — toggle the sim clock. Paused, the input band (edits + placements) still applies and renders; domain stats and the day clock freeze until resume."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		USimulationSubsystem* Sim = World ? World->GetSubsystem<USimulationSubsystem>() : nullptr;
		if (!Sim)
		{
			UE_LOG(LogAwsim, Warning, TEXT("No SimulationSubsystem in this world."));
			return;
		}
		Sim->SetPaused(Sim->IsRunning());
		UE_LOG(LogAwsim, Log, TEXT("Sim %s."), Sim->IsRunning() ? TEXT("resumed") : TEXT("paused (input band still live)"));
	}));

static FAutoConsoleCommandWithWorldAndArgs GCmdSetSpeed(
	TEXT("awsim.SetSpeed"),
	TEXT("awsim.SetSpeed <Multiplier> — sim speed (1 = normal, 2/3 = fast). Use awsim.Pause to pause; speed 0 also halts but doesn't remember your previous speed."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		USimulationSubsystem* Sim = World ? World->GetSubsystem<USimulationSubsystem>() : nullptr;
		if (!Sim || Args.Num() < 1)
		{
			UE_LOG(LogAwsim, Warning, TEXT("Expected: <Multiplier> (and a world with a sim subsystem)."));
			return;
		}
		Sim->SetGameSpeed(FCString::Atof(*Args[0]));
		UE_LOG(LogAwsim, Log, TEXT("Sim speed: %.1fx"), Sim->GetGameSpeed());
	}));

static FAutoConsoleCommandWithWorldAndArgs GCmdDeleteCity(
	TEXT("awsim.DeleteCity"),
	TEXT("awsim.DeleteCity — demolish ALL grid content immediately (buildings, roads, utilities). Funds and population are untouched; population then decays naturally. Use awsim.NewCity for a full reset."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		UGridSubsystem* Grid = World ? World->GetSubsystem<UGridSubsystem>() : nullptr;
		if (!Grid)
		{
			UE_LOG(LogAwsim, Warning, TEXT("No GridSubsystem in this world."));
			return;
		}
		// Collect targets first: removal mutates the containers being walked.
		TArray<FGridCoord> Targets;
		for (const FPlacedBuilding& Building : Grid->GetBuildings()) Targets.Add(Building.Origin);
		for (const TPair<FGridCoord, FGridContent>& Road : Grid->GetRoads()) Targets.Add(Road.Key);
		for (const TPair<FGridCoord, FGridContent>& Util : Grid->GetUtilities()) Targets.Add(Util.Key);
		for (const FGridCoord& Target : Targets)
		{
			Grid->SetContent(Target, FGridContent());
		}
		UE_LOG(LogAwsim, Log, TEXT("Demolished %d pieces of grid content."), Targets.Num());
	}));

static FAutoConsoleCommandWithWorldAndArgs GCmdNewCity(
	TEXT("awsim.NewCity"),
	TEXT("awsim.NewCity — reload the current level. World subsystems are torn down and recreated, so the whole sim (grid, funds, population, day counter) starts fresh."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		if (!World)
		{
			return;
		}
		const FString LevelName = UGameplayStatics::GetCurrentLevelName(World);
		UE_LOG(LogAwsim, Log, TEXT("Reloading '%s' — starting a fresh city."), *LevelName);
		UGameplayStatics::OpenLevel(World, FName(*LevelName));
	}));

static FAutoConsoleCommandWithWorldAndArgs GCmdSpawnTestCity(
	TEXT("awsim.SpawnTestCity"),
	TEXT("awsim.SpawnTestCity — funds + 5 buildings (2 homes, power, water, business) at the grid center."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		if (UGamePlayerFundsSubsystem* Funds = World ? World->GetSubsystem<UGamePlayerFundsSubsystem>() : nullptr)
		{
			Funds->Deposit(10000.f);
			Funds->CommitDeposits();
		}

		// Centered row; 2-tile gaps keep neighbours in connector reach (one island) but visually separate.
		const int32 X = UGridSubsystem::GetWidth() / 2 - 24;
		const int32 Y = UGridSubsystem::GetHeight() / 2 - 5;
		QueuePlace(World, FGridCoord(X, Y), HomeDef(), TEXT("home"));
		QueuePlace(World, FGridCoord(X + 8, Y), HomeDef(), TEXT("home"));
		QueuePlace(World, FGridCoord(X + 16, Y), PowerDef(), TEXT("power plant"));
		QueuePlace(World, FGridCoord(X + 28, Y), WaterPlantDef(), TEXT("water plant"));
		QueuePlace(World, FGridCoord(X + 40, Y), BusinessDef(), TEXT("business"));
	}));

static FAutoConsoleCommandWithWorldAndArgs GCmdSpawnHeavyCity(
	TEXT("awsim.SpawnHeavyCity"),
	TEXT("awsim.SpawnHeavyCity — stress test: 8 full-width roads split the grid into bands, then 80% of the possible 6x6 building slots are filled (homes/business/power/water). Funds are deposited to cover it. Best on an empty city."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		UGridSubsystem* Grid = World ? World->GetSubsystem<UGridSubsystem>() : nullptr;
		UGamePlayerFundsSubsystem* Funds = World ? World->GetSubsystem<UGamePlayerFundsSubsystem>() : nullptr;
		if (!Grid || !Funds)
		{
			UE_LOG(LogAwsim, Warning, TEXT("No grid/funds subsystem in this world."));
			return;
		}

		constexpr int32 Side = 6; // uniform footprint so slots form a regular lattice
		const int32 W = UGridSubsystem::GetWidth();
		const int32 H = UGridSubsystem::GetHeight();

		// 8 evenly spaced full-width road rows, splitting the grid into 9 bands.
		TArray<int32> RoadRows;
		for (int32 i = 1; i <= 8; ++i)
		{
			RoadRows.Add(i * H / 9);
		}

		// 6x6 variants of the dev defs; two homes per cycle so housing dominates.
		UPlaceableDef* Home = MakeDevDef(TEXT("HeavyHome"), {{EDomain::Housing, 10.f}, {EDomain::Energy, -5.f}, {EDomain::Water, -5.f}}, 100.f, 1.f, FIntPoint(Side, Side));
		UPlaceableDef* Business = MakeDevDef(TEXT("HeavyBusiness"), {{EDomain::Economy, 50.f}, {EDomain::Energy, -5.f}}, 200.f, 5.f, FIntPoint(Side, Side));
		UPlaceableDef* Power = MakeDevDef(TEXT("HeavyPower"), {{EDomain::Energy, 100.f}}, 500.f, 20.f, FIntPoint(Side, Side));
		UPlaceableDef* Water = MakeDevDef(TEXT("HeavyWater"), {{EDomain::Water, 100.f}}, 500.f, 15.f, FIntPoint(Side, Side));
		UPlaceableDef* Cycle[] = {Home, Business, Home, Power, Water};

		float TotalCost = 0.f;
		int32 Slots = 0, Queued = 0;
		int32 y = 0;
		while (y + Side <= H)
		{
			// A road row cutting through this building row? Resume just below it.
			const int32* Blocker = RoadRows.FindByPredicate([y](int32 Row) { return Row >= y && Row < y + Side; });
			if (Blocker)
			{
				y = *Blocker + 1;
				continue;
			}
			for (int32 x = 0; x + Side <= W; x += Side)
			{
				if (Slots++ % 5 == 4)
				{
					continue; // leave every 5th slot empty -> 80% of the possible slots
				}
				FGridContent Content;
				Content.Type = EPlaceableType::Building;
				Content.Facing = EPlaceableDirection::North;
				Content.Definition = Cycle[Queued % UE_ARRAY_COUNT(Cycle)];
				Grid->QueuePlacement(FGridCoord(x, y), Content);
				TotalCost += Content.Definition->Cost;
				++Queued;
			}
			y += Side;
		}

		UPlaceableDef* RoadDef = NewObject<UPlaceableDef>(GetTransientPackage(),
			MakeUniqueObjectName(GetTransientPackage(), UPlaceableDef::StaticClass(), TEXT("HeavyRoad")));
		RoadDef->Type = EPlaceableType::Road;
		RoadDef->Dimensions = FIntPoint(1, 1);
		RoadDef->Cost = 10.f;
		for (const int32 Row : RoadRows)
		{
			for (int32 x = 0; x < W; ++x)
			{
				FGridContent Content;
				Content.Type = EPlaceableType::Road;
				Content.Facing = EPlaceableDirection::None;
				Content.Definition = RoadDef;
				Grid->QueuePlacement(FGridCoord(x, Row), Content);
				TotalCost += RoadDef->Cost;
			}
		}

		Funds->Deposit(TotalCost);
		Funds->CommitDeposits(); // available before the queue drains on the next sim step
		UE_LOG(LogAwsim, Log, TEXT("Queued %d of %d building slots (80%%) and %d road tiles across 8 roads; deposited %.0f to cover it. Applies on the next sim step."),
			Queued, Slots, RoadRows.Num() * W, TotalCost);
	}));

#endif // !UE_BUILD_SHIPPING
