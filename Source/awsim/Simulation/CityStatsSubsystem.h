#pragma once

#include "CoreMinimal.h"
#include "Simulation/SimPhase.h"
#include "CityStatsSubsystem.generated.h"

UCLASS()
class AWSIM_API UCityStatsSubsystem : public USimPhase
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Step(float StepSeconds) override;
	virtual int32 PhaseOrder() const override { return 800; }

	int32 GetPopulation() const { return Population; }
	float GetPlayerRating() const { return PlayerRating; }

	static float Clamp(float Rating)
	{
		return FMath::Clamp(Rating, 0.f, 1.f);
	}

private:
	UPROPERTY() int32 Population = 0;
	UPROPERTY() float PlayerRating = 0.5f;
};
