#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Game/LRSaveGame.h"
#include "Simulation/LRSimulation.h"
#include "Simulation/LRSimTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "LRGameSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FLRSimpleEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLRActionCompletedEvent, const FLRActionResult&, Result);

class ULRSaveIndex;
class ULRUserSettings;

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

	UFUNCTION(BlueprintPure, Category = "Quantum Recursion")
	bool GetItemDef(FName Item, FLRItemDef& OutDef) const;

	// ---- Commands ---------------------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = "Quantum Recursion")
	FLRActionResult RequestAction(const FLRActionRequest& Request);

	UFUNCTION(BlueprintCallable, Category = "Quantum Recursion")
	FLRActionResult RequestSimpleAction(FName Action);

	/** Build a recipe in the selected cell. */
	UFUNCTION(BlueprintCallable, Category = "Quantum Recursion")
	FLRActionResult Craft(FName RecipeId);

	/** An action on the selected cell (Perturb, Open, Dismantle...). */
	UFUNCTION(BlueprintCallable, Category = "Quantum Recursion")
	FLRActionResult RequestActionWithSelection(FName Action);

	// ---- Queries ----------------------------------------------------------------------
	UFUNCTION(BlueprintPure, Category = "Quantum Recursion")
	FLRActionStatus GetActionStatus(FName Action) const;

	/** How much of a material a cell holds. */
	UFUNCTION(BlueprintPure, Category = "Quantum Recursion")
	int32 GetMatterAt(FIntVector Cell, FName Item) const;

	/** How much of a material the whole pocket universe holds. */
	UFUNCTION(BlueprintPure, Category = "Quantum Recursion")
	int32 GetTotalMatter(FName Item) const;

	UFUNCTION(BlueprintPure, Category = "Quantum Recursion")
	bool GetPlacedAt(FIntVector Cell, FLRPlacedEntity& OutEntity) const;

	UFUNCTION(BlueprintPure, Category = "Quantum Recursion")
	double GetSimTime() const;

	// ---- Cosmos (epochs, cosmic clock, host black hole) --------------------------------
	/** Display name of the current epoch ("Reheating"), or empty if the data defines none. */
	UFUNCTION(BlueprintPure, Category = "Quantum Recursion|Cosmos")
	FString GetEpochName() const;

	/** Cosmic time since the pocket universe's Big Bang, formatted ("380 thousand years"). */
	UFUNCTION(BlueprintPure, Category = "Quantum Recursion|Cosmos")
	FString GetCosmicTimeText() const;

	/** The host black hole's mass, kg (0 once it has evaporated). */
	UFUNCTION(BlueprintPure, Category = "Quantum Recursion|Cosmos")
	double GetHostMass() const;

	// ---- Outside: the facility's injectors (the drop-down panel) -------------------------
	/** Set the feed dial: the injection rate to aim for, kg/s. */
	UFUNCTION(BlueprintCallable, Category = "Quantum Recursion|Outside")
	void SetInjectorTarget(double KgPerSecond);

	UFUNCTION(BlueprintPure, Category = "Quantum Recursion|Outside")
	double GetInjectorTarget() const;

	/** Let the feed dial follow the HOLD or LIMIT mark by itself, or stop it (Off). */
	UFUNCTION(BlueprintCallable, Category = "Quantum Recursion|Outside")
	void SetInjectorAuto(ELRInjectorAuto Mode);

	UFUNCTION(BlueprintPure, Category = "Quantum Recursion|Outside")
	ELRInjectorAuto GetInjectorAuto() const;

	/** Run the injectors in reverse to shed mass, or stop. */
	UFUNCTION(BlueprintCallable, Category = "Quantum Recursion|Outside")
	FLRActionResult SetVenting(bool bVent);

	UFUNCTION(BlueprintPure, Category = "Quantum Recursion|Outside")
	bool IsVenting() const;

	/** Whether the outside panel (the injector controls) is dropped down. UI state, not saved. */
	UFUNCTION(BlueprintPure, Category = "Quantum Recursion|Outside")
	bool IsOutsideViewOpen() const { return bOutsideViewOpen; }

	UFUNCTION(BlueprintCallable, Category = "Quantum Recursion|Outside")
	void SetOutsideViewOpen(bool bOpen) { bOutsideViewOpen = bOpen; }

	/** The status bar button and its hotkey. */
	UFUNCTION(BlueprintCallable, Category = "Quantum Recursion|Outside")
	void ToggleOutsideView() { bOutsideViewOpen = !bOutsideViewOpen; }

	/** How opaque the primordial plasma is right now (0..1). */
	UFUNCTION(BlueprintPure, Category = "Quantum Recursion|Cosmos")
	float GetPlasmaOpacity() const;

	// ---- Selection, hover, build layer & camera focus (UI state, not saved) -----------
	/** Select a grid cell. With bToggle, selecting the already-selected cell deselects it. */
	UFUNCTION(BlueprintCallable, Category = "Quantum Recursion|Selection")
	void SelectCell(FIntVector Cell, bool bToggle = true);

	UFUNCTION(BlueprintCallable, Category = "Quantum Recursion|Selection")
	void ClearCellSelection();

	UFUNCTION(BlueprintPure, Category = "Quantum Recursion|Selection")
	bool GetSelectedCell(FIntVector& OutCell) const;

	/** The cell under the mouse cursor (set every frame by the world grid). */
	void SetHoveredCell(const FIntVector& Cell) { HoveredCell = Cell; bHasHoveredCell = true; }
	void ClearHoveredCell() { bHasHoveredCell = false; }

	UFUNCTION(BlueprintPure, Category = "Quantum Recursion|Selection")
	bool GetHoveredCell(FIntVector& OutCell) const;

	/** The Z layer you are building on. */
	UFUNCTION(BlueprintPure, Category = "Quantum Recursion|Selection")
	int32 GetBuildLayer() const { return BuildLayer; }

	UFUNCTION(BlueprintCallable, Category = "Quantum Recursion|Selection")
	void SetBuildLayer(int32 Layer);

	UFUNCTION(BlueprintCallable, Category = "Quantum Recursion|Selection")
	void ChangeBuildLayer(int32 Delta) { SetBuildLayer(BuildLayer + Delta); }

	/** Ask the camera to fly to a cell (and switch to its layer). */
	UFUNCTION(BlueprintCallable, Category = "Quantum Recursion|Selection")
	void FocusOnCell(FIntVector Cell);

	/** Increments on every FocusOnCell; the camera watches it. */
	int32 GetFocusRequestSerial() const { return FocusRequestSerial; }
	FIntVector GetFocusRequestCell() const { return FocusRequestCell; }

	/** Rails: the Trim button - fly to the first deployed entity (or the origin). */
	UFUNCTION(BlueprintCallable, Category = "Quantum Recursion|Selection")
	void FocusHome();

	/** Deployed entities sorted by layer, then Y, then X (for lists). */
	TArray<FLRPlacedEntity> GetPlacedSorted() const;

	// ---- Debug / meta -----------------------------------------------------------------
	/** Cheat: add a material to the selected cell (or the origin if none is selected). */
	UFUNCTION(BlueprintCallable, Category = "Quantum Recursion|Debug")
	bool GiveMatter(FName Item, int32 Count);

	UFUNCTION(BlueprintCallable, Category = "Quantum Recursion|Debug")
	void SetTimeScale(float InTimeScale) { TimeScale = FMath::Clamp(InTimeScale, 0.f, 100.f); }

	UFUNCTION(BlueprintPure, Category = "Quantum Recursion|Debug")
	float GetTimeScale() const { return TimeScale; }

	UFUNCTION(BlueprintCallable, Category = "Quantum Recursion|Debug")
	void ResetGame();

	/**
	 * Autosave now (the autosave slot). It skips a game that's over, or whose host alarm is at
	 * WARNING or worse, so the autosave is always somewhere a lost game can start again.
	 */
	UFUNCTION(BlueprintCallable, Category = "Quantum Recursion|Debug")
	bool SaveNow();

	UFUNCTION(BlueprintPure, Category = "Quantum Recursion|Menu")
	bool HasAutosave() const;

	/** Load the autosave (the game-over screen's first choice). */
	UFUNCTION(BlueprintCallable, Category = "Quantum Recursion|Menu")
	bool LoadAutosave();

	// ---- The game menu: pause, save slots, settings -------------------------------------
	/** Paused (the Esc menu is open): the simulation doesn't advance and nothing autosaves. */
	UFUNCTION(BlueprintCallable, Category = "Quantum Recursion|Menu")
	void SetPaused(bool bInPaused) { bPaused = bInPaused; }

	UFUNCTION(BlueprintPure, Category = "Quantum Recursion|Menu")
	bool IsPaused() const { return bPaused; }

	/** Every save, the autosave first, then the newest first. */
	UFUNCTION(BlueprintCallable, Category = "Quantum Recursion|Menu")
	TArray<FLRSaveSlotInfo> GetSaveSlots() const;

	/** Save the game as a new named save. */
	UFUNCTION(BlueprintCallable, Category = "Quantum Recursion|Menu")
	bool SaveToNewSlot(const FString& DisplayName);

	/** Save the game over an existing save (keeping its name). */
	UFUNCTION(BlueprintCallable, Category = "Quantum Recursion|Menu")
	bool SaveToSlot(const FString& SlotName);

	/** Load a save; the game then carries on autosaving into the autosave slot. */
	UFUNCTION(BlueprintCallable, Category = "Quantum Recursion|Menu")
	bool LoadFromSlot(const FString& SlotName);

	/** Delete a manual save (not the autosave). */
	UFUNCTION(BlueprintCallable, Category = "Quantum Recursion|Menu")
	bool DeleteSlot(const FString& SlotName);

	/** The player's settings (never null after Initialize). Change them, then call SaveUserSettings. */
	ULRUserSettings* GetUserSettings() const { return UserSettings; }

	/** Save the settings and tell everyone to apply them (OnSettingsChanged). */
	void SaveUserSettings();

	// ---- Events (Rails: PlayerInventoryChannel / PlayerActionsChannel broadcasts) ------
	/** Matter in some cell changed. */
	UPROPERTY(BlueprintAssignable, Category = "Quantum Recursion")
	FLRSimpleEvent OnMatterChanged;

	UPROPERTY(BlueprintAssignable, Category = "Quantum Recursion")
	FLRSimpleEvent OnWorldChanged;

	UPROPERTY(BlueprintAssignable, Category = "Quantum Recursion")
	FLRSimpleEvent OnSelectionChanged;

	UPROPERTY(BlueprintAssignable, Category = "Quantum Recursion")
	FLRActionCompletedEvent OnActionCompleted;

	/** The player's settings changed (key bindings, camera, UI scale). */
	UPROPERTY(BlueprintAssignable, Category = "Quantum Recursion")
	FLRSimpleEvent OnSettingsChanged;

	/** The autosave's slot. */
	static const FString SaveSlotName;
	static const FString SaveIndexSlotName;

private:
	bool Tick(float DeltaSeconds);
	/** Load the game in a slot into the simulation. */
	bool LoadGame(const FString& SlotName);
	/** Write the game to a slot, and list it in the index under DisplayName. */
	bool WriteSlot(const FString& SlotName, const FString& DisplayName, bool bAutosave);
	ULRSaveIndex* LoadSaveIndex() const;
	/** After the simulation's state was replaced: clear the selection and redraw everything. */
	void HandleGameReplaced();
	void HandleSimMatterChanged();
	void HandleSimWorldChanged();
	void HandleSimActionCompleted(const FLRActionResult& Result);

	TUniquePtr<FLRSimulation> Simulation;
	TArray<FString> DataErrors;
	FTSTicker::FDelegateHandle TickHandle;

	float TimeScale = 1.f;
	double SecondsSinceAutosave = 0.0;
	bool bPaused = false;

	UPROPERTY(Transient)
	TObjectPtr<ULRUserSettings> UserSettings;

	FIntVector SelectedCell = FIntVector::ZeroValue;
	bool bHasSelectedCell = false;
	FIntVector HoveredCell = FIntVector::ZeroValue;
	bool bHasHoveredCell = false;
	int32 BuildLayer = 0;
	FIntVector FocusRequestCell = FIntVector::ZeroValue;
	int32 FocusRequestSerial = 0;
	bool bOutsideViewOpen = false;
};
