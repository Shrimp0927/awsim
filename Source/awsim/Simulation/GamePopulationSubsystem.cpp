#include "GamePopulationSubsystem.h"
#include "GameHousingSubsystem.h"
#include "GameEconomySubsystem.h"
#include "Engine/World.h"

void UPopulationSubsystem::Step(float StepSeconds)
{
	const UHousingSubsystem* HousingSubsystem = ResolveHousing();
	const UEconomySubsystem* EconomySubsystem = ResolveEconomy();
	if (!HousingSubsystem || !EconomySubsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("Population: Housing/Economy subsystem not found!"));
		return;
	}

	// People only live in serviced homes; unserviced capacity holds nobody.
	const float Target = FMath::Max(0.f, HousingSubsystem->GetServicedCapacity());

	// NOTE: placeholder growth model — exponential gap close, GDP boosts speed up to 2x.
	const float EconomyBoost = FMath::Clamp(EconomySubsystem->GetGDP() * 0.001f, 0.f, 1.f);
	const float Speed = BaseGrowthSpeed * (1.f + EconomyBoost);

	Count = FMath::Max(0.f, FMath::FInterpTo(Count, Target, StepSeconds, Speed));
}

UHousingSubsystem* UPopulationSubsystem::ResolveHousing() const
{
	if (Housing)
	{
		return Housing;
	}
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UHousingSubsystem>() : nullptr;
}

UEconomySubsystem* UPopulationSubsystem::ResolveEconomy() const
{
	if (Economy)
	{
		return Economy;
	}
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UEconomySubsystem>() : nullptr;
}
