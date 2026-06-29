#include "Simulation/CityStatsSubsystem.h"

void UCityStatsSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// Loose seeding until the grid's housing/buildings drive these (DESIGN.md
	// §4/§5). Stand-in for the old per-citizen seed: 100 people, ~1000 each.
	if (Population == 0)
	{
		Population = 100;
		TaxableWealth = 100000.f;
	}
}

void UCityStatsSubsystem::Step(float StepSeconds)
{
	// Loose first pass. Later: derive population from housing capacity and
	// satisfaction from how well the grid's buildings/services meet the
	// population (DESIGN.md §4/§5). For now satisfaction holds steady and the
	// rating simply follows it.
	OverlordRating = RatingFromSatisfaction(AverageSatisfaction);
}
