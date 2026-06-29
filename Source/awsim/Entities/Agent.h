#pragma once

#include "CoreMinimal.h"
#include "Agent.generated.h"

UENUM()
enum class EAgentKind : uint8
{
	None,
	Pedestrian,
	Car,
	Aircraft,
	Rocket,
	Boat
};

USTRUCT()
struct FAgentKindDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere) EAgentKind Kind = EAgentKind::None;
	UPROPERTY(EditAnywhere) float Lifespan = 8.f; // Lifespan in seconds
	UPROPERTY(EditAnywhere) float MoveSpeed = 150.f;
	UPROPERTY(EditAnywhere) FName AnimSet;
};

USTRUCT()
struct FAgent
{
	GENERATED_BODY()

	UPROPERTY() EAgentKind Kind = EAgentKind::None;
	UPROPERTY() FVector Position = FVector::ZeroVector;
	UPROPERTY() FVector Velocity = FVector::ZeroVector;
	UPROPERTY() float Age = 0.f;
	UPROPERTY() float Lifespan = 8.f;
};
