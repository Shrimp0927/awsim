#include "Core/AwsimGameMode.h"
#include "Camera/GameCameraPawn.h"

AAwsimGameMode::AAwsimGameMode()
{
	DefaultPawnClass = AGameCameraPawn::StaticClass();
}
