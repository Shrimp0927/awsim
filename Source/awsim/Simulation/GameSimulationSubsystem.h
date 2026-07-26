#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "GameSimulationSubsystem.generated.h"

class USimPhase;

UCLASS()
class AWSIM_API USimulationSubsystem : public UWorldSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	virtual void Tick(float DeltaSeconds) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;

	void SetGameSpeed(float Speed) { GameSpeed = FMath::Max(0.f, Speed); }
	float GetGameSpeed() const { return GameSpeed; }
	void SetPaused(bool bPaused) { bRunning = !bPaused; }
	bool IsRunning() const { return bRunning; }

	int32 GetDay() const { return DayCount; }
	int32 GetStepCounter() const { return StepCounter; }

	// Save/load only.
	void RestoreClock(int32 InDay, int32 InStepCounter)
	{
		DayCount = FMath::Max(0, InDay);
		StepCounter = FMath::Clamp(InStepCounter, 0, StepsPerDay - 1);
		Accumulator = 0.f;
	}

	// Injectable phase list for world-less specs; sorted by PhaseOrder.
	void SetPhases(const TArray<USimPhase*>& InPhases);

private:
	void StepOnce();
	void RebuildPhaseOrder();
	void NotifySaveCheckpoint();

	UPROPERTY(Transient)
	TArray<TObjectPtr<USimPhase>> OrderedPhases;

	float FixedStep = 1.f / 30.f;

	float Accumulator = 0.f;

	float GameSpeed = 1.f;
	bool bRunning = false;

	int32 StepsPerDay = 1000;
	int32 StepCounter = 0;
	int32 DayCount = 0;
};
