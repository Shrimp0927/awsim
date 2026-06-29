#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SimPhase.generated.h"

UCLASS(Abstract)
class AWSIM_API USimPhase : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Step(float StepSeconds) {}

	virtual int32 PhaseOrder() const { return 0; }
};
