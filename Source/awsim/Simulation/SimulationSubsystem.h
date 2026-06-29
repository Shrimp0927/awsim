#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "SimulationSubsystem.generated.h"

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

private:
	void StepOnce();
	void RebuildPhaseOrder();

	UPROPERTY(Transient)
	TArray<TObjectPtr<USimPhase>> OrderedPhases;

	float FixedStep = 1.f / 30.f;

	float Accumulator = 0.f;

	float GameSpeed = 1.f;
	bool bRunning = false;

	int32 StepsPerDay = 300;
	int32 StepCounter = 0;
	int32 DayCount = 0;
};
