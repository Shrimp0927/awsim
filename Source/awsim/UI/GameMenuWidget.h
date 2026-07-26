#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class UGameFlowSubsystem;

// Slate main/pause menu; a dumb view that forwards clicks to the flow
// subsystem. Save/Load switch the panel to a five-slot picker page.
class SAwsimMenu : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAwsimMenu) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UGameFlowSubsystem>, Flow)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	enum class EPage : uint8 { Main, Save, Load };

	TSharedRef<SWidget> MakeMainPage();
	TSharedRef<SWidget> MakeSlotsPage(bool bSavePage);
	TSharedRef<SWidget> MakeButton(const FText& Label, TFunction<void()> Action);
	void ShowPage(EPage InPage);

	TWeakObjectPtr<UGameFlowSubsystem> Flow;
	EPage Page = EPage::Main;
	TArray<bool> SlotUsed; // refreshed on page switch, not per frame
};
