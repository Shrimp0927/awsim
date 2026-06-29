#pragma once

#include "CoreMinimal.h"
#include "Simulation/SimPhase.h"
#include "CityStatsSubsystem.generated.h"

/**
 * The city's macro stats — the top-down source of truth for "how's the city
 * doing" (DESIGN.md §4).
 *
 * Under the macro model individual citizens are not simulated; instead a handful
 * of aggregate numbers, derived from the grid each tick, drive the whole game and
 * everything that gets visualised. Runs as the last simulation phase (PhaseOrder
 * 800) — right before the Agent crowd projects it (order 900) — so the visualised
 * crowd reflects the freshest stats. It reads the finished grid, and exposes a
 * taxable-wealth figure for a future economy phase (the previous EconomySubsystem
 * was scrapped pending rework).
 */
UCLASS()
class AWSIM_API UCityStatsSubsystem : public USimPhase
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Step(float StepSeconds) override;
	virtual int32 PhaseOrder() const override { return 800; }

	// --- Macro readouts ---
	int32 GetPopulation() const { return Population; }
	float GetAverageSatisfaction() const { return AverageSatisfaction; } // 0..1
	float GetOverlordRating() const { return OverlordRating; }           // 0..1

	/** Aggregate citizen wealth — the base a future economy phase will tax. */
	float GetTaxableWealth() const { return TaxableWealth; }

	/** A future economy phase calls this to remove taxed wealth from the populace. */
	void DeductWealth(float Amount) { TaxableWealth = FMath::Max(0.f, TaxableWealth - Amount); }

	/** Pure mapping from average satisfaction to Overlord rating. Unit-tested. */
	static float RatingFromSatisfaction(float Satisfaction)
	{
		return FMath::Clamp(Satisfaction, 0.f, 1.f);
	}

private:
	// Authoritative macro state (NOT Transient — the save layer persists it).
	UPROPERTY() int32 Population = 0;
	UPROPERTY() float AverageSatisfaction = 0.5f;
	UPROPERTY() float OverlordRating = 0.5f;
	UPROPERTY() float TaxableWealth = 0.f;
};
