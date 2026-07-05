#include "EnergySubsystem.h"

void UEnergySubsystem::Step(float StepSeconds)
{
	// Grab islands from the grid subsystem and process energy flow
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
