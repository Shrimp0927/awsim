#pragma once

#include "CoreMinimal.h"
#include "Simulation/SimPhase.h"
#include "EconomySubsystem.generated.h"

/**
 * The Overlord's treasury and the tax cycle (DESIGN.md §5).
 *
 * Runs after Population (PhaseOrder 500) so it taxes citizens' freshly updated
 * net worth. Each tax cycle it takes a % of total net worth into the budget,
 * which is later spent on buildings & services.
 *
 * Skeleton: the numbers are loose placeholders and spending/building costs come
 * later — for now this just closes the collect half of the core loop.
 */
UCLASS()
class AWSIM_API UEconomySubsystem : public USimPhase
{
	GENERATED_BODY()

public:
	virtual void Step(float StepSeconds) override;
	virtual int32 PhaseOrder() const override { return 500; }

	// --- Treasury ---
	float GetBudget() const { return Budget; }
	void AddFunds(float Amount) { Budget += Amount; }

	/** Try to spend Amount. Returns false and spends nothing if unaffordable. */
	bool TrySpend(float Amount);

private:
	void CollectTaxes();

	/** Overlord's treasury. */
	UPROPERTY(Transient)
	float Budget = 0.f;

	/** Fraction of each citizen's net worth taken per tax cycle. Loose default. */
	UPROPERTY()
	float TaxRate = 0.02f;

	/** Sim-seconds between collections. Stands in for "every X days" until a
	 *  TimeSubsystem owns the calendar. */
	UPROPERTY()
	float TaxIntervalSeconds = 10.f;

	/** Time accrued toward the next collection. */
	float TaxTimer = 0.f;
};
