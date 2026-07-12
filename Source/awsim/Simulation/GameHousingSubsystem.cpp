#include "GameHousingSubsystem.h"
#include "GameEnergySubsystem.h"
#include "GameWaterSubsystem.h"
#include "GamePlayerFundsSubsystem.h"
#include "GameSimulationSubsystem.h"
#include "Engine/World.h"

namespace
{
	// NOTE: placeholder tax model — each serviced home pays a flat daily tax.
	// Subject to change.
	constexpr float TaxPerHome = 10.f;
}

void UHousingSubsystem::Step(float StepSeconds)
{
	UGridSubsystem* GridSubsystem = ResolveGrid();
	UEnergySubsystem* EnergySubsystem = ResolveEnergy();
	UGameWaterSubsystem* WaterSubsystem = ResolveWater();
	if (!GridSubsystem || !EnergySubsystem || !WaterSubsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("Housing: Grid/Energy/Water subsystem not found!"));
		return;
	}

	Recompute(*GridSubsystem, *EnergySubsystem, *WaterSubsystem);

	// Settle money once per in-game day, skipping the day the sim starts on.
	const UWorld* World = GetWorld();
	if (const USimulationSubsystem* Sim = World ? World->GetSubsystem<USimulationSubsystem>() : nullptr)
	{
		const int32 Day = Sim->GetDay();
		if (LastSettledDay == INDEX_NONE)
		{
			LastSettledDay = Day;
		}
		else if (Day != LastSettledDay)
		{
			LastSettledDay = Day;
			SettleDay();
		}
	}
}

void UHousingSubsystem::Recompute(const UGridSubsystem& GridSubsystem, const UEnergySubsystem& EnergySubsystem, const UGameWaterSubsystem& WaterSubsystem)
{
	Capacity = 0.f;
	ServicedCapacity = 0.f;
	TaxRevenue = 0.f;

	const TArray<TArray<FGridCoord>>& Islands = GridSubsystem.GetIslands();
	for (int32 i = 0; i < Islands.Num(); ++i)
	{
		float IslandHousing = 0.f;
		int32 IslandHomes = 0;
		for (const FGridCoord& Origin : Islands[i])
		{
			const float Amount = DomainAmount(GridSubsystem.GetContentAt(Origin), EDomain::Housing);
			IslandHousing += Amount;
			if (Amount > 0.f)
			{
				++IslandHomes;
			}
		}

		Capacity += IslandHousing;

		// A home only functions when its island has BOTH energy and water.
		if (EnergySubsystem.IsIslandServiced(i) && WaterSubsystem.IsIslandServiced(i))
		{
			ServicedCapacity += IslandHousing;
			TaxRevenue += IslandHomes * TaxPerHome;
		}
	}
}

void UHousingSubsystem::SettleDay()
{
	if (UGamePlayerFundsSubsystem* PlayerFunds = ResolveFunds())
	{
		PlayerFunds->Deposit(TaxRevenue);
	}
}

UGridSubsystem* UHousingSubsystem::ResolveGrid() const
{
	if (Grid)
	{
		return Grid;
	}
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UGridSubsystem>() : nullptr;
}

UEnergySubsystem* UHousingSubsystem::ResolveEnergy() const
{
	if (Energy)
	{
		return Energy;
	}
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UEnergySubsystem>() : nullptr;
}

UGameWaterSubsystem* UHousingSubsystem::ResolveWater() const
{
	if (Water)
	{
		return Water;
	}
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UGameWaterSubsystem>() : nullptr;
}

UGamePlayerFundsSubsystem* UHousingSubsystem::ResolveFunds() const
{
	if (Funds)
	{
		return Funds;
	}
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UGamePlayerFundsSubsystem>() : nullptr;
}
