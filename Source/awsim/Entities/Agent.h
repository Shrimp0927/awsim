#pragma once

#include "CoreMinimal.h"
#include "Agent.generated.h"

/**
 * What kind of visual agent this is. Drives mesh, animation set, lifespan and
 * movement behaviour (see FAgentKindDef). Extensible: add a value per new kind.
 */
UENUM()
enum class EAgentKind : uint8
{
	Pedestrian, // a "citizen" walking the streets
	Vehicle,    // a car on the road network
	Aircraft    // a plane using an airstrip
};

/**
 * Data-driven description of an agent *kind* (ARCHITECTURE.md §2.3).
 *
 * Defines how a kind looks and behaves — the "type", meant to be filled by a
 * Data Table / Data Asset in the editor rather than hardcoded. UAgentSubsystem
 * reads these to spawn the right ephemeral instances for the current game state.
 */
USTRUCT()
struct FAgentKindDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere) EAgentKind Kind = EAgentKind::Pedestrian;

	/** Seconds this kind stays on screen before it despawns. */
	UPROPERTY(EditAnywhere) float Lifespan = 8.f;

	/** World units per second while moving. */
	UPROPERTY(EditAnywhere) float MoveSpeed = 150.f;

	/** Animation set to play; bound to an asset by a Blueprint/Data Asset later. */
	UPROPERTY(EditAnywhere) FName AnimSet;
};

/**
 * One live visual agent — a citizen/car/plane currently on screen.
 *
 * PURE EPHEMERAL (DESIGN.md §4): this is NOT game state and is NOT saved. The
 * source of truth is the macro stats + the grid; agents are a disposable
 * *projection* of that state (ARCHITECTURE.md §5), spawned and recycled in a
 * cycle by UAgentSubsystem. Carries only what's needed to move, animate, and
 * time out.
 */
USTRUCT()
struct FAgent
{
	GENERATED_BODY()

	UPROPERTY() EAgentKind Kind = EAgentKind::Pedestrian;
	UPROPERTY() FVector Position = FVector::ZeroVector;
	UPROPERTY() FVector Velocity = FVector::ZeroVector;

	/** Seconds alive so far; once Age >= Lifespan the agent despawns. */
	UPROPERTY() float Age = 0.f;
	UPROPERTY() float Lifespan = 8.f;
};
