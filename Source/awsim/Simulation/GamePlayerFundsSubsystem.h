#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GamePlayerFundsSubsystem.generated.h"

// The player's treasury. Authoritative saved state, NOT a sim
// phase: it never steps on the clock. Mutated by events only — the Economy
// phase deposits taxes; placement/upkeep spend from it.
UCLASS()
class AWSIM_API UGamePlayerFundsSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	float GetBalance() const { return Balance; }

	bool CanAfford(float Cost) const { return Cost <= Balance; }

	// Deducts Cost if affordable; returns whether the spend happened.
	// Override forces the spend through regardless of balance.
	bool TrySpend(float Cost, bool Override = false);

	// Deposits buffer during a sim step and land in Balance together when the
	// orchestrator calls CommitDeposits at the end of the step. This gives every
	// tick two clear money phases — subtraction while phases step, then one
	// addition — so subsystems stay synchronized regardless of phase order.
	void Deposit(float Amount);
	void CommitDeposits();
	float GetPendingDeposits() const { return PendingDeposits; }

private:
	UPROPERTY() float Balance = 0.f;

	UPROPERTY(Transient) float PendingDeposits = 0.f;
};
