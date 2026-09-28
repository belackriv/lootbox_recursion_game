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
 * Everything lives in the pocket universe: there is no player inventory. Cells hold matter
 * (an amount per material) next to at most one entity. Building at a cell pays from the matter
 * in cells within the reach radius, and puts the result in that cell.
 *
 * Time is simulation time: the owner calls Advance(DeltaSeconds) every frame. That makes
 * pausing, fast-forwarding (LRTimeScale) and deterministic tests trivial.
 */
class LOOTBOXRECURSION_API FLRSimulation
{
public:
	/** 2: 3D grid, wood -> carbon. 3: irradiator contents. 4: unlocks + stats. 5: epochs, host, primordial materials. 6: matter in cells, no inventory. 7: irradiator ids renamed (nebula, corona). */
	static constexpr int32 SaveVersion = 7;
	/** Oldest save that still loads (newer fields just start empty). 7 renamed the irradiator ids, so older saves start fresh. */
	static constexpr int32 MinCompatibleSaveVersion = 7;
	/** Simulation seconds the sky takes to fade to a new epoch's plasma opacity. */
	static constexpr double PlasmaFadeSeconds = 6.0;

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
	const TMap<FIntVector, FLRPlacedEntity>& GetPlaced() const { return Placed; }
	const FLRPlacedEntity* FindPlaced(const FIntVector& Cell) const { return Placed.Find(Cell); }
	const FLRLootBoxInstance* FindLootBox(int32 InstanceId) const { return LootBoxes.Find(InstanceId); }
	/** The cache in a cell: a cache entity, or the one in an irradiator's chamber. Null if none. */
	const FLRLootBoxInstance* FindCacheAt(const FIntVector& Cell) const;
	FLRActionStatus GetActionStatus(FName ActionName) const;

	// ---- Matter -----------------------------------------------------------------------
	/** Every cell that holds matter. */
	const TMap<FIntVector, FLRCellMatter>& GetAllMatter() const { return Matter; }
	/** How much of Item a cell holds. */
	int32 GetMatter(const FIntVector& Cell, FName Item) const;
	/** How much of Item the whole pocket universe holds. */
	int32 GetTotalMatter(FName Item) const;
	/** How much of Item is within reach of Cell: in cells at most GetReachRadius() steps away, in its layer. */
	int32 GetMatterInReach(const FIntVector& Cell, FName Item) const;
	bool CanAffordAt(const FIntVector& Cell, const TArray<FLRItemAmount>& Cost) const;
	int32 GetReachRadius() const { return FMath::Max(0, Data.ReachRadius); }

	/**
	 * Why a recipe can't be built at Cell right now (NAME_None if it can): unknown or locked
	 * recipe, the cell can't take the output, or not enough matter within reach.
	 */
	FName ValidateBuild(FName RecipeId, const FIntVector& Cell) const;
	/** A failure reason as a short phrase for the log and tooltips ("not enough matter within reach"). */
	static FString DescribeReason(FName Reason);

	// ---- Tech tree / stats --------------------------------------------------------------
	/** A recipe with no reveal requirements is always unlocked; otherwise once they've been met. */
	bool IsRecipeUnlocked(FName RecipeId) const;
	bool IsActionUnlocked(FName ActionName) const;
	/** Retired actions (retireRequirements met once) are hidden and refused for good. */
	bool IsActionRetired(FName ActionName) const;
	bool IsUnlocked(FName Key) const { return Unlocked.Contains(Key); }
	int32 GetStat(FName Key) const;
	const TMap<FName, int32>& GetStats() const { return Stats; }
	static FName StatKey(const TCHAR* Prefix, FName Id);

	// ---- Cosmos: epochs, the cosmic clock and the host black hole -----------------------
	/** Index into GetData().Epochs (0 when there are no epochs). */
	int32 GetEpochIndex() const { return EpochIndex; }
	/** The current epoch, or null if the data defines none. */
	const FLREpochDef* GetEpoch() const { return Data.Epochs.IsValidIndex(EpochIndex) ? &Data.Epochs[EpochIndex] : nullptr; }
	/** Simulation seconds since the current epoch began. */
	double GetEpochAge() const { return Now - EpochStartedAt; }
	/** Cosmic seconds since the pocket universe's Big Bang. */
	double GetCosmicTime() const { return CosmicTime; }
	/** How opaque the primordial plasma is right now (0..1), fading between epochs. */
	float GetPlasmaOpacity() const;
	/** The host black hole's mass as a fraction of its starting mass (1.0). */
	double GetHostMass() const { return HostMass; }
	/** The host has evaporated: nothing in the pocket universe advances until it's fed. */
	bool IsFrozen() const { return HostMass <= 0.0; }
	/** "10^-36 s", "3 min", "380 thousand years", "13.8 billion years". */
	static FString FormatCosmicTime(double Seconds);

	// ---- Commands ---------------------------------------------------------------------
	/**
	 * Rails: User#perform_action. Validates, starts the cooldown, then either executes
	 * immediately (castTime 0) or schedules execution for when the cast finishes.
	 * Failures and immediate completions are also broadcast through OnActionCompleted.
	 */
	FLRActionResult RequestAction(const FLRActionRequest& Request);

	/** Debug/cheat: add a material to a cell. Returns false for unknown or non-material items. Doesn't count toward stats. */
	bool GiveMatter(const FIntVector& Cell, FName Item, int32 Count);

	/** Attach a modifier to a loot box instance (hook for irradiation). */
	bool AddLootBoxModifier(int32 InstanceId, const FLRLootModifier& Modifier);

	// ---- Loot (public for tests and future tools) --------------------------------------
	/** Rails: LootTable#roll */
	TArray<FLRItemAmount> RollLootTable(const FLRLootTableDef& Table);
	/** Rails: loot_box_modifiers.reduce(config) { |cfg, mod| mod.apply(cfg) } */
	static FLRLootTableDef ApplyModifiers(const FLRLootTableDef& Table, const TArray<FLRLootModifier>& Modifiers);

	// ---- Events (Rails: Action Cable broadcasts) ---------------------------------------
	/** Matter in some cell changed. */
	FOnChanged OnMatterChanged;
	/** Entities, their contents or the cosmos changed. */
	FOnChanged OnWorldChanged;
	FOnActionCompleted OnActionCompleted;

	/** "(q, r, layer)" */
	static FString DescribeCell(const FIntVector& Cell);
	/** Human-readable effect, e.g. "Carbon amounts x1.25". */
	static FString DescribeModifier(const FLRLootModifier& Modifier, const FLRGameData& InData);
	/** A box in an irradiator stops gaining stacks once revealed or at the irradiator's cap. */
	static bool IsExposureComplete(const FLRLootBoxInstance& Box, const FLRItemDef& IrradiatorDef);
	FString DescribeAmounts(const TArray<FLRItemAmount>& Amounts) const;

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
		TMap<FIntVector, FLRCellMatter> Matter;
		TMap<int32, FLRLootBoxInstance> LootBoxes;
		TMap<FIntVector, FLRPlacedEntity> Placed;
		int32 NextInstanceId;
		bool bCommitted = false;
	};

	FLRActionResult Execute(const FLRActionRequest& Request);
	FLRActionResult ExecuteLootAction(const FLRActionRequest& Request, const FLRActionDef& Def);
	FLRActionResult ExecuteCraft(const FLRActionRequest& Request);
	FLRActionResult ExecuteUse(const FLRActionRequest& Request);
	FLRActionResult ExecuteDismantle(const FLRActionRequest& Request);
	/** Seeding actions (Def.Places set, e.g. perturb): create or deepen an overdensity. */
	FLRActionResult ExecuteSeed(const FLRActionRequest& Request, const FLRActionDef& Def);
	FName ValidateSeed(const FLRActionRequest& Request, const FLRActionDef& Def) const;
	FLRActionResult ExecuteFeed(const FLRActionRequest& Request);

	/** Irradiation: advance every loaded irradiator and apply exposures that completed. */
	void AdvanceIrradiation(double DeltaSeconds);
	/** Apply one exposure of Radiation to Box; returns a log message. */
	FString ApplyExposure(FLRLootBoxInstance& Box, const FLRRadiationDef& Radiation, const FLRItemDef& IrradiatorDef);
	/** Host evaporation and the cosmic clock. */
	void AdvanceCosmos(double DeltaSeconds);
	/** Overdensities: yield matter into their own cell and, in later epochs, deepen on their own. */
	void AdvanceStructures(double DeltaSeconds);
	/** Make Index the current epoch (clock, start time). */
	void EnterEpoch(int32 Index);
	/** Broadcast event messages (host warnings) through OnActionCompleted. */
	void AnnounceEvents(FName Event, const TArray<FString>& Messages);

	/** Cheap pre-checks at request time so impossible requests don't burn a cooldown. */
	FName ValidateRequest(const FLRActionRequest& Request) const;
	void Complete(const FLRActionResult& Result);

	bool CheckRequirement(const FLRRequirement& Requirement) const;
	bool CheckRequirements(const TArray<FLRRequirement>& Requirements) const;

	// ---- Matter and entity primitives -----------------------------------------------------
	void AddMatter(const FIntVector& Cell, FName Item, int32 Count);
	/** Take Cost from cells within reach of Cell, the cell itself first, then ring by ring. All or nothing. */
	bool RemoveMatterInReach(const FIntVector& Cell, const TArray<FLRItemAmount>& Cost);
	/** Put rolled or built items into the universe at Cell: materials into its matter, caches as new caches. Returns caches lost for lack of room. */
	int32 Spill(const FIntVector& Cell, const TArray<FLRItemAmount>& Amounts);
	/**
	 * A new, unirradiated cache of Item: into Cell (empty, or an irradiator with an empty chamber),
	 * else the nearest empty cell within reach. Returns false if there was no room.
	 */
	bool PlaceNewCache(const FIntVector& Cell, FName Item);
	/** A fresh loot box instance for a cache item; returns its instance id. */
	int32 CreateLootBox(FName Item);
	/** Return what Item cost to build (its recipe) to Cell's matter; returns what came back. */
	TArray<FLRItemAmount> Refund(const FIntVector& Cell, FName Item);
	int32 AllocateInstanceId() { return NextInstanceId++; }

	static TArray<FLRItemAmount> MergeAmounts(const TArray<FLRItemAmount>& Amounts);
	static FLRActionResult MakeFailure(FName Action, FName Reason, const FString& Message);

	void AddStat(FName Key, int32 Delta = 1);
	/** Latch every reveal / retire requirement and advance epochs that are now met; returns log messages. */
	TArray<FString> RefreshUnlocks();
	void AnnounceUnlocks(const TArray<FString>& Messages);

	FLRGameData Data;
	TSet<FName> Unlocked;
	TMap<FName, int32> Stats;
	double Now = 0.0;
	FRandomStream Rng;
	int32 NextInstanceId = 1;
	TMap<FIntVector, FLRCellMatter> Matter;    // by grid cell; cells with no matter are absent
	TMap<int32, FLRLootBoxInstance> LootBoxes; // by instance id
	TMap<FIntVector, FLRPlacedEntity> Placed;  // by grid cell
	TMap<FName, FLRActionState> ActionStates;  // by action name
	int32 EpochIndex = 0;
	double EpochStartedAt = 0.0;
	double CosmicTime = 0.0;
	double HostMass = 1.0;
};
