#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class FLRSimulation;
class SVerticalBox;
class ULRGameSubsystem;
struct FLRActionStatus;
struct FLRInventorySlot;

/**
 * The whole in-game UI, written in Slate (Unreal's C++ UI framework).
 *
 * Coming from Vue: SNew(...) + [ children ] is the template, and every *_Lambda attribute
 * (Text_Lambda, IsEnabled_Lambda, ...) is re-evaluated each frame - think of them as computed
 * properties. Because of that most of the UI is simply "bound" to the simulation and never
 * needs manual refreshes. Only the message log is event-driven (AddLogMessage).
 *
 * Layout mirrors Pages/Index/Index.vue + MainLayout.vue: Actions and Inventory on the left,
 * the 3D world visible in the middle, the Deployed (world) list + Info + Log on the right.
 */
class LOOTBOXRECURSION_API SLRGameHud : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SLRGameHud) {}
		SLATE_ARGUMENT(TWeakObjectPtr<ULRGameSubsystem>, Subsystem)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void AddLogMessage(const FString& Message, bool bIsError);

private:
	enum class EHoverKind : uint8
	{
		None,
		Action,
		Recipe,
		InventorySlot,
		WorldCell,
	};

	// Layout builders
	TSharedRef<SWidget> BuildHeader();
	TSharedRef<SWidget> BuildActionsPanel();
	TSharedRef<SWidget> BuildInventoryPanel();
	TSharedRef<SWidget> BuildWorldPanel();
	TSharedRef<SWidget> BuildInfoPanel();
	TSharedRef<SWidget> BuildLogPanel();
	TSharedRef<SWidget> MakePanel(const FText& Title, const TSharedRef<SWidget>& Content,
		const TSharedRef<SWidget>& HeaderExtra, bool bFillHeight = false);
	TSharedRef<SWidget> MakeActionButton(FName ActionName, TFunction<void()> OnClick, TFunction<bool()> ExtraEnabled = nullptr);
	TSharedRef<SWidget> MakeActionProgress(FName ActionName);
	TSharedRef<SWidget> MakeRecipeButton(FName RecipeId);
	TSharedRef<SWidget> MakeInventorySlot(int32 SlotIndex);
	TSharedRef<SWidget> MakeWorldRow(int32 RowIndex);
	TSharedRef<SWidget> MakeSmallButton(const FText& Label, TFunction<void()> OnClick);

	// Data helpers
	ULRGameSubsystem* GetSubsystem() const { return Subsystem.Get(); }
	const FLRSimulation* GetSimulation() const;
	FLRActionStatus GetStatus(FName ActionName) const;
	const FLRInventorySlot* GetSlot(int32 SlotIndex) const;
	int32 GetRowCoordinate(int32 RowIndex) const;
	bool IsCellSelected(int32 Coordinate) const;
	bool HasSelectedCell(bool bWantOccupied) const;

	// Hover / info panel
	void SetHover(EHoverKind Kind, FName Name, int32 Index = 0);
	void ClearHover() { HoverKind = EHoverKind::None; }
	FText GetHoverTitle() const;
	FText GetHoverBody() const;
	FString DescribeCost(FName RecipeId, bool bMultiline) const;

	void RebuildLog();

	TWeakObjectPtr<ULRGameSubsystem> Subsystem;

	EHoverKind HoverKind = EHoverKind::None;
	FName HoverName;
	int32 HoverIndex = 0;

	struct FLogLine
	{
		FString Text;
		bool bIsError = false;
	};
	TArray<FLogLine> LogLines;
	TSharedPtr<SVerticalBox> LogBox;

	static constexpr int32 InventoryColumns = 10;
	static constexpr int32 WorldRows = 15;
	static constexpr int32 MaxLogLines = 8;
};
