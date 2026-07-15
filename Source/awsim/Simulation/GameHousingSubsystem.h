#pragma once

#include "CoreMinimal.h"
#include "Simulation/GameSimPhase.h"
#include "Entities/GridCoord.h"
#include "Entities/GridContent.h"
#include "GameGridSubsystem.h"
#include "GameHousingSubsystem.generated.h"

class UGamePlayerFundsSubsystem;
class UEnergySubsystem;
class UGameWaterSubsystem;

UCLASS()
class AWSIM_API UHousingSubsystem : public USimPhase
{
	GENERATED_BODY()

public:
	virtual void Step(float StepSeconds) override;
	virtual int32 PhaseOrder() const override { return 203; }

	// Raw housing capacity: signed sum of Housing effects across all islands.
	float GetCapacity() const { return Capacity; }

	// Only islands serviced by BOTH energy and water contribute; population
	// grows toward this, not raw capacity.
	float GetServicedCapacity() const { return ServicedCapacity; }

	// Daily tax owed by serviced homes, deposited on day rollover.
	float GetTaxRevenue() const { return TaxRevenue; }

	// Injectable for world-less specs; resolved from the owning world when unset.
	void SetGrid(UGridSubsystem* InGrid) { Grid = InGrid; }
	void SetEnergy(UEnergySubsystem* InEnergy) { Energy = InEnergy; }
	void SetWater(UGameWaterSubsystem* InWater) { Water = InWater; }
	void SetFunds(UGamePlayerFundsSubsystem* InFunds) { Funds = InFunds; }

	// Tax revenue is queued until the orchestrator commits deposits at end of step.
	void SettleDay();

private:
	void Recompute(const UGridSubsystem& GridSubsystem, const UEnergySubsystem& EnergySubsystem, const UGameWaterSubsystem& WaterSubsystem);
	UGridSubsystem* ResolveGrid() const;
	UEnergySubsystem* ResolveEnergy() const;
	UGameWaterSubsystem* ResolveWater() const;
	UGamePlayerFundsSubsystem* ResolveFunds() const;

	UPROPERTY(Transient)
	TObjectPtr<UGridSubsystem> Grid;

	UPROPERTY(Transient)
	TObjectPtr<UEnergySubsystem> Energy;

	UPROPERTY(Transient)
	TObjectPtr<UGameWaterSubsystem> Water;

	UPROPERTY(Transient)
	TObjectPtr<UGamePlayerFundsSubsystem> Funds;

	float Capacity = 0.f;
	float ServicedCapacity = 0.f;
	float TaxRevenue = 0.f;

	int32 LastSettledDay = INDEX_NONE;
};
