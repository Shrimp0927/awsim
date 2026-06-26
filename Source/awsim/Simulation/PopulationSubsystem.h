#pragma once

#include "CoreMinimal.h"
#include "Simulation/SimPhase.h"
#include "Entities/Citizen.h"
#include "PopulationSubsystem.generated.h"

/**
 * Owns every citizen's data and advances their daily state each step.
 *
 * Citizens are plain structs in a contiguous array (ARCHITECTURE.md §5); this
 * subsystem is their single source of truth. The per-citizen update is
 * independent, so it's a natural fit for data-parallelism later (§6) — kept as
 * a simple loop for now.
 */
UCLASS()
class AWSIM_API UPopulationSubsystem : public USimPhase
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Step(float StepSeconds) override;
	virtual int32 PhaseOrder() const override { return 400; }

	int32 NumCitizens() const { return Citizens.Num(); }

	/** Average productivity across the population, 0..1. Loose city-health readout. */
	float AverageProductivity() const;

	/** Sum of every citizen's net worth — the tax base for the Economy phase. */
	float TotalNetWorth() const;

	/** Spawn Count citizens with default state. Temporary seeding for first tests. */
	void SeedCitizens(int32 Count);

private:
	UPROPERTY(Transient)
	TArray<FCitizen> Citizens;
};
