#include "Simulation/EconomySubsystem.h"
#include "Simulation/PopulationSubsystem.h"
#include "awsim.h"
#include "Engine/World.h"

void UEconomySubsystem::Step(float StepSeconds)
{
	TaxTimer += StepSeconds;
	if (TaxTimer >= TaxIntervalSeconds)
	{
		TaxTimer -= TaxIntervalSeconds;
		CollectTaxes();
	}
}

void UEconomySubsystem::CollectTaxes()
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Economy runs after Population, so this reads up-to-date net worth.
	const UPopulationSubsystem* Population = World->GetSubsystem<UPopulationSubsystem>();
	if (!Population)
	{
		return;
	}

	const float Revenue = Population->TotalNetWorth() * TaxRate;
	Budget += Revenue;

	UE_LOG(LogAwsim, Log, TEXT("Tax collected: +%.0f (budget now %.0f) from %d citizens."),
		Revenue, Budget, Population->NumCitizens());
}

bool UEconomySubsystem::TrySpend(float Amount)
{
	if (Amount < 0.f || Amount > Budget)
	{
		return false;
	}
	Budget -= Amount;
	return true;
}
