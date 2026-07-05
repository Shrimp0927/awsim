#pragma once

#include "CoreMinimal.h"
#include "Simulation/SimPhase.h"
#include "Entities/GridCoord.h"
#include "Entities/GridContent.h"
#include "GridSubsystem.h"
#include "WaterSubsystem.generated.h"

UCLASS()
class AWSIM_API UWaterSubsystem : public USimPhase
{
	GENERATED_BODY()

public:
	virtual void Step(float StepSeconds) override;
	virtual int32 PhaseOrder() const override { return 202; }

};
