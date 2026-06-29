#pragma once

#include "CoreMinimal.h"
#include "Simulation/SimPhase.h"
#include "PopulationSubsystem.generated.h"

UCLASS()
class AWSIM_API UPopulationSubsystem : public USimPhase
{
	GENERATED_BODY()

public:
	virtual void Step(float StepSeconds) override;
	virtual int32 PhaseOrder() const override { return 300; }
};
