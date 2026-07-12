#pragma once

#include "CoreMinimal.h"
#include "Simulation/GameSimPhase.h"
#include "Entities/GridCoord.h"
#include "GameGridSubsystem.h"
#include "GameEditSubsystem.generated.h"

// One queued player edit: set a placed building's slider to a new value.
USTRUCT()
struct FSliderEdit
{
	GENERATED_BODY()

	UPROPERTY() FGridCoord Target;
	UPROPERTY() int32 SliderIndex = 0;
	UPROPERTY() float Value = 0.f;
};

// Player-edit queue phase. The UI queues edits at any time; they apply FIFO
// when this phase steps (order 150 — the input band before the grid at 200),
// so every phase of a tick sees a fully-edited world. Edits always target a
// building placed on a past tick, so running before placement processing is
// never an ordering problem. Slider edits are the only feature for now.
UCLASS()
class AWSIM_API UEditSubsystem : public USimPhase
{
	GENERATED_BODY()

public:
	virtual void Step(float StepSeconds) override;
	virtual int32 PhaseOrder() const override { return 150; }

	void QueueSliderEdit(FGridCoord Target, int32 SliderIndex, float Value);
	int32 NumPendingEdits() const { return PendingEdits.Num(); }

	// Injectable for world-less specs; resolved from the owning world when unset.
	void SetGrid(UGridSubsystem* InGrid) { Grid = InGrid; }

private:
	UGridSubsystem* ResolveGrid() const;

	UPROPERTY(Transient)
	TObjectPtr<UGridSubsystem> Grid;

	UPROPERTY(Transient)
	TArray<FSliderEdit> PendingEdits;
};
