#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "SimulationSubsystem.generated.h"

class USimPhase;

/**
 * The single clock for the whole simulation.
 *
 * Owns the fixed-timestep accumulator and the game-speed multiplier, and steps
 * every USimPhase in PhaseOrder once per fixed step (ARCHITECTURE.md §6). This is
 * the ONLY thing in the sim that ticks per frame; everything else is stepped from
 * here so phase order — and therefore the result — is deterministic.
 */
UCLASS()
class AWSIM_API USimulationSubsystem : public UWorldSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	// UWorldSubsystem
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	// FTickableGameObject
	virtual void Tick(float DeltaSeconds) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;

	// --- Speed control (pause / 1x / 2x / 3x ...) ---
	void SetGameSpeed(float Speed) { GameSpeed = FMath::Max(0.f, Speed); }
	float GetGameSpeed() const { return GameSpeed; }
	void SetPaused(bool bPaused) { bRunning = !bPaused; }
	bool IsRunning() const { return bRunning; }

	/** In-game days elapsed. Loose timekeeping for now. */
	int32 GetDay() const { return DayCount; }

private:
	void StepOnce();
	void RebuildPhaseOrder();

	/** Phases sorted by PhaseOrder(), low to high. Rebuilt on begin play. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USimPhase>> OrderedPhases;

	/** Seconds in one fixed simulation step. */
	float FixedStep = 1.f / 30.f;

	/** Real (speed-scaled) seconds owed to the sim. */
	float Accumulator = 0.f;

	float GameSpeed = 1.f;
	bool bRunning = false;

	// Loose day clock: StepsPerDay fixed steps == one in-game day, for now.
	int32 StepsPerDay = 300;
	int32 StepCounter = 0;
	int32 DayCount = 0;
};
