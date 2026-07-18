#include "GameEnergySubsystem.h"
#include "GamePlayerFundsSubsystem.h"
#include "GameSimulationSubsystem.h"
#include "Engine/World.h"


void UEnergySubsystem::Step(float StepSeconds)
{
	UGridSubsystem* GridSubsystem = ResolveGrid();
	if (!GridSubsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("GridSubsystem not found!"));
		return;
	}

	// Skip the island walk when neither grid content nor sliders changed.
	const uint64 ContentRev = GridSubsystem->GetContentRevision();
	const uint64 SliderRev = GridSubsystem->GetSliderRevision();
	if (ContentRev != LastContentRevision || SliderRev != LastSliderRevision)
	{
		LastContentRevision = ContentRev;
		LastSliderRevision = SliderRev;
		Recompute(*GridSubsystem);
	}

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

void UEnergySubsystem::Recompute(const UGridSubsystem& GridSubsystem)
{
	Capacity = 0.f;
	Consumption = 0.f;
	MaintenanceCost = 0.f;
	IslandServiced.Reset();

	// Supply and demand are island-scoped: a producer only serves its own island.
	for (const TArray<FGridCoord>& Island : GridSubsystem.GetIslands())
	{
		float IslandCapacity = 0.f;
		float IslandConsumption = 0.f;
		for (const FGridCoord& Origin : Island)
		{
			const float Amount = DomainAmount(GridSubsystem.GetContentAt(Origin), EDomain::Energy);
			if (Amount > 0.f) IslandCapacity += Amount;
			else IslandConsumption += -Amount;
		}

		const bool bServiced = IslandCapacity >= IslandConsumption;
		IslandServiced.Add(bServiced);
		if (!bServiced)
		{
			// decrease player rating
		}

		Capacity += IslandCapacity;
		Consumption += IslandConsumption;
	}

	for (const FPlacedBuilding& Building : GridSubsystem.GetBuildings())
	{
		if (DefHasDomain(Building.Content.Definition, EDomain::Energy))
		{
			MaintenanceCost += Building.Content.Definition->DailyMaintenanceCost;
		}
	}

	// NOTE: placeholder revenue model — consumption * 10. Subject to change.
	Revenue = Consumption * 10.f;
}

void UEnergySubsystem::SettleDay()
{
	UGamePlayerFundsSubsystem* PlayerFunds = ResolveFunds();
	if (!PlayerFunds)
	{
		return;
	}
	PlayerFunds->TrySpend(MaintenanceCost, /*Override=*/true);
	PlayerFunds->Deposit(Revenue);
}

UGamePlayerFundsSubsystem* UEnergySubsystem::ResolveFunds() const
{
	if (Funds)
	{
		return Funds;
	}
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UGamePlayerFundsSubsystem>() : nullptr;
}

UGridSubsystem* UEnergySubsystem::ResolveGrid() const
{
	if (Grid)
	{
		return Grid;
	}
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UGridSubsystem>() : nullptr;
}
