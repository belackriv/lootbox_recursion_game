#include "UI/SLRGameHud.h"

#include "Game/LRGameSubsystem.h"
#include "Simulation/LRSimulation.h"
#include "UI/LRHudStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
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
}

// ---------------------------------------------------------------------------------------
// Construction / layout
// ---------------------------------------------------------------------------------------

void SLRGameHud::Construct(const FArguments& InArgs)
{
	Subsystem = InArgs._Subsystem;
	SetVisibility(EVisibility::SelfHitTestInvisible);

	// SelfHitTestInvisible: the container itself lets clicks through to the 3D world,
	// while its children (the panels) still receive them.
	ChildSlot
	[
		SNew(SOverlay)
		.Visibility(EVisibility::SelfHitTestInvisible)
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

				// Left column: actions + inventory
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
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(FMargin(0.f, 8.f, 0.f, 0.f))
					[
						BuildUniversePanel()
					]
				]

				// Middle: nothing - the 3D world shows through
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				[
					SNew(SSpacer)
					.Visibility(EVisibility::SelfHitTestInvisible)
				]

				// Right column: grid and selection, info
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBox)
					.WidthOverride(280.f)
					.Visibility(EVisibility::SelfHitTestInvisible)
					[
						SNew(SVerticalBox)
						.Visibility(EVisibility::SelfHitTestInvisible)
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							BuildWorldPanel()
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
	];
}

TSharedRef<SWidget> SLRGameHud::MakePanel(const FText& Title, const TSharedRef<SWidget>& Content,
	const TSharedRef<SWidget>& HeaderExtra, bool bFillHeight)
{
	const FLRHudStyle& Style = FLRHudStyle::Get();

	TSharedRef<SVerticalBox> Body = SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			// Title bar (Rails: .fac-title-bar)
			SNew(SBorder)
			.BorderImage(&Style.WhiteBrush)
			.BorderBackgroundColor(Style.PanelInner)
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
					.ColorAndOpacity(Style.Orange)
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
		.BorderBackgroundColor(Style.Border)
		.Padding(FMargin(2.f))
		[
			SNew(SBorder)
			.BorderImage(&Style.WhiteBrush)
			.BorderBackgroundColor(Style.Panel)
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
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			.Padding(FMargin(16.f, 0.f))
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Help", "Click a cell to select it  |  WASD pan, wheel zoom, hold right mouse to look, Q/E orbit, PgUp/PgDn layer, R reset view, H home  |  ~ console: LRGive hydrogen 500 (into the selected cell), LRTimeScale 10"))
				.Font(Style.SmallFont)
				.ColorAndOpacity(Style.TextDim)
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
			// The host black hole's mass: red once it's low, and while the universe is frozen.
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(0.f, 0.f, 16.f, 0.f))
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
					return Sim->IsFrozen()
						? LOCTEXT("HostFrozen", "Host evaporated: universe frozen, feed the horizon!")
						: AsText(FString::Printf(TEXT("Host mass %d%%"), FMath::FloorToInt32(Sim->GetHostMass() * 100.0)));
				})
				.ColorAndOpacity_Lambda([this]() -> FSlateColor
				{
					const FLRHudStyle& S = FLRHudStyle::Get();
					const FLRSimulation* Sim = GetSimulation();
					const bool bLow = Sim && Sim->GetHostMass() <= Sim->GetData().Host.WarningMass;
					return bLow ? S.Red : S.TextDim;
				})
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

	Box->AddSlot()
	.AutoHeight()
	.Padding(FMargin(0.f, 0.f, 0.f, 6.f))
	[
		MakeActionButton(LRNames::Perturb,
			[this]()
			{
				if (ULRGameSubsystem* Sub = GetSubsystem())
				{
					Sub->RequestActionWithSelection(LRNames::Perturb);
				}
			},
			// Needs a selected cell that is empty or holds a ripple that can still deepen.
			[this]() { return CanPerturbSelectedCell(); })
	];

	Box->AddSlot()
	.AutoHeight()
	.Padding(FMargin(0.f, 0.f, 0.f, 6.f))
	[
		MakeActionButton(LRNames::Feed, [this]()
		{
			if (ULRGameSubsystem* Sub = GetSubsystem())
			{
				Sub->RequestSimpleAction(LRNames::Feed);
			}
		})
	];

	// Build: one button per recipe, built in the selected cell (Rails: CraftToggleButton +
	// CraftActionButton choices).
	Box->AddSlot()
	.AutoHeight()
	.Padding(FMargin(0.f, 4.f, 0.f, 2.f))
	[
		SNew(SHorizontalBox)
		.Visibility_Lambda([this]() { return GetStatus(LRNames::Craft).bRevealed ? EVisibility::Visible : EVisibility::Collapsed; })
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(FMargin(0.f, 0.f, 8.f, 0.f))
		[
			SNew(STextBlock)
			.Font(Style.HeadingFont)
			.ColorAndOpacity(Style.Text)
			.Text_Lambda([this]() { return AsText(GetStatus(LRNames::Craft).Label); })
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		.VAlign(VAlign_Center)
		[
			MakeActionProgress(LRNames::Craft)
		]
	];

	if (const FLRSimulation* Sim = GetSimulation())
	{
		for (const FLRRecipeDef& Recipe : Sim->GetData().Recipes)
		{
			Box->AddSlot()
			.AutoHeight()
			.Padding(FMargin(0.f, 2.f))
			[
				MakeRecipeButton(Recipe.Id)
			];
		}
	}

	Box->AddSlot()
	.AutoHeight()
	.Padding(FMargin(0.f, 4.f, 0.f, 0.f))
	[
		SNew(STextBlock)
		.Font(Style.SmallFont)
		.ColorAndOpacity(Style.TextDark)
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
			FIntVector Cell;
			const FString Hint = GetSelected(Cell) ? FString() : TEXT("Select a cell to build in.  ");
			return AsText(Hint + (Locked > 0 ? FString::Printf(TEXT("%d recipe(s) still to discover..."), Locked) : FString()));
		})
		.Visibility_Lambda([this]() { return GetStatus(LRNames::Craft).bRevealed ? EVisibility::Visible : EVisibility::Collapsed; })
	];

	return MakePanel(LOCTEXT("Actions", "ACTIONS"),
		SNew(SBox).WidthOverride(440.f)[Box],
		SNullWidget::NullWidget);
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
		SNew(SBox).WidthOverride(440.f)[Box],
		SNullWidget::NullWidget);
}

TSharedRef<SWidget> SLRGameHud::BuildWorldPanel()
{
	const FLRHudStyle& Style = FLRHudStyle::Get();
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);

	// Build layer controls
	Box->AddSlot()
	.AutoHeight()
	.Padding(FMargin(0.f, 0.f, 0.f, 4.f))
	[
		SNew(SHorizontalBox)
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
	];

	// Cursor / selection readout
	Box->AddSlot()
	.AutoHeight()
	.Padding(FMargin(0.f, 2.f))
	[
		SNew(STextBlock)
		.Font(Style.SmallFont)
		.ColorAndOpacity(Style.TextDim)
		.Text_Lambda([this]()
		{
			const ULRGameSubsystem* Sub = GetSubsystem();
			FIntVector Cell;
			if (!Sub || !Sub->GetHoveredCell(Cell))
			{
				return LOCTEXT("NoCursor", "Cursor: -");
			}
			return AsText(FString::Printf(TEXT("Cursor: %s"), *FLRSimulation::DescribeCell(Cell)));
		})
	];
	Box->AddSlot()
	.AutoHeight()
	.Padding(FMargin(0.f, 2.f, 0.f, 4.f))
	[
		SNew(STextBlock)
		.Font(Style.BodyFont)
		.ColorAndOpacity(Style.Orange)
		.AutoWrapText(true)
		.Text_Lambda([this]()
		{
			const ULRGameSubsystem* Sub = GetSubsystem();
			const FLRSimulation* Sim = GetSimulation();
			FIntVector Cell;
			if (!Sub || !Sim || !Sub->GetSelectedCell(Cell))
			{
				return LOCTEXT("NoSelection", "Selected: none (click a cell)");
			}
			const FLRPlacedEntity* Entity = Sim->FindPlaced(Cell);
			return AsText(FString::Printf(TEXT("Selected: %s  %s"), *FLRSimulation::DescribeCell(Cell),
				Entity ? *Sim->GetData().GetDisplayName(Entity->Item) : TEXT("(empty)")));
		})
	];

	// Open and Dismantle act on whatever is in the selected cell.
	Box->AddSlot()
	.AutoHeight()
	.Padding(FMargin(0.f, 2.f, 0.f, 6.f))
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		.Padding(FMargin(0.f, 0.f, 3.f, 0.f))
		[
			MakeActionButton(LRNames::Use,
				[this]()
				{
					if (ULRGameSubsystem* Sub = GetSubsystem()) { Sub->RequestActionWithSelection(LRNames::Use); }
				},
				[this]() { return CanOpenSelectedCell(); })
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		.Padding(FMargin(3.f, 0.f, 0.f, 0.f))
		[
			MakeActionButton(LRNames::Dismantle,
				[this]()
				{
					if (ULRGameSubsystem* Sub = GetSubsystem()) { Sub->RequestActionWithSelection(LRNames::Dismantle); }
				},
				[this]() { return CanDismantleSelectedCell(); })
		]
	];

	// Irradiation: shown only when the selected cell holds an enclosure.
	Box->AddSlot()
	.AutoHeight()
	.Padding(FMargin(0.f, 0.f, 0.f, 6.f))
	[
		SNew(SBorder)
		.BorderImage(&Style.WhiteBrush)
		.BorderBackgroundColor(Style.PanelInner)
		.Padding(FMargin(6.f))
		.Visibility_Lambda([this]() { return GetSelectedEnclosure() ? EVisibility::Visible : EVisibility::Collapsed; })
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
					const FLRPlacedEntity* Enclosure = GetSelectedEnclosure();
					return Enclosure ? AsText(DescribeEnclosure(*Enclosure)) : FText::GetEmpty();
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
	];

	// Everything in the pocket universe, on every layer. Rebuilt when the world changes (RebuildDeployedList).
	Box->AddSlot()
	.AutoHeight()
	[
		SNew(SBox)
		.MaxDesiredHeight(220.f)
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			[
				SAssignNew(DeployedBox, SVerticalBox)
			]
		]
	];
	RebuildDeployedList();

	TSharedRef<SWidget> HomeButton = MakeSmallButton(LOCTEXT("Home", "Home"), [this]()
	{
		if (ULRGameSubsystem* Sub = GetSubsystem()) { Sub->FocusHome(); }
	});

	return MakePanel(LOCTEXT("Grid", "GRID"), Box, HomeButton);
}

void SLRGameHud::RebuildDeployedList()
{
	if (!DeployedBox.IsValid())
	{
		return;
	}
	const FLRHudStyle& Style = FLRHudStyle::Get();
	DeployedBox->ClearChildren();

	const ULRGameSubsystem* Sub = GetSubsystem();
	const TArray<FLRPlacedEntity> Placed = Sub ? Sub->GetPlacedSorted() : TArray<FLRPlacedEntity>();
	if (Placed.IsEmpty())
	{
		DeployedBox->AddSlot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Font(Style.SmallFont)
			.ColorAndOpacity(Style.TextDark)
			.AutoWrapText(true)
			.Text(LOCTEXT("NothingDeployed", "The pocket universe is empty. Click a cell, then Perturb to seed a ripple there."))
		];
		return;
	}
	for (const FLRPlacedEntity& Entity : Placed)
	{
		DeployedBox->AddSlot()
		.AutoHeight()
		.Padding(FMargin(0.f, 1.f))
		[
			MakeDeployedRow(Entity)
		];
	}
}

TSharedRef<SWidget> SLRGameHud::BuildInfoPanel()
{
	const FLRHudStyle& Style = FLRHudStyle::Get();

	TSharedRef<SWidget> Content = SNew(SBox)
		.MinDesiredHeight(120.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(0.f, 0.f, 0.f, 4.f))
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

TSharedRef<SWidget> SLRGameHud::MakeActionButton(FName ActionName, TFunction<void()> OnClick, TFunction<bool()> ExtraEnabled)
{
	const FLRHudStyle& Style = FLRHudStyle::Get();

	// Tech tree: actions stay hidden until revealed (then they stay).
	return SNew(SVerticalBox)
		.Visibility_Lambda([this, ActionName]() { return GetStatus(ActionName).bRevealed ? EVisibility::Visible : EVisibility::Collapsed; })
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SButton)
			.ButtonStyle(&Style.ButtonStyle)
			.IsFocusable(false)
			.HAlign(HAlign_Center)
			.ContentPadding(FMargin(10.f, 5.f))
			.IsEnabled_Lambda([this, ActionName, ExtraEnabled]()
			{
				return GetStatus(ActionName).bEnabled && (!ExtraEnabled || ExtraEnabled());
			})
			.OnClicked_Lambda([OnClick]()
			{
				OnClick();
				return FReply::Handled();
			})
			.OnHovered_Lambda([this, ActionName]() { SetHover(EHoverKind::Action, ActionName); })
			.OnUnhovered_Lambda([this]() { ClearHover(); })
			[
				SNew(STextBlock)
				.Font(Style.HeadingFont)
				.ColorAndOpacity(Style.Text)
				.Text_Lambda([this, ActionName]()
				{
					const FLRActionStatus Status = GetStatus(ActionName);
					if (Status.bCasting)
					{
						return AsText(FString::Printf(TEXT("%s..."), *Status.Label));
					}
					return AsText(Status.Label);
				})
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(0.f, 2.f, 0.f, 0.f))
		[
			MakeActionProgress(ActionName)
		];
}

TSharedRef<SWidget> SLRGameHud::MakeActionProgress(FName ActionName)
{
	const FLRHudStyle& Style = FLRHudStyle::Get();

	// Cast progress fills orange; cooldown drains dim. Rails: ActionButton.vue's progress bar.
	return SNew(SBox)
		.HeightOverride(4.f)
		[
			SNew(SProgressBar)
			.Style(&Style.ProgressStyle)
			.Percent_Lambda([this, ActionName]() -> TOptional<float>
			{
				const FLRActionStatus Status = GetStatus(ActionName);
				if (Status.bCasting)
				{
					return Status.CastProgress;
				}
				if (Status.bOnCooldown && Status.Cooldown > 0.f)
				{
					return Status.CooldownRemaining / Status.Cooldown;
				}
				return 0.f;
			})
			.FillColorAndOpacity_Lambda([this, ActionName]() -> FSlateColor
			{
				const FLRHudStyle& S = FLRHudStyle::Get();
				return GetStatus(ActionName).bCasting ? S.Orange : S.TextDark;
			})
		];
}

TSharedRef<SWidget> SLRGameHud::MakeRecipeButton(FName RecipeId)
{
	const FLRHudStyle& Style = FLRHudStyle::Get();
	const FLRSimulation* Simulation = GetSimulation();
	const FLRRecipeDef* RecipeDef = Simulation ? Simulation->GetData().FindRecipe(RecipeId) : nullptr;
	const FText Label = AsText(RecipeDef ? RecipeDef->Label : RecipeId.ToString());

	return SNew(SButton)
		.ButtonStyle(&Style.ButtonStyle)
		.IsFocusable(false)
		.ContentPadding(FMargin(10.f, 4.f))
		// Tech tree: only unlocked recipes are shown.
		.Visibility_Lambda([this, RecipeId]()
		{
			const FLRSimulation* S = GetSimulation();
			return (S && S->IsRecipeUnlocked(RecipeId) && GetStatus(LRNames::Craft).bRevealed) ? EVisibility::Visible : EVisibility::Collapsed;
		})
		// Buildable in the selected cell: it can take the output and there's enough matter in reach.
		.IsEnabled_Lambda([this, RecipeId]()
		{
			const FLRSimulation* S = GetSimulation();
			FIntVector Cell;
			return S && GetStatus(LRNames::Craft).bEnabled && GetSelected(Cell) && S->ValidateBuild(RecipeId, Cell).IsNone();
		})
		.OnClicked_Lambda([this, RecipeId]()
		{
			if (ULRGameSubsystem* Sub = GetSubsystem())
			{
				Sub->Craft(RecipeId);
			}
			return FReply::Handled();
		})
		.OnHovered_Lambda([this, RecipeId]() { SetHover(EHoverKind::Recipe, RecipeId); })
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
				.Text(Label)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Font(Style.SmallFont)
				.Text(AsText(DescribeCost(RecipeId, /*bMultiline*/ false)))
				.ColorAndOpacity_Lambda([this, RecipeId]() -> FSlateColor
				{
					const FLRHudStyle& S = FLRHudStyle::Get();
					const FLRSimulation* Sim = GetSimulation();
					const FLRRecipeDef* R = Sim ? Sim->GetData().FindRecipe(RecipeId) : nullptr;
					FIntVector Cell;
					if (!R || !GetSelected(Cell))
					{
						return S.TextDim;
					}
					return Sim->CanAffordAt(Cell, R->Cost) ? S.TextDim : S.Red;
				})
			]
		];
}

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

TSharedRef<SWidget> SLRGameHud::MakeDeployedRow(const FLRPlacedEntity& Entity)
{
	const FLRHudStyle& Style = FLRHudStyle::Get();
	const FIntVector Cell = Entity.Cell;
	const FLRSimulation* Sim = GetSimulation();
	const FString Name = Sim ? Sim->GetData().GetDisplayName(Entity.Item) : Entity.Item.ToString();

	// Rails: WorldCellSlot.vue. Clicking a row selects the cell and flies the camera there.
	return SNew(SBorder)
		.BorderImage(&Style.WhiteBrush)
		.Padding(FMargin(1.f))
		.BorderBackgroundColor_Lambda([this, Cell]() -> FSlateColor
		{
			const FLRHudStyle& S = FLRHudStyle::Get();
			return IsCellSelected(Cell) ? S.Orange : S.SlotBorder;
		})
		[
			SNew(SButton)
			.ButtonStyle(&Style.SlotButtonStyle)
			.IsFocusable(false)
			.ContentPadding(FMargin(6.f, 2.f))
			.OnClicked_Lambda([this, Cell]()
			{
				if (ULRGameSubsystem* Sub = GetSubsystem())
				{
					Sub->SelectCell(Cell, /*bToggle*/ false);
					Sub->FocusOnCell(Cell);
				}
				return FReply::Handled();
			})
			.OnHovered_Lambda([this, Cell]() { SetHover(EHoverKind::WorldCell, NAME_None, 0, Cell); })
			.OnUnhovered_Lambda([this]() { ClearHover(); })
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				[
					SNew(STextBlock)
					.Font(Style.SmallFont)
					.ColorAndOpacity(Style.Text)
					.Text(AsText(Name))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(STextBlock)
					.Font(Style.SmallFont)
					.ColorAndOpacity(Style.TextDim)
					.Text(AsText(FLRSimulation::DescribeCell(Cell)))
				]
			]
		];
}

TSharedRef<SWidget> SLRGameHud::MakeSmallButton(const FText& Label, TFunction<void()> OnClick)
{
	const FLRHudStyle& Style = FLRHudStyle::Get();
	return SNew(SButton)
		.ButtonStyle(&Style.ButtonStyle)
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
			.ColorAndOpacity(Style.Text)
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

const FLRPlacedEntity* SLRGameHud::GetSelectedEnclosure() const
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
	return (Def && Def->IsEnclosure()) ? Entity : nullptr;
}

TOptional<float> SLRGameHud::GetSelectedExposureFraction() const
{
	const FLRSimulation* Sim = GetSimulation();
	const FLRPlacedEntity* Enclosure = GetSelectedEnclosure();
	const FLRItemDef* Def = (Sim && Enclosure) ? Sim->GetData().FindItem(Enclosure->Item) : nullptr;
	if (!Def || Def->ExposureSeconds <= 0.f)
	{
		return 0.f;
	}
	return FMath::Clamp(static_cast<float>(Enclosure->ExposureProgress / Def->ExposureSeconds), 0.f, 1.f);
}

FString SLRGameHud::DescribeEnclosure(const FLRPlacedEntity& Enclosure) const
{
	const FLRSimulation* Sim = GetSimulation();
	if (!Sim)
	{
		return FString();
	}
	const FLRGameData& Data = Sim->GetData();
	const FLRItemDef* Def = Data.FindItem(Enclosure.Item);
	FString Text = FString::Printf(TEXT("%s: tier <= %d, up to %d stacks, %.0fs per exposure"),
		*Data.GetDisplayName(Enclosure.Item), Def ? Def->MaxRadiationTier : 0, Def ? Def->MaxExposureStacks : 0, Def ? Def->ExposureSeconds : 0.f);

	const FLRLootBoxInstance* Box = Enclosure.Chamber.IsEmpty() ? nullptr : Sim->FindLootBox(Enclosure.Chamber.InstanceId);
	if (Enclosure.Chamber.IsEmpty())
	{
		Text += TEXT("\nChamber: empty");
	}
	else if (Box && Box->bRevealed)
	{
		Text += FString::Printf(TEXT("\nChamber: %s, observed (contents fixed)"), *Data.GetDisplayName(Enclosure.Chamber.Item));
	}
	else
	{
		Text += FString::Printf(TEXT("\nChamber: %s, %d/%d stacks"), *Data.GetDisplayName(Enclosure.Chamber.Item),
			Box ? Box->Modifiers.Num() : 0, Def ? Def->MaxExposureStacks : 0);
	}

	if (Enclosure.Source.IsEmpty())
	{
		Text += TEXT("\nSource: empty");
	}
	else
	{
		const FLRItemDef* SourceDef = Data.FindItem(Enclosure.Source.Item);
		const FLRRadiationDef* Radiation = SourceDef ? Data.FindRadiation(SourceDef->Radiation) : nullptr;
		Text += FString::Printf(TEXT("\nSource: %s (%s, tier %d)"), *Data.GetDisplayName(Enclosure.Source.Item),
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
	else if (Enclosure.Chamber.IsEmpty() || Enclosure.Source.IsEmpty())
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

bool SLRGameHud::CanOpenSelectedCell() const
{
	const FLRSimulation* Sim = GetSimulation();
	FIntVector Cell;
	return Sim && GetSelected(Cell) && Sim->FindCacheAt(Cell) != nullptr;
}

bool SLRGameHud::CanDismantleSelectedCell() const
{
	const FLRSimulation* Sim = GetSimulation();
	FIntVector Cell;
	const FLRPlacedEntity* Entity = (Sim && GetSelected(Cell)) ? Sim->FindPlaced(Cell) : nullptr;
	const FLRItemDef* Def = Entity ? Sim->GetData().FindItem(Entity->Item) : nullptr;
	return Entity && !(Def && Def->IsStructure());
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

bool SLRGameHud::CanPerturbSelectedCell() const
{
	const ULRGameSubsystem* Sub = GetSubsystem();
	const FLRSimulation* Sim = GetSimulation();
	const FLRActionDef* Perturb = Sim ? Sim->GetData().FindAction(LRNames::Perturb) : nullptr;
	FIntVector Selected;
	if (!Sub || !Perturb || !Sub->GetSelectedCell(Selected))
	{
		return false;
	}
	const FLRPlacedEntity* Entity = Sim->FindPlaced(Selected);
	if (!Entity)
	{
		return true;
	}
	const FLRItemDef* Def = Sim->GetData().FindItem(Entity->Item);
	return Entity->Item == Perturb->Places && Def && Entity->Amplitude < Def->MaxAmplitude;
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
		Text += TEXT("\nFrozen: the host black hole has evaporated.");
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
	const ULRGameSubsystem* Sub = GetSubsystem();
	return HoverKind == EHoverKind::None && Sub && Sub->GetHoveredCell(OutCell);
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
		Body += FString::Printf(TEXT("\n\nCast %.0fs, cooldown %.0fs."), Status.CastTime, Status.Cooldown);
		if (Status.bCasting)                { Body += TEXT("\nIn progress..."); }
		else if (Status.bOnCooldown)        { Body += FString::Printf(TEXT("\nReady in %.1fs."), Status.CooldownRemaining); }
		else if (!Status.bRequirementsMet)  { Body += TEXT("\nRequirements not met."); }
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
		if (Def && Def->IsEnclosure())
		{
			Body = FString::Printf(TEXT("%s\n\n%s"), *Tooltip, *DescribeEnclosure(*Entity));
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

#undef LOCTEXT_NAMESPACE
