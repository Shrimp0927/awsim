#include "Simulation/GameCityStatsSubsystem.h"
#include "Simulation/GamePopulationSubsystem.h"
#include "Engine/World.h"

void UCityStatsSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

}

void UCityStatsSubsystem::Step(float StepSeconds)
{
	Super::Step(StepSeconds);

	// Aggregate from the domain phases (this runs at 899, after all of them).
	const UWorld* World = GetWorld();
	if (const UPopulationSubsystem* PopulationSubsystem = World ? World->GetSubsystem<UPopulationSubsystem>() : nullptr)
	{
		Population = PopulationSubsystem->GetCount();
	}
}
