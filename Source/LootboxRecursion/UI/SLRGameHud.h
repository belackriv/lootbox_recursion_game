#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class FLRSimulation;
class SLRGameMenu;
class SScrollBox;
class SVerticalBox;
class ULRGameSubsystem;
class UMaterialInterface;
struct FLRActionStatus;
struct FLRLootBoxInstance;
struct FLRPlacedEntity;

/**
 * The whole in-game UI, written in Slate (Unreal's C++ UI framework).
 *
 * Coming from Vue: SNew(...) + [ children ] is the template, and every *_Lambda attribute
 * (Text_Lambda, IsEnabled_Lambda, ...) is re-evaluated each frame - think of them as computed
 * properties. Because of that most of the UI is simply "bound" to the simulation and never
 * needs manual refreshes. Only the message log is event-driven (AddLogMessage).
 *
 * Layout: the Actions panel top left is a command card (docs/DESIGN.md, "The command card"):
 * ten slots with fixed keys (1-0 by default), filled with what the selected cell allows, and a
 * Build page of recipes; the build layer strip sits under it. The 3D grid shows in the middle;
 * the Universe (matter totals) and Info panels sit at the bottom right, above the Log across
 * the bottom. There is no inventory: everything the player makes lives in grid cells, so
 * actions work on the selected cell.
 *
 * That's the inside view. The outside panel (the facility's injectors: the feed dial, the host
 * in its chamber, the stored charge and Ignite) drops down from the top when the status bar's
 * OUTSIDE button or F / Tab is pressed (docs/DESIGN.md, "Two views").
 *
 * While the instruments are down (the stored charge is rebuilding), TV static covers the 3D
 * view and the chamber camera, and every action is disabled.
 *
 * Esc (or the MENU button) opens the game menu (SLRGameMenu) over everything and pauses. The
 * whole HUD is scaled by the player's UI scale setting.
 */
class LOOTBOXRECURSION_API SLRGameHud : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SLRGameHud)
		: _StaticMaterial(nullptr)
		{}
		SLATE_ARGUMENT(TWeakObjectPtr<ULRGameSubsystem>, Subsystem)
		/** The M_Static hook, or nullptr (the owner keeps it alive). */
		SLATE_ARGUMENT(UMaterialInterface*, StaticMaterial)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	void AddLogMessage(const FString& Message, bool bIsError);

	/** The game menu is open (the game is paused). */
	bool IsMenuOpen() const { return bMenuOpen; }
	/** The menu key: back out of the menu's page, help, the Build card or the outside panel, else open the menu. */
	void HandleMenuKey();
	/**
	 * A command card slot's key (0-based): does what that slot shows, if it's enabled. While the
	 * outside panel is down, the same keys press its buttons (RunOutsideSlot) instead.
	 */
	void ActivateCardSlot(int32 Slot);
	/**
	 * Turn the feed dial with the keys (the camera's pan keys, while the outside panel is down):
	 * Direction +1 up, -1 down; bCoarse for the up/down keys, fine for left/right.
	 */
	void NudgeDial(float Direction, bool bCoarse, float DeltaSeconds);

	/** How fast the keys turn the dial, as a fraction of its arc per second. */
	static constexpr float DialFineSpeed = 0.1f;
	static constexpr float DialCoarseSpeed = 0.4f;

private:
	enum class EHoverKind : uint8
	{
		None,
		Action,
		Recipe,
		Material,
		WorldCell,
	};

	// Layout builders
	TSharedRef<SWidget> BuildHeader();
	TSharedRef<SWidget> BuildActionsPanel();
	TSharedRef<SWidget> BuildUniversePanel();
	TSharedRef<SWidget> BuildInfoPanel();
	/** Under the card: build layer down / up, and Home. */
	TSharedRef<SWidget> BuildLayerStrip();
	TSharedRef<SWidget> BuildLogPanel();
	/** The outside view: the facility's injector controls. */
	TSharedRef<SWidget> BuildOutsidePanel();
	/** TV static over the 3D view while the instruments are down, with a banner saying why. */
	TSharedRef<SWidget> BuildInstrumentStatic();
	/** The controls, opened with the status bar's (?) button. */
	TSharedRef<SWidget> BuildHelpDialog();
	/** Something outside needs the player: the host is shrinking, gone, near the cap, or the charge is rebuilding. */
	bool DoesOutsideNeedAttention() const;
	TSharedRef<SWidget> MakePanel(const FText& Title, const TSharedRef<SWidget>& Content,
		const TSharedRef<SWidget>& HeaderExtra, bool bFillHeight = false, bool bConsole = false);
	TSharedRef<SWidget> MakeMatterRow(FName Item);
	TSharedRef<SWidget> MakeCardSlot(int32 Slot);

	// The command card
	enum class ECardKind : uint8
	{
		Empty,
		/** An action on the selected cell (Perturb, Open, Dismantle). */
		Action,
		/** Opens the Build page. */
		BuildPage,
		/** A recipe on the Build page. */
		Recipe,
		/** Back from the Build page. */
		Back,
	};
	struct FCardEntry
	{
		ECardKind Kind = ECardKind::Empty;
		FName Name;
	};
	/** What the card's slots hold for the current selection (fewer than CardSlotCount: the rest are empty). */
	TArray<FCardEntry> GetCardEntries() const;
	FCardEntry GetCardEntry(int32 Slot) const;
	bool IsCardEntryEnabled(const FCardEntry& Entry) const;
	FText GetCardEntryLabel(const FCardEntry& Entry) const;
	/** Small text on the right of a slot: a recipe's cost. */
	FText GetCardEntryDetail(const FCardEntry& Entry) const;
	void RunCardEntry(const FCardEntry& Entry);
	/** The key bound to a command, for labels ("1"), or empty. */
	FText GetKeyLabel(FName Command) const;
	void SetMenuOpen(bool bOpen);

	// The outside panel's buttons, in key order (slot keys 1-8 while the panel is down)
	enum class EOutsideSlot : uint8
	{
		Off,
		Hold,
		Limit,
		Max,
		AutoHold,
		AutoLimit,
		Vent,
		Ignite,
		Count,
	};
	bool IsOutsideSlotEnabled(EOutsideSlot Button) const;
	void RunOutsideSlot(EOutsideSlot Button);
	/** "[1] Off": the button's label with its key. */
	FText GetOutsideSlotLabel(EOutsideSlot Button, const FText& Label) const;
	float GetUIScale() const;
	/** bConsole: styled for the light outside console. */
	TSharedRef<SWidget> MakeSmallButton(const FText& Label, TFunction<void()> OnClick, bool bConsole = false);

	// Data helpers
	ULRGameSubsystem* GetSubsystem() const { return Subsystem.Get(); }
	const FLRSimulation* GetSimulation() const;
	FLRActionStatus GetStatus(FName ActionName) const;
	bool IsCellSelected(const FIntVector& Cell) const;
	/** The selected grid cell, if any. */
	bool GetSelected(FIntVector& OutCell) const;
	/** The deployed irradiator in the selected cell, if any. */
	const FLRPlacedEntity* GetSelectedIrradiator() const;
	TOptional<float> GetSelectedExposureFraction() const;
	FString DescribeIrradiator(const FLRPlacedEntity& Irradiator) const;
	/** Perturb can seed or deepen a ripple in the selected cell. */
	bool CanPerturbSelectedCell() const;
	/** The selected cell holds a cache (or an irradiator with one) to open. */
	bool CanOpenSelectedCell() const;
	/** The selected cell holds something Dismantle can take apart (not a structure). */
	bool CanDismantleSelectedCell() const;
	FString DescribeOverdensity(const FLRPlacedEntity& Overdensity) const;
	FString DescribeCache(const FLRLootBoxInstance& Cache) const;
	/** The matter in a cell, one material per line ("" if none). */
	FString DescribeCellMatter(const FIntVector& Cell) const;

	// Hover / info panel
	void SetHover(EHoverKind Kind, FName Name, int32 Index = 0, const FIntVector& Cell = FIntVector::ZeroValue);
	/** The grid cell the info panel should describe, if any. */
	bool GetInfoCell(FIntVector& OutCell) const;
	void ClearHover() { HoverKind = EHoverKind::None; }
	FText GetHoverTitle() const;
	FText GetHoverBody() const;
	FString DescribeCost(FName RecipeId, bool bMultiline) const;

	void RebuildLog();

	TWeakObjectPtr<ULRGameSubsystem> Subsystem;

	EHoverKind HoverKind = EHoverKind::None;
	FName HoverName;
	int32 HoverIndex = 0;
	FIntVector HoverCell = FIntVector::ZeroValue;

	struct FLogLine
	{
		FString Text;
		bool bIsError = false;
	};
	TArray<FLogLine> LogLines;
	TSharedPtr<SVerticalBox> LogBox;
	TSharedPtr<SScrollBox> LogScroll;

	/** The outside panel, and how far it has dropped down (0 = hidden, 1 = fully down). */
	TSharedPtr<SWidget> OutsidePanel;
	float OutsideDrop = 0.f;

	/**
	 * The outside panel's size follows the viewport (ViewSize, from Tick): this share of its
	 * height, and at most this share of its width. The chamber camera fills the space left
	 * under the dial and the chamber, at 4:3 (CameraSize, fitted in Tick from CameraArea).
	 * Below the minimums the whole panel scales down instead.
	 */
	static constexpr float OutsideHeightShare = 0.8f;
	static constexpr float OutsideMaxWidthShare = 0.9f;
	static constexpr float OutsideMinHeight = 680.f;
	static constexpr float OutsideMinWidth = 1000.f;
	/** Roughly the readouts' and the stored charge's columns, with the gaps: the camera's width cap. */
	static constexpr float OutsideSideColumnsWidth = 500.f;
	static constexpr float CameraMinHeight = 240.f;
	/** The camera screen's border and padding, both sides together. */
	static constexpr float CameraChrome = 12.f;
	FVector2f ViewSize = FVector2f(1920.f, 1080.f);
	FVector2f CameraSize = FVector2f(640.f, 480.f);
	TSharedPtr<SWidget> CameraArea;
	bool bHelpOpen = false;
	/** The card shows the Build page (recipes) rather than the cell's actions. */
	bool bBuildPage = false;
	/** The selection the card was last showing: a new selection goes back to the main page. */
	FIntVector CardCell = FIntVector::ZeroValue;
	bool bCardHadSelection = false;
	bool bMenuOpen = false;
	TSharedPtr<SLRGameMenu> Menu;
	/** How strong the static is (0 = none), fading towards whether the instruments are down. */
	float StaticLevel = 0.f;
	UMaterialInterface* StaticMaterial = nullptr;

	/** Log history kept for scrolling back; the panel shows LogVisibleLines of it at a time. */
	static constexpr int32 MaxLogLines = 200;
	static constexpr int32 LogVisibleLines = 8;
};
