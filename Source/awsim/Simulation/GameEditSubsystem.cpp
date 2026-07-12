#include "GameEditSubsystem.h"
#include "Engine/World.h"

void UEditSubsystem::Step(float StepSeconds)
{
	UGridSubsystem* GridSubsystem = ResolveGrid();
	if (!GridSubsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("GridSubsystem not found!"));
		PendingEdits.Reset();
		return;
	}

	// FIFO: later edits to the same slider overwrite earlier ones. Edits with
	// an invalid target/index are dropped by the grid.
	for (const FSliderEdit& Edit : PendingEdits)
	{
		GridSubsystem->SetSliderValue(Edit.Target, Edit.SliderIndex, Edit.Value);
	}
	PendingEdits.Reset();
}

void UEditSubsystem::QueueSliderEdit(FGridCoord Target, int32 SliderIndex, float Value)
{
	FSliderEdit Edit;
	Edit.Target = Target;
	Edit.SliderIndex = SliderIndex;
	Edit.Value = Value;
	PendingEdits.Add(Edit);
}

UGridSubsystem* UEditSubsystem::ResolveGrid() const
{
	if (Grid)
	{
		return Grid;
	}
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UGridSubsystem>() : nullptr;
}
