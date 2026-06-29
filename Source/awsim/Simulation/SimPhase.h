#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SimPhase.generated.h"

/**
 * Base class for one phase of the simulation.
 *
 * Phases are World subsystems that the orchestrator (USimulationSubsystem) steps
 * in a fixed order every sim tick — see ARCHITECTURE.md §6. They do NOT tick
 * themselves: a single clock stepping phases in order is the determinism
 * backbone, and Unreal does not guarantee tick order between subsystems.
 *
 * To add a phase: subclass this, pick a PhaseOrder, implement Step().
 */
UCLASS(Abstract)
class AWSIM_API USimPhase : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Advance this phase by one fixed step. Runs after every lower-ordered phase. */
	virtual void Step(float StepSeconds) {}

	/** Lower runs first. Suggested bands: Time=100, Grid=200, CityStats=800
	 *  (last sim phase), Agents=900 (representation; reads finished sim output). */
	virtual int32 PhaseOrder() const { return 0; }
};
