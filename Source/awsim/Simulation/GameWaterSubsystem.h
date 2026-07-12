#pragma once

#include "CoreMinimal.h"
#include "Simulation/GameSimPhase.h"
#include "Entities/GridCoord.h"
#include "Entities/GridContent.h"
#include "GameGridSubsystem.h"
#include "GameWaterSubsystem.generated.h"

class UGamePlayerFundsSubsystem;

UCLASS()
class AWSIM_API UGameWaterSubsystem : public USimPhase
{
	GENERATED_BODY()

public:
	virtual void Step(float StepSeconds) override;
	virtual int32 PhaseOrder() const override { return 202; }

	float GetCapacity() const { return Capacity; }
	float GetConsumption() const { return Consumption; }
	float GetMaintenanceCost() const { return MaintenanceCost; }
	float GetRevenue() const { return Revenue; }

	// Per-island service result from the last Recompute, index-aligned with
	// the grid's GetIslands(): serviced when local supply covers local demand.
	// Downstream phases (Housing, Economy) gate on this.
	bool IsIslandServiced(int32 IslandIndex) const
	{
		return IslandServiced.IsValidIndex(IslandIndex) && IslandServiced[IslandIndex];
	}

	// Injectable for world-less specs; resolved from the owning world when unset.
	void SetFunds(UGamePlayerFundsSubsystem* InFunds) { Funds = InFunds; }
	void SetGrid(UGridSubsystem* InGrid) { Grid = InGrid; }

	// Daily settlement: maintenance is forced out of player funds (Override, so
	// the balance can go negative); revenue is queued and lands when the
	// orchestrator commits deposits at end of step. Called by Step on day
	// rollover; public so specs can drive it directly.
	void SettleDay();

private:
	void Recompute(const UGridSubsystem& GridSubsystem);
	UGamePlayerFundsSubsystem* ResolveFunds() const;
	UGridSubsystem* ResolveGrid() const;

	UPROPERTY(Transient)
	TObjectPtr<UGamePlayerFundsSubsystem> Funds;

	UPROPERTY(Transient)
	TObjectPtr<UGridSubsystem> Grid;

	TArray<bool> IslandServiced;

	float Capacity = 0.f;
	float Consumption = 0.f;
	float MaintenanceCost = 0.f;
	float Revenue = 0.f;

	int32 LastSettledDay = INDEX_NONE;
};
