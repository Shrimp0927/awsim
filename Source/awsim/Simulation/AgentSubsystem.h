#pragma once

#include "CoreMinimal.h"
#include "Simulation/SimPhase.h"
#include "Entities/Agent.h"
#include "AgentSubsystem.generated.h"

UCLASS()
class AWSIM_API UAgentSubsystem : public USimPhase
{
	GENERATED_BODY()

public:
	virtual void Step(float StepSeconds) override;
	virtual int32 PhaseOrder() const override { return 900; }

	int32 NumAgents() const { return Agents.Num(); }

private:
	int32 DesiredAgentCount() const;

	void SpawnAgent(EAgentKind Kind);

	UPROPERTY(Transient)
	TArray<FAgent> Agents;

	int32 PeoplePerAgent = 10;
};
