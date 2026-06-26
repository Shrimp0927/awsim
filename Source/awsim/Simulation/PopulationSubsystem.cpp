#include "Simulation/PopulationSubsystem.h"

void UPopulationSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// Loose: seed a small population so there's something to simulate.
	if (Citizens.Num() == 0)
	{
		SeedCitizens(100);
	}
}

void UPopulationSubsystem::SeedCitizens(int32 Count)
{
	Citizens.Reserve(Citizens.Num() + Count);
	for (int32 i = 0; i < Count; ++i)
	{
		FCitizen C;
		C.NetWorth = 1000.f;
		Citizens.Add(C);
	}
}

void UPopulationSubsystem::Step(float StepSeconds)
{
	// Loose first pass: daily needs drift over time. Service quality (not yet
	// modelled) will later offset these. Independent per citizen — parallel-ready.
	for (FCitizen& C : Citizens)
	{
		C.Hunger = FMath::Clamp(C.Hunger + 0.02f * StepSeconds, 0.f, 1.f);
		C.Energy = FMath::Clamp(C.Energy - 0.01f * StepSeconds, 0.f, 1.f);

		// Mood follows being fed and rested, loosely.
		const float MoodTarget = FMath::Clamp((1.f - C.Hunger) * 0.5f + C.Energy * 0.5f, 0.f, 1.f);
		C.Mood = FMath::FInterpTo(C.Mood, MoodTarget, StepSeconds, 1.f);

		// Long-term satisfaction drifts toward a net-worth / health proxy (DESIGN §4).
		const float SatTarget = FMath::Clamp(
			C.Health * 0.5f + FMath::Min(C.NetWorth / 2000.f, 1.f) * 0.5f, 0.f, 1.f);
		C.Satisfaction = FMath::FInterpTo(C.Satisfaction, SatTarget, StepSeconds, 0.1f);
	}
}

float UPopulationSubsystem::AverageProductivity() const
{
	if (Citizens.Num() == 0)
	{
		return 0.f;
	}

	float Sum = 0.f;
	for (const FCitizen& C : Citizens)
	{
		Sum += C.Productivity();
	}
	return Sum / Citizens.Num();
}

float UPopulationSubsystem::TotalNetWorth() const
{
	float Sum = 0.f;
	for (const FCitizen& C : Citizens)
	{
		Sum += C.NetWorth;
	}
	return Sum;
}
