#pragma once

#include "CoreMinimal.h"
#include "Simulation/GameSimPhase.h"
#include "Entities/GridCoord.h"
#include "Entities/GridContent.h"
#include "GameGridSubsystem.h"
#include "GameEconomySubsystem.generated.h"

class UEnergySubsystem;

UCLASS()
class AWSIM_API UEconomySubsystem : public USimPhase
{
	GENERATED_BODY()

public:
	virtual void Step(float StepSeconds) override;
	virtual int32 PhaseOrder() const override { return 204; }

	// Signed sum of Economy effects on energy-serviced islands. The economy is
	// pegged to energy only: a business needs power to produce, not water.
	float GetGDP() const { return GDP; }

	// Injectable for world-less specs; resolved from the owning world when unset.
	void SetGrid(UGridSubsystem* InGrid) { Grid = InGrid; }
	void SetEnergy(UEnergySubsystem* InEnergy) { Energy = InEnergy; }

private:
	void Recompute(const UGridSubsystem& GridSubsystem, const UEnergySubsystem& EnergySubsystem);
	UGridSubsystem* ResolveGrid() const;
	UEnergySubsystem* ResolveEnergy() const;

	UPROPERTY(Transient)
	TObjectPtr<UGridSubsystem> Grid;

	UPROPERTY(Transient)
	TObjectPtr<UEnergySubsystem> Energy;

	float GDP = 0.f;
};
