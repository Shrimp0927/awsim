#include "WaterSubsystem.h"

void UWaterSubsystem::Step(float StepSeconds)
{
	// Grab islands from the grid subsystem and process water flow
	UGridSubsystem* GridSubsystem = GetWorld()->GetSubsystem<UGridSubsystem>();
	if (GridSubsystem)
	{
		const TArray<TArray<FGridCoord>>& Islands = GridSubsystem->GetIslands();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("GridSubsystem not found!"));
	}
}
