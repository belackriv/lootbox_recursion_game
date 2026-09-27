#include "UI/SLRGameHud.h"

#include "Game/LRGameSubsystem.h"
#include "Simulation/LRSimulation.h"
#include "UI/LRHudStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
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
				.Text(LOCTEXT("Title", "LOOT BOX RECURSION"))
				.Font(Style.TitleFont)
				.ColorAndOpacity(Style.Orange)
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			.Padding(FMargin(16.f, 0.f))
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Help", "Click a cell in the world or the Deployed list to select it  |  A/D, arrows or mouse wheel to pan, H for home  |  ~ opens the console (LRGive wood 500, LRTimeScale 10, LRReset)"))
				.Font(Style.SmallFont)
				.ColorAndOpacity(Style.TextDim)
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
		MakeActionButton(LRNames::Scavenge, [this]()
		{
			if (ULRGameSubsystem* Sub = GetSubsystem())
			{
				Sub->RequestSimpleAction(LRNames::Scavenge);
			}
		})
	];

	Box->AddSlot()
	.AutoHeight()
	.Padding(FMargin(0.f, 0.f, 0.f, 6.f))
	[
		MakeActionButton(LRNames::Use, [this]()
		{
			if (ULRGameSubsystem* Sub = GetSubsystem())
			{
				Sub->RequestActionWithSelection(LRNames::Use);
			}
		})
	];

	// Craft: one button per recipe (Rails: CraftToggleButton + CraftActionButton choices).
	Box->AddSlot()
	.AutoHeight()
	.Padding(FMargin(0.f, 4.f, 0.f, 2.f))
	[
		SNew(SHorizontalBox)
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

	// Sort lives in the inventory title bar, like SortButton.vue.
	TSharedRef<SWidget> SortButton = SNew(SBox)
		.WidthOverride(70.f)
		[
			MakeActionButton(LRNames::SortInventory, [this]()
			{
				if (ULRGameSubsystem* Sub = GetSubsystem())
				{
					Sub->RequestSimpleAction(LRNames::SortInventory);
				}
			})
		];

	return MakePanel(LOCTEXT("Inventory", "INVENTORY"), Grid, SortButton);
}

TSharedRef<SWidget> SLRGameHud::BuildWorldPanel()
{
	const FLRHudStyle& Style = FLRHudStyle::Get();
	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);

	// Paging controls
	Box->AddSlot()
	.AutoHeight()
	.Padding(FMargin(0.f, 0.f, 0.f, 4.f))
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			MakeSmallButton(LOCTEXT("PageUp", "< 8"), [this]()
			{
				if (ULRGameSubsystem* Sub = GetSubsystem()) { Sub->PanFocus(-8); }
			})
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Font(Style.SmallFont)
			.ColorAndOpacity(Style.TextDim)
			.Text_Lambda([this]()
			{
				const ULRGameSubsystem* Sub = GetSubsystem();
				return AsText(FString::Printf(TEXT("centred on %d"), Sub ? Sub->GetFocusCoordinate() : 0));
			})
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			MakeSmallButton(LOCTEXT("PageDown", "8 >"), [this]()
			{
				if (ULRGameSubsystem* Sub = GetSubsystem()) { Sub->PanFocus(8); }
			})
		]
	];

	for (int32 Row = 0; Row < WorldRows; ++Row)
	{
		Box->AddSlot()
		.AutoHeight()
		.Padding(FMargin(0.f, 1.f))
		[
			MakeWorldRow(Row)
		];
	}

	// Deploy / Recall act on the selected cell (and selected inventory slot for Deploy).
	Box->AddSlot()
	.AutoHeight()
	.Padding(FMargin(0.f, 6.f, 0.f, 0.f))
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
				[this]() { return HasSelectedCell(/*bWantOccupied*/ true); })
		]
	];

	TSharedRef<SWidget> HomeButton = MakeSmallButton(LOCTEXT("Home", "Home"), [this]()
	{
		if (ULRGameSubsystem* Sub = GetSubsystem()) { Sub->FocusHome(); }
	});

	return MakePanel(LOCTEXT("Deployed", "DEPLOYED"), Box, HomeButton);
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

	return SNew(SVerticalBox)
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

TSharedRef<SWidget> SLRGameHud::MakeWorldRow(int32 RowIndex)
{
	const FLRHudStyle& Style = FLRHudStyle::Get();

	// Rails: WorldCellSlot.vue. Rows are recycled: row N always shows focus - 7 + N.
	return SNew(SBorder)
		.BorderImage(&Style.WhiteBrush)
		.Padding(FMargin(1.f))
		.BorderBackgroundColor_Lambda([this, RowIndex]() -> FSlateColor
		{
			const FLRHudStyle& S = FLRHudStyle::Get();
			return IsCellSelected(GetRowCoordinate(RowIndex)) ? S.Orange : S.SlotBorder;
		})
		[
			SNew(SButton)
			.ButtonStyle(&Style.SlotButtonStyle)
			.IsFocusable(false)
			.ContentPadding(FMargin(6.f, 2.f))
			.OnClicked_Lambda([this, RowIndex]()
			{
				if (ULRGameSubsystem* Sub = GetSubsystem())
				{
					Sub->SelectCell(GetRowCoordinate(RowIndex));
				}
				return FReply::Handled();
			})
			.OnHovered_Lambda([this, RowIndex]() { SetHover(EHoverKind::WorldCell, NAME_None, GetRowCoordinate(RowIndex)); })
			.OnUnhovered_Lambda([this]() { ClearHover(); })
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBox)
					.WidthOverride(52.f)
					[
						SNew(STextBlock)
						.Font(Style.SmallFont)
						.Text_Lambda([this, RowIndex]() { return FText::AsNumber(GetRowCoordinate(RowIndex)); })
						.ColorAndOpacity_Lambda([this, RowIndex]() -> FSlateColor
						{
							// Ruler ticks: major every 16, minor every 4 (WorldGrid.vue tickKind)
							const FLRHudStyle& S = FLRHudStyle::Get();
							const int32 Coordinate = GetRowCoordinate(RowIndex);
							if (Coordinate % 16 == 0) { return S.Orange; }
							if (Coordinate % 4 == 0)  { return S.Text; }
							return S.TextDark;
						})
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				[
					SNew(STextBlock)
					.Font(Style.SmallFont)
					.ColorAndOpacity(Style.Text)
					.Text_Lambda([this, RowIndex]()
					{
						const FLRSimulation* Sim = GetSimulation();
						const FLRPlacedEntity* Entity = Sim ? Sim->FindPlaced(GetRowCoordinate(RowIndex)) : nullptr;
						return Entity ? AsText(Sim->GetData().GetDisplayName(Entity->Item)) : FText::GetEmpty();
					})
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

int32 SLRGameHud::GetRowCoordinate(int32 RowIndex) const
{
	const ULRGameSubsystem* Sub = GetSubsystem();
	return (Sub ? Sub->GetFocusCoordinate() : 0) - WorldRows / 2 + RowIndex;
}

bool SLRGameHud::IsCellSelected(int32 Coordinate) const
{
	const ULRGameSubsystem* Sub = GetSubsystem();
	int32 Selected = 0;
	return Sub && Sub->GetSelectedCell(Selected) && Selected == Coordinate;
}

bool SLRGameHud::HasSelectedCell(bool bWantOccupied) const
{
	const ULRGameSubsystem* Sub = GetSubsystem();
	const FLRSimulation* Sim = GetSimulation();
	int32 Selected = 0;
	if (!Sub || !Sim || !Sub->GetSelectedCell(Selected))
	{
		return false;
	}
	return (Sim->FindPlaced(Selected) != nullptr) == bWantOccupied;
}

// ---------------------------------------------------------------------------------------
// Info panel (Rails: the hoveredTooltip sidebar in MainLayout.vue)
// ---------------------------------------------------------------------------------------

void SLRGameHud::SetHover(EHoverKind Kind, FName Name, int32 Index)
{
	HoverKind = Kind;
	HoverName = Name;
	HoverIndex = Index;
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

	case EHoverKind::WorldCell:
		return AsText(FString::Printf(TEXT("Cell %d"), HoverIndex));

	default:
		break;
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
					Body += FString::Printf(TEXT("\nModifier: %s %s x%.2f"), *Modifier.Kind.ToString(), *Modifier.Item.ToString(), Modifier.Value);
				}
			}
			Body += TEXT("\n\nClick to select.");
			return AsText(Body);
		}
		break;

	case EHoverKind::WorldCell:
		if (const FLRPlacedEntity* Entity = Sim->FindPlaced(HoverIndex))
		{
			const FLRItemDef* Def = Sim->GetData().FindItem(Entity->Item);
			return AsText(FString::Printf(TEXT("%s\n%s\n\nDeployed at t=%.0fs. Select, then Recall to pick it up."),
				*Sim->GetData().GetDisplayName(Entity->Item), Def ? *Def->Tooltip : TEXT(""), Entity->PlacedAt));
		}
		return LOCTEXT("EmptyCell", "Empty cell. Select it, select a deployable item, then Deploy.");

	default:
		break;
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
