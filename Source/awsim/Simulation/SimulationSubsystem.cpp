#include "Simulation/SimulationSubsystem.h"
#include "Simulation/SimPhase.h"
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

	// Collect every phase subsystem in this world and order them low -> high.
	TArray<USimPhase*> Phases = World->GetSubsystemArrayCopy<USimPhase>();
	Phases.Sort([](const USimPhase& A, const USimPhase& B)
	{
		return A.PhaseOrder() < B.PhaseOrder();
	});

	OrderedPhases.Reserve(Phases.Num());
	for (USimPhase* Phase : Phases)
	{
		OrderedPhases.Add(Phase);
	}
}

bool USimulationSubsystem::IsTickable() const
{
	const UWorld* World = GetWorld();
	return bRunning && World && World->IsGameWorld();
}

void USimulationSubsystem::Tick(float DeltaSeconds)
{
	if (!bRunning || OrderedPhases.Num() == 0)
	{
		return;
	}

	Accumulator += DeltaSeconds * GameSpeed;

	// Cap steps per frame so a hitch can't spiral into a catch-up freeze.
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
	// Serialized, ordered phases: each sees the completed result of the last.
	for (USimPhase* Phase : OrderedPhases)
	{
		if (Phase)
		{
			Phase->Step(FixedStep);
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
