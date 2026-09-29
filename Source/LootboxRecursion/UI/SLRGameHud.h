#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class FLRSimulation;
class SScrollBox;
class SVerticalBox;
class ULRGameSubsystem;
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
 * Layout: Actions (with the build list) and the Universe (matter totals) on the left, the 3D
 * grid visible in the middle, the Grid panel (layer, selection, cell actions, entity list) and
 * Info on the right, and the Log across the bottom. There is no inventory: everything the
 * player makes lives in grid cells, so most actions work on the selected cell.
 *
 * That's the inside view. The outside panel (the facility's injectors: the feed dial, the host
 * in its chamber, the stored charge and Ignite) drops down from the top when the status bar's
 * OUTSIDE button or F / Tab is pressed (docs/DESIGN.md, "Two views").
 */
class LOOTBOXRECURSION_API SLRGameHud : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SLRGameHud) {}
		SLATE_ARGUMENT(TWeakObjectPtr<ULRGameSubsystem>, Subsystem)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	void AddLogMessage(const FString& Message, bool bIsError);

	/** Refresh the list of deployed entities (call when the world changes). */
	void RebuildDeployedList();

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
	TSharedRef<SWidget> BuildWorldPanel();
	TSharedRef<SWidget> BuildInfoPanel();
	TSharedRef<SWidget> BuildLogPanel();
	/** The outside view: the facility's injector controls. */
	TSharedRef<SWidget> BuildOutsidePanel();
	/** The controls, opened with the status bar's (?) button. */
	TSharedRef<SWidget> BuildHelpDialog();
	/** Something outside needs the player: the host is shrinking, gone, near the cap, or the charge is rebuilding. */
	bool DoesOutsideNeedAttention() const;
	TSharedRef<SWidget> MakePanel(const FText& Title, const TSharedRef<SWidget>& Content,
		const TSharedRef<SWidget>& HeaderExtra, bool bFillHeight = false, bool bConsole = false);
	TSharedRef<SWidget> MakeActionButton(FName ActionName, TFunction<void()> OnClick, TFunction<bool()> ExtraEnabled = nullptr);
	TSharedRef<SWidget> MakeActionProgress(FName ActionName);
	TSharedRef<SWidget> MakeRecipeButton(FName RecipeId);
	TSharedRef<SWidget> MakeMatterRow(FName Item);
	TSharedRef<SWidget> MakeDeployedRow(const FLRPlacedEntity& Entity);
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
	TSharedPtr<SVerticalBox> DeployedBox;

	/** The outside panel, and how far it has dropped down (0 = hidden, 1 = fully down). */
	TSharedPtr<SWidget> OutsidePanel;
	float OutsideDrop = 0.f;
	bool bHelpOpen = false;

	/** Log history kept for scrolling back; the panel shows LogVisibleLines of it at a time. */
	static constexpr int32 MaxLogLines = 200;
	static constexpr int32 LogVisibleLines = 8;
};
