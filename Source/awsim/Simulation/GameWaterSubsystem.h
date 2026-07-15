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

	// Index-aligned with the grid's GetIslands(); serviced when island-local
	// supply covers island-local demand.
	bool IsIslandServiced(int32 IslandIndex) const
	{
		return IslandServiced.IsValidIndex(IslandIndex) && IslandServiced[IslandIndex];
	}

	// Injectable for world-less specs; resolved from the owning world when unset.
	void SetFunds(UGamePlayerFundsSubsystem* InFunds) { Funds = InFunds; }
	void SetGrid(UGridSubsystem* InGrid) { Grid = InGrid; }

	// Maintenance is forced out of funds (balance may go negative); revenue is
	// queued until the orchestrator commits deposits at end of step.
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
