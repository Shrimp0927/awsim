#pragma once

#include "CoreMinimal.h"
#include "Simulation/GameSimPhase.h"
#include "GameCityStatsSubsystem.generated.h"

UCLASS()
class AWSIM_API UCityStatsSubsystem : public USimPhase
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Step(float StepSeconds) override;
	virtual int32 PhaseOrder() const override { return 899; }

	int32 GetPopulation() const { return Population; }
	float GetPlayerRating() const { return PlayerRating; }

	static float Clamp(float Rating)
	{
		return FMath::Clamp(Rating, 0.f, 1.f);
	}

private:
#if !UE_BUILD_SHIPPING
	// Dev-only on-screen dump of the whole sim's macro state, drawn each step
	// while `awsim.DebugStats 1`. CityStats runs last (899), so every domain's
	// numbers for this tick are final when this reads them.
	void DrawDebugStats() const;
#endif

	UPROPERTY() int32 Population = 0;
	UPROPERTY() float PlayerRating = 0.5f;
};
