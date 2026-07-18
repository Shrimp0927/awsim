#pragma once

#include "CoreMinimal.h"
#include "Simulation/GameSimPhase.h"
#include "Tickable.h"
#include "GameCityStatsSubsystem.generated.h"

UCLASS()
class AWSIM_API UCityStatsSubsystem : public USimPhase, public FTickableGameObject
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Step(float StepSeconds) override;
	virtual int32 PhaseOrder() const override { return 899; }

	// Draws on the frame tick so the overlay keeps updating while the sim is paused.
	virtual void Tick(float DeltaSeconds) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;

	int32 GetPopulation() const { return Population; }
	float GetPlayerRating() const { return PlayerRating; }

	static float Clamp(float Rating)
	{
		return FMath::Clamp(Rating, 0.f, 1.f);
	}

private:
#if !UE_BUILD_SHIPPING
	// On-screen dump while `awsim.DebugStats 1`; at phase 899 every domain's numbers are final.
	void DrawDebugStats() const;
#endif

	UPROPERTY() int32 Population = 0;
	UPROPERTY() float PlayerRating = 0.5f;
};
