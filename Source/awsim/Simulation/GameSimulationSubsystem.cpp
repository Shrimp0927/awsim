#include "Simulation/GameSimulationSubsystem.h"
#include "Simulation/GameSimPhase.h"
#include "Simulation/GamePlayerFundsSubsystem.h"
#include "awsim.h"
#include "Engine/World.h"

void USimulationSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	RebuildPhaseOrder();
	bRunning = true;

	UE_LOG(LogAwsim, Log, TEXT("Simulation started with %d phase(s)."), OrderedPhases.Num());
}

void USimulationSubsystem::RebuildPhaseOrder()
{
	OrderedPhases.Reset();

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	SetPhases(World->GetSubsystemArrayCopy<USimPhase>());
}

void USimulationSubsystem::SetPhases(const TArray<USimPhase*>& InPhases)
{
	TArray<USimPhase*> Phases = InPhases;
	Phases.Sort([](const USimPhase& A, const USimPhase& B)
	{
		return A.PhaseOrder() < B.PhaseOrder();
	});

	OrderedPhases.Reset();
	OrderedPhases.Reserve(Phases.Num());
	for (USimPhase* Phase : Phases)
	{
		OrderedPhases.Add(Phase);
	}
}

bool USimulationSubsystem::IsTickable() const
{
	// Ticks while paused: pause gates the sim clock, not the tick.
	const UWorld* World = GetWorld();
	return World && World->IsGameWorld();
}

void USimulationSubsystem::Tick(float DeltaSeconds)
{
	if (OrderedPhases.Num() == 0)
	{
		return;
	}

	if (!bRunning)
	{
		// Paused: input-band phases still pump once per frame with dt = 0 so
		// player placements/edits keep applying; no sim time passes.
		for (USimPhase* Phase : OrderedPhases)
		{
			if (Phase && Phase->StepsWhilePaused())
			{
				Phase->Step(0.f);
			}
		}
		return;
	}

	Accumulator += DeltaSeconds * GameSpeed;

	const int32 MaxStepsPerFrame = 8;
	int32 Steps = 0;
	while (Accumulator >= FixedStep && Steps < MaxStepsPerFrame)
	{
		StepOnce();
		Accumulator -= FixedStep;
		++Steps;
	}
}

void USimulationSubsystem::StepOnce()
{
	for (USimPhase* Phase : OrderedPhases)
	{
		if (Phase)
		{
			Phase->Step(FixedStep);
		}
	}

	// Queued deposits land together at end of step: subtraction first, then
	// one synchronized addition.
	if (UWorld* World = GetWorld())
	{
		if (UGamePlayerFundsSubsystem* Funds = World->GetSubsystem<UGamePlayerFundsSubsystem>())
		{
			Funds->CommitDeposits();
		}
	}

	if (++StepCounter >= StepsPerDay)
	{
		StepCounter = 0;
		++DayCount;
	}
}

TStatId USimulationSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(USimulationSubsystem, STATGROUP_Tickables);
}
