#include "UI/GameUiSubsystem.h"
#include "Core/GameFlowSubsystem.h"
#include "Simulation/GameEconomySubsystem.h"
#include "Simulation/GameEnergySubsystem.h"
#include "Simulation/GamePlayerFundsSubsystem.h"
#include "Simulation/GamePopulationSubsystem.h"
#include "Simulation/GameSimulationSubsystem.h"
#include "Simulation/GameWaterSubsystem.h"
#include "UI/GameHudWidget.h"
#include "UI/GameMenuWidget.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"

void UGameUiSubsystem::Deinitialize()
{
	RemoveWidget(MenuWidget);
	RemoveWidget(HudWidget);
	Super::Deinitialize();
}

void UGameUiSubsystem::Tick(float DeltaSeconds)
{
	UWorld* World = GetWorld();
	UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr;
	UGameFlowSubsystem* Flow = World ? World->GetSubsystem<UGameFlowSubsystem>() : nullptr;
	if (!Viewport || !Flow)
	{
		return;
	}

	const bool bWantMenu = Flow->GetState() == EGameFlowState::Menu;
	if (bWantMenu && !MenuWidget.IsValid())
	{
		MenuWidget = SNew(SAwsimMenu).Flow(Flow);
		Viewport->AddViewportWidgetContent(MenuWidget.ToSharedRef(), /*ZOrder*/ 100);
	}
	else if (!bWantMenu && MenuWidget.IsValid())
	{
		RemoveWidget(MenuWidget);
	}

	const bool bWantHud = Flow->GetState() == EGameFlowState::Playing;
	if (bWantHud && !HudWidget.IsValid())
	{
		HudWidget = SNew(SAwsimHud)
			.Sim(World->GetSubsystem<USimulationSubsystem>())
			.Funds(World->GetSubsystem<UGamePlayerFundsSubsystem>())
			.Population(World->GetSubsystem<UPopulationSubsystem>())
			.Energy(World->GetSubsystem<UEnergySubsystem>())
			.Water(World->GetSubsystem<UGameWaterSubsystem>())
			.Economy(World->GetSubsystem<UEconomySubsystem>());
		Viewport->AddViewportWidgetContent(HudWidget.ToSharedRef(), /*ZOrder*/ 50);
	}
	else if (!bWantHud && HudWidget.IsValid())
	{
		RemoveWidget(HudWidget);
	}
}

void UGameUiSubsystem::RemoveWidget(TSharedPtr<SWidget>& Widget)
{
	if (!Widget.IsValid())
	{
		return;
	}
	UWorld* World = GetWorld();
	if (UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr)
	{
		Viewport->RemoveViewportWidgetContent(Widget.ToSharedRef());
	}
	Widget.Reset();
}

bool UGameUiSubsystem::IsTickable() const
{
	const UWorld* World = GetWorld();
	return World && World->IsGameWorld();
}

TStatId UGameUiSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGameUiSubsystem, STATGROUP_Tickables);
}
