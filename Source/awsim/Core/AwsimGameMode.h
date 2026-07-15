#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "AwsimGameMode.generated.h"

// Session rules for awsim: the player is a god-view camera, not a character.
UCLASS()
class AWSIM_API AAwsimGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AAwsimGameMode();
};
