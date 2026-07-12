#include "GamePlayerFundsSubsystem.h"

bool UGamePlayerFundsSubsystem::TrySpend(float Cost, bool Override)
{
	if (Override)
	{
		Balance -= Cost;
		return true;
	}
	if (Cost < 0.f || !CanAfford(Cost))
	{
		return false;
	}
	Balance -= Cost;
	return true;
}

void UGamePlayerFundsSubsystem::Deposit(float Amount)
{
	if (Amount > 0.f)
	{
		PendingDeposits += Amount;
	}
}

void UGamePlayerFundsSubsystem::CommitDeposits()
{
	Balance += PendingDeposits;
	PendingDeposits = 0.f;
}
