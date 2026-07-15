#include "Simulation/GameCityStatsSubsystem.h"
#include "Simulation/GamePopulationSubsystem.h"
#include "Engine/World.h"

#if !UE_BUILD_SHIPPING
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"
#include "Simulation/GameSimulationSubsystem.h"
#include "Simulation/GameGridSubsystem.h"
#include "Simulation/GameEnergySubsystem.h"
#include "Simulation/GameWaterSubsystem.h"
#include "Simulation/GameHousingSubsystem.h"
#include "Simulation/GameEconomySubsystem.h"
#include "Simulation/GameAgentSubsystem.h"
#include "Simulation/GamePlayerFundsSubsystem.h"

static TAutoConsoleVariable<int32> CVarAwsimDebugStats(
	TEXT("awsim.DebugStats"), 0,
	TEXT("1 = draw the awsim macro stats on screen each sim step."));
#endif

void UCityStatsSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

}

void UCityStatsSubsystem::Step(float StepSeconds)
{
	Super::Step(StepSeconds);

	// Aggregate from the domain phases (this runs at 899, after all of them).
	const UWorld* World = GetWorld();
	if (const UPopulationSubsystem* PopulationSubsystem = World ? World->GetSubsystem<UPopulationSubsystem>() : nullptr)
	{
		Population = PopulationSubsystem->GetCount();
	}
}

void UCityStatsSubsystem::Tick(float DeltaSeconds)
{
#if !UE_BUILD_SHIPPING
	if (CVarAwsimDebugStats.GetValueOnGameThread() != 0)
	{
		DrawDebugStats();
	}
#endif
}

bool UCityStatsSubsystem::IsTickable() const
{
#if !UE_BUILD_SHIPPING
	const UWorld* World = GetWorld();
	return World && World->IsGameWorld();
#else
	return false;
#endif
}

TStatId UCityStatsSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UCityStatsSubsystem, STATGROUP_Tickables);
}

#if !UE_BUILD_SHIPPING
void UCityStatsSubsystem::DrawDebugStats() const
{
	const UWorld* World = GetWorld();
	if (!GEngine || !World)
	{
		return;
	}

	const USimulationSubsystem* Sim = World->GetSubsystem<USimulationSubsystem>();
	const UGridSubsystem* Grid = World->GetSubsystem<UGridSubsystem>();
	const UGamePlayerFundsSubsystem* Funds = World->GetSubsystem<UGamePlayerFundsSubsystem>();
	const UEnergySubsystem* Energy = World->GetSubsystem<UEnergySubsystem>();
	const UGameWaterSubsystem* Water = World->GetSubsystem<UGameWaterSubsystem>();
	const UHousingSubsystem* Housing = World->GetSubsystem<UHousingSubsystem>();
	const UEconomySubsystem* Economy = World->GetSubsystem<UEconomySubsystem>();
	const UAgentSubsystem* Agents = World->GetSubsystem<UAgentSubsystem>();

	// Stable keys so each line updates in place instead of stacking.
	int32 Key = 71000;
	auto Draw = [&Key](const FString& Text, FColor Color)
	{
		GEngine->AddOnScreenDebugMessage(Key++, 1.f, Color, Text);
	};

	if (Sim)
	{
		Draw(FString::Printf(TEXT("[awsim] day %d  speed %.1fx%s"),
			Sim->GetDay(), Sim->GetGameSpeed(), Sim->IsRunning() ? TEXT("") : TEXT("  PAUSED")), FColor::White);
	}
	if (Funds)
	{
		Draw(FString::Printf(TEXT("funds   %.0f  (pending +%.0f)"),
			Funds->GetBalance(), Funds->GetPendingDeposits()), FColor::Yellow);
	}
	if (Energy)
	{
		Draw(FString::Printf(TEXT("energy  %.1f / %.1f  (maint %.0f, rev %.0f)"),
			Energy->GetConsumption(), Energy->GetCapacity(), Energy->GetMaintenanceCost(), Energy->GetRevenue()), FColor::Orange);
	}
	if (Water)
	{
		Draw(FString::Printf(TEXT("water   %.1f / %.1f  (maint %.0f, rev %.0f)"),
			Water->GetConsumption(), Water->GetCapacity(), Water->GetMaintenanceCost(), Water->GetRevenue()), FColor::Cyan);
	}
	if (Housing)
	{
		Draw(FString::Printf(TEXT("housing %.1f serviced / %.1f total  (tax %.0f)"),
			Housing->GetServicedCapacity(), Housing->GetCapacity(), Housing->GetTaxRevenue()), FColor::Green);
	}
	if (Economy)
	{
		Draw(FString::Printf(TEXT("GDP     %.1f"), Economy->GetGDP()), FColor::Emerald);
	}
	Draw(FString::Printf(TEXT("pop     %d  (rating %.2f)"), Population, PlayerRating), FColor::White);
	if (Agents && Grid)
	{
		Draw(FString::Printf(TEXT("agents  %d   buildings %d   islands %d"),
			Agents->NumAgents(), Grid->GetBuildings().Num(), Grid->GetIslands().Num()), FColor::Silver);
	}
}
#endif // !UE_BUILD_SHIPPING
