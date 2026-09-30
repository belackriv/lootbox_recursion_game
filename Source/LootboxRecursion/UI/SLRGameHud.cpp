#include "UI/SLRGameHud.h"

#include "Framework/Application/SlateApplication.h"
#include "Game/LRGameSubsystem.h"
#include "Game/LRInputCommands.h"
#include "Game/LRKeyBindings.h"
#include "Game/LRUserSettings.h"
#include "Simulation/LRPhysics.h"
#include "Simulation/LRSimulation.h"
#include "UI/LRHudStyle.h"
#include "UI/SLRChamberCamera.h"
#include "UI/SLRChamberView.h"
#include "UI/SLRFeedDial.h"
#include "UI/SLRGameMenu.h"
#include "UI/SLRStaticNoise.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "LRGameHud"

namespace
{
	FText AsText(const FString& String)
	{
		return FText::FromString(String);
	}

	/** "45 s", "12 min", "3.2 hours", "12 days". */
	FString FormatDuration(double Seconds)
	{
		if (Seconds >= 1e15)
		{
			return TEXT("practically forever");
		}
		if (Seconds < 90.0)
		{
			return FString::Printf(TEXT("%.0f s"), FMath::Max(Seconds, 0.0));
		}
		if (Seconds < 5400.0)
		{
			return FString::Printf(TEXT("%.0f min"), Seconds / 60.0);
		}
		if (Seconds < 172800.0)
		{
			return FString::Printf(TEXT("%.1f hours"), Seconds / 3600.0);
		}
		return FString::Printf(TEXT("%.0f days"), Seconds / 86400.0);
	}

	/** "+1.2 t/s" / "-324 kg/s". */
	FString FormatSignedRate(double KgPerSecond)
	{
		return (KgPerSecond >= 0.0 ? TEXT("+") : TEXT("-")) + FLRSimulation::FormatRate(FMath::Abs(KgPerSecond));
	}

	/** How far the outside panel has slid down, eased. */
	float EaseOut(float Alpha)
	{
		const float Inverse = 1.f - FMath::Clamp(Alpha, 0.f, 1.f);
		return 1.f - Inverse * Inverse * Inverse;
	}
}

// ---------------------------------------------------------------------------------------
// Construction / layout
// ---------------------------------------------------------------------------------------

void SLRGameHud::Construct(const FArguments& InArgs)
{
	Subsystem = InArgs._Subsystem;
	StaticMaterial = InArgs._StaticMaterial;
	SetVisibility(EVisibility::SelfHitTestInvisible);

	// SelfHitTestInvisible: the container itself lets clicks through to the 3D world,
	// while its children (the panels) still receive them.
	ChildSlot
	[
		// The player's UI scale applies to everything.
		SNew(SDPIScaler)
		.Visibility(EVisibility::SelfHitTestInvisible)
		.DPIScale_Lambda([this]() { return GetUIScale(); })
		[
		SNew(SOverlay)
		.Visibility(EVisibility::SelfHitTestInvisible)
		// Under the panels: static over the 3D view while the instruments are down.
		+ SOverlay::Slot()
		[
			BuildInstrumentStatic()
		]
		+ SOverlay::Slot()
		.Padding(FMargin(8.f))
		[
			SNew(SVerticalBox)
			.Visibility(EVisibility::SelfHitTestInvisible)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				BuildHeader()
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.f)
			.Padding(FMargin(0.f, 8.f, 0.f, 0.f))
			[
				SNew(SHorizontalBox)
				.Visibility(EVisibility::SelfHitTestInvisible)

				// Top left: the command card, with the build layer strip under it.
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SVerticalBox)
					.Visibility(EVisibility::SelfHitTestInvisible)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						BuildActionsPanel()
					]
				]

				// Middle: nothing - the 3D world shows through
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				[
					SNew(SSpacer)
					.Visibility(EVisibility::SelfHitTestInvisible)
				]

				// Right: the Universe (matter) at the top, and Info at the bottom, just above the log
				// (Info changes size with what it describes, so it grows upwards from there).
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBox)
					.WidthOverride(360.f)
					.Visibility(EVisibility::SelfHitTestInvisible)
					[
						SNew(SVerticalBox)
						.Visibility(EVisibility::SelfHitTestInvisible)
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							BuildUniversePanel()
						]
						+ SVerticalBox::Slot()
						.FillHeight(1.f)
						[
							SNew(SSpacer)
							.Visibility(EVisibility::SelfHitTestInvisible)
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(FMargin(0.f, 8.f, 0.f, 0.f))
						[
							BuildInfoPanel()
						]
					]
				]
			]
			// Bottom: the log, across the full width.
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(0.f, 8.f, 0.f, 0.f))
			[
				BuildLogPanel()
			]
		]
		// The outside view: drops down from the top over the inside view (which stays live).
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Top)
		.Padding(FMargin(8.f, 60.f, 8.f, 8.f))
		[
			// Most of the screen: OutsideHeightShare of its height, and at most OutsideMaxWidthShare
			// of its width. The camera takes up the slack.
			SAssignNew(OutsidePanel, SBox)
			.HeightOverride_Lambda([this]() -> FOptionalSize { return FMath::Max(ViewSize.Y * OutsideHeightShare, OutsideMinHeight); })
			.MaxDesiredWidth_Lambda([this]() -> FOptionalSize { return FMath::Max(ViewSize.X * OutsideMaxWidthShare, OutsideMinWidth); })
			.Visibility_Lambda([this]() { return OutsideDrop > 0.001f ? EVisibility::Visible : EVisibility::Collapsed; })
			.RenderTransform_Lambda([this]() -> TOptional<FSlateRenderTransform>
			{
				const float Height = OutsidePanel.IsValid() ? static_cast<float>(OutsidePanel->GetDesiredSize().Y) : 500.f;
				return FSlateRenderTransform(FVector2f(0.f, -(1.f - EaseOut(OutsideDrop)) * (Height + 70.f)));
			})
			[
				// On a viewport too small for even the smallest camera, shrink the whole panel to fit.
				SNew(SScaleBox)
				.Stretch(EStretch::ScaleToFit)
				.StretchDirection(EStretchDirection::DownOnly)
				[
					BuildOutsidePanel()
				]
			]
		]
		// Help: the (?) button on the status bar opens it.
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.Visibility_Lambda([this]() { return bHelpOpen ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				BuildHelpDialog()
			]
		]
		// The game menu, over everything (Esc or MENU; it pauses the game).
		+ SOverlay::Slot()
		[
			SAssignNew(Menu, SLRGameMenu)
			.Subsystem(Subsystem)
			.OnClose(FSimpleDelegate::CreateLambda([this]() { SetMenuOpen(false); }))
			.Visibility_Lambda([this]() { return bMenuOpen ? EVisibility::Visible : EVisibility::Collapsed; })
		]
		]
	];
}

float SLRGameHud::GetUIScale() const
{
	const ULRGameSubsystem* Sub = GetSubsystem();
	const ULRUserSettings* Settings = Sub ? Sub->GetUserSettings() : nullptr;
	return Settings ? FMath::Clamp(Settings->UIScale, 0.5f, 2.f) : 1.f;
}

void SLRGameHud::SetMenuOpen(bool bOpen)
{
	bMenuOpen = bOpen;
	if (ULRGameSubsystem* Sub = GetSubsystem())
	{
		Sub->SetPaused(bOpen);
	}
	if (bOpen)
	{
		bHelpOpen = false;
		if (Menu.IsValid())
		{
			Menu->Open();
		}
	}
	else
	{
		FSlateApplication::Get().SetAllUserFocusToGameViewport();
	}
}

void SLRGameHud::HandleMenuKey()
{
	if (bMenuOpen)
	{
		if (Menu.IsValid())
		{
			Menu->Back();
		}
		return;
	}
	ULRGameSubsystem* Sub = GetSubsystem();
	if (bHelpOpen)
	{
		bHelpOpen = false;
	}
	else if (bBuildPage)
	{
		bBuildPage = false;
	}
	else if (Sub && Sub->IsOutsideViewOpen())
	{
		Sub->SetOutsideViewOpen(false);
	}
	else
	{
		SetMenuOpen(true);
	}
}

void SLRGameHud::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	const ULRGameSubsystem* Sub = GetSubsystem();
	const float Goal = (Sub && Sub->IsOutsideViewOpen()) ? 1.f : 0.f;
	OutsideDrop = FMath::FInterpConstantTo(OutsideDrop, Goal, InDeltaTime, 4.f); // a quarter of a second
	// In the UI's own (scaled) units, which is what the panels are laid out in.
	ViewSize = FVector2f(AllottedGeometry.GetLocalSize()) / GetUIScale();

	// A different selection (or none) puts the card back on its main page.
	FIntVector Selected = FIntVector::ZeroValue;
	const bool bHasSelection = GetSelected(Selected);
	if (bHasSelection != bCardHadSelection || (bHasSelection && Selected != CardCell))
	{
		bBuildPage = false;
		bCardHadSelection = bHasSelection;
		CardCell = Selected;
	}

	// Fit the 4:3 camera into the space under the dial and the chamber, no wider than the panel
	// allows next to its other columns, and never below CameraMinHeight.
	if (CameraArea.IsValid() && OutsideDrop > 0.f)
	{
		const float AreaHeight = CameraArea->GetTickSpaceGeometry().GetLocalSize().Y - CameraChrome;
		const float MaxWidth = ViewSize.X * OutsideMaxWidthShare - OutsideSideColumnsWidth - CameraChrome;
		const float Height = FMath::Max(FMath::Min(AreaHeight, MaxWidth * 0.75f), CameraMinHeight);
		CameraSize = FVector2f(Height * 4.f / 3.f, Height);
	}
	const FLRSimulation* Sim = GetSimulation();
	const float StaticGoal = (Sim && Sim->AreInstrumentsDown()) ? 1.f : 0.f;
	StaticLevel = FMath::FInterpConstantTo(StaticLevel, StaticGoal, InDeltaTime, 3.f); // a third of a second
}

TSharedRef<SWidget> SLRGameHud::BuildInstrumentStatic()
{
	const FLRHudStyle& Style = FLRHudStyle::Get();
	return SNew(SOverlay)
		.Visibility(EVisibility::HitTestInvisible)
		+ SOverlay::Slot()
		[
			SNew(SLRStaticNoise)
			.Resolution(FIntPoint(480, 270))
			.Material(StaticMaterial)
			.Intensity_Lambda([this]() { return StaticLevel * 0.9f; })
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBorder)
			.Visibility_Lambda([this]() { return StaticLevel > 0.5f ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			.BorderImage(&Style.WhiteBrush)
			.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.8f))
			.Padding(FMargin(18.f, 10.f))
			[
				SNew(STextBlock)
				.Font(Style.HeadingFont)
				.ColorAndOpacity(Style.Red)
				.Justification(ETextJustify::Center)
				.Text_Lambda([this]()
				{
					const FLRSimulation* Sim = GetSimulation();
					if (Sim && !Sim->IsRecharging() && (Sim->IsVenting() || Sim->GetInjectorFlow() < 0.0))
					{
						return AsText(Sim->IsVenting()
							? FString(TEXT("INSTRUMENTS DOWN\nVenting floods the chamber with radiation"))
							: FString(TEXT("INSTRUMENTS DOWN\nThe reversed injectors are winding down")));
					}
					const double Full = Sim ? Sim->GetData().Host.ChargeCapacity : 0.0;
					const double Percent = (Sim && Full > 0.0) ? 100.0 * Sim->GetStoredCharge() / Full : 0.0;
					return AsText(FString::Printf(TEXT("INSTRUMENTS DOWN\nThey come back when the stored charge is full (%.0f%%)"), Percent));
				})
			]
		];
}

TSharedRef<SWidget> SLRGameHud::MakePanel(const FText& Title, const TSharedRef<SWidget>& Content,
	const TSharedRef<SWidget>& HeaderExtra, bool bFillHeight, bool bConsole)
{
	const FLRHudStyle& Style = FLRHudStyle::Get();

	TSharedRef<SVerticalBox> Body = SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			// Title bar (Rails: .fac-title-bar)
			SNew(SBorder)
			.BorderImage(&Style.WhiteBrush)
			.BorderBackgroundColor(bConsole ? Style.ConsoleInner : Style.PanelInner)
			.Padding(FMargin(8.f, 4.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(Title)
					.Font(Style.HeadingFont)
					.ColorAndOpacity(bConsole ? Style.ConsoleAccent : Style.Orange)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					HeaderExtra
				]
			]
		];

	if (bFillHeight)
	{
		Body->AddSlot().FillHeight(1.f).Padding(FMargin(8.f))[Content];
	}
	else
	{
		Body->AddSlot().AutoHeight().Padding(FMargin(8.f))[Content];
	}

	// Outer border + panel fill (Rails: .fac-panel)
	return SNew(SBorder)
		.BorderImage(&Style.WhiteBrush)
		.BorderBackgroundColor(bConsole ? Style.ConsoleBorder : Style.Border)
		.Padding(FMargin(2.f))
		[
			SNew(SBorder)
			.BorderImage(&Style.WhiteBrush)
			.BorderBackgroundColor(bConsole ? Style.ConsolePanel : Style.Panel)
			.Padding(FMargin(0.f))
			[
				Body
			]
		];
}

TSharedRef<SWidget> SLRGameHud::BuildHeader()
{
	const FLRHudStyle& Style = FLRHudStyle::Get();

	TSharedRef<SWidget> Content = SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Title", "QUANTUM RECURSION"))
				.Font(Style.TitleFont)
				.ColorAndOpacity(Style.Orange)
			]
			// (?): the controls, in a dialog.
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(12.f, 0.f, 0.f, 0.f))
			[
				SNew(SButton)
				.ButtonStyle(&Style.ButtonStyle)
				.IsFocusable(false)
				.ContentPadding(FMargin(8.f, 1.f))
				.ToolTipText(LOCTEXT("HelpTip", "Controls and help"))
				.OnClicked_Lambda([this]()
				{
					bHelpOpen = !bHelpOpen;
					return FReply::Handled();
				})
				[
					SNew(STextBlock)
					.Font(Style.HeadingFont)
					.ColorAndOpacity(Style.Orange)
					.Text(LOCTEXT("HelpButton", "?"))
				]
			]
			// MENU: the game menu (Esc).
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(6.f, 0.f, 0.f, 0.f))
			[
				SNew(SButton)
				.ButtonStyle(&Style.ButtonStyle)
				.IsFocusable(false)
				.ContentPadding(FMargin(8.f, 1.f))
				.ToolTipText(LOCTEXT("MenuTip", "The menu: save, load, settings, quit (Esc)"))
				.OnClicked_Lambda([this]()
				{
					SetMenuOpen(true);
					return FReply::Handled();
				})
				[
					SNew(STextBlock)
					.Font(Style.HeadingFont)
					.ColorAndOpacity(Style.Orange)
					.Text(LOCTEXT("MenuButton", "MENU"))
				]
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			[
				SNew(SSpacer)
			]
			// The cosmic clock: epoch and time since the pocket universe's Big Bang.
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(0.f, 0.f, 16.f, 0.f))
			[
				SNew(STextBlock)
				.Font(Style.HeadingFont)
				.ColorAndOpacity(Style.Orange)
				.Text_Lambda([this]()
				{
					const ULRGameSubsystem* Sub = GetSubsystem();
					if (!Sub || Sub->GetEpochName().IsEmpty())
					{
						return FText::GetEmpty();
					}
					return AsText(FString::Printf(TEXT("%s  |  %s after the Big Bang"), *Sub->GetEpochName(), *Sub->GetCosmicTimeText()));
				})
			]
			// The host black hole, whichever view is up: its mass and net rate, red while it shrinks.
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(0.f, 0.f, 10.f, 0.f))
			[
				SNew(STextBlock)
				.Font(Style.BodyFont)
				.Text_Lambda([this]()
				{
					const FLRSimulation* Sim = GetSimulation();
					if (!Sim || !Sim->GetData().Host.IsDefined())
					{
						return FText::GetEmpty();
					}
					if (Sim->IsFrozen())
					{
						return LOCTEXT("HostFrozen", "Host evaporated: universe frozen. Ignite a new one (F)");
					}
					const double Net = Sim->GetNetRate();
					FString Trend = Net < 0.0
						? FString::Printf(TEXT("evaporates in %s"), *FormatDuration(Sim->GetTimeToEvaporation()))
						: FString(TEXT("growing"));
					if (Sim->GetHostMass() < Sim->GetTippingMass())
					{
						Trend += TEXT("  BELOW THE POINT OF NO RETURN");
					}
					if (Sim->IsVenting())
					{
						Trend += TEXT("  VENTING");
					}
					if (Sim->AreInstrumentsDown())
					{
						Trend += TEXT("  INSTRUMENTS DOWN");
					}
					return AsText(FString::Printf(TEXT("Host %s  %s  %s"), *FLRSimulation::FormatMass(Sim->GetHostMass()),
						*FormatSignedRate(Net), *Trend));
				})
				.ColorAndOpacity_Lambda([this]() -> FSlateColor
				{
					const FLRHudStyle& S = FLRHudStyle::Get();
					const FLRSimulation* Sim = GetSimulation();
					if (!Sim || Sim->IsFrozen())
					{
						return S.Red;
					}
					return Sim->GetNetRate() < 0.0 ? S.Red : S.Green;
				})
			]
			// OUTSIDE: drops the injector panel down. It pulses when the outside needs attention.
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(0.f, 0.f, 16.f, 0.f))
			[
				SNew(SButton)
				.ButtonStyle(&Style.ButtonStyle)
				.IsFocusable(false)
				.ContentPadding(FMargin(10.f, 3.f))
				.ToolTipText(LOCTEXT("OutsideTip", "The facility's injectors: the feed dial, the host in its chamber, the stored charge and Ignite (F or Tab)"))
				.ButtonColorAndOpacity_Lambda([this]() -> FSlateColor
				{
					if (!DoesOutsideNeedAttention())
					{
						return FLinearColor::White;
					}
					const float Pulse = 0.5f + 0.5f * FMath::Sin(static_cast<float>(FSlateApplication::Get().GetCurrentTime()) * 6.f);
					return FMath::Lerp(FLinearColor::White, FLinearColor(2.2f, 0.9f, 0.9f), Pulse);
				})
				.OnClicked_Lambda([this]()
				{
					if (ULRGameSubsystem* Sub = GetSubsystem())
					{
						Sub->ToggleOutsideView();
					}
					return FReply::Handled();
				})
				[
					SNew(STextBlock)
					.Font(Style.HeadingFont)
					.ColorAndOpacity(Style.Orange)
					.Text_Lambda([this]()
					{
						const ULRGameSubsystem* Sub = GetSubsystem();
						return (Sub && Sub->IsOutsideViewOpen()) ? LOCTEXT("InsideButton", "INSIDE [F]") : LOCTEXT("OutsideButton", "OUTSIDE [F]");
					})
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Font(Style.BodyFont)
				.ColorAndOpacity(Style.TextDim)
				.Text_Lambda([this]()
				{
					const ULRGameSubsystem* Sub = GetSubsystem();
					if (!Sub)
					{
						return FText::GetEmpty();
					}
					return AsText(FString::Printf(TEXT("t = %.0fs   speed %.1fx"), Sub->GetSimTime(), Sub->GetTimeScale()));
				})
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(0.f, 4.f, 0.f, 0.f))
		[
			// Shown only if Content/Data/*.json failed to load or validate.
			SNew(STextBlock)
			.Font(Style.BodyFont)
			.ColorAndOpacity(Style.Red)
			.AutoWrapText(true)
			.Visibility_Lambda([this]()
			{
				const ULRGameSubsystem* Sub = GetSubsystem();
				return (Sub && !Sub->GetDataErrors().IsEmpty()) ? EVisibility::Visible : EVisibility::Collapsed;
			})
			.Text_Lambda([this]()
			{
				const ULRGameSubsystem* Sub = GetSubsystem();
				if (!Sub || Sub->GetDataErrors().IsEmpty())
				{
					return FText::GetEmpty();
				}
				const TArray<FString>& Errors = Sub->GetDataErrors();
				return AsText(FString::Printf(TEXT("GAME DATA ERROR: %s%s  (see Output Log; saving is disabled)"),
					*Errors[0], Errors.Num() > 1 ? *FString::Printf(TEXT(" (+%d more)"), Errors.Num() - 1) : TEXT("")));
			})
		];

	return SNew(SBorder)
		.BorderImage(&Style.WhiteBrush)
		.BorderBackgroundColor(Style.Border)
		.Padding(FMargin(2.f))
		[
			SNew(SBorder)
			.BorderImage(&Style.WhiteBrush)
			.BorderBackgroundColor(Style.Panel)
			.Padding(FMargin(10.f, 6.f))
			[
				Content
			]
		];
}

TSharedRef<SWidget> SLRGameHud::BuildActionsPanel()
{
	const FLRHudStyle& Style = FLRHudStyle::Get();
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);

	// What the card is for: the selected cell and what's in it.
	Box->AddSlot()
	.AutoHeight()
	.Padding(FMargin(0.f, 0.f, 0.f, 6.f))
	[
		SNew(STextBlock)
		.Font(Style.BodyFont)
		.ColorAndOpacity(Style.Orange)
		.AutoWrapText(true)
		.Text_Lambda([this]()
		{
			const FLRSimulation* Sim = GetSimulation();
			FIntVector Cell;
			if (!Sim || !GetSelected(Cell))
			{
				return LOCTEXT("CardNoSelection", "Click a cell to see what you can do there.");
			}
			const FLRPlacedEntity* Entity = Sim->FindPlaced(Cell);
			FString Text = FString::Printf(TEXT("%s%s  %s"), bBuildPage ? TEXT("Build in ") : TEXT(""), *FLRSimulation::DescribeCell(Cell),
				Entity ? *Sim->GetData().GetDisplayName(Entity->Item) : TEXT("(empty)"));
			if (const FLRCellJob* Job = Sim->FindJob(Cell))
			{
				Text += FString::Printf(TEXT("\n%s under way: %.0f%%"), *DescribeJob(*Job), 100.f * Job->GetProgress(Sim->GetNow()));
			}
			return AsText(Text);
		})
	];

	for (int32 Slot = 0; Slot < LRInput::CardSlotCount; ++Slot)
	{
		Box->AddSlot()
		.AutoHeight()
		.Padding(FMargin(0.f, 1.f))
		[
			MakeCardSlot(Slot)
		];
	}

	// On the Build page: how many recipes are still to discover.
	Box->AddSlot()
	.AutoHeight()
	.Padding(FMargin(0.f, 4.f, 0.f, 0.f))
	[
		SNew(STextBlock)
		.Font(Style.SmallFont)
		.ColorAndOpacity(Style.TextDark)
		.Visibility_Lambda([this]() { return bBuildPage ? EVisibility::Visible : EVisibility::Collapsed; })
		.Text_Lambda([this]()
		{
			const FLRSimulation* Sim = GetSimulation();
			int32 Locked = 0;
			if (Sim)
			{
				for (const FLRRecipeDef& Recipe : Sim->GetData().Recipes)
				{
					Locked += Sim->IsRecipeUnlocked(Recipe.Id) ? 0 : 1;
				}
			}
			return Locked > 0 ? AsText(FString::Printf(TEXT("%d recipe(s) still to discover..."), Locked)) : FText::GetEmpty();
		})
	];

	Box->AddSlot()
	.AutoHeight()
	.Padding(FMargin(0.f, 8.f, 0.f, 0.f))
	[
		BuildLayerStrip()
	];

	TSharedRef<SWidget> PageLabel = SNew(STextBlock)
		.Font(Style.SmallFont)
		.ColorAndOpacity(Style.TextDim)
		.Text_Lambda([this]()
		{
			if (!bBuildPage)
			{
				return FText::GetEmpty();
			}
			return AsText(FString::Printf(TEXT("BUILD  (%s: back)"), *GetKeyLabel(LRInput::Names::Menu).ToString()));
		});

	return MakePanel(LOCTEXT("Actions", "ACTIONS"), SNew(SBox).WidthOverride(340.f)[Box], PageLabel);
}

TSharedRef<SWidget> SLRGameHud::MakeCardSlot(int32 Slot)
{
	const FLRHudStyle& Style = FLRHudStyle::Get();
	const FName Command = LRInput::SlotCommand(Slot);

	// The bar under a slot: the selected cell's job fills orange if it's this slot's; otherwise a
	// cooldown drains dim (for the slot's action, or Build for a recipe).
	auto ActionOf = [this, Slot]()
	{
		const FCardEntry Entry = GetCardEntry(Slot);
		return Entry.Kind == ECardKind::Action ? Entry.Name : (Entry.Kind == ECardKind::Recipe ? LRNames::Craft : FName());
	};
	auto JobOf = [this, Slot]() -> const FLRCellJob*
	{
		const FLRSimulation* Sim = GetSimulation();
		FIntVector Cell;
		const FLRCellJob* Job = (Sim && GetSelected(Cell)) ? Sim->FindJob(Cell) : nullptr;
		const FCardEntry Entry = GetCardEntry(Slot);
		const bool bThisSlot = Job && ((Entry.Kind == ECardKind::Action && Job->Request.Action == Entry.Name)
			|| (Entry.Kind == ECardKind::Recipe && Job->Request.Action == LRNames::Craft && Job->Request.Choice == Entry.Name));
		return bThisSlot ? Job : nullptr;
	};

	return SNew(SHorizontalBox)
		// The slot's key.
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(FMargin(0.f, 0.f, 4.f, 0.f))
		[
			SNew(SBox)
			.WidthOverride(28.f)
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Font(Style.SmallFont)
				.ColorAndOpacity(Style.TextDim)
				.Text_Lambda([this, Command]() { return GetKeyLabel(Command); })
			]
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SButton)
				.ButtonStyle(&Style.ButtonStyle)
				.IsFocusable(false)
				.ContentPadding(FMargin(8.f, 4.f))
				.IsEnabled_Lambda([this, Slot]() { return IsCardEntryEnabled(GetCardEntry(Slot)); })
				.OnClicked_Lambda([this, Slot]()
				{
					ActivateCardSlot(Slot);
					return FReply::Handled();
				})
				.OnHovered_Lambda([this, Slot]()
				{
					const FCardEntry Entry = GetCardEntry(Slot);
					if (Entry.Kind == ECardKind::Action)
					{
						SetHover(EHoverKind::Action, Entry.Name);
					}
					else if (Entry.Kind == ECardKind::Recipe)
					{
						SetHover(EHoverKind::Recipe, Entry.Name);
					}
					else
					{
						ClearHover();
					}
				})
				.OnUnhovered_Lambda([this]() { ClearHover(); })
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Font(Style.BodyFont)
						.ColorAndOpacity(Style.Text)
						.Text_Lambda([this, Slot]() { return GetCardEntryLabel(GetCardEntry(Slot)); })
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Font(Style.SmallFont)
						.Text_Lambda([this, Slot]() { return GetCardEntryDetail(GetCardEntry(Slot)); })
						.ColorAndOpacity_Lambda([this, Slot]() -> FSlateColor
						{
							// Red when something stops it: not enough matter (the cost), busy, occupied...
							const FLRHudStyle& S = FLRHudStyle::Get();
							return GetCardEntryBlocker(GetCardEntry(Slot)).IsNone() ? S.TextDim : S.Red;
						})
					]
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(0.f, 1.f, 0.f, 0.f))
			[
				SNew(SBox)
				.HeightOverride(3.f)
				[
					SNew(SProgressBar)
					.Style(&Style.ProgressStyle)
					.Percent_Lambda([this, ActionOf, JobOf]() -> TOptional<float>
					{
						if (const FLRCellJob* Job = JobOf())
						{
							return Job->GetProgress(GetSimulation()->GetNow());
						}
						const FName ActionName = ActionOf();
						if (ActionName.IsNone())
						{
							return 0.f;
						}
						const FLRActionStatus Status = GetStatus(ActionName);
						return (Status.bOnCooldown && Status.Cooldown > 0.f) ? Status.CooldownRemaining / Status.Cooldown : 0.f;
					})
					.FillColorAndOpacity_Lambda([JobOf]() -> FSlateColor
					{
						const FLRHudStyle& S = FLRHudStyle::Get();
						return JobOf() ? S.Orange : S.TextDark;
					})
				]
			]
		];
}

TArray<SLRGameHud::FCardEntry> SLRGameHud::GetCardEntries() const
{
	TArray<FCardEntry> Entries;
	const FLRSimulation* Sim = GetSimulation();
	FIntVector Cell;
	if (!Sim || !GetSelected(Cell))
	{
		return Entries;
	}
	const FLRGameData& Data = Sim->GetData();

	// The Build page: every unlocked recipe, and Back in the last slot.
	if (bBuildPage)
	{
		for (const FLRRecipeDef& Recipe : Data.Recipes)
		{
			if (Sim->IsRecipeUnlocked(Recipe.Id) && Entries.Num() < LRInput::CardSlotCount - 1)
			{
				Entries.Add({ ECardKind::Recipe, Recipe.Id });
			}
		}
		Entries.SetNum(LRInput::CardSlotCount - 1);
		Entries.Add({ ECardKind::Back, NAME_None });
		return Entries;
	}

	// The main page: what can be done with this cell's contents, from the top, in a fixed order.
	const FLRPlacedEntity* Entity = Sim->FindPlaced(Cell);
	const FLRItemDef* Def = Entity ? Data.FindItem(Entity->Item) : nullptr;
	const FLRActionDef* Perturb = Data.FindAction(LRNames::Perturb);
	const bool bRipple = Entity && Perturb && Entity->Item == Perturb->Places;
	if (GetStatus(LRNames::Perturb).bRevealed && (!Entity || bRipple))
	{
		Entries.Add({ ECardKind::Action, LRNames::Perturb });
	}
	if (GetStatus(LRNames::Craft).bRevealed && (!Entity || (Def && Def->IsIrradiator())))
	{
		Entries.Add({ ECardKind::BuildPage, LRNames::Craft });
	}
	if (GetStatus(LRNames::Use).bRevealed && Def && (Def->IsLootBox() || Def->IsIrradiator()))
	{
		Entries.Add({ ECardKind::Action, LRNames::Use });
	}
	if (GetStatus(LRNames::Dismantle).bRevealed && Entity && !(Def && Def->IsStructure()))
	{
		Entries.Add({ ECardKind::Action, LRNames::Dismantle });
	}
	return Entries;
}

SLRGameHud::FCardEntry SLRGameHud::GetCardEntry(int32 Slot) const
{
	const TArray<FCardEntry> Entries = GetCardEntries();
	return Entries.IsValidIndex(Slot) ? Entries[Slot] : FCardEntry();
}

FName SLRGameHud::GetCardEntryBlocker(const FCardEntry& Entry) const
{
	const FLRSimulation* Sim = GetSimulation();
	FIntVector Cell;
	if (!Sim || !GetSelected(Cell))
	{
		return NAME_None;
	}
	// The same checks the simulation makes when the request arrives.
	FLRActionRequest Request;
	Request.Cell = Cell;
	Request.bHasCell = true;
	switch (Entry.Kind)
	{
	case ECardKind::Action:
		Request.Action = Entry.Name;
		break;
	case ECardKind::Recipe:
		Request.Action = LRNames::Craft;
		Request.Choice = Entry.Name;
		break;
	case ECardKind::BuildPage:
		return Sim->IsCellBusy(Cell) ? FName(TEXT("cell_busy")) : NAME_None;
	default:
		return NAME_None;
	}
	const FName Reason = Sim->CheckRequest(Request);
	return Reason == FName(TEXT("on_cooldown")) ? NAME_None : Reason; // a moment's pause isn't a blocker
}

bool SLRGameHud::IsCardEntryEnabled(const FCardEntry& Entry) const
{
	switch (Entry.Kind)
	{
	case ECardKind::Action:
	case ECardKind::Recipe:
	{
		const FLRSimulation* Sim = GetSimulation();
		FIntVector Cell;
		// Nothing stops it, and it isn't in the short global cooldown.
		return Sim && GetSelected(Cell) && GetCardEntryBlocker(Entry).IsNone()
			&& !GetStatus(Entry.Kind == ECardKind::Action ? Entry.Name : LRNames::Craft).bOnCooldown;
	}
	case ECardKind::BuildPage:
		return GetStatus(LRNames::Craft).bRevealed && GetCardEntryBlocker(Entry).IsNone();
	case ECardKind::Back:
		return true;
	default:
		return false;
	}
}

FText SLRGameHud::GetCardEntryLabel(const FCardEntry& Entry) const
{
	switch (Entry.Kind)
	{
	case ECardKind::Action:
		return AsText(GetStatus(Entry.Name).Label);
	case ECardKind::BuildPage:
		return AsText(FString::Printf(TEXT("%s..."), *GetStatus(LRNames::Craft).Label));
	case ECardKind::Recipe:
	{
		const FLRSimulation* Sim = GetSimulation();
		const FLRRecipeDef* Recipe = Sim ? Sim->GetData().FindRecipe(Entry.Name) : nullptr;
		return AsText(Recipe ? Recipe->Label : Entry.Name.ToString());
	}
	case ECardKind::Back:
		return LOCTEXT("CardBack", "Back");
	default:
		return FText::GetEmpty();
	}
}

FText SLRGameHud::GetCardEntryDetail(const FCardEntry& Entry) const
{
	// Why it can't be done, in a word or two (in red); a recipe shows its cost instead when
	// matter is what's missing, or when nothing is.
	const FName Blocker = GetCardEntryBlocker(Entry);
	const bool bCost = Entry.Kind == ECardKind::Recipe && (Blocker.IsNone() || Blocker == FName(TEXT("insufficient_materials")));
	if (bCost)
	{
		return AsText(DescribeCost(Entry.Name, /*bMultiline*/ false));
	}
	return AsText(FLRSimulation::DescribeReasonShort(Blocker));
}

FString SLRGameHud::DescribeJob(const FLRCellJob& Job) const
{
	const FLRSimulation* Sim = GetSimulation();
	if (!Sim)
	{
		return FString();
	}
	if (Job.Request.Action == LRNames::Craft)
	{
		const FLRRecipeDef* Recipe = Sim->GetData().FindRecipe(Job.Request.Choice);
		return FString::Printf(TEXT("Building %s"), Recipe ? *Recipe->Label : *Job.Request.Choice.ToString());
	}
	return GetStatus(Job.Request.Action).Label;
}

void SLRGameHud::RunCardEntry(const FCardEntry& Entry)
{
	ULRGameSubsystem* Sub = GetSubsystem();
	if (!Sub)
	{
		return;
	}
	switch (Entry.Kind)
	{
	case ECardKind::Action:
		Sub->RequestActionWithSelection(Entry.Name);
		break;
	case ECardKind::BuildPage:
		bBuildPage = true;
		ClearHover();
		break;
	case ECardKind::Recipe:
		if (Sub->Craft(Entry.Name).bSuccess)
		{
			bBuildPage = false; // built: back to the cell's actions
		}
		break;
	case ECardKind::Back:
		bBuildPage = false;
		break;
	default:
		break;
	}
}

void SLRGameHud::ActivateCardSlot(int32 Slot)
{
	if (bMenuOpen)
	{
		return;
	}
	// While the outside panel is down, the slot keys press its buttons instead.
	const ULRGameSubsystem* Sub = GetSubsystem();
	if (Sub && Sub->IsOutsideViewOpen())
	{
		if (Slot >= 0 && Slot < static_cast<int32>(EOutsideSlot::Count))
		{
			RunOutsideSlot(static_cast<EOutsideSlot>(Slot));
		}
		return;
	}
	const FCardEntry Entry = GetCardEntry(Slot);
	if (IsCardEntryEnabled(Entry))
	{
		RunCardEntry(Entry);
	}
}

FText SLRGameHud::GetKeyLabel(FName Command) const
{
	const ULRGameSubsystem* Sub = GetSubsystem();
	const FKey Primary = LRKeyBindings::GetKey(Sub, Command, 0);
	const FKey Shown = Primary.IsValid() ? Primary : LRKeyBindings::GetKey(Sub, Command, 1);
	return Shown.IsValid() ? Shown.GetDisplayName(/*bLongDisplayName*/ false) : FText::GetEmpty();
}

TSharedRef<SWidget> SLRGameHud::BuildLayerStrip()
{
	const FLRHudStyle& Style = FLRHudStyle::Get();
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			MakeSmallButton(LOCTEXT("LayerDown", "- Layer"), [this]()
			{
				if (ULRGameSubsystem* Sub = GetSubsystem()) { Sub->ChangeBuildLayer(-1); }
			})
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Font(Style.HeadingFont)
			.ColorAndOpacity(Style.Text)
			.Text_Lambda([this]()
			{
				const ULRGameSubsystem* Sub = GetSubsystem();
				return AsText(FString::Printf(TEXT("Layer Z = %d"), Sub ? Sub->GetBuildLayer() : 0));
			})
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			MakeSmallButton(LOCTEXT("LayerUp", "Layer +"), [this]()
			{
				if (ULRGameSubsystem* Sub = GetSubsystem()) { Sub->ChangeBuildLayer(1); }
			})
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(FMargin(4.f, 0.f, 0.f, 0.f))
		[
			MakeSmallButton(LOCTEXT("Home", "Home"), [this]()
			{
				if (ULRGameSubsystem* Sub = GetSubsystem()) { Sub->FocusHome(); }
			})
		];
}

TSharedRef<SWidget> SLRGameHud::BuildUniversePanel()
{
	// No inventory: matter lives in grid cells. This is the total of each material, and how
	// much of it the selected cell can reach (what building there can use).
	const FLRHudStyle& Style = FLRHudStyle::Get();
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);

	auto Heading = [&Style](const FText& Label, float Width)
	{
		return SNew(SBox)
			.WidthOverride(Width)
			.HAlign(HAlign_Right)
			[
				SNew(STextBlock)
				.Font(Style.SmallFont)
				.ColorAndOpacity(Style.TextDark)
				.Text(Label)
			];
	};
	Box->AddSlot()
	.AutoHeight()
	.Padding(FMargin(4.f, 0.f, 4.f, 2.f))
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		[
			SNew(STextBlock)
			.Font(Style.SmallFont)
			.ColorAndOpacity(Style.TextDark)
			.Text(LOCTEXT("MatterHeading", "Matter"))
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			Heading(LOCTEXT("TotalHeading", "In the universe"), 100.f)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			Heading(LOCTEXT("ReachHeading", "Within reach"), 100.f)
		]
	];

	if (const FLRSimulation* Sim = GetSimulation())
	{
		for (const TPair<FName, FLRItemDef>& Pair : Sim->GetData().Items)
		{
			if (Pair.Value.Category == LRNames::CategoryMaterial)
			{
				Box->AddSlot()
				.AutoHeight()
				.Padding(FMargin(0.f, 1.f))
				[
					MakeMatterRow(Pair.Key)
				];
			}
		}
	}

	Box->AddSlot()
	.AutoHeight()
	.Padding(FMargin(4.f, 4.f, 4.f, 0.f))
	[
		SNew(STextBlock)
		.Font(Style.SmallFont)
		.ColorAndOpacity(Style.TextDark)
		.AutoWrapText(true)
		.Text_Lambda([this]()
		{
			const FLRSimulation* Sim = GetSimulation();
			const int32 Radius = Sim ? Sim->GetReachRadius() : 0;
			FIntVector Cell;
			if (!GetSelected(Cell))
			{
				return FText::Format(LOCTEXT("ReachHint", "Select a cell to see what's within reach of it: matter up to {0} cells away."), FText::AsNumber(Radius));
			}
			return FText::Format(LOCTEXT("ReachOf", "Within reach of {0}: matter up to {1} cells away. Building there pays from it, nearest first."),
				AsText(FLRSimulation::DescribeCell(Cell)), FText::AsNumber(Radius));
		})
	];

	return MakePanel(LOCTEXT("Universe", "UNIVERSE"),
		Box, // as wide as the right-hand column
		SNullWidget::NullWidget);
}

TSharedRef<SWidget> SLRGameHud::BuildInfoPanel()
{
	const FLRHudStyle& Style = FLRHudStyle::Get();

	TSharedRef<SWidget> Content = SNew(SBox)
		.MinDesiredHeight(120.f)
		[
			SNew(SVerticalBox)
			// Where things are: the selected cell, and the cell under the cursor.
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Font(Style.SmallFont)
				.ColorAndOpacity(Style.TextDim)
				.Text_Lambda([this]()
				{
					const ULRGameSubsystem* Sub = GetSubsystem();
					const FLRSimulation* Sim = GetSimulation();
					FIntVector Cell;
					FString Selected = TEXT("Selected: none");
					if (Sub && Sim && Sub->GetSelectedCell(Cell))
					{
						const FLRPlacedEntity* Entity = Sim->FindPlaced(Cell);
						Selected = FString::Printf(TEXT("Selected: %s %s"), *FLRSimulation::DescribeCell(Cell),
							Entity ? *Sim->GetData().GetDisplayName(Entity->Item) : TEXT("(empty)"));
					}
					FIntVector Hovered;
					const FString Cursor = (Sub && Sub->GetHoveredCell(Hovered)) ? FLRSimulation::DescribeCell(Hovered) : FString(TEXT("-"));
					return AsText(FString::Printf(TEXT("%s    Cursor: %s"), *Selected, *Cursor));
				})
			]
			// Irradiation: shown only when the selected cell holds an irradiator.
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(0.f, 4.f, 0.f, 0.f))
			[
				SNew(SBorder)
				.BorderImage(&Style.WhiteBrush)
				.BorderBackgroundColor(Style.PanelInner)
				.Padding(FMargin(6.f))
				.Visibility_Lambda([this]() { return GetSelectedIrradiator() ? EVisibility::Visible : EVisibility::Collapsed; })
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Font(Style.SmallFont)
						.ColorAndOpacity(Style.Text)
						.AutoWrapText(true)
						.Text_Lambda([this]()
						{
							const FLRPlacedEntity* Irradiator = GetSelectedIrradiator();
							return Irradiator ? AsText(DescribeIrradiator(*Irradiator)) : FText::GetEmpty();
						})
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(FMargin(0.f, 4.f))
					[
						SNew(SBox)
						.HeightOverride(4.f)
						[
							SNew(SProgressBar)
							.Style(&Style.ProgressStyle)
							.FillColorAndOpacity(Style.Orange)
							.Percent_Lambda([this]() -> TOptional<float> { return GetSelectedExposureFraction(); })
						]
					]
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(0.f, 6.f, 0.f, 4.f))
			[
				SNew(STextBlock)
				.Font(Style.HeadingFont)
				.ColorAndOpacity(Style.Orange)
				.AutoWrapText(true)
				.Text_Lambda([this]() { return GetHoverTitle(); })
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Font(Style.BodyFont)
				.ColorAndOpacity(Style.TextDim)
				.AutoWrapText(true)
				.Text_Lambda([this]() { return GetHoverBody(); })
			]
		];

	return MakePanel(LOCTEXT("Info", "INFO"), Content, SNullWidget::NullWidget);
}

TSharedRef<SWidget> SLRGameHud::BuildLogPanel()
{
	// Newest first, LogVisibleLines tall; scroll down for older ones (up to MaxLogLines).
	// A line of the small font plus its padding is about 16px.
	constexpr float LineHeight = 16.f;
	SAssignNew(LogBox, SVerticalBox);
	RebuildLog();

	TSharedRef<SWidget> ToTop = MakeSmallButton(LOCTEXT("LogToTop", "To top"), [this]()
	{
		if (LogScroll.IsValid())
		{
			LogScroll->ScrollToStart();
		}
	});

	return MakePanel(LOCTEXT("Log", "LOG"),
		SNew(SBox)
		.HeightOverride(LogVisibleLines * LineHeight)
		[
			SAssignNew(LogScroll, SScrollBox)
			+ SScrollBox::Slot()
			[
				LogBox.ToSharedRef()
			]
		],
		ToTop);
}

// ---------------------------------------------------------------------------------------
// Reusable pieces
// ---------------------------------------------------------------------------------------

TSharedRef<SWidget> SLRGameHud::MakeMatterRow(FName Item)
{
	const FLRHudStyle& Style = FLRHudStyle::Get();
	const FLRSimulation* Sim = GetSimulation();
	const FLRItemDef* Def = Sim ? Sim->GetData().FindItem(Item) : nullptr;
	const FLinearColor Swatch = Def ? Def->GetLinearColor() : FLinearColor::Gray;
	const FText Name = AsText(Sim ? Sim->GetData().GetDisplayName(Item) : Item.ToString());

	auto Number = [&Style](const FSlateColor& Color, TFunction<FText()> Value)
	{
		return SNew(SBox)
			.WidthOverride(100.f)
			.HAlign(HAlign_Right)
			[
				SNew(STextBlock)
				.Font(Style.BodyFont)
				.ColorAndOpacity(Color)
				.Text_Lambda(MoveTemp(Value))
			];
	};

	// A button only for its hover events (the info panel describes the material).
	return SNew(SButton)
		.ButtonStyle(&Style.SlotButtonStyle)
		.IsFocusable(false)
		.ContentPadding(FMargin(4.f, 2.f))
		.OnHovered_Lambda([this, Item]() { SetHover(EHoverKind::Material, Item); })
		.OnUnhovered_Lambda([this]() { ClearHover(); })
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(0.f, 0.f, 6.f, 0.f))
			[
				SNew(SBox)
				.WidthOverride(12.f)
				.HeightOverride(12.f)
				[
					SNew(SBorder)
					.BorderImage(&Style.WhiteBrush)
					.BorderBackgroundColor(Swatch)
				]
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Font(Style.BodyFont)
				.ColorAndOpacity(Style.Text)
				.Text(Name)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				Number(Style.Text, [this, Item]()
				{
					const FLRSimulation* S = GetSimulation();
					return FText::AsNumber(S ? S->GetTotalMatter(Item) : 0);
				})
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				Number(Style.Orange, [this, Item]()
				{
					const FLRSimulation* S = GetSimulation();
					FIntVector Cell;
					return (S && GetSelected(Cell)) ? FText::AsNumber(S->GetMatterInReach(Cell, Item)) : FText::FromString(TEXT("-"));
				})
			]
		];
}

TSharedRef<SWidget> SLRGameHud::MakeSmallButton(const FText& Label, TFunction<void()> OnClick, bool bConsole)
{
	const FLRHudStyle& Style = FLRHudStyle::Get();
	return SNew(SButton)
		.ButtonStyle(bConsole ? &Style.ConsoleButtonStyle : &Style.ButtonStyle)
		.IsFocusable(false)
		.ContentPadding(FMargin(8.f, 2.f))
		.OnClicked_Lambda([OnClick]()
		{
			OnClick();
			return FReply::Handled();
		})
		[
			SNew(STextBlock)
			.Font(Style.SmallFont)
			.ColorAndOpacity(bConsole ? Style.ConsoleText : Style.Text)
			.Text(Label)
		];
}

// ---------------------------------------------------------------------------------------
// Data helpers
// ---------------------------------------------------------------------------------------

const FLRSimulation* SLRGameHud::GetSimulation() const
{
	const ULRGameSubsystem* Sub = GetSubsystem();
	return Sub ? Sub->GetSimulation() : nullptr;
}

FLRActionStatus SLRGameHud::GetStatus(FName ActionName) const
{
	const FLRSimulation* Sim = GetSimulation();
	return Sim ? Sim->GetActionStatus(ActionName) : FLRActionStatus();
}

const FLRPlacedEntity* SLRGameHud::GetSelectedIrradiator() const
{
	const ULRGameSubsystem* Sub = GetSubsystem();
	const FLRSimulation* Sim = GetSimulation();
	FIntVector Cell;
	if (!Sub || !Sim || !Sub->GetSelectedCell(Cell))
	{
		return nullptr;
	}
	const FLRPlacedEntity* Entity = Sim->FindPlaced(Cell);
	const FLRItemDef* Def = Entity ? Sim->GetData().FindItem(Entity->Item) : nullptr;
	return (Def && Def->IsIrradiator()) ? Entity : nullptr;
}

TOptional<float> SLRGameHud::GetSelectedExposureFraction() const
{
	const FLRSimulation* Sim = GetSimulation();
	const FLRPlacedEntity* Irradiator = GetSelectedIrradiator();
	const FLRItemDef* Def = (Sim && Irradiator) ? Sim->GetData().FindItem(Irradiator->Item) : nullptr;
	if (!Def || Def->ExposureSeconds <= 0.f)
	{
		return 0.f;
	}
	return FMath::Clamp(static_cast<float>(Irradiator->ExposureProgress / Def->ExposureSeconds), 0.f, 1.f);
}

FString SLRGameHud::DescribeIrradiator(const FLRPlacedEntity& Irradiator) const
{
	const FLRSimulation* Sim = GetSimulation();
	if (!Sim)
	{
		return FString();
	}
	const FLRGameData& Data = Sim->GetData();
	const FLRItemDef* Def = Data.FindItem(Irradiator.Item);
	FString Text = FString::Printf(TEXT("%s: tier <= %d, up to %d stacks, %.0fs per exposure"),
		*Data.GetDisplayName(Irradiator.Item), Def ? Def->MaxRadiationTier : 0, Def ? Def->MaxExposureStacks : 0, Def ? Def->ExposureSeconds : 0.f);

	const FLRLootBoxInstance* Box = Irradiator.Chamber.IsEmpty() ? nullptr : Sim->FindLootBox(Irradiator.Chamber.InstanceId);
	if (Irradiator.Chamber.IsEmpty())
	{
		Text += TEXT("\nChamber: empty");
	}
	else if (Box && Box->bRevealed)
	{
		Text += FString::Printf(TEXT("\nChamber: %s, observed (contents fixed)"), *Data.GetDisplayName(Irradiator.Chamber.Item));
	}
	else
	{
		Text += FString::Printf(TEXT("\nChamber: %s, %d/%d stacks"), *Data.GetDisplayName(Irradiator.Chamber.Item),
			Box ? Box->Modifiers.Num() : 0, Def ? Def->MaxExposureStacks : 0);
	}

	if (Irradiator.Source.IsEmpty())
	{
		Text += TEXT("\nSource: empty");
	}
	else
	{
		const FLRItemDef* SourceDef = Data.FindItem(Irradiator.Source.Item);
		const FLRRadiationDef* Radiation = SourceDef ? Data.FindRadiation(SourceDef->Radiation) : nullptr;
		Text += FString::Printf(TEXT("\nSource: %s (%s, tier %d)"), *Data.GetDisplayName(Irradiator.Source.Item),
			Radiation ? *Radiation->Name : TEXT("?"), Radiation ? Radiation->Tier : 0);
	}

	if (Box)
	{
		const FString CacheText = DescribeCache(*Box);
		Text += CacheText.IsEmpty() ? FString() : TEXT("\n") + CacheText;
	}
	if (Box && Def && FLRSimulation::IsExposureComplete(*Box, *Def))
	{
		Text += TEXT("\nDone. Open the cache here.");
	}
	else if (Irradiator.Chamber.IsEmpty() || Irradiator.Source.IsEmpty())
	{
		Text += TEXT("\nIdle: build a cache and a radiation source into it.");
	}
	return Text;
}

bool SLRGameHud::GetSelected(FIntVector& OutCell) const
{
	const ULRGameSubsystem* Sub = GetSubsystem();
	return Sub && Sub->GetSelectedCell(OutCell);
}

FString SLRGameHud::DescribeCache(const FLRLootBoxInstance& Cache) const
{
	const FLRSimulation* Sim = GetSimulation();
	if (!Sim)
	{
		return FString();
	}
	TArray<FString> Lines;
	for (const FLRLootModifier& Modifier : Cache.Modifiers)
	{
		const FLRRadiationDef* From = Sim->GetData().FindRadiation(Modifier.Source);
		Lines.Add(FString::Printf(TEXT("Irradiated: %s%s"), *FLRSimulation::DescribeModifier(Modifier, Sim->GetData()),
			From ? *FString::Printf(TEXT(" (%s)"), *From->Name) : TEXT("")));
	}
	if (Cache.bRevealed)
	{
		Lines.Add(FString::Printf(TEXT("Observed contents: %s"),
			Cache.RevealedContents.IsEmpty() ? TEXT("nothing") : *Sim->DescribeAmounts(Cache.RevealedContents)));
	}
	return FString::Join(Lines, TEXT("\n"));
}

FString SLRGameHud::DescribeCellMatter(const FIntVector& Cell) const
{
	const FLRSimulation* Sim = GetSimulation();
	const FLRCellMatter* CellMatter = Sim ? Sim->GetAllMatter().Find(Cell) : nullptr;
	if (!CellMatter)
	{
		return FString();
	}
	TArray<FString> Lines;
	for (const FLRItemAmount& Amount : CellMatter->Amounts)
	{
		Lines.Add(FString::Printf(TEXT("%d %s"), Amount.Count, *Sim->GetData().GetDisplayName(Amount.Item)));
	}
	return FString::Join(Lines, TEXT(", "));
}

bool SLRGameHud::IsCellSelected(const FIntVector& Cell) const
{
	const ULRGameSubsystem* Sub = GetSubsystem();
	FIntVector Selected;
	return Sub && Sub->GetSelectedCell(Selected) && Selected == Cell;
}

FString SLRGameHud::DescribeOverdensity(const FLRPlacedEntity& Overdensity) const
{
	const FLRSimulation* Sim = GetSimulation();
	const FLRItemDef* Def = Sim ? Sim->GetData().FindItem(Overdensity.Item) : nullptr;
	if (!Def)
	{
		return FString();
	}
	FString Text = FString::Printf(TEXT("Amplitude %d of %d: %d roll(s) every %.0fs."),
		Overdensity.Amplitude, Def->MaxAmplitude, Overdensity.Amplitude, Def->YieldSeconds);

	const FLREpochDef* Epoch = Sim->GetEpoch();
	const FLRLootTableDef* Table = (Epoch && !Epoch->YieldTable.IsNone()) ? Sim->GetData().FindLootTable(Epoch->YieldTable) : nullptr;
	if (Table)
	{
		TArray<FString> Items;
		for (const FLRLootEntry& Entry : Table->Entries)
		{
			Items.AddUnique(Sim->GetData().GetDisplayName(Entry.Item));
		}
		Text += FString::Printf(TEXT("\nGathering: %s"), *FString::Join(Items, TEXT(", ")));
	}
	else
	{
		Text += TEXT("\nNothing to gather yet: there is no matter in this epoch.");
	}

	if (Sim->IsFrozen())
	{
		Text += TEXT("\nFrozen: the host black hole has evaporated. Ignite a new one (outside panel, F).");
	}
	else if (Overdensity.Amplitude >= Def->MaxAmplitude)
	{
		Text += TEXT("\nAs deep as it gets.");
	}
	else if (Epoch && Epoch->RippleGrowthSeconds > 0.f)
	{
		Text += FString::Printf(TEXT("\nGravity deepens it in %.0fs."), Epoch->RippleGrowthSeconds - Overdensity.GrowthProgress);
	}
	else if (Sim->GetActionStatus(LRNames::Perturb).bRevealed)
	{
		Text += TEXT("\nPerturb it again to deepen it.");
	}
	return Text;
}

// ---------------------------------------------------------------------------------------
// Info panel (Rails: the hoveredTooltip sidebar in MainLayout.vue)
// ---------------------------------------------------------------------------------------

void SLRGameHud::SetHover(EHoverKind Kind, FName Name, int32 Index, const FIntVector& Cell)
{
	HoverKind = Kind;
	HoverName = Name;
	HoverIndex = Index;
	HoverCell = Cell;
}

bool SLRGameHud::GetInfoCell(FIntVector& OutCell) const
{
	// A hovered Deployed-list row wins; otherwise show the cell under the 3D cursor.
	if (HoverKind == EHoverKind::WorldCell)
	{
		OutCell = HoverCell;
		return true;
	}
	// Otherwise the cell under the 3D cursor, or failing that the selected one.
	const ULRGameSubsystem* Sub = GetSubsystem();
	return HoverKind == EHoverKind::None && Sub && (Sub->GetHoveredCell(OutCell) || Sub->GetSelectedCell(OutCell));
}

FString SLRGameHud::DescribeCost(FName RecipeId, bool bMultiline) const
{
	const FLRSimulation* Sim = GetSimulation();
	const FLRRecipeDef* Recipe = Sim ? Sim->GetData().FindRecipe(RecipeId) : nullptr;
	if (!Recipe)
	{
		return FString();
	}

	FIntVector Cell;
	const bool bHasCell = GetSelected(Cell);
	TArray<FString> Parts;
	for (const FLRItemAmount& Cost : Recipe->Cost)
	{
		const FLRItemDef* Def = Sim->GetData().FindItem(Cost.Item);
		if (bMultiline)
		{
			if (bHasCell)
			{
				const int32 Have = Sim->GetMatterInReach(Cell, Cost.Item);
				Parts.Add(FString::Printf(TEXT("%s: %d  (%d within reach) %s"), *Sim->GetData().GetDisplayName(Cost.Item), Cost.Count, Have,
					Have >= Cost.Count ? TEXT("OK") : TEXT("- need more")));
			}
			else
			{
				Parts.Add(FString::Printf(TEXT("%s: %d"), *Sim->GetData().GetDisplayName(Cost.Item), Cost.Count));
			}
		}
		else
		{
			Parts.Add(FString::Printf(TEXT("%d %s"), Cost.Count, Def && !Def->Abbrev.IsEmpty() ? *Def->Abbrev : *Cost.Item.ToString()));
		}
	}
	return FString::Join(Parts, bMultiline ? TEXT("\n") : TEXT("  "));
}

FText SLRGameHud::GetHoverTitle() const
{
	const FLRSimulation* Sim = GetSimulation();
	if (!Sim)
	{
		return FText::GetEmpty();
	}

	switch (HoverKind)
	{
	case EHoverKind::Action:
		return AsText(GetStatus(HoverName).Label);

	case EHoverKind::Recipe:
		if (const FLRRecipeDef* Recipe = Sim->GetData().FindRecipe(HoverName))
		{
			return AsText(FString::Printf(TEXT("Build: %s"), *Recipe->Label));
		}
		break;

	case EHoverKind::Material:
		return AsText(Sim->GetData().GetDisplayName(HoverName));

	default:
		break;
	}

	FIntVector Cell;
	if (GetInfoCell(Cell))
	{
		return AsText(FString::Printf(TEXT("Cell %s"), *FLRSimulation::DescribeCell(Cell)));
	}
	return LOCTEXT("InfoIdle", "Info");
}

FText SLRGameHud::GetHoverBody() const
{
	const FLRSimulation* Sim = GetSimulation();
	if (!Sim)
	{
		return FText::GetEmpty();
	}
	const FLRGameData& Data = Sim->GetData();

	switch (HoverKind)
	{
	case EHoverKind::Action:
	{
		const FLRActionStatus Status = GetStatus(HoverName);
		FString Body = Status.Tooltip;
		Body += Status.CastTime > 0.f
			? FString::Printf(TEXT("\n\nTakes %.0fs in its cell (the cell is busy meanwhile; you can act elsewhere)."), Status.CastTime)
			: FString(TEXT("\n\nInstant."));
		// Why not, in full, when something stops it in the selected cell.
		const FName Blocker = GetCardEntryBlocker({ ECardKind::Action, HoverName });
		if (!Blocker.IsNone())
		{
			Body += FString::Printf(TEXT("\nCan't right now: %s."), *FLRSimulation::DescribeReason(Blocker));
		}
		return AsText(Body);
	}

	case EHoverKind::Recipe:
		if (const FLRRecipeDef* Recipe = Data.FindRecipe(HoverName))
		{
			FString Body = FString::Printf(TEXT("%s\n\nCost:\n%s"), *Recipe->Tooltip, *DescribeCost(HoverName, /*bMultiline*/ true));
			FIntVector Cell;
			if (!GetSelected(Cell))
			{
				Body += TEXT("\n\nSelect a cell to build in.");
			}
			else
			{
				const FName Blocker = Sim->ValidateBuild(HoverName, Cell);
				Body += Blocker.IsNone()
					? FString::Printf(TEXT("\n\nBuilds in %s."), *FLRSimulation::DescribeCell(Cell))
					: FString::Printf(TEXT("\n\nCan't build in %s: %s."), *FLRSimulation::DescribeCell(Cell), *FLRSimulation::DescribeReason(Blocker));
			}
			return AsText(Body);
		}
		break;

	case EHoverKind::Material:
		if (const FLRItemDef* Def = Data.FindItem(HoverName))
		{
			FString Body = FString::Printf(TEXT("%s\n\n%d in the pocket universe."), *Def->Tooltip, Sim->GetTotalMatter(HoverName));
			FIntVector Cell;
			if (GetSelected(Cell))
			{
				Body += FString::Printf(TEXT("\n%d within reach of %s, %d in the cell itself."),
					Sim->GetMatterInReach(Cell, HoverName), *FLRSimulation::DescribeCell(Cell), Sim->GetMatter(Cell, HoverName));
			}
			return AsText(Body);
		}
		break;

	default:
		break;
	}

	FIntVector Cell;
	if (!GetInfoCell(Cell))
	{
		return LOCTEXT("InfoHint", "Hover an action, recipe, material or cell to see details.");
	}

	FString Body;
	if (const FLRPlacedEntity* Entity = Sim->FindPlaced(Cell))
	{
		const FLRItemDef* Def = Data.FindItem(Entity->Item);
		const FString Tooltip = Def ? Def->Tooltip : FString();
		if (Def && Def->IsIrradiator())
		{
			Body = FString::Printf(TEXT("%s\n\n%s"), *Tooltip, *DescribeIrradiator(*Entity));
		}
		else if (Def && Def->IsOverdensity())
		{
			Body = FString::Printf(TEXT("%s\n\n%s"), *Tooltip, *DescribeOverdensity(*Entity));
		}
		else if (Def && Def->IsLootBox())
		{
			const FLRLootBoxInstance* Cache = Sim->FindCacheAt(Cell);
			const FString CacheText = Cache ? DescribeCache(*Cache) : FString();
			Body = FString::Printf(TEXT("%s\n%s%s\n\nOpen it here, or Dismantle it to get its matter back."),
				*Data.GetDisplayName(Entity->Item), *Tooltip, CacheText.IsEmpty() ? TEXT("") : *(TEXT("\n\n") + CacheText));
		}
		else
		{
			Body = FString::Printf(TEXT("%s\n%s\n\nBuilt at t=%.0fs. Dismantle returns what it cost to this cell."),
				*Data.GetDisplayName(Entity->Item), *Tooltip, Entity->PlacedAt);
		}
	}
	else
	{
		Body = GetStatus(LRNames::Perturb).bRevealed
			? TEXT("Empty cell. Click to select it, then Perturb to seed a ripple here, or Build something in it.")
			: TEXT("Empty cell. Click to select it, then Build something in it.");
	}

	const FString MatterText = DescribeCellMatter(Cell);
	Body += FString::Printf(TEXT("\n\nMatter here: %s"), MatterText.IsEmpty() ? TEXT("none") : *MatterText);
	return AsText(Body);
}

// ---------------------------------------------------------------------------------------
// Log
// ---------------------------------------------------------------------------------------

void SLRGameHud::AddLogMessage(const FString& Message, bool bIsError)
{
	LogLines.Add({ Message, bIsError });
	if (LogLines.Num() > MaxLogLines)
	{
		LogLines.RemoveAt(0, LogLines.Num() - MaxLogLines);
	}
	RebuildLog();
}

void SLRGameHud::RebuildLog()
{
	if (!LogBox.IsValid())
	{
		return;
	}
	const FLRHudStyle& Style = FLRHudStyle::Get();

	LogBox->ClearChildren();
	// Newest first.
	for (int32 Index = LogLines.Num() - 1; Index >= 0; --Index)
	{
		const FLogLine& Line = LogLines[Index];
		LogBox->AddSlot()
		.AutoHeight()
		.Padding(FMargin(0.f, 1.f))
		[
			SNew(STextBlock)
			.Font(Style.SmallFont)
			.AutoWrapText(true)
			.ColorAndOpacity(Line.bIsError ? Style.Red : (Index == LogLines.Num() - 1 ? Style.Text : Style.TextDim))
			.Text(AsText(Line.Text))
		];
	}
}

TSharedRef<SWidget> SLRGameHud::BuildHelpDialog()
{
	const FLRHudStyle& Style = FLRHudStyle::Get();
	struct FHelpRow
	{
		const TCHAR* Keys;
		const TCHAR* Does;
	};
	const FHelpRow Rows[] = {
		{ TEXT("Click a cell"), TEXT("Select it (click again to deselect). The Actions card fills with what you can do there.") },
		{ TEXT("1-9, 0"), TEXT("The Actions card's slots, top to bottom (Build opens a page of recipes; Esc goes back)") },
		{ TEXT("WASD / arrows"), TEXT("Pan across the build layer") },
		{ TEXT("Mouse wheel"), TEXT("Zoom") },
		{ TEXT("Right mouse (hold)"), TEXT("Look around: orbit and tilt") },
		{ TEXT("Q / E"), TEXT("Orbit") },
		{ TEXT("PgUp / PgDn, ] / ["), TEXT("Build layer up / down") },
		{ TEXT("R"), TEXT("Reset the camera angle and zoom") },
		{ TEXT("H / Home"), TEXT("Fly to the first thing you placed") },
		{ TEXT("F / Tab, OUTSIDE"), TEXT("The outside console: feed the host with the dial (drag or scroll, or WASD / arrows: A/D fine, W/S coarse), watch it in its chamber, Ignite a new one if it evaporates. While it's down, 1-8 press its buttons.") },
		{ TEXT("Esc / F10, MENU"), TEXT("The menu: save, load, settings (rebind any key), quit. The game pauses while it's open. (In the editor, Esc stops Play; use F10.)") },
		{ TEXT("~"), TEXT("Console: LRGive hydrogen 500 (into the selected cell), LRTimeScale 10, LRSave, LRReset") },
		{ TEXT("Keys"), TEXT("These are the defaults: rebind any of them in MENU > Settings.") },
	};

	TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
	for (const FHelpRow& Row : Rows)
	{
		List->AddSlot()
		.AutoHeight()
		.Padding(FMargin(0.f, 3.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(150.f)
				[
					SNew(STextBlock).Font(Style.HeadingFont).ColorAndOpacity(Style.Orange).Text(AsText(Row.Keys))
				]
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			[
				SNew(STextBlock).Font(Style.BodyFont).ColorAndOpacity(Style.Text).AutoWrapText(true).Text(AsText(Row.Does))
			]
		];
	}
	List->AddSlot()
	.AutoHeight()
	.Padding(FMargin(0.f, 10.f, 0.f, 0.f))
	[
		SNew(STextBlock)
		.Font(Style.SmallFont)
		.ColorAndOpacity(Style.TextDim)
		.AutoWrapText(true)
		.Text(LOCTEXT("HelpGoal", "Keep the host black hole alive from outside while the pocket universe grows inside it: seed ripples, gather matter, build caches and irradiators. The status bar shows the host's mass, how fast it's changing and how long it has left."))
	];

	return SNew(SBox)
		.WidthOverride(620.f)
		[
			MakePanel(LOCTEXT("HelpTitle", "HELP"), List,
				MakeSmallButton(LOCTEXT("HelpClose", "Close"), [this]() { bHelpOpen = false; }))
		];
}

// ---------------------------------------------------------------------------------------
// Outside: the facility's injectors
// ---------------------------------------------------------------------------------------

bool SLRGameHud::DoesOutsideNeedAttention() const
{
	const FLRSimulation* Sim = GetSimulation();
	if (!Sim || !Sim->GetData().Host.IsDefined())
	{
		return false;
	}
	const double Cap = Sim->GetSafetyCap();
	return Sim->IsFrozen() || Sim->IsRecharging() || Sim->GetNetRate() < 0.0
		|| (Cap > 0.0 && Sim->GetHostMass() >= Cap * 0.95);
}

bool SLRGameHud::IsOutsideSlotEnabled(EOutsideSlot Button) const
{
	const FLRSimulation* Sim = GetSimulation();
	if (!Sim || !Sim->GetData().Host.IsDefined())
	{
		return false;
	}
	switch (Button)
	{
	case EOutsideSlot::Vent:   return Sim->IsVenting() || Sim->CanVent();
	case EOutsideSlot::Ignite: return Sim->CanIgnite();
	default:                   return !Sim->IsVenting(); // only the dial itself works while venting
	}
}

void SLRGameHud::RunOutsideSlot(EOutsideSlot Button)
{
	ULRGameSubsystem* Sub = GetSubsystem();
	const FLRSimulation* Sim = Sub ? Sub->GetSimulation() : nullptr;
	if (!Sim || !IsOutsideSlotEnabled(Button))
	{
		return;
	}
	switch (Button)
	{
	case EOutsideSlot::Off:       Sub->SetInjectorTarget(0.0); break;
	case EOutsideSlot::Hold:      Sub->SetInjectorTarget(Sim->GetEvaporationRate() * 1.05); break; // a little over the HOLD mark
	case EOutsideSlot::Limit:     Sub->SetInjectorTarget(Sim->GetRatedLimit()); break;
	case EOutsideSlot::Max:       Sub->SetInjectorTarget(Sim->GetData().Host.InjectorMaxRate); break;
	case EOutsideSlot::AutoHold:
		Sub->SetInjectorAuto(Sub->GetInjectorAuto() == ELRInjectorAuto::Hold ? ELRInjectorAuto::Off : ELRInjectorAuto::Hold);
		break;
	case EOutsideSlot::AutoLimit:
		Sub->SetInjectorAuto(Sub->GetInjectorAuto() == ELRInjectorAuto::Limit ? ELRInjectorAuto::Off : ELRInjectorAuto::Limit);
		break;
	case EOutsideSlot::Vent:      Sub->SetVenting(!Sub->IsVenting()); break;
	case EOutsideSlot::Ignite:    Sub->Ignite(); break;
	default: break;
	}
}

FText SLRGameHud::GetOutsideSlotLabel(EOutsideSlot Button, const FText& Label) const
{
	const FText Key = GetKeyLabel(LRInput::SlotCommand(static_cast<int32>(Button)));
	return Key.IsEmpty() ? Label : FText::Format(LOCTEXT("OutsideSlotLabel", "[{0}] {1}"), Key, Label);
}

void SLRGameHud::NudgeDial(float Direction, bool bCoarse, float DeltaSeconds)
{
	ULRGameSubsystem* Sub = GetSubsystem();
	const FLRSimulation* Sim = Sub ? Sub->GetSimulation() : nullptr;
	if (!Sim || !Sim->GetData().Host.IsDefined() || bMenuOpen)
	{
		return;
	}
	// Along the dial's arc (so it's logarithmic, like dragging it), the same scale as the widget.
	const double MaxRate = FMath::Max(Sim->GetData().Host.InjectorMaxRate, SLRFeedDial::MinRate * 10.0);
	const double Step = Direction * (bCoarse ? DialCoarseSpeed : DialFineSpeed) * DeltaSeconds;
	const double Fraction = FMath::Clamp(SLRFeedDial::RateToFraction(Sim->GetInjectorTarget(), MaxRate) + Step, 0.0, 1.0);
	// Turning it down snaps to OFF at the bottom (turning it up from OFF has to start from there).
	const bool bOff = Step < 0.0 && Fraction < 0.01;
	Sub->SetInjectorTarget(bOff ? 0.0 : SLRFeedDial::FractionToRate(Fraction, MaxRate));
}

TSharedRef<SWidget> SLRGameHud::BuildOutsidePanel()
{
	const FLRHudStyle& Style = FLRHudStyle::Get();

	// A line of readout text bound to the simulation.
	auto Readout = [this, &Style](TFunction<FString(const FLRSimulation&)> Describe, const FSlateFontInfo& Font) -> TSharedRef<SWidget>
	{
		return SNew(STextBlock)
			.Font(Font)
			.ColorAndOpacity(Style.ConsoleText)
			.AutoWrapText(true)
			.Text_Lambda([this, Describe]()
			{
				const FLRSimulation* Sim = GetSimulation();
				return (Sim && Sim->GetData().Host.IsDefined()) ? AsText(Describe(*Sim)) : FText::GetEmpty();
			});
	};
	// The buttons are numbered (RunOutsideSlot): while the panel is down, their keys (1-8 by
	// default) press them. A preset sets the dial once; they're disabled while venting.
	auto Preset = [this](const FText& Label, const FText& Tip, EOutsideSlot Button) -> TSharedRef<SWidget>
	{
		const FLRHudStyle& S = FLRHudStyle::Get();
		return SNew(SButton)
			.ButtonStyle(&S.ConsoleButtonStyle)
			.IsFocusable(false)
			.ContentPadding(FMargin(4.f, 2.f))
			.HAlign(HAlign_Center)
			.IsEnabled_Lambda([this, Button]() { return IsOutsideSlotEnabled(Button); })
			.ToolTipText(Tip)
			.OnClicked_Lambda([this, Button]()
			{
				RunOutsideSlot(Button);
				return FReply::Handled();
			})
			[
				SNew(STextBlock)
				.Font(S.SmallFont)
				.ColorAndOpacity(S.ConsoleText)
				.Text_Lambda([this, Label, Button]() { return GetOutsideSlotLabel(Button, Label); })
			];
	};
	// A toggle for one of the dial's auto modes: lit while it's on; clicking it again stops it.
	auto AutoToggle = [this](const FText& Label, const FText& Tip, ELRInjectorAuto Mode, EOutsideSlot Button) -> TSharedRef<SWidget>
	{
		const FLRHudStyle& S = FLRHudStyle::Get();
		auto IsOn = [this, Mode]()
		{
			const ULRGameSubsystem* Sub = GetSubsystem();
			return Sub && Sub->GetInjectorAuto() == Mode;
		};
		return SNew(SButton)
			.ButtonStyle(&S.ConsoleButtonStyle)
			.IsFocusable(false)
			.ContentPadding(FMargin(4.f, 2.f))
			.HAlign(HAlign_Center)
			.IsEnabled_Lambda([this, Button]() { return IsOutsideSlotEnabled(Button); })
			.ToolTipText(Tip)
			.ButtonColorAndOpacity_Lambda([IsOn]() -> FSlateColor { return IsOn() ? FLinearColor(1.f, 0.72f, 0.35f) : FLinearColor::White; })
			.OnClicked_Lambda([this, Button]()
			{
				RunOutsideSlot(Button);
				return FReply::Handled();
			})
			[
				SNew(STextBlock)
				.Font(S.SmallFont)
				.Text_Lambda([this, Label, Button]() { return GetOutsideSlotLabel(Button, Label); })
				.ColorAndOpacity_Lambda([IsOn]() -> FSlateColor
				{
					const FLRHudStyle& Inner = FLRHudStyle::Get();
					return IsOn() ? Inner.ConsoleAccent : Inner.ConsoleText;
				})
			];
	};

	// The instruments (the dial, the chamber, the camera) sit in dark screens set into the light
	// console. The dial and the chamber are the same size.
	auto Screen = [&Style](const TSharedRef<SWidget>& Instrument, float Width = 250.f, float Height = 240.f,
		TAttribute<FSlateColor> Background = TAttribute<FSlateColor>()) -> TSharedRef<SWidget>
	{
		return SNew(SBorder)
			.BorderImage(&Style.WhiteBrush)
			.BorderBackgroundColor(Style.ConsoleScreenBorder)
			.Padding(FMargin(2.f))
			[
				SNew(SBorder)
				.BorderImage(&Style.WhiteBrush)
				.BorderBackgroundColor(Background.IsSet() ? Background : TAttribute<FSlateColor>(FSlateColor(Style.ConsoleScreen)))
				.Padding(FMargin(4.f))
				[
					SNew(SBox)
					.WidthOverride(Width)
					.HeightOverride(Height)
					[
						Instrument
					]
				]
			];
	};

	// Left: the dial, with presets.
	TSharedRef<SWidget> DialColumn = SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		[
			// Its screen turns violet while the injectors run in reverse.
			Screen(SNew(SLRFeedDial).Subsystem(Subsystem), 250.f, 240.f, TAttribute<FSlateColor>::CreateLambda([this]() -> FSlateColor
			{
				const FLRHudStyle& S = FLRHudStyle::Get();
				const FLRSimulation* Sim = GetSimulation();
				return (Sim && (Sim->IsVenting() || Sim->GetInjectorFlow() < 0.0)) ? S.VentScreen : S.ConsoleScreen;
			}))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		.Padding(FMargin(0.f, 4.f, 0.f, 2.f))
		[
			SNew(SBox)
			.WidthOverride(260.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).Padding(FMargin(2.f, 0.f))
				[
					Preset(LOCTEXT("DialOff", "Off"), LOCTEXT("DialOffTip", "Turn the injectors off"), EOutsideSlot::Off)
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).Padding(FMargin(2.f, 0.f))
				[
					Preset(LOCTEXT("DialHold", "Hold"), LOCTEXT("DialHoldTip", "Just outpace evaporation: the host holds its mass (a little over the HOLD mark). The mark moves as the mass changes; Auto Hold follows it."),
						EOutsideSlot::Hold)
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).Padding(FMargin(2.f, 0.f))
				[
					Preset(LOCTEXT("DialLimit", "Limit"), LOCTEXT("DialLimitTip", "Feed at the rated limit: the fastest the host can grow right now. The limit rises as it grows; Auto Limit follows it."),
						EOutsideSlot::Limit)
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).Padding(FMargin(2.f, 0.f))
				[
					Preset(LOCTEXT("DialMax", "Max"), LOCTEXT("DialMaxTip", "Open the injectors all the way. Anything over the rated limit is blown back out."),
						EOutsideSlot::Max)
				]
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		.Padding(FMargin(0.f, 0.f, 0.f, 4.f))
		[
			SNew(SBox)
			.WidthOverride(260.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).Padding(FMargin(2.f, 0.f))
				[
					AutoToggle(LOCTEXT("DialAutoHold", "Auto Hold"), LOCTEXT("DialAutoHoldTip", "Keep the dial on the HOLD mark as the host's mass changes, so it holds its mass. Moving the dial or a preset stops it."),
						ELRInjectorAuto::Hold, EOutsideSlot::AutoHold)
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).Padding(FMargin(2.f, 0.f))
				[
					AutoToggle(LOCTEXT("DialAutoLimit", "Auto Limit"), LOCTEXT("DialAutoLimitTip", "Keep the dial on the LIMIT mark as the host grows, the fastest growth there is. It doesn't stop at the chamber wall: watch the safeties. Moving the dial or a preset stops it."),
						ELRInjectorAuto::Limit, EOutsideSlot::AutoLimit)
				]
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBox)
			.WidthOverride(250.f)
			[
				SNew(STextBlock)
				.Font(Style.SmallFont)
				.ColorAndOpacity(Style.ConsoleTextDim)
				.AutoWrapText(true)
				.Text(LOCTEXT("DialHelp", "Drag or scroll the dial. Green: the host grows. Above LIMIT the flow is blown back out. The flow lags the dial (the lens magnets ramp), so dial back before the cap."))
			]
		];

	// Middle: the numbers.
	TSharedRef<SWidget> Numbers = SNew(SBox)
		.WidthOverride(230.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 2.f))
			[
				Readout([](const FLRSimulation& Sim)
				{
					return Sim.IsFrozen() ? FString(TEXT("Host: evaporated"))
						: FString::Printf(TEXT("Host: %s"), *FLRSimulation::FormatMass(Sim.GetHostMass()));
				}, Style.HeadingFont)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 2.f))
			[
				Readout([](const FLRSimulation& Sim)
				{
					const double Net = Sim.GetNetRate();
					return FString::Printf(TEXT("Net %s (%s)"), *FormatSignedRate(Net),
						Sim.IsFrozen() ? TEXT("frozen") : (Net >= 0.0 ? TEXT("growing") : TEXT("shrinking")));
				}, Style.BodyFont)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 2.f))
			[
				Readout([](const FLRSimulation& Sim)
				{
					const double Flow = Sim.GetInjectorFlow();
					if (Sim.IsVenting() || Flow < 0.0)
					{
						return FString::Printf(TEXT("Venting %s out, dial %s"), *FLRSimulation::FormatRate(FMath::Abs(Flow)),
							*FLRSimulation::FormatRate(Sim.GetInjectorTarget()));
					}
					return FString::Printf(TEXT("Flow %s, dial %s"), *FLRSimulation::FormatRate(Flow),
						*FLRSimulation::FormatRate(Sim.GetInjectorTarget()));
				}, Style.BodyFont)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 2.f))
			[
				Readout([](const FLRSimulation& Sim)
				{
					return FString::Printf(TEXT("LIMIT %s: rated at %.1e x Eddington"), *FLRSimulation::FormatRate(Sim.GetRatedLimit()),
						Sim.GetData().Host.GetEddingtonMultiple());
				}, Style.BodyFont)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 2.f))
			[
				Readout([](const FLRSimulation& Sim)
				{
					return FString::Printf(TEXT("HOLD %s: Hawking evaporation"), *FLRSimulation::FormatRate(Sim.GetEvaporationRate()));
				}, Style.BodyFont)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 2.f))
			[
				Readout([](const FLRSimulation& Sim)
				{
					if (Sim.IsFrozen())
					{
						return FString();
					}
					const FString Unfed = FormatDuration(Sim.GetData().Host.GetUnfedLifetime(Sim.GetHostMass()));
					return Sim.GetNetRate() < 0.0
						? FString::Printf(TEXT("At this rate it evaporates in %s (unfed: %s)"), *FormatDuration(Sim.GetTimeToEvaporation()), *Unfed)
						: FString::Printf(TEXT("Growing. Unfed, it would last %s"), *Unfed);
				}, Style.BodyFont)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 2.f))
			[
				Readout([](const FLRSimulation& Sim)
				{
					const double Cap = Sim.GetSafetyCap();
					return Cap > 0.0 ? FString::Printf(TEXT("Safety cap %s (%.0f%% of it)"), *FLRSimulation::FormatMass(Cap), 100.0 * Sim.GetHostMass() / Cap)
						: FString(TEXT("No safety cap"));
				}, Style.BodyFont)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 2.f))
			[
				SNew(STextBlock)
				.Font(Style.BodyFont)
				.AutoWrapText(true)
				.Text_Lambda([this]()
				{
					const FLRSimulation* Sim = GetSimulation();
					if (!Sim || !Sim->GetData().Host.IsDefined() || Sim->GetTippingMass() <= 0.0 || Sim->GetTippingMass() > 1e30)
					{
						return FText::GetEmpty();
					}
					const bool bBelow = !Sim->IsFrozen() && Sim->GetHostMass() < Sim->GetTippingMass();
					return AsText(FString::Printf(TEXT("Point of no return %s%s"), *FLRSimulation::FormatMass(Sim->GetTippingMass()),
						bBelow ? TEXT(": BELOW IT, it can't be saved") : TEXT("")));
				})
				.ColorAndOpacity_Lambda([this]() -> FSlateColor
				{
					const FLRHudStyle& S = FLRHudStyle::Get();
					const FLRSimulation* Sim = GetSimulation();
					const bool bBelow = Sim && !Sim->IsFrozen() && Sim->GetHostMass() < Sim->GetTippingMass();
					return bBelow ? S.Red : S.ConsoleText;
				})
			]
		];

	// Next to the dial: the host in its chamber, to scale.
	TSharedRef<SWidget> Chamber = SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		[
			Screen(SNew(SLRChamberView).Subsystem(Subsystem))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			Readout([](const FLRSimulation& Sim)
			{
				const FLRHostDef& Host = Sim.GetData().Host;
				return FString::Printf(TEXT("1 g radius %.1f mm, wall %.1f mm"), Host.GetGravityRadius(Sim.GetHostMass()) * 1000.0,
					static_cast<double>(Host.ChamberRadius) * 1000.0);
			}, Style.SmallFont)
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			Readout([](const FLRSimulation& Sim)
			{
				const double Mass = Sim.GetHostMass();
				return Mass <= 0.0 ? FString()
					: FString::Printf(TEXT("Horizon %.1e m. Glows at %.1e K, %.1e W"), LRPhysics::SchwarzschildRadius(Mass),
						LRPhysics::HawkingTemperature(Mass), LRPhysics::HawkingPower(Mass));
			}, Style.SmallFont)
		];

	// Far right: the stored charge and Ignite.
	TSharedRef<SWidget> Ring = SNew(SBox)
		.WidthOverride(210.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).Font(Style.HeadingFont).ColorAndOpacity(Style.ConsoleAccent).Text(LOCTEXT("ChargeTitle", "STORED CHARGE"))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 4.f))
			[
				SNew(SBox)
				.HeightOverride(10.f)
				[
					SNew(SProgressBar)
					.Style(&Style.ProgressStyle)
					.Percent_Lambda([this]() -> TOptional<float>
					{
						const FLRSimulation* Sim = GetSimulation();
						const double Full = Sim ? Sim->GetData().Host.ChargeCapacity : 0.0;
						return Full > 0.0 ? static_cast<float>(Sim->GetStoredCharge() / Full) : 0.f;
					})
					.FillColorAndOpacity_Lambda([this]() -> FSlateColor
					{
						const FLRHudStyle& S = FLRHudStyle::Get();
						const FLRSimulation* Sim = GetSimulation();
						return (Sim && Sim->IsRecharging()) ? S.Red : S.Green;
					})
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 2.f))
			[
				Readout([](const FLRSimulation& Sim)
				{
					const double Full = Sim.GetData().Host.ChargeCapacity;
					if (Sim.IsRecharging())
					{
						return FString::Printf(TEXT("Recharging (%.0f%%): feeding is locked out"), Full > 0.0 ? 100.0 * Sim.GetStoredCharge() / Full : 0.0);
					}
					return FString::Printf(TEXT("Charged: %s of neutronium"), *FLRSimulation::FormatMass(Sim.GetStoredCharge()));
				}, Style.BodyFont)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 6.f, 0.f, 0.f))
			[
				SNew(SButton)
				.ButtonStyle(&Style.ConsoleButtonStyle)
				.IsFocusable(false)
				.HAlign(HAlign_Center)
				.ContentPadding(FMargin(8.f, 5.f))
				.IsEnabled_Lambda([this]() { return IsOutsideSlotEnabled(EOutsideSlot::Vent); })
				.ButtonColorAndOpacity_Lambda([this]() -> FSlateColor
				{
					const FLRSimulation* Sim = GetSimulation();
					return (Sim && Sim->IsVenting()) ? FLinearColor(1.f, 0.6f, 0.9f) : FLinearColor::White;
				})
				.ToolTipText(LOCTEXT("VentTip", "Run the injectors in reverse to shed mass: the graviton lens drives the horizon into stimulated emission and the beamline draws the radiation off, as fast as the dial says (up to the rated limit). Needs a full stored charge. The radiation floods the chamber, so the instruments are down while it runs; only the dial works. It doesn't use the charge up, and nothing stops it at the point of no return: vent too far and the host is lost."))
				.OnClicked_Lambda([this]()
				{
					RunOutsideSlot(EOutsideSlot::Vent);
					return FReply::Handled();
				})
				[
					SNew(STextBlock)
					.Font(Style.HeadingFont)
					.ColorAndOpacity(Style.ConsoleAccent)
					.Text_Lambda([this]()
					{
						const FLRSimulation* Sim = GetSimulation();
						return GetOutsideSlotLabel(EOutsideSlot::Vent,
							(Sim && Sim->IsVenting()) ? LOCTEXT("VentStop", "STOP VENTING") : LOCTEXT("Vent", "VENT"));
					})
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 6.f))
			[
				SNew(SButton)
				.ButtonStyle(&Style.ConsoleButtonStyle)
				.IsFocusable(false)
				.HAlign(HAlign_Center)
				.ContentPadding(FMargin(8.f, 5.f))
				.IsEnabled_Lambda([this]() { return IsOutsideSlotEnabled(EOutsideSlot::Ignite); })
				.ToolTipText(LOCTEXT("IgniteTip", "Once the host has evaporated: fire the whole stored charge at the singularity. A charge that big collapses straight into a new horizon."))
				.OnClicked_Lambda([this]()
				{
					RunOutsideSlot(EOutsideSlot::Ignite);
					return FReply::Handled();
				})
				[
					SNew(STextBlock)
					.Font(Style.HeadingFont)
					.ColorAndOpacity(Style.ConsoleAccent)
					.Text_Lambda([this]() { return GetOutsideSlotLabel(EOutsideSlot::Ignite, LOCTEXT("Ignite", "IGNITE")); })
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 6.f, 0.f, 0.f))
			[
				SNew(STextBlock)
				.Font(Style.SmallFont)
				.ColorAndOpacity(Style.ConsoleTextDim)
				.AutoWrapText(true)
				.Text(LOCTEXT("InjectorLore", "Neutronium injectors: neutral matter the host's glow barely pushes, focused by a graviton lens. The safeties dump the beam when the 1 g sphere reaches the chamber wall, and the instruments inside go dark until the charge is back."))
			]
		];

	// Under the dial and the chamber: a 4:3 camera looking into the chamber, filling the space
	// left there (Tick sizes it). For now it's a dark screen (later, a 3D view of the
	// containment field, the injectors and the singularity), and it shows static while the
	// instruments are down.
	TSharedRef<SWidget> CameraScreen = SNew(SBox)
		.WidthOverride_Lambda([this]() -> FOptionalSize { return CameraSize.X + CameraChrome; })
		.HeightOverride_Lambda([this]() -> FOptionalSize { return CameraSize.Y + CameraChrome; })
		[
			SNew(SBorder)
			.BorderImage(&Style.WhiteBrush)
			.BorderBackgroundColor(Style.ConsoleScreenBorder)
			.Padding(FMargin(2.f))
			[
				SNew(SBorder)
				.BorderImage(&Style.WhiteBrush)
				.BorderBackgroundColor(Style.ConsoleScreen)
				.Padding(FMargin(4.f))
				[
					SNew(SOverlay)
					+ SOverlay::Slot()
					[
						SNew(SLRChamberCamera).Subsystem(Subsystem)
					]
					+ SOverlay::Slot()
					[
						// Specks the same size on screen however big the camera is.
						SNew(SLRStaticNoise)
						.Resolution(FIntPoint(256, 192))
						.PixelSize(2.5f)
						.Material(StaticMaterial)
						.Intensity_Lambda([this]() { return StaticLevel; })
					]
					+ SOverlay::Slot()
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Top)
					.Padding(FMargin(6.f, 4.f))
					[
						SNew(STextBlock)
						.Font(Style.SmallFont)
						.ColorAndOpacity(Style.TextDim)
						.Text(LOCTEXT("CameraLabel", "CAM 1  CONTAINMENT CHAMBER"))
					]
					+ SOverlay::Slot()
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Font(Style.HeadingFont)
						.ColorAndOpacity(Style.Red)
						.Visibility_Lambda([this]() { return StaticLevel > 0.5f ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
						.Text(LOCTEXT("CameraNoSignal", "NO SIGNAL"))
					]
				]
			]
		];
	// The space the camera may use: Tick measures this and fits the camera into it.
	TSharedRef<SWidget> Camera = SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SAssignNew(CameraArea, SSpacer)
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Top)
		[
			CameraScreen
		];

	TSharedRef<SWidget> Instruments = SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 12.f, 0.f))[DialColumn]
			// The camera below is wider than the two screens: put them at its edges.
			+ SHorizontalBox::Slot().FillWidth(1.f)
			[
				SNew(SSpacer)
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SBox).WidthOverride(262.f)[Chamber] // the screen's width
			]
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		.Padding(FMargin(0.f, 8.f, 0.f, 0.f))
		[
			Camera
		];

	TSharedRef<SWidget> Body = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 12.f, 0.f))[Instruments]
		+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 12.f, 0.f))[Numbers]
		+ SHorizontalBox::Slot().AutoWidth()[Ring];

	return MakePanel(LOCTEXT("OutsideTitle", "OUTSIDE: THE FACILITY'S INJECTORS"), Body,
		MakeSmallButton(LOCTEXT("OutsideClose", "Back inside [F]"), [this]()
		{
			if (ULRGameSubsystem* Sub = GetSubsystem())
			{
				Sub->SetOutsideViewOpen(false);
			}
		}, /*bConsole*/ true),
		/*bFillHeight*/ true, /*bConsole*/ true);
}

#undef LOCTEXT_NAMESPACE
