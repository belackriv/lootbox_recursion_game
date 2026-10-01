#include "UI/SLRGameMenu.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Game/LRGameSubsystem.h"
#include "Game/LRInputCommands.h"
#include "Game/LRKeyBindings.h"
#include "Game/LRUserSettings.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Simulation/LRSimulation.h"
#include "UI/LRHudStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "LRGameMenu"

void SLRGameMenu::Construct(const FArguments& InArgs)
{
	Subsystem = InArgs._Subsystem;
	OnClose = InArgs._OnClose;
	const FLRHudStyle& Style = FLRHudStyle::Get();

	// A dim backdrop over the whole screen (it also blocks clicks to the game), and the menu
	// in the middle.
	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(&Style.WhiteBrush)
		.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.6f))
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBorder)
			.BorderImage(&Style.WhiteBrush)
			.BorderBackgroundColor(Style.Border)
			.Padding(FMargin(1.f))
			[
				SNew(SBorder)
				.BorderImage(&Style.WhiteBrush)
				.BorderBackgroundColor(Style.Panel)
				.Padding(FMargin(20.f, 16.f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Center)
					.Padding(FMargin(0.f, 0.f, 0.f, 12.f))
					[
						SNew(STextBlock)
						.Font(Style.TitleFont)
						.ColorAndOpacity(Style.Orange)
						.Text_Lambda([this]() { return IsGameOver() ? LOCTEXT("GameOver", "GAME OVER") : LOCTEXT("Paused", "PAUSED"); })
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SAssignNew(PageBox, SBox)
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Center)
					.Padding(FMargin(0.f, 10.f, 0.f, 0.f))
					[
						SNew(STextBlock)
						.Font(Style.SmallFont)
						.ColorAndOpacity(Style.TextDim)
						.Text_Lambda([this]() { return FText::FromString(Notice); })
					]
				]
			]
		]
	];
	ShowPage(EPage::Main);
}

void SLRGameMenu::Open()
{
	CaptureCommand = NAME_None;
	Notice.Reset();
	ShowPage(EPage::Main);
	FSlateApplication::Get().SetKeyboardFocus(SharedThis(this), EFocusCause::SetDirectly);
}

void SLRGameMenu::Back()
{
	if (!CaptureCommand.IsNone())
	{
		CaptureCommand = NAME_None;
		return;
	}
	if (Page == EPage::Settings)
	{
		SettingsChanged(/*bSave*/ true);
	}
	if (Page != EPage::Main && Page != EPage::GameOver)
	{
		Notice.Reset();
		ShowPage(EPage::Main);
		return;
	}
	if (!IsGameOver())
	{
		OnClose.ExecuteIfBound(); // there's no going back into a game that's over
	}
}

bool SLRGameMenu::IsGameOver() const
{
	const ULRGameSubsystem* Sub = Subsystem.Get();
	const FLRSimulation* Sim = Sub ? Sub->GetSimulation() : nullptr;
	return Sim && Sim->IsFrozen();
}

FReply SLRGameMenu::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (!CaptureCommand.IsNone())
	{
		// Rebinding: the next key press is the new key. Esc cancels; Backspace clears it.
		if (Key == EKeys::Escape)
		{
			CaptureCommand = NAME_None;
			return FReply::Handled();
		}
		if (!LRKeyBindings::SetKey(Subsystem.Get(), CaptureCommand, CaptureIndex, Key == EKeys::BackSpace ? EKeys::Invalid : Key))
		{
			Notice = TEXT("That key couldn't be bound.");
		}
		CaptureCommand = NAME_None;
		return FReply::Handled();
	}

	// The menu's own keys: Esc, or whatever the menu command is bound to.
	const ULRGameSubsystem* Sub = Subsystem.Get();
	const bool bMenuKey = Key == EKeys::Escape
		|| LRKeyBindings::GetKey(Sub, LRInput::Names::Menu, 0) == Key || LRKeyBindings::GetKey(Sub, LRInput::Names::Menu, 1) == Key;
	if (bMenuKey)
	{
		Back();
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

ULRUserSettings* SLRGameMenu::GetSettings() const
{
	const ULRGameSubsystem* Sub = Subsystem.Get();
	return Sub ? Sub->GetUserSettings() : nullptr;
}

void SLRGameMenu::SettingsChanged(bool bSave)
{
	ULRGameSubsystem* Sub = Subsystem.Get();
	if (!Sub)
	{
		return;
	}
	if (bSave)
	{
		Sub->SaveUserSettings(); // also tells everyone to apply them
	}
	else
	{
		Sub->OnSettingsChanged.Broadcast();
	}
}

void SLRGameMenu::ShowPage(EPage NewPage)
{
	// Once the game is over, the game-over page is the menu's front page.
	Page = (NewPage == EPage::Main && IsGameOver()) ? EPage::GameOver : NewPage;
	CaptureCommand = NAME_None;
	if (!PageBox.IsValid())
	{
		return;
	}
	switch (Page)
	{
	case EPage::Main:     PageBox->SetContent(BuildMainPage()); break;
	case EPage::Save:     PageBox->SetContent(BuildSavePage()); break;
	case EPage::Load:     PageBox->SetContent(BuildLoadPage()); break;
	case EPage::Settings: PageBox->SetContent(BuildSettingsPage()); break;
	case EPage::GameOver: PageBox->SetContent(BuildGameOverPage()); break;
	case EPage::ConfirmNewGame:
		PageBox->SetContent(BuildConfirmPage(LOCTEXT("ConfirmNew", "Start a new game? Anything you haven't saved is lost."), [this]()
		{
			if (ULRGameSubsystem* Sub = Subsystem.Get())
			{
				Sub->ResetGame();
			}
			OnClose.ExecuteIfBound();
		}));
		break;
	case EPage::ConfirmQuit:
		PageBox->SetContent(BuildConfirmPage(LOCTEXT("ConfirmQuit", "Quit the game? It autosaves on the way out."), [this]()
		{
			ULRGameSubsystem* Sub = Subsystem.Get();
			UWorld* World = Sub ? Sub->GetWorld() : nullptr;
			if (World)
			{
				UKismetSystemLibrary::QuitGame(World, World->GetFirstPlayerController(), EQuitPreference::Quit, false);
			}
		}));
		break;
	}
}

TSharedRef<SWidget> SLRGameMenu::MakeMenuButton(const FText& Label, TFunction<void()> OnClick, float Width)
{
	const FLRHudStyle& Style = FLRHudStyle::Get();
	return SNew(SBox)
		.WidthOverride(Width)
		[
			SNew(SButton)
			.ButtonStyle(&Style.ButtonStyle)
			.IsFocusable(false)
			.HAlign(HAlign_Center)
			.ContentPadding(FMargin(10.f, 5.f))
			.OnClicked_Lambda([OnClick]()
			{
				OnClick();
				return FReply::Handled();
			})
			[
				SNew(STextBlock).Font(Style.HeadingFont).ColorAndOpacity(Style.Text).Text(Label)
			]
		];
}

TSharedRef<SWidget> SLRGameMenu::MakeHeading(const FText& Text)
{
	const FLRHudStyle& Style = FLRHudStyle::Get();
	return SNew(STextBlock).Font(Style.HeadingFont).ColorAndOpacity(Style.Orange).Text(Text);
}

TSharedRef<SWidget> SLRGameMenu::BuildMainPage()
{
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	auto Add = [&Box](const TSharedRef<SWidget>& Widget)
	{
		Box->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 3.f))[Widget];
	};
	Add(MakeMenuButton(LOCTEXT("Resume", "Resume"), [this]() { OnClose.ExecuteIfBound(); }));
	Add(MakeMenuButton(LOCTEXT("NewGame", "New Game"), [this]() { ShowPage(EPage::ConfirmNewGame); }));
	Add(MakeMenuButton(LOCTEXT("Save", "Save"), [this]() { ShowPage(EPage::Save); }));
	Add(MakeMenuButton(LOCTEXT("Load", "Load"), [this]() { ShowPage(EPage::Load); }));
	Add(MakeMenuButton(LOCTEXT("Settings", "Settings"), [this]() { ShowPage(EPage::Settings); }));
	Add(MakeMenuButton(LOCTEXT("Quit", "Quit"), [this]() { ShowPage(EPage::ConfirmQuit); }));
	return Box;
}

TSharedRef<SWidget> SLRGameMenu::BuildGameOverPage()
{
	const FLRHudStyle& Style = FLRHudStyle::Get();
	ULRGameSubsystem* Sub = Subsystem.Get();
	const FLRSimulation* Sim = Sub ? Sub->GetSimulation() : nullptr;
	const bool bBreach = Sim && Sim->GetGameOver() == ELRGameOver::Breach;

	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	Box->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 0.f, 0.f, 6.f))
	[
		SNew(STextBlock)
		.Font(Style.HeadingFont)
		.ColorAndOpacity(Style.Red)
		.Text(bBreach ? LOCTEXT("Breach", "CONTAINMENT BREACH") : LOCTEXT("Evaporated", "THE HOST HAS EVAPORATED"))
	];
	Box->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 0.f, 0.f, 12.f))
	[
		SNew(SBox)
		.WidthOverride(520.f)
		[
			SNew(STextBlock)
			.Font(Style.BodyFont)
			.ColorAndOpacity(Style.Text)
			.AutoWrapText(true)
			.Justification(ETextJustify::Center)
			.Text(FText::FromString(Sim ? Sim->DescribeGameOver() : FString()))
		]
	];
	auto Add = [&Box](const TSharedRef<SWidget>& Widget)
	{
		Box->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f, 3.f))[Widget];
	};
	if (Sub && Sub->HasAutosave())
	{
		Add(MakeMenuButton(LOCTEXT("LoadAutosave", "Load the autosave"), [this]()
		{
			ULRGameSubsystem* Owner = Subsystem.Get();
			if (Owner && Owner->LoadAutosave() && !IsGameOver())
			{
				OnClose.ExecuteIfBound();
				return;
			}
			Notice = TEXT("Couldn't load the autosave.");
			ShowPage(EPage::GameOver);
		}));
	}
	Add(MakeMenuButton(LOCTEXT("LoadOther", "Load a save"), [this]() { ShowPage(EPage::Load); }));
	Add(MakeMenuButton(LOCTEXT("NewGameOver", "New Game"), [this]()
	{
		if (ULRGameSubsystem* Owner = Subsystem.Get())
		{
			Owner->ResetGame();
		}
		OnClose.ExecuteIfBound();
	}));
	Add(MakeMenuButton(LOCTEXT("QuitOver", "Quit"), [this]() { ShowPage(EPage::ConfirmQuit); }));
	return Box;
}

TSharedRef<SWidget> SLRGameMenu::BuildConfirmPage(const FText& Question, TFunction<void()> OnYes)
{
	const FLRHudStyle& Style = FLRHudStyle::Get();
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		.Padding(FMargin(0.f, 0.f, 0.f, 10.f))
		[
			SNew(STextBlock).Font(Style.BodyFont).ColorAndOpacity(Style.Text).Text(Question)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(4.f, 0.f))
			[
				MakeMenuButton(LOCTEXT("Yes", "Yes"), OnYes, 120.f)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(4.f, 0.f))
			[
				MakeMenuButton(LOCTEXT("No", "No"), [this]() { ShowPage(EPage::Main); }, 120.f)
			]
		];
}

TSharedRef<SWidget> SLRGameMenu::BuildSavePage()
{
	const FLRHudStyle& Style = FLRHudStyle::Get();
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);

	// A new save, named.
	Box->AddSlot()
	.AutoHeight()
	.Padding(FMargin(0.f, 0.f, 0.f, 8.f))
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		.VAlign(VAlign_Center)
		.Padding(FMargin(0.f, 0.f, 6.f, 0.f))
		[
			SNew(SBox)
			.WidthOverride(260.f)
			[
				SAssignNew(SaveNameBox, SEditableTextBox)
				.HintText(LOCTEXT("SaveNameHint", "Name this save"))
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			MakeMenuButton(LOCTEXT("SaveNew", "Save"), [this]()
			{
				ULRGameSubsystem* Sub = Subsystem.Get();
				const FString Name = SaveNameBox.IsValid() ? SaveNameBox->GetText().ToString() : FString();
				Notice = (Sub && Sub->SaveToNewSlot(Name)) ? TEXT("Saved.") : TEXT("Couldn't save.");
				ShowPage(EPage::Save);
			}, 120.f)
		]
	];

	// Or over an existing one.
	const ULRGameSubsystem* Sub = Subsystem.Get();
	const TArray<FLRSaveSlotInfo> Slots = Sub ? Sub->GetSaveSlots() : TArray<FLRSaveSlotInfo>();
	bool bAny = false;
	for (const FLRSaveSlotInfo& Info : Slots)
	{
		if (Info.bAutosave)
		{
			continue;
		}
		bAny = true;
		const FString SlotName = Info.SlotName;
		Box->AddSlot()
		.AutoHeight()
		.Padding(FMargin(0.f, 2.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Font(Style.BodyFont)
				.ColorAndOpacity(Style.Text)
				.Text(FText::FromString(FString::Printf(TEXT("%s  (%s)"), *Info.DisplayName, *Info.SavedAt.ToString(TEXT("%Y-%m-%d %H:%M")))))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(8.f, 0.f, 0.f, 0.f))
			[
				MakeMenuButton(LOCTEXT("Overwrite", "Overwrite"), [this, SlotName]()
				{
					ULRGameSubsystem* Owner = Subsystem.Get();
					Notice = (Owner && Owner->SaveToSlot(SlotName)) ? TEXT("Saved.") : TEXT("Couldn't save.");
					ShowPage(EPage::Save);
				}, 110.f)
			]
		];
	}
	if (!bAny)
	{
		Box->AddSlot()
		.AutoHeight()
		[
			SNew(STextBlock).Font(Style.SmallFont).ColorAndOpacity(Style.TextDim)
			.Text(LOCTEXT("NoSaves", "No saves yet (the game also autosaves every 30 seconds)."))
		];
	}
	Box->AddSlot()
	.AutoHeight()
	.HAlign(HAlign_Center)
	.Padding(FMargin(0.f, 12.f, 0.f, 0.f))
	[
		MakeMenuButton(LOCTEXT("Back", "Back"), [this]() { Back(); }, 120.f)
	];
	return SNew(SBox).WidthOverride(460.f)[Box];
}

TSharedRef<SWidget> SLRGameMenu::BuildLoadPage()
{
	const FLRHudStyle& Style = FLRHudStyle::Get();
	TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
	const ULRGameSubsystem* Sub = Subsystem.Get();
	const TArray<FLRSaveSlotInfo> Slots = Sub ? Sub->GetSaveSlots() : TArray<FLRSaveSlotInfo>();
	for (const FLRSaveSlotInfo& Info : Slots)
	{
		const FString SlotName = Info.SlotName;
		TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Font(Style.BodyFont)
					.ColorAndOpacity(Info.bAutosave ? Style.Orange : Style.Text)
					.Text(FText::FromString(FString::Printf(TEXT("%s  (%s)"), *Info.DisplayName, *Info.SavedAt.ToString(TEXT("%Y-%m-%d %H:%M")))))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Font(Style.SmallFont).ColorAndOpacity(Style.TextDim).Text(FText::FromString(Info.Summary))
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(8.f, 0.f, 0.f, 0.f))
			[
				MakeMenuButton(LOCTEXT("LoadSlot", "Load"), [this, SlotName]()
				{
					ULRGameSubsystem* Owner = Subsystem.Get();
					if (Owner && Owner->LoadFromSlot(SlotName))
					{
						OnClose.ExecuteIfBound();
						return;
					}
					Notice = TEXT("Couldn't load that save (it may be from an older version).");
					ShowPage(EPage::Load);
				}, 90.f)
			];
		if (!Info.bAutosave)
		{
			Row->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(4.f, 0.f, 0.f, 0.f))
			[
				MakeMenuButton(LOCTEXT("DeleteSlot", "Delete"), [this, SlotName]()
				{
					if (ULRGameSubsystem* Owner = Subsystem.Get())
					{
						Owner->DeleteSlot(SlotName);
					}
					ShowPage(EPage::Load);
				}, 90.f)
			];
		}
		List->AddSlot().AutoHeight().Padding(FMargin(0.f, 3.f))[Row];
	}
	if (Slots.IsEmpty())
	{
		List->AddSlot()
		.AutoHeight()
		[
			SNew(STextBlock).Font(Style.SmallFont).ColorAndOpacity(Style.TextDim).Text(LOCTEXT("NothingToLoad", "No saves yet."))
		];
	}

	return SNew(SBox)
		.WidthOverride(560.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBox)
				.MaxDesiredHeight(420.f)
				[
					SNew(SScrollBox) + SScrollBox::Slot()[List]
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(FMargin(0.f, 12.f, 0.f, 0.f))
			[
				MakeMenuButton(LOCTEXT("Back", "Back"), [this]() { Back(); }, 120.f)
			]
		];
}

TSharedRef<SWidget> SLRGameMenu::MakeKeyButton(FName Command, int32 Index)
{
	const FLRHudStyle& Style = FLRHudStyle::Get();
	return SNew(SBox)
		.WidthOverride(120.f)
		[
			SNew(SButton)
			.ButtonStyle(&Style.ButtonStyle)
			.IsFocusable(false)
			.HAlign(HAlign_Center)
			.ContentPadding(FMargin(6.f, 2.f))
			.OnClicked_Lambda([this, Command, Index]()
			{
				CaptureCommand = Command;
				CaptureIndex = Index;
				FSlateApplication::Get().SetKeyboardFocus(SharedThis(this), EFocusCause::SetDirectly);
				return FReply::Handled();
			})
			[
				SNew(STextBlock)
				.Font(Style.BodyFont)
				.Text_Lambda([this, Command, Index]()
				{
					if (CaptureCommand == Command && CaptureIndex == Index)
					{
						return LOCTEXT("PressAKey", "press a key...");
					}
					const FKey Key = LRKeyBindings::GetKey(Subsystem.Get(), Command, Index);
					return Key.IsValid() ? Key.GetDisplayName(/*bLongDisplayName*/ false) : LOCTEXT("Unbound", "-");
				})
				.ColorAndOpacity_Lambda([this, Command, Index]() -> FSlateColor
				{
					const FLRHudStyle& S = FLRHudStyle::Get();
					return (CaptureCommand == Command && CaptureIndex == Index) ? S.Orange : S.Text;
				})
			]
		];
}

TSharedRef<SWidget> SLRGameMenu::MakeSliderRow(const FText& Label, float Min, float Max, TFunction<float(const ULRUserSettings&)> Get,
	TFunction<void(ULRUserSettings&, float)> Set)
{
	const FLRHudStyle& Style = FLRHudStyle::Get();
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(180.f)
			[
				SNew(STextBlock).Font(Style.BodyFont).ColorAndOpacity(Style.Text).Text(Label)
			]
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		.VAlign(VAlign_Center)
		[
			SNew(SSlider)
			.MinValue(Min)
			.MaxValue(Max)
			.StepSize(0.05f)
			.Value_Lambda([this, Get]()
			{
				const ULRUserSettings* Settings = GetSettings();
				return Settings ? Get(*Settings) : 1.f;
			})
			.OnValueChanged_Lambda([this, Set](float NewValue)
			{
				if (ULRUserSettings* Settings = GetSettings())
				{
					Set(*Settings, NewValue);
					SettingsChanged(/*bSave*/ false);
				}
			})
			.OnMouseCaptureEnd_Lambda([this]() { SettingsChanged(/*bSave*/ true); })
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(FMargin(8.f, 0.f, 0.f, 0.f))
		[
			SNew(SBox).WidthOverride(50.f)
			[
				SNew(STextBlock)
				.Font(Style.BodyFont)
				.ColorAndOpacity(Style.TextDim)
				.Text_Lambda([this, Get]()
				{
					const ULRUserSettings* Settings = GetSettings();
					return FText::FromString(FString::Printf(TEXT("%.2fx"), Settings ? Get(*Settings) : 1.f));
				})
			]
		];
}

TSharedRef<SWidget> SLRGameMenu::BuildSettingsPage()
{
	const FLRHudStyle& Style = FLRHudStyle::Get();
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	auto Row = [&Box](const TSharedRef<SWidget>& Widget, float Top = 2.f)
	{
		Box->AddSlot().AutoHeight().Padding(FMargin(0.f, Top, 0.f, 2.f))[Widget];
	};

	// Controls: every rebindable command, by group, with its two keys.
	Row(MakeHeading(LOCTEXT("Controls", "CONTROLS")), 0.f);
	Row(SNew(STextBlock).Font(Style.SmallFont).ColorAndOpacity(Style.TextDim).AutoWrapText(true)
		.Text(LOCTEXT("ControlsHint", "Click a key, then press the new one (Esc cancels, Backspace clears). A key does one thing: binding it here takes it off anything else. The mouse is fixed: wheel zooms, right-drag looks around, left-click selects.")));
	FString LastGroup;
	for (const LRInput::FCommand& Command : LRInput::GetCommands())
	{
		if (Command.Group != LastGroup)
		{
			LastGroup = Command.Group;
			Row(SNew(STextBlock).Font(Style.BodyFont).ColorAndOpacity(Style.TextDim).Text(FText::FromString(Command.Group)), 8.f);
		}
		Row(SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock).Font(Style.BodyFont).ColorAndOpacity(Style.Text).Text(FText::FromString(Command.Label))
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(4.f, 0.f))[MakeKeyButton(Command.Name, 0)]
			+ SHorizontalBox::Slot().AutoWidth()[MakeKeyButton(Command.Name, 1)]);
	}
	Row(MakeMenuButton(LOCTEXT("ResetKeys", "Reset keys to defaults"), [this]()
	{
		LRKeyBindings::ResetToDefaults(Subsystem.Get());
	}), 8.f);

	// Camera.
	Row(MakeHeading(LOCTEXT("CameraHeading", "CAMERA")), 14.f);
	Row(MakeSliderRow(LOCTEXT("LookSpeed", "Free look speed"), 0.25f, 3.f,
		[](const ULRUserSettings& S) { return S.LookSensitivity; }, [](ULRUserSettings& S, float V) { S.LookSensitivity = V; }));
	Row(SNew(SCheckBox)
		.IsChecked_Lambda([this]()
		{
			const ULRUserSettings* Settings = GetSettings();
			return (Settings && Settings->bInvertLook) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		})
		.OnCheckStateChanged_Lambda([this](ECheckBoxState State)
		{
			if (ULRUserSettings* Settings = GetSettings())
			{
				Settings->bInvertLook = State == ECheckBoxState::Checked;
				SettingsChanged(/*bSave*/ true);
			}
		})
		[
			SNew(STextBlock).Font(Style.BodyFont).ColorAndOpacity(Style.Text).Text(LOCTEXT("InvertLook", "Invert free look (mouse up tilts down)"))
		]);
	Row(MakeSliderRow(LOCTEXT("PanSpeed", "Pan speed"), 0.25f, 3.f,
		[](const ULRUserSettings& S) { return S.PanSpeed; }, [](ULRUserSettings& S, float V) { S.PanSpeed = V; }));

	// Interface.
	Row(MakeHeading(LOCTEXT("InterfaceHeading", "INTERFACE")), 14.f);
	Row(MakeSliderRow(LOCTEXT("UIScale", "UI scale"), 0.75f, 1.5f,
		[](const ULRUserSettings& S) { return S.UIScale; }, [](ULRUserSettings& S, float V) { S.UIScale = V; }));

	// Graphics: the engine's scalability levels (kept by UGameUserSettings).
	Row(MakeHeading(LOCTEXT("GraphicsHeading", "GRAPHICS")), 14.f);
	TSharedRef<SHorizontalBox> Quality = SNew(SHorizontalBox);
	constexpr int32 LevelCount = 4;
	const FText Levels[LevelCount] = { LOCTEXT("Low", "Low"), LOCTEXT("Medium", "Medium"), LOCTEXT("High", "High"), LOCTEXT("Epic", "Epic") };
	for (int32 Level = 0; Level < LevelCount; ++Level)
	{
		Quality->AddSlot()
		.AutoWidth()
		.Padding(FMargin(0.f, 0.f, 4.f, 0.f))
		[
			SNew(SButton)
			.ButtonStyle(&Style.ButtonStyle)
			.IsFocusable(false)
			.ContentPadding(FMargin(10.f, 3.f))
			.ButtonColorAndOpacity_Lambda([Level]() -> FSlateColor
			{
				const UGameUserSettings* Graphics = GEngine ? GEngine->GetGameUserSettings() : nullptr;
				return (Graphics && Graphics->GetOverallScalabilityLevel() == Level) ? FLinearColor(1.f, 0.72f, 0.35f) : FLinearColor::White;
			})
			.OnClicked_Lambda([Level]()
			{
				if (UGameUserSettings* Graphics = GEngine ? GEngine->GetGameUserSettings() : nullptr)
				{
					Graphics->SetOverallScalabilityLevel(Level);
					Graphics->ApplySettings(/*bCheckForCommandLineOverrides*/ false);
				}
				return FReply::Handled();
			})
			[
				SNew(STextBlock).Font(Style.BodyFont).ColorAndOpacity(Style.Text).Text(Levels[Level])
			]
		];
	}
	Row(Quality);

	return SNew(SBox)
		.WidthOverride(520.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBox)
				.MaxDesiredHeight(560.f)
				[
					SNew(SScrollBox) + SScrollBox::Slot()[Box]
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(FMargin(0.f, 12.f, 0.f, 0.f))
			[
				MakeMenuButton(LOCTEXT("Back", "Back"), [this]() { Back(); }, 120.f)
			]
		];
}

#undef LOCTEXT_NAMESPACE
