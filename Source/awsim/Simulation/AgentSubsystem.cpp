#include "Simulation/AgentSubsystem.h"
#include "Simulation/CityStatsSubsystem.h"
#include "Engine/World.h"

void UAgentSubsystem::Step(float StepSeconds)
{
	// Age the pool and recycle anything past its lifespan.
	for (int32 i = Agents.Num() - 1; i >= 0; --i)
	{
		Agents[i].Age += StepSeconds;
		if (Agents[i].Age >= Agents[i].Lifespan)
		{
			Agents.RemoveAtSwap(i);
		}
	}

	// Top up toward the count the macro state wants on screen, so agents appear
	// and disappear in a continuous cycle. Kind mix, spawn placement, movement,
	// and ISM rendering are all TODO.
	const int32 Desired = DesiredAgentCount();
	while (Agents.Num() < Desired)
	{
		SpawnAgent(EAgentKind::Pedestrian);
	}
}

int32 UAgentSubsystem::DesiredAgentCount() const
{
	const UWorld* World = GetWorld();
	const UCityStatsSubsystem* Stats = World ? World->GetSubsystem<UCityStatsSubsystem>() : nullptr;
	if (!Stats || PeoplePerAgent <= 0)
	{
		return 0;
	}
	return Stats->GetPopulation() / PeoplePerAgent;
}

void UAgentSubsystem::SpawnAgent(EAgentKind Kind)
{
	FAgent Agent;
	Agent.Kind = Kind;
	// Lifespan would come from the kind's FAgentKindDef (data-driven); loose
	// default for now.
	Agent.Lifespan = 8.f;
	Agents.Add(Agent);
}
