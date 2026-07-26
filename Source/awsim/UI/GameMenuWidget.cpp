#include "UI/GameMenuWidget.h"
#include "Core/GameFlowSubsystem.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

void SAwsimMenu::Construct(const FArguments& InArgs)
{
	Flow = InArgs._Flow;
	SlotUsed.Init(false, UGameFlowSubsystem::NumSaveSlots);

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.55f)) // dim the city behind
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(320.f)
			[
				SNew(SWidgetSwitcher)
				.WidgetIndex_Lambda([this] { return static_cast<int32>(Page); })
				+ SWidgetSwitcher::Slot() [ MakeMainPage() ]
				+ SWidgetSwitcher::Slot() [ MakeSlotsPage(/*bSavePage*/ true) ]
				+ SWidgetSwitcher::Slot() [ MakeSlotsPage(/*bSavePage*/ false) ]
			]
		]
	];
}

TSharedRef<SWidget> SAwsimMenu::MakeMainPage()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 24.f).HAlign(HAlign_Center)
		[
			SNew(STextBlock)
			.Text(INVTEXT("awsim"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 42))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f)
		[
			SNew(SBox)
			.IsEnabled_Lambda([this] { const UGameFlowSubsystem* F = Flow.Get(); return F && F->CanResume(); })
			[
				MakeButton(INVTEXT("Resume"), [this] { if (UGameFlowSubsystem* F = Flow.Get()) F->Resume(); })
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f)
		[
			MakeButton(INVTEXT("New Game"), [this] { if (UGameFlowSubsystem* F = Flow.Get()) F->StartNewGame(); })
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f)
		[
			MakeButton(INVTEXT("Save Game"), [this] { ShowPage(EPage::Save); })
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f)
		[
			MakeButton(INVTEXT("Load Game"), [this] { ShowPage(EPage::Load); })
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f)
		[
			MakeButton(INVTEXT("Quit Game"), [this] { if (UGameFlowSubsystem* F = Flow.Get()) F->QuitGame(); })
		];
}

TSharedRef<SWidget> SAwsimMenu::MakeSlotsPage(bool bSavePage)
{
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 24.f).HAlign(HAlign_Center)
		[
			SNew(STextBlock)
			.Text(bSavePage ? INVTEXT("Save Game") : INVTEXT("Load Game"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 28))
		];

	for (int32 i = 0; i < UGameFlowSubsystem::NumSaveSlots; ++i)
	{
		TSharedRef<SWidget> Button = SNew(SButton)
			.HAlign(HAlign_Center)
			.ContentPadding(FMargin(0.f, 10.f))
			.IsEnabled_Lambda([this, bSavePage, i] { return bSavePage || SlotUsed[i]; })
			.OnClicked_Lambda([this, bSavePage, i]
			{
				if (UGameFlowSubsystem* F = Flow.Get())
				{
					bSavePage ? F->SaveGame(i) : F->LoadGame(i);
				}
				ShowPage(EPage::Main);
				return FReply::Handled();
			})
			[
				SNew(STextBlock)
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 18))
				.Text_Lambda([this, i]
				{
					return FText::Format(SlotUsed[i] ? INVTEXT("Slot {0}") : INVTEXT("Slot {0} — empty"), i + 1);
				})
			];
		Box->AddSlot().AutoHeight().Padding(0.f, 6.f) [ Button ];
	}

	Box->AddSlot().AutoHeight().Padding(0.f, 24.f, 0.f, 0.f)
	[
		MakeButton(INVTEXT("Back"), [this] { ShowPage(EPage::Main); })
	];
	return Box;
}

TSharedRef<SWidget> SAwsimMenu::MakeButton(const FText& Label, TFunction<void()> Action)
{
	return SNew(SButton)
		.HAlign(HAlign_Center)
		.ContentPadding(FMargin(0.f, 10.f))
		.OnClicked_Lambda([Action = MoveTemp(Action)]
		{
			Action();
			return FReply::Handled();
		})
		[
			SNew(STextBlock)
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 18))
			.Text(Label)
		];
}

void SAwsimMenu::ShowPage(EPage InPage)
{
	Page = InPage;
	if (const UGameFlowSubsystem* F = Flow.Get())
	{
		for (int32 i = 0; i < SlotUsed.Num(); ++i)
		{
			SlotUsed[i] = F->SlotExists(i);
		}
	}
}
