#include "Core/GameFlowSubsystem.h"
#include "awsim.h"
#include "Core/GameSaveSubsystem.h"
#include "Simulation/GameCityStatsSubsystem.h"
#include "Simulation/GameGridSubsystem.h"
#include "Simulation/GamePlayerFundsSubsystem.h"
#include "Simulation/GamePopulationSubsystem.h"
#include "Simulation/GameSimulationSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"

void UGameFlowSubsystem::OpenMenu()
{
	State = EGameFlowState::Menu;
	if (USimulationSubsystem* S = ResolveSim())
	{
		S->SetPaused(true);
	}
}

void UGameFlowSubsystem::Resume()
{
	if (!bSessionActive)
	{
		return; // nothing to resume until a game starts or loads
	}
	State = EGameFlowState::Playing;
	if (USimulationSubsystem* S = ResolveSim())
	{
		S->SetPaused(false);
	}
}

void UGameFlowSubsystem::ToggleMenu()
{
	State == EGameFlowState::Menu ? Resume() : OpenMenu();
}

void UGameFlowSubsystem::StartNewGame()
{
	if (UGridSubsystem* G = ResolveGrid())
	{
		G->ResetForLoad();
	}
	if (UGamePlayerFundsSubsystem* F = ResolveFunds())
	{
		F->RestoreBalance(StartingFunds);
	}
	if (USimulationSubsystem* S = ResolveSim())
	{
		S->RestoreClock(0, 0);
	}
	UWorld* World = GetWorld();
	if (UPopulationSubsystem* Population = World ? World->GetSubsystem<UPopulationSubsystem>() : nullptr)
	{
		Population->SetCount(0.f);
	}
	if (UCityStatsSubsystem* Stats = World ? World->GetSubsystem<UCityStatsSubsystem>() : nullptr)
	{
		Stats->RestorePlayerRating(0.5f);
	}
	bSessionActive = true;
	Resume();
	UE_LOG(LogAwsim, Log, TEXT("New game started with %.0f funds."), StartingFunds);
}

void UGameFlowSubsystem::SaveGame(int32 SlotIndex)
{
	UGameSaveSubsystem* S = ResolveSave();
	if (S && SlotIndex >= 0 && SlotIndex < NumSaveSlots)
	{
		S->RequestSave(SlotNameFor(SlotIndex));
	}
}

void UGameFlowSubsystem::LoadGame(int32 SlotIndex)
{
	UGameSaveSubsystem* S = ResolveSave();
	if (S && SlotIndex >= 0 && SlotIndex < NumSaveSlots && S->LoadNow(SlotNameFor(SlotIndex)))
	{
		bSessionActive = true;
		Resume();
	}
}

bool UGameFlowSubsystem::SlotExists(int32 SlotIndex) const
{
	return SlotIndex >= 0 && SlotIndex < NumSaveSlots
		&& IFileManager::Get().FileExists(*UGameSaveSubsystem::SlotToPath(SlotNameFor(SlotIndex)));
}

void UGameFlowSubsystem::QuitGame()
{
	UWorld* World = GetWorld();
	if (APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr)
	{
		PC->ConsoleCommand(TEXT("quit"));
	}
}

USimulationSubsystem* UGameFlowSubsystem::ResolveSim() const
{
	if (Sim) return Sim;
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<USimulationSubsystem>() : nullptr;
}

UGridSubsystem* UGameFlowSubsystem::ResolveGrid() const
{
	if (Grid) return Grid;
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UGridSubsystem>() : nullptr;
}

UGamePlayerFundsSubsystem* UGameFlowSubsystem::ResolveFunds() const
{
	if (Funds) return Funds;
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UGamePlayerFundsSubsystem>() : nullptr;
}

UGameSaveSubsystem* UGameFlowSubsystem::ResolveSave() const
{
	if (Save) return Save;
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UGameSaveSubsystem>() : nullptr;
}
