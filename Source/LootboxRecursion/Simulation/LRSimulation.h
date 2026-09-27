#pragma once

#include "CoreMinimal.h"
#include "Data/LRGameData.h"
#include "Math/RandomStream.h"
#include "Simulation/LRSimTypes.h"

/**
 * The game rules, as plain C++ with no engine/world dependencies.
 *
 * Rails equivalent: the models (User, Entity, InventoryItemMutation, LootBox, LootTable,
 * PlayerAction, PlaceableEntity...) plus PerformPlayerActionJob. Where Rails persisted each
 * change to Postgres and broadcast it over Action Cable, this class mutates in-memory state
 * and fires delegates. Persistence is a snapshot (Save/Load) handled by ULRGameSubsystem.
 *
 * Time is simulation time: the owner calls Advance(DeltaSeconds) every frame. That makes
 * pausing, fast-forwarding (LRTimeScale) and deterministic tests trivial.
 */
class LOOTBOXRECURSION_API FLRSimulation
{
public:
	static constexpr int32 PlayerInventorySlots = 50; // Rails: User::BASE_INVENTORY_SLOTS
	static constexpr int32 SaveVersion = 4; // 2: 3D grid, wood -> carbon. 3: enclosure contents. 4: unlocks + stats
	/** Oldest save that still loads (newer fields just start empty). */
	static constexpr int32 MinCompatibleSaveVersion = 3;

	DECLARE_MULTICAST_DELEGATE(FOnChanged);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnActionCompleted, const FLRActionResult& /*Result*/);

	explicit FLRSimulation(const FLRGameData& InData, int32 Seed = 0);

	// ---- Lifecycle / persistence ------------------------------------------------------
	void Reset(int32 Seed);
	FLRSaveData Save() const;
	/** Returns false (and leaves state untouched) if the save is from an incompatible version. */
	bool Load(const FLRSaveData& SaveData);

	// ---- Clock ------------------------------------------------------------------------
	/** Advances simulation time and completes any casts that finished. */
	void Advance(double DeltaSeconds);
	double GetNow() const { return Now; }

	// ---- Queries ----------------------------------------------------------------------
	const FLRGameData& GetData() const { return Data; }
	const TArray<FLRInventorySlot>& GetInventory() const { return Inventory; }
	const TMap<FIntVector, FLRPlacedEntity>& GetPlaced() const { return Placed; }
	const FLRPlacedEntity* FindPlaced(const FIntVector& Cell) const { return Placed.Find(Cell); }
	const FLRLootBoxInstance* FindLootBox(int32 InstanceId) const { return LootBoxes.Find(InstanceId); }

	int32 CountItem(FName Item) const;
	int32 CountCategory(FName Category) const;
	bool CanAfford(const TArray<FLRItemAmount>& Cost) const;
	bool CanAffordAnyRecipe() const;
	/** Rails: Entity#inventory_sort_needed? */
	bool IsSortNeeded() const;
	FLRActionStatus GetActionStatus(FName ActionName) const;

	// ---- Tech tree / stats --------------------------------------------------------------
	/** A recipe with no reveal requirements is always unlocked; otherwise once they've been met. */
	bool IsRecipeUnlocked(FName RecipeId) const;
	bool IsActionUnlocked(FName ActionName) const;
	bool IsUnlocked(FName Key) const { return Unlocked.Contains(Key); }
	int32 GetStat(FName Key) const;
	const TMap<FName, int32>& GetStats() const { return Stats; }
	static FName StatKey(const TCHAR* Prefix, FName Id);

	// ---- Commands ---------------------------------------------------------------------
	/**
	 * Rails: User#perform_action. Validates, starts the cooldown, then either executes
	 * immediately (castTime 0) or schedules execution for when the cast finishes.
	 * Failures and immediate completions are also broadcast through OnActionCompleted.
	 */
	FLRActionResult RequestAction(const FLRActionRequest& Request);

	/** Debug/cheat: add items directly. Returns false if they don't fit (nothing is added). */
	bool GiveItem(FName Item, int32 Count);

	/** Attach a modifier to a loot box instance (hook for irradiation). */
	bool AddLootBoxModifier(int32 InstanceId, const FLRLootModifier& Modifier);

	// ---- Loot (public for tests and future tools) --------------------------------------
	/** Rails: LootTable#roll */
	TArray<FLRItemAmount> RollLootTable(const FLRLootTableDef& Table);
	/** Rails: loot_box_modifiers.reduce(config) { |cfg, mod| mod.apply(cfg) } */
	static FLRLootTableDef ApplyModifiers(const FLRLootTableDef& Table, const TArray<FLRLootModifier>& Modifiers);

	// ---- Events (Rails: Action Cable broadcasts) ---------------------------------------
	FOnChanged OnInventoryChanged;
	FOnChanged OnWorldChanged;
	FOnActionCompleted OnActionCompleted;

private:
	/**
	 * Snapshot of mutable state that restores itself unless Commit() is called.
	 * Rails: ActiveRecord::Base.transaction { ... raise ActiveRecord::Rollback }.
	 */
	class FTransaction
	{
	public:
		explicit FTransaction(FLRSimulation& InSim);
		~FTransaction();
		void Commit() { bCommitted = true; }

	private:
		FLRSimulation& Sim;
		TArray<FLRInventorySlot> Inventory;
		TMap<int32, FLRLootBoxInstance> LootBoxes;
		TMap<FIntVector, FLRPlacedEntity> Placed;
		int32 NextInstanceId;
		bool bCommitted = false;
	};

	FLRActionResult Execute(const FLRActionRequest& Request);
	FLRActionResult ExecuteLootAction(const FLRActionDef& Def);
	FLRActionResult ExecuteCraft(const FLRActionRequest& Request);
	FLRActionResult ExecuteUse(const FLRActionRequest& Request);
	FLRActionResult ExecuteDeploy(const FLRActionRequest& Request);
	FLRActionResult ExecuteRecall(const FLRActionRequest& Request);
	FLRActionResult ExecuteSort();
	FLRActionResult ExecuteLoad(const FLRActionRequest& Request);
	FLRActionResult ExecuteUnload(const FLRActionRequest& Request);
	FLRActionResult ExecuteAnnihilate(const FLRActionRequest& Request);
	FName ValidateLoad(const FLRActionRequest& Request) const;

	/** Irradiation: advance every loaded enclosure and apply exposures that completed. */
	void AdvanceIrradiation(double DeltaSeconds);
	/** Apply one exposure of Radiation to Box; returns a log message. */
	FString ApplyExposure(FLRLootBoxInstance& Box, const FLRRadiationDef& Radiation, const FLRItemDef& EnclosureDef);

	/** Cheap pre-checks at request time so impossible requests don't burn a cooldown. */
	FName ValidateRequest(const FLRActionRequest& Request) const;
	void Complete(const FLRActionResult& Result);

	bool CheckRequirement(const FLRRequirement& Requirement) const;
	bool CheckRequirements(const TArray<FLRRequirement>& Requirements) const;

	/** All-or-nothing add, topping up existing stacks first. Rails: Entity#add_inventory */
	bool AddItem(FName Item, int32 Count);
	/** Put one unique item with a known instance id into the first empty slot. */
	bool AddUnique(FName Item, int32 InstanceId);
	/** All-or-nothing remove, from the last slot backwards. Rails: Entity#remove_inventory */
	bool RemoveItem(FName Item, int32 Count);
	int32 FindFirstEmptySlot() const;
	int32 ResolveLootBoxSlot(int32 PreferredSlot) const;
	int32 ResolvePlaceableSlot(int32 PreferredSlot) const;
	int32 AllocateInstanceId() { return NextInstanceId++; }

	/** Rails: Entity#build_sorted_stacks */
	TArray<FLRInventorySlot> BuildSortedInventory() const;

	FString DescribeAmounts(const TArray<FLRItemAmount>& Amounts) const;

public:
	/** "(x, y, z)" */
	static FString DescribeCell(const FIntVector& Cell);
	/** Human-readable effect, e.g. "Carbon amounts x1.25". */
	static FString DescribeModifier(const FLRLootModifier& Modifier, const FLRGameData& InData);
	/** A box in an enclosure stops gaining stacks once revealed or at the enclosure's cap. */
	static bool IsExposureComplete(const FLRLootBoxInstance& Box, const FLRItemDef& EnclosureDef);

private:
	static TArray<FLRItemAmount> MergeAmounts(const TArray<FLRItemAmount>& Amounts);
	static FLRActionResult MakeFailure(FName Action, FName Reason, const FString& Message);

	void AddStat(FName Key, int32 Delta = 1);
	/** Latch every reveal requirement that is now met; returns "unlocked" log messages. */
	TArray<FString> RefreshUnlocks();
	void AnnounceUnlocks(const TArray<FString>& Messages);

	FLRGameData Data;
	TSet<FName> Unlocked;
	TMap<FName, int32> Stats;
	double Now = 0.0;
	FRandomStream Rng;
	int32 NextInstanceId = 1;
	TArray<FLRInventorySlot> Inventory;
	TMap<int32, FLRLootBoxInstance> LootBoxes; // by instance id
	TMap<FIntVector, FLRPlacedEntity> Placed;  // by grid cell
	TMap<FName, FLRActionState> ActionStates;  // by action name
};
