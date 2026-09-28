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
						BuildInventoryPanel()
					]
				]

				// Middle: nothing - the 3D world shows through
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				[
					SNew(SSpacer)
					.Visibility(EVisibility::SelfHitTestInvisible)
				]

				// Right column: world list, info, log
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
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(FMargin(0.f, 8.f, 0.f, 0.f))
						[
							BuildLogPanel()
						]
					]
				]
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
				.Text(LOCTEXT("Help", "Click a cell to select it  |  WASD pan, wheel zoom, hold right mouse to look, Q/E orbit, PgUp/PgDn layer, R reset view, H home  |  ~ console: LRGive hydrogen 500, LRTimeScale 10"))
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

	Box->AddSlot()
	.AutoHeight()
	.Padding(FMargin(0.f, 0.f, 0.f, 6.f))
	[
		MakeActionButton(LRNames::Use,
			[this]()
			{
				if (ULRGameSubsystem* Sub = GetSubsystem())
				{
					Sub->RequestActionWithSelection(LRNames::Use);
				}
			},
			// Only when the selected inventory slot holds something usable (a loot box).
			[this]() { return IsSelectedSlotUsable(); })
	];

	// Craft: one button per recipe (Rails: CraftToggleButton + CraftActionButton choices).
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
			.Text(LOCTEXT("Craft", "Craft"))
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
			return Locked > 0 ? AsText(FString::Printf(TEXT("%d recipe(s) still to discover..."), Locked)) : FText::GetEmpty();
		})
		.Visibility_Lambda([this]() { return GetStatus(LRNames::Craft).bRevealed ? EVisibility::Visible : EVisibility::Collapsed; })
	];

	return MakePanel(LOCTEXT("Actions", "ACTIONS"),
		SNew(SBox).WidthOverride(440.f)[Box],
		SNullWidget::NullWidget);
}

TSharedRef<SWidget> SLRGameHud::BuildInventoryPanel()
{
	TSharedRef<SUniformGridPanel> Grid = SNew(SUniformGridPanel).SlotPadding(FMargin(2.f));
	for (int32 Index = 0; Index < FLRSimulation::PlayerInventorySlots; ++Index)
	{
		Grid->AddSlot(Index % InventoryColumns, Index / InventoryColumns)
		[
			MakeInventorySlot(Index)
		];
	}

	// Annihilate and Sort live in the inventory title bar (Rails: SortButton.vue).
	TSharedRef<SWidget> HeaderButtons = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(FMargin(0.f, 0.f, 4.f, 0.f))
		[
			SNew(SBox)
			.WidthOverride(100.f)
			[
				MakeActionButton(LRNames::Annihilate,
					[this]()
					{
						if (ULRGameSubsystem* Sub = GetSubsystem())
						{
							Sub->RequestActionWithSelection(LRNames::Annihilate);
						}
					},
					// Only with something selected in the inventory.
					[this]()
					{
						const ULRGameSubsystem* Sub = GetSubsystem();
						const FLRInventorySlot* Slot = Sub ? GetSlot(Sub->GetSelectedSlot()) : nullptr;
						return Slot && !Slot->IsEmpty();
					})
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(70.f)
			[
				MakeActionButton(LRNames::SortInventory, [this]()
				{
					if (ULRGameSubsystem* Sub = GetSubsystem())
					{
						Sub->RequestSimpleAction(LRNames::SortInventory);
					}
				})
			]
		];

	return MakePanel(LOCTEXT("Inventory", "INVENTORY"), Grid, HeaderButtons);
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

	// Deploy / Recall act on the selected cell (and selected inventory slot for Deploy).
	Box->AddSlot()
	.AutoHeight()
	.Padding(FMargin(0.f, 2.f, 0.f, 6.f))
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		.Padding(FMargin(0.f, 0.f, 3.f, 0.f))
		[
			MakeActionButton(LRNames::Deploy,
				[this]()
				{
					if (ULRGameSubsystem* Sub = GetSubsystem()) { Sub->RequestActionWithSelection(LRNames::Deploy); }
				},
				[this]() { return HasSelectedCell(/*bWantOccupied*/ false); })
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		.Padding(FMargin(3.f, 0.f, 0.f, 0.f))
		[
			MakeActionButton(LRNames::Recall,
				[this]()
				{
					if (ULRGameSubsystem* Sub = GetSubsystem()) { Sub->RequestActionWithSelection(LRNames::Recall); }
				},
				[this]() { return IsSelectedCellRecallable(); })
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
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.Padding(FMargin(0.f, 0.f, 3.f, 0.f))
				[
					MakeActionButton(LRNames::Load,
						[this]()
						{
							if (ULRGameSubsystem* Sub = GetSubsystem()) { Sub->RequestActionWithSelection(LRNames::Load); }
						},
						// A loot box or a radiation source must be selected in the inventory.
						[this]() { return IsSelectedSlotLoadable(); })
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.Padding(FMargin(3.f, 0.f, 0.f, 0.f))
				[
					MakeActionButton(LRNames::Unload,
						[this]()
						{
							if (ULRGameSubsystem* Sub = GetSubsystem()) { Sub->RequestActionWithSelection(LRNames::Unload); }
						},
						[this]()
						{
							const FLRPlacedEntity* Enclosure = GetSelectedEnclosure();
							return Enclosure && (!Enclosure->Chamber.IsEmpty() || !Enclosure->Source.IsEmpty());
						})
				]
			]
		]
	];

	// Everything deployed, on every layer. Rebuilt when the world changes (RebuildDeployedList).
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
	SAssignNew(LogBox, SVerticalBox);
	RebuildLog();
	return MakePanel(LOCTEXT("Log", "LOG"),
		SNew(SBox).MinDesiredHeight(110.f)[LogBox.ToSharedRef()],
		SNullWidget::NullWidget);
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
		.IsEnabled_Lambda([this, RecipeId]()
		{
			const FLRSimulation* S = GetSimulation();
			const FLRRecipeDef* R = S ? S->GetData().FindRecipe(RecipeId) : nullptr;
			return R && GetStatus(LRNames::Craft).bEnabled && S->CanAfford(R->Cost);
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
					return (R && Sim->CanAfford(R->Cost)) ? S.TextDim : S.Red;
				})
			]
		];
}

TSharedRef<SWidget> SLRGameHud::MakeInventorySlot(int32 SlotIndex)
{
	const FLRHudStyle& Style = FLRHudStyle::Get();

	// Rails: InventoryItemSlot.vue
	return SNew(SBox)
		.WidthOverride(40.f)
		.HeightOverride(40.f)
		[
			SNew(SBorder)
			.BorderImage(&Style.WhiteBrush)
			.Padding(FMargin(2.f))
			.BorderBackgroundColor_Lambda([this, SlotIndex]() -> FSlateColor
			{
				const FLRHudStyle& S = FLRHudStyle::Get();
				const ULRGameSubsystem* Sub = GetSubsystem();
				return (Sub && Sub->GetSelectedSlot() == SlotIndex) ? S.Orange : S.SlotBorder;
			})
			[
				SNew(SButton)
				.ButtonStyle(&Style.SlotButtonStyle)
				.IsFocusable(false)
				.ContentPadding(FMargin(0.f))
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				.OnClicked_Lambda([this, SlotIndex]()
				{
					if (ULRGameSubsystem* Sub = GetSubsystem())
					{
						Sub->SelectSlot(SlotIndex);
					}
					return FReply::Handled();
				})
				.OnHovered_Lambda([this, SlotIndex]() { SetHover(EHoverKind::InventorySlot, NAME_None, SlotIndex); })
				.OnUnhovered_Lambda([this]() { ClearHover(); })
				[
					SNew(SOverlay)
					// Colour swatch standing in for an icon
					+ SOverlay::Slot()
					[
						SNew(SBorder)
						.BorderImage(&Style.WhiteBrush)
						.BorderBackgroundColor_Lambda([this, SlotIndex]() -> FSlateColor
						{
							const FLRInventorySlot* Slot = GetSlot(SlotIndex);
							const FLRSimulation* Sim = GetSimulation();
							const FLRItemDef* Def = (Slot && !Slot->IsEmpty() && Sim) ? Sim->GetData().FindItem(Slot->Item) : nullptr;
							if (!Def)
							{
								return FLinearColor::Transparent;
							}
							FLinearColor Color = Def->GetLinearColor();
							Color.A = 0.45f;
							return Color;
						})
					]
					+ SOverlay::Slot()
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Font(Style.HeadingFont)
						.ColorAndOpacity(Style.Text)
						.Text_Lambda([this, SlotIndex]()
						{
							const FLRInventorySlot* Slot = GetSlot(SlotIndex);
							const FLRSimulation* Sim = GetSimulation();
							if (!Slot || Slot->IsEmpty() || !Sim)
							{
								return FText::GetEmpty();
							}
							const FLRItemDef* Def = Sim->GetData().FindItem(Slot->Item);
							return AsText(Def && !Def->Abbrev.IsEmpty() ? Def->Abbrev : Slot->Item.ToString().Left(3));
						})
					]
					+ SOverlay::Slot()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Bottom)
					.Padding(FMargin(0.f, 0.f, 2.f, 0.f))
					[
						SNew(STextBlock)
						.Font(Style.SmallFont)
						.ColorAndOpacity(Style.Text)
						.Text_Lambda([this, SlotIndex]()
						{
							const FLRInventorySlot* Slot = GetSlot(SlotIndex);
							return (Slot && !Slot->IsEmpty() && Slot->Count > 1) ? FText::AsNumber(Slot->Count) : FText::GetEmpty();
						})
					]
				]
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

const FLRInventorySlot* SLRGameHud::GetSlot(int32 SlotIndex) const
{
	const FLRSimulation* Sim = GetSimulation();
	return (Sim && Sim->GetInventory().IsValidIndex(SlotIndex)) ? &Sim->GetInventory()[SlotIndex] : nullptr;
}

bool SLRGameHud::IsSelectedSlotLoadable() const
{
	const ULRGameSubsystem* Sub = GetSubsystem();
	const FLRSimulation* Sim = GetSimulation();
	const FLRInventorySlot* Slot = Sub ? GetSlot(Sub->GetSelectedSlot()) : nullptr;
	const FLRItemDef* Def = (Slot && !Slot->IsEmpty() && Sim) ? Sim->GetData().FindItem(Slot->Item) : nullptr;
	return Def && (Def->IsLootBox() || Def->IsSource());
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

	if (Box && Def && FLRSimulation::IsExposureComplete(*Box, *Def))
	{
		Text += TEXT("\nDone. Unload the cache to open it.");
	}
	else if (Enclosure.Chamber.IsEmpty() || Enclosure.Source.IsEmpty())
	{
		Text += TEXT("\nIdle: needs a cache and a source.");
	}
	return Text;
}

bool SLRGameHud::IsSelectedSlotUsable() const
{
	const ULRGameSubsystem* Sub = GetSubsystem();
	const FLRSimulation* Sim = GetSimulation();
	const FLRInventorySlot* Slot = Sub ? GetSlot(Sub->GetSelectedSlot()) : nullptr;
	const FLRItemDef* Def = (Slot && !Slot->IsEmpty() && Sim) ? Sim->GetData().FindItem(Slot->Item) : nullptr;
	return Def && Def->IsLootBox();
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

bool SLRGameHud::IsSelectedCellRecallable() const
{
	const ULRGameSubsystem* Sub = GetSubsystem();
	const FLRSimulation* Sim = GetSimulation();
	FIntVector Selected;
	const FLRPlacedEntity* Entity = (Sub && Sim && Sub->GetSelectedCell(Selected)) ? Sim->FindPlaced(Selected) : nullptr;
	const FLRItemDef* Def = Entity ? Sim->GetData().FindItem(Entity->Item) : nullptr;
	return Entity && !(Def && Def->IsStructure());
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

bool SLRGameHud::HasSelectedCell(bool bWantOccupied) const
{
	const ULRGameSubsystem* Sub = GetSubsystem();
	const FLRSimulation* Sim = GetSimulation();
	FIntVector Selected;
	if (!Sub || !Sim || !Sub->GetSelectedCell(Selected))
	{
		return false;
	}
	return (Sim->FindPlaced(Selected) != nullptr) == bWantOccupied;
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

	TArray<FString> Parts;
	for (const FLRItemAmount& Cost : Recipe->Cost)
	{
		const FLRItemDef* Def = Sim->GetData().FindItem(Cost.Item);
		if (bMultiline)
		{
			const int32 Have = Sim->CountItem(Cost.Item);
			Parts.Add(FString::Printf(TEXT("%s: %d  (have %d) %s"),
				*Sim->GetData().GetDisplayName(Cost.Item), Cost.Count, Have, Have >= Cost.Count ? TEXT("OK") : TEXT("- need more")));
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
			return AsText(FString::Printf(TEXT("Craft: %s"), *Recipe->Label));
		}
		break;

	case EHoverKind::InventorySlot:
		if (const FLRInventorySlot* Slot = GetSlot(HoverIndex))
		{
			return Slot->IsEmpty()
				? AsText(FString::Printf(TEXT("Slot %d"), HoverIndex + 1))
				: AsText(Sim->GetData().GetDisplayName(Slot->Item));
		}
		break;

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
		if (const FLRRecipeDef* Recipe = Sim->GetData().FindRecipe(HoverName))
		{
			return AsText(FString::Printf(TEXT("%s\n\nCost:\n%s"), *Recipe->Tooltip, *DescribeCost(HoverName, /*bMultiline*/ true)));
		}
		break;

	case EHoverKind::InventorySlot:
		if (const FLRInventorySlot* Slot = GetSlot(HoverIndex))
		{
			if (Slot->IsEmpty())
			{
				return LOCTEXT("EmptySlot", "Empty.");
			}
			const FLRItemDef* Def = Sim->GetData().FindItem(Slot->Item);
			FString Body = Def ? Def->Tooltip : FString();
			Body += FString::Printf(TEXT("\n\nCount: %d"), Slot->Count);
			if (Def)
			{
				Body += FString::Printf(TEXT(" / %d"), Def->StackSize);
			}
			if (const FLRLootBoxInstance* Box = Sim->FindLootBox(Slot->InstanceId))
			{
				for (const FLRLootModifier& Modifier : Box->Modifiers)
				{
					const FLRRadiationDef* From = Sim->GetData().FindRadiation(Modifier.Source);
					Body += FString::Printf(TEXT("\nIrradiated: %s%s"), *FLRSimulation::DescribeModifier(Modifier, Sim->GetData()),
						From ? *FString::Printf(TEXT(" (%s)"), *From->Name) : TEXT(""));
				}
				if (Box->bRevealed)
				{
					TArray<FString> Parts;
					for (const FLRItemAmount& Amount : Box->RevealedContents)
					{
						Parts.Add(FString::Printf(TEXT("%d %s"), Amount.Count, *Sim->GetData().GetDisplayName(Amount.Item)));
					}
					Body += FString::Printf(TEXT("\nObserved contents: %s"), Parts.IsEmpty() ? TEXT("nothing") : *FString::Join(Parts, TEXT(", ")));
				}
			}
			if (Def && Def->IsSource())
			{
				if (const FLRRadiationDef* Radiation = Sim->GetData().FindRadiation(Def->Radiation))
				{
					Body += FString::Printf(TEXT("\nRadiation: %s (tier %d, energy %d)\nEach exposure: %s"), *Radiation->Name,
						Radiation->Tier, Radiation->Energy, *FLRSimulation::DescribeModifier(Radiation->Effect, Sim->GetData()));
				}
			}
			if (Def && Def->IsEnclosure())
			{
				Body += FString::Printf(TEXT("\nContains radiation up to tier %d, adds up to %d stacks, one every %.0fs."),
					Def->MaxRadiationTier, Def->MaxExposureStacks, Def->ExposureSeconds);
			}
			Body += TEXT("\n\nClick to select.");
			return AsText(Body);
		}
		break;

	default:
		break;
	}

	FIntVector Cell;
	if (GetInfoCell(Cell))
	{
		if (const FLRPlacedEntity* Entity = Sim->FindPlaced(Cell))
		{
			const FLRItemDef* Def = Sim->GetData().FindItem(Entity->Item);
			if (Def && Def->IsEnclosure())
			{
				return AsText(FString::Printf(TEXT("%s\n\n%s"), Def ? *Def->Tooltip : TEXT(""), *DescribeEnclosure(*Entity)));
			}
			if (Def && Def->IsOverdensity())
			{
				return AsText(FString::Printf(TEXT("%s\n\n%s"), *Def->Tooltip, *DescribeOverdensity(*Entity)));
			}
			return AsText(FString::Printf(TEXT("%s\n%s\n\nDeployed at t=%.0fs. Select it, then Recall to pick it up."),
				*Sim->GetData().GetDisplayName(Entity->Item), Def ? *Def->Tooltip : TEXT(""), Entity->PlacedAt));
		}
		return GetStatus(LRNames::Perturb).bRevealed
			? LOCTEXT("EmptyCellPerturb", "Empty cell. Click to select it, then Perturb to seed a ripple here, or pick a deployable item in the inventory and Deploy it.")
			: LOCTEXT("EmptyCell", "Empty cell. Click to select it, pick a deployable item in the inventory, then Deploy.");
	}
	return LOCTEXT("InfoHint", "Hover an action, item or cell to see details.");
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
