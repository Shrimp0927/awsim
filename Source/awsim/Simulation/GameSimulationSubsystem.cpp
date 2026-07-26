#include "Simulation/GameSimulationSubsystem.h"
#include "Simulation/GameSimPhase.h"
#include "Simulation/GamePlayerFundsSubsystem.h"
#include "awsim.h"
#include "Core/GameSaveSubsystem.h"
#include "Engine/World.h"

void USimulationSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	RebuildPhaseOrder();
	bRunning = false; // the flow subsystem starts the clock when leaving the menu

	UE_LOG(LogAwsim, Log, TEXT("Simulation ready with %d phase(s)."), OrderedPhases.Num());
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
		// Paused: input-band phases still pump with dt = 0; no sim time passes.
		for (USimPhase* Phase : OrderedPhases)
		{
			if (Phase && Phase->StepsWhilePaused())
			{
				Phase->Step(0.f);
			}
		}
		NotifySaveCheckpoint();
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

	// Cap hit with debt left: drop the backlog so the sim slows instead of spiraling.
	if (Accumulator >= FixedStep)
	{
		Accumulator = 0.f;
	}

	NotifySaveCheckpoint();
}

void USimulationSubsystem::NotifySaveCheckpoint()
{
	UWorld* World = GetWorld();
	if (UGameSaveSubsystem* Save = World ? World->GetSubsystem<UGameSaveSubsystem>() : nullptr)
	{
		Save->NotifyStepCompleted();
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

	// Deposits land together at end of step: subtractions first, one synchronized addition.
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
