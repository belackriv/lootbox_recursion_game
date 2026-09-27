#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Simulation/LRSimulation.h"
#include "Simulation/LRSimTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "LRGameSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FLRSimpleEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLRActionCompletedEvent, const FLRActionResult&, Result);

/**
 * Owns the simulation for the lifetime of the game session, ticks it, saves/loads it, and
 * holds UI selection state. This is the one object the HUD, world view, and Blueprints talk to.
 *
 * Rails/Vue equivalent: the controllers + channels (commands in, broadcasts out) and the
 * Pinia player store (selection, focus). A GameInstanceSubsystem is created automatically
 * with the game instance - no spawning or wiring needed; get it from anywhere with
 * GetGameInstance()->GetSubsystem<ULRGameSubsystem>().
 */
UCLASS()
class LOOTBOXRECURSION_API ULRGameSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static ULRGameSubsystem* Get(const UObject* WorldContextObject);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Direct access for C++ (HUD, tests). May be null only if called before Initialize. */
	FLRSimulation* GetSimulation() const { return Simulation.Get(); }

	// ---- Data -------------------------------------------------------------------------
	/** Errors from loading the JSON files in Content/Data. Non-empty means the game data is broken. */
	const TArray<FString>& GetDataErrors() const { return DataErrors; }

	UFUNCTION(BlueprintPure, Category = "Loot Box Recursion")
	bool GetItemDef(FName Item, FLRItemDef& OutDef) const;

	// ---- Commands ---------------------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = "Loot Box Recursion")
	FLRActionResult RequestAction(const FLRActionRequest& Request);

	UFUNCTION(BlueprintCallable, Category = "Loot Box Recursion")
	FLRActionResult RequestSimpleAction(FName Action);

	UFUNCTION(BlueprintCallable, Category = "Loot Box Recursion")
	FLRActionResult Craft(FName RecipeId);

	/** Use / Deploy / Recall with the current selection filled in. */
	UFUNCTION(BlueprintCallable, Category = "Loot Box Recursion")
	FLRActionResult RequestActionWithSelection(FName Action);

	// ---- Queries ----------------------------------------------------------------------
	UFUNCTION(BlueprintPure, Category = "Loot Box Recursion")
	FLRActionStatus GetActionStatus(FName Action) const;

	UFUNCTION(BlueprintPure, Category = "Loot Box Recursion")
	TArray<FLRInventorySlot> GetInventory() const;

	UFUNCTION(BlueprintPure, Category = "Loot Box Recursion")
	int32 CountItem(FName Item) const;

	UFUNCTION(BlueprintPure, Category = "Loot Box Recursion")
	bool GetPlacedAt(int32 Coordinate, FLRPlacedEntity& OutEntity) const;

	UFUNCTION(BlueprintPure, Category = "Loot Box Recursion")
	double GetSimTime() const;

	// ---- Selection & camera focus (UI state, not saved) --------------------------------
	UFUNCTION(BlueprintCallable, Category = "Loot Box Recursion|Selection")
	void SelectSlot(int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category = "Loot Box Recursion|Selection")
	void SelectCell(int32 Coordinate);

	UFUNCTION(BlueprintCallable, Category = "Loot Box Recursion|Selection")
	void ClearCellSelection();

	UFUNCTION(BlueprintPure, Category = "Loot Box Recursion|Selection")
	int32 GetSelectedSlot() const { return SelectedSlot; }

	UFUNCTION(BlueprintPure, Category = "Loot Box Recursion|Selection")
	bool GetSelectedCell(int32& OutCoordinate) const;

	/** The world coordinate the camera centres on. */
	UFUNCTION(BlueprintPure, Category = "Loot Box Recursion|Selection")
	int32 GetFocusCoordinate() const { return FocusCoordinate; }

	UFUNCTION(BlueprintCallable, Category = "Loot Box Recursion|Selection")
	void SetFocusCoordinate(int32 Coordinate);

	UFUNCTION(BlueprintCallable, Category = "Loot Box Recursion|Selection")
	void PanFocus(int32 Delta) { SetFocusCoordinate(FocusCoordinate + Delta); }

	/** Rails: the Trim button - jump back to the first deployed entity (or 0). */
	UFUNCTION(BlueprintCallable, Category = "Loot Box Recursion|Selection")
	void FocusHome();

	// ---- Debug / meta -----------------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = "Loot Box Recursion|Debug")
	bool GiveItem(FName Item, int32 Count);

	UFUNCTION(BlueprintCallable, Category = "Loot Box Recursion|Debug")
	void SetTimeScale(float InTimeScale) { TimeScale = FMath::Clamp(InTimeScale, 0.f, 100.f); }

	UFUNCTION(BlueprintPure, Category = "Loot Box Recursion|Debug")
	float GetTimeScale() const { return TimeScale; }

	UFUNCTION(BlueprintCallable, Category = "Loot Box Recursion|Debug")
	void ResetGame();

	UFUNCTION(BlueprintCallable, Category = "Loot Box Recursion|Debug")
	bool SaveNow();

	// ---- Events (Rails: PlayerInventoryChannel / PlayerActionsChannel broadcasts) ------
	UPROPERTY(BlueprintAssignable, Category = "Loot Box Recursion")
	FLRSimpleEvent OnInventoryChanged;

	UPROPERTY(BlueprintAssignable, Category = "Loot Box Recursion")
	FLRSimpleEvent OnWorldChanged;

	UPROPERTY(BlueprintAssignable, Category = "Loot Box Recursion")
	FLRSimpleEvent OnSelectionChanged;

	UPROPERTY(BlueprintAssignable, Category = "Loot Box Recursion")
	FLRActionCompletedEvent OnActionCompleted;

	static const FString SaveSlotName;

private:
	bool Tick(float DeltaSeconds);
	bool LoadGame();
	void HandleSimInventoryChanged();
	void HandleSimWorldChanged();
	void HandleSimActionCompleted(const FLRActionResult& Result);

	TUniquePtr<FLRSimulation> Simulation;
	TArray<FString> DataErrors;
	FTSTicker::FDelegateHandle TickHandle;

	float TimeScale = 1.f;
	double SecondsSinceAutosave = 0.0;

	int32 SelectedSlot = INDEX_NONE;
	int32 SelectedCell = 0;
	bool bHasSelectedCell = false;
	int32 FocusCoordinate = 0;
};
