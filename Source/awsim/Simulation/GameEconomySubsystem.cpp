#include "GameEconomySubsystem.h"
#include "GameEnergySubsystem.h"
#include "Engine/World.h"

void UEconomySubsystem::Step(float StepSeconds)
{
	UGridSubsystem* GridSubsystem = ResolveGrid();
	UEnergySubsystem* EnergySubsystem = ResolveEnergy();
	if (!GridSubsystem || !EnergySubsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("Economy: Grid/Energy subsystem not found!"));
		return;
	}

	Recompute(*GridSubsystem, *EnergySubsystem);
}

void UEconomySubsystem::Recompute(const UGridSubsystem& GridSubsystem, const UEnergySubsystem& EnergySubsystem)
{
	GDP = 0.f;

	const TArray<TArray<FGridCoord>>& Islands = GridSubsystem.GetIslands();
	for (int32 i = 0; i < Islands.Num(); ++i)
	{
		// A blacked-out island produces nothing, regardless of its businesses.
		if (!EnergySubsystem.IsIslandServiced(i))
		{
			continue;
		}

		for (const FGridCoord& Origin : Islands[i])
		{
			GDP += DomainAmount(GridSubsystem.GetContentAt(Origin), EDomain::Economy);
		}
	}
}

UGridSubsystem* UEconomySubsystem::ResolveGrid() const
{
	if (Grid)
	{
		return Grid;
	}
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UGridSubsystem>() : nullptr;
}

UEnergySubsystem* UEconomySubsystem::ResolveEnergy() const
{
	if (Energy)
	{
		return Energy;
	}
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UEnergySubsystem>() : nullptr;
}
