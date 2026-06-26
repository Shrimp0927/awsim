#pragma once

#include "CoreMinimal.h"
#include "Citizen.generated.h"

/** Coarse activity state of a citizen during a day. */
UENUM()
enum class ECitizenState : uint8
{
	AtHome,
	Commuting,
	Working,
	Leisure
};

/**
 * One simulated citizen.
 *
 * Plain value data — NOT a UObject. Thousands of these live contiguously in a
 * TArray owned by UPopulationSubsystem (see ARCHITECTURE.md §5). Keep it cheap
 * and POD-friendly so the daily update can run data-parallel later.
 *
 * Two tiers of state (DESIGN.md §4):
 *   - long-term: net worth, health, satisfaction  -> drives the Overlord rating
 *   - daily:     hunger, energy, mood              -> drives productivity
 */
USTRUCT()
struct FCitizen
{
	GENERATED_BODY()

	// ---- Long-term state (changes slowly) ----
	UPROPERTY() float NetWorth = 0.f;
	UPROPERTY() float Health = 1.f;        // 0..1
	UPROPERTY() float Satisfaction = 0.5f; // 0..1

	// ---- Daily state (changes each day) ----
	UPROPERTY() float Hunger = 0.f; // 0 = full, 1 = starving
	UPROPERTY() float Energy = 1.f; // 1 = rested, 0 = exhausted
	UPROPERTY() float Mood = 0.5f;  // 0..1

	UPROPERTY() ECitizenState State = ECitizenState::AtHome;

	/** How effectively this citizen works right now, 0..1. Derived from daily state. */
	float Productivity() const
	{
		// Loose first pass: hungry / tired / low-mood citizens work below max.
		const float Fed = 1.f - Hunger;
		return FMath::Clamp((Fed + Energy + Mood) / 3.f, 0.f, 1.f);
	}
};
