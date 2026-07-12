// Dev-only console commands for driving the simulation without UI. They wrap
// the same player-intent APIs the real UI will call (QueuePlacement /
// QueueSliderEdit), so they exercise the production input path. Compiled out
// of shipping builds entirely.

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "HAL/IConsoleManager.h"
#include "Engine/World.h"
#include "awsim.h"
#include "Entities/GridContent.h"
#include "Simulation/GameGridSubsystem.h"
#include "Simulation/GameEditSubsystem.h"
#include "Simulation/GamePlayerFundsSubsystem.h"

namespace
{
	// Effects run AmountAtMin = 0.5x .. AmountAtMax = 1x so slider edits are
	// visible in the stats overlay.
	UPlaceableDef* MakeDevDef(const TArray<TPair<EDomain, float>>& Effects, float Cost, float DailyMaintenance)
	{
		UPlaceableDef* Def = NewObject<UPlaceableDef>(GetTransientPackage());
		Def->Type = EPlaceableType::Building;
		Def->Dimensions = FIntPoint(1, 1);
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

	UPlaceableDef* HomeDef()       { return MakeDevDef({{EDomain::Housing, 10.f}, {EDomain::Energy, -5.f}, {EDomain::Water, -5.f}}, 100.f, 1.f); }
	UPlaceableDef* PowerDef()      { return MakeDevDef({{EDomain::Energy, 100.f}}, 500.f, 20.f); }
	UPlaceableDef* WaterPlantDef() { return MakeDevDef({{EDomain::Water, 100.f}}, 500.f, 15.f); }
	UPlaceableDef* BusinessDef()   { return MakeDevDef({{EDomain::Economy, 50.f}, {EDomain::Energy, -5.f}}, 200.f, 5.f); }

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

static FAutoConsoleCommandWithWorldAndArgs GCmdSpawnTestCity(
	TEXT("awsim.SpawnTestCity"),
	TEXT("awsim.SpawnTestCity [X Y] — funds + a serviced cluster (2 homes, power, water, business). Default origin 500 500."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		UGamePlayerFundsSubsystem* Funds = World ? World->GetSubsystem<UGamePlayerFundsSubsystem>() : nullptr;
		if (Funds)
		{
			Funds->Deposit(10000.f);
			Funds->CommitDeposits();
		}
		const int32 X = Args.Num() >= 2 ? FCString::Atoi(*Args[0]) : 500;
		const int32 Y = Args.Num() >= 2 ? FCString::Atoi(*Args[1]) : 500;
		QueuePlace(World, FGridCoord(X, Y), HomeDef(), TEXT("home"));
		QueuePlace(World, FGridCoord(X + 1, Y), HomeDef(), TEXT("home"));
		QueuePlace(World, FGridCoord(X + 2, Y), PowerDef(), TEXT("power plant"));
		QueuePlace(World, FGridCoord(X, Y + 1), WaterPlantDef(), TEXT("water plant"));
		QueuePlace(World, FGridCoord(X + 1, Y + 1), BusinessDef(), TEXT("business"));
		UE_LOG(LogAwsim, Log, TEXT("Test city queued around (%d, %d). Enable awsim.DebugStats 1 to watch it."), X, Y);
	}));

#endif // !UE_BUILD_SHIPPING
