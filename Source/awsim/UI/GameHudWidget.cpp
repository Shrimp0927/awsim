#include "UI/GameHudWidget.h"
#include "Simulation/GameEconomySubsystem.h"
#include "Simulation/GameEnergySubsystem.h"
#include "Simulation/GamePlayerFundsSubsystem.h"
#include "Simulation/GamePopulationSubsystem.h"
#include "Simulation/GameSimulationSubsystem.h"
#include "Simulation/GameWaterSubsystem.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

void SAwsimHud::Construct(const FArguments& InArgs)
{
	Sim = InArgs._Sim;
	Funds = InArgs._Funds;
	Population = InArgs._Population;
	Energy = InArgs._Energy;
	Water = InArgs._Water;
	Economy = InArgs._Economy;

	// Stats only — the whole bar must never intercept world clicks.
	SetVisibility(EVisibility::HitTestInvisible);

	const auto Stat = [](TFunction<FText()> Getter) -> TSharedRef<SWidget>
	{
		return SNew(STextBlock)
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))
			.ColorAndOpacity(FLinearColor(0.92f, 0.92f, 0.92f))
			.Text_Lambda(MoveTemp(Getter));
	};

	TSharedRef<SHorizontalBox> Bar = SNew(SHorizontalBox);
	const auto AddStat = [&Bar, &Stat](TFunction<FText()> Getter)
	{
		Bar->AddSlot().AutoWidth().Padding(14.f, 8.f) [ Stat(MoveTemp(Getter)) ];
	};

	AddStat([this]
	{
		const USimulationSubsystem* S = Sim.Get();
		const bool bPaused = S && !S->IsRunning();
		return FText::Format(INVTEXT("Day {0}{1}"), S ? S->GetDay() : 0,
			bPaused ? INVTEXT(" (paused)") : FText::GetEmpty());
	});
	AddStat([this]
	{
		const UGamePlayerFundsSubsystem* F = Funds.Get();
		return FText::Format(INVTEXT("$ {0}"), FText::AsNumber(F ? static_cast<int64>(F->GetBalance()) : 0));
	});
	AddStat([this]
	{
		const UPopulationSubsystem* P = Population.Get();
		return FText::Format(INVTEXT("Pop {0}"), P ? P->GetCount() : 0);
	});
	AddStat([this]
	{
		const UEnergySubsystem* Fx = Energy.Get();
		return FText::Format(INVTEXT("Energy {0}/{1}"),
			Fx ? static_cast<int32>(Fx->GetConsumption()) : 0, Fx ? static_cast<int32>(Fx->GetCapacity()) : 0);
	});
	AddStat([this]
	{
		const UGameWaterSubsystem* Fx = Water.Get();
		return FText::Format(INVTEXT("Water {0}/{1}"),
			Fx ? static_cast<int32>(Fx->GetConsumption()) : 0, Fx ? static_cast<int32>(Fx->GetCapacity()) : 0);
	});
	AddStat([this]
	{
		const UEconomySubsystem* E = Economy.Get();
		return FText::Format(INVTEXT("GDP {0}"), E ? static_cast<int32>(E->GetGDP()) : 0);
	});

	ChildSlot
	.HAlign(HAlign_Center)
	.VAlign(VAlign_Bottom)
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.45f))
			[
				Bar
			]
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f, 2.f, 0.f, 2.f)
		[
			SNew(STextBlock)
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
			.ColorAndOpacity(FLinearColor(0.75f, 0.75f, 0.75f))
			.Text(INVTEXT("1: Energy   2: Water   3: Residential   4: Business   5: Road   6: Power Line   7: Pipe   X: Demolish   R: Rotate   M: Menu"))
		]
	];
}
