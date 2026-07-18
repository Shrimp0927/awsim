#pragma once

#include "CoreMinimal.h"
#include "Simulation/GameSimPhase.h"
#include "Entities/GridCoord.h"
#include "Entities/GridContent.h"
#include "GameGridSubsystem.h"
#include "GamePopulationSubsystem.generated.h"

class UHousingSubsystem;
class UEconomySubsystem;

UCLASS()
class AWSIM_API UPopulationSubsystem : public USimPhase
{
	GENERATED_BODY()

public:
	virtual void Step(float StepSeconds) override;
	virtual int32 PhaseOrder() const override { return 600; }

	int32 GetCount() const { return FMath::RoundToInt32(Count); }

	// Directly establish a population (save/load, scenarios, specs).
	void SetCount(float InCount) { Count = FMath::Max(0.f, InCount); }

	// Injectable for world-less specs; resolved from the owning world when unset.
	void SetHousing(UHousingSubsystem* InHousing) { Housing = InHousing; }
	void SetEconomy(UEconomySubsystem* InEconomy) { Economy = InEconomy; }

private:
	UHousingSubsystem* ResolveHousing() const;
	UEconomySubsystem* ResolveEconomy() const;

	UPROPERTY(Transient)
	TObjectPtr<UHousingSubsystem> Housing;

	UPROPERTY(Transient)
	TObjectPtr<UEconomySubsystem> Economy;

	// Grows toward serviced housing capacity over time; authoritative saved state.
	UPROPERTY() float Count = 0.f;

	float BaseGrowthSpeed = 0.1f; // fraction of the gap closed per second
};
