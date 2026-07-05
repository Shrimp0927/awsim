#pragma once

#include "CoreMinimal.h"
#include "Simulation/SimPhase.h"
#include "Entities/GridCoord.h"
#include "Entities/GridContent.h"
#include "GridSubsystem.h"
#include "EnergySubsystem.generated.h"

UCLASS()
class AWSIM_API UEnergySubsystem : public USimPhase
{
	GENERATED_BODY()
	
public:
	virtual void Step(float StepSeconds) override;
	virtual int32 PhaseOrder() const override { return 201; }

};
