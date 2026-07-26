#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameSaveSubsystem.generated.h"

class UEditSubsystem;
class UGamePlayerFundsSubsystem;
class UGridSubsystem;

// Saves/loads the sim: authoritative grid content plus scalar stats; derived
// state rebuilds after load. N + 1 contract: RequestSave arms a slot, and the
// sim's per-step NotifyStepCompleted call snapshots once the queues drain.
UCLASS()
class AWSIM_API UGameSaveSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static constexpr uint32 SaveVersion = 1;

	void RequestSave(const FString& SlotName) { PendingSlot = SlotName; }
	bool HasPendingSave() const { return !PendingSlot.IsEmpty(); }
	void NotifyStepCompleted();

	// Refuses (false) while intents are queued; use RequestSave for the N + 1 flow.
	bool SaveNow(const FString& SlotName);
	bool LoadNow(const FString& SlotName);

	static FString SlotToPath(const FString& SlotName);

	// Injectable for world-less specs; resolved from the owning world when unset.
	void SetGrid(UGridSubsystem* In) { Grid = In; }
	void SetEdit(UEditSubsystem* In) { Edit = In; }
	void SetFunds(UGamePlayerFundsSubsystem* In) { Funds = In; }

private:
	UGridSubsystem* ResolveGrid() const;
	UEditSubsystem* ResolveEdit() const;
	UGamePlayerFundsSubsystem* ResolveFunds() const;

	FString PendingSlot; // empty = no armed save

	UPROPERTY(Transient) TObjectPtr<UGridSubsystem> Grid;
	UPROPERTY(Transient) TObjectPtr<UEditSubsystem> Edit;
	UPROPERTY(Transient) TObjectPtr<UGamePlayerFundsSubsystem> Funds;
};
