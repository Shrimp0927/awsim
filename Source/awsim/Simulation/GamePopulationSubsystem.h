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

	// Population is a macro quantity with growth dynamics: it moves toward the
	// serviced housing capacity over time rather than jumping, at a speed the
	// economy modulates. Authoritative saved state.
	UPROPERTY() float Count = 100.f; // seed population

	float BaseGrowthSpeed = 0.1f; // fraction of the gap closed per second
};
