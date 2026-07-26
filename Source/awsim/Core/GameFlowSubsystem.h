#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameFlowSubsystem.generated.h"

class UGameSaveSubsystem;
class UGamePlayerFundsSubsystem;
class UGridSubsystem;
class USimulationSubsystem;

UENUM()
enum class EGameFlowState : uint8
{
	Menu,
	Playing
};

// App-level state machine: boots into Menu with the sim clock gated; menu
// actions drive the sim/save APIs. Pause stays the sim's toggle, not a state.
UCLASS()
class AWSIM_API UGameFlowSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static constexpr float StartingFunds = 10000.f;

	EGameFlowState GetState() const { return State; }

	// False until a game starts or loads this session; Resume is a no-op before that.
	bool CanResume() const { return bSessionActive; }

	static constexpr int32 NumSaveSlots = 5;

	void OpenMenu();
	void Resume();
	void ToggleMenu();
	void StartNewGame();
	void SaveGame(int32 SlotIndex); // arms an N + 1 save request; written after the next drained step
	void LoadGame(int32 SlotIndex); // resumes on success, stays in the menu on failure
	void QuitGame();

	FString SlotNameFor(int32 SlotIndex) const { return FString::Printf(TEXT("%s%d"), *SlotPrefix, SlotIndex + 1); }
	bool SlotExists(int32 SlotIndex) const;

	// Settable so specs stay off the player's real slots.
	FString SlotPrefix = TEXT("slot");

	// Injectable for world-less specs; resolved from the owning world when unset.
	void SetSim(USimulationSubsystem* In) { Sim = In; }
	void SetGrid(UGridSubsystem* In) { Grid = In; }
	void SetFunds(UGamePlayerFundsSubsystem* In) { Funds = In; }
	void SetSave(UGameSaveSubsystem* In) { Save = In; }

private:
	USimulationSubsystem* ResolveSim() const;
	UGridSubsystem* ResolveGrid() const;
	UGamePlayerFundsSubsystem* ResolveFunds() const;
	UGameSaveSubsystem* ResolveSave() const;

	EGameFlowState State = EGameFlowState::Menu;
	bool bSessionActive = false; // runtime-only; never saved

	UPROPERTY(Transient) TObjectPtr<USimulationSubsystem> Sim;
	UPROPERTY(Transient) TObjectPtr<UGridSubsystem> Grid;
	UPROPERTY(Transient) TObjectPtr<UGamePlayerFundsSubsystem> Funds;
	UPROPERTY(Transient) TObjectPtr<UGameSaveSubsystem> Save;
};
