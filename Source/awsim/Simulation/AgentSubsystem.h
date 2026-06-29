#pragma once

#include "CoreMinimal.h"
#include "Simulation/SimPhase.h"
#include "Entities/Agent.h"
#include "AgentSubsystem.generated.h"

/**
 * The crowd: spawns, ages, and recycles the ephemeral visual agents
 * (citizens/cars/planes) that represent the macro state on screen.
 *
 * Representation layer, not simulation (ARCHITECTURE.md §4/§5): agents are a
 * disposable projection of the macro stats + grid, never the source of truth.
 * Runs last (PhaseOrder 900) so it reads finished grid/macro/economy output. The
 * pool is PURE EPHEMERAL — Transient, rebuilt at runtime, never saved.
 *
 * Skeleton: the lifecycle (spawn-to-target, age out) lives here; the actual
 * movement, animation, and ISM rendering are TODO (no visuals are built yet).
 */
UCLASS()
class AWSIM_API UAgentSubsystem : public USimPhase
{
	GENERATED_BODY()

public:
	virtual void Step(float StepSeconds) override;
	virtual int32 PhaseOrder() const override { return 900; }

	int32 NumAgents() const { return Agents.Num(); }

private:
	/** How many agents should currently be visible, derived from the macro state. */
	int32 DesiredAgentCount() const;

	void SpawnAgent(EAgentKind Kind);

	/** Live visual agents. Ephemeral projection of macro state — Transient. */
	UPROPERTY(Transient)
	TArray<FAgent> Agents;

	/** Roughly one visible pedestrian per this many citizens. Loose. */
	int32 PeoplePerAgent = 10;
};
