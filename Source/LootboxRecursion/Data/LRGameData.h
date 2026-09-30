#pragma once

#include "CoreMinimal.h"
#include "LRGameData.generated.h"

/**
 * Static game content ("definitions"), loaded from the JSON files in Content/Data.
 *
 * Rails equivalent: the YAML files in app/data + the constants on the InventoryItem subclasses
 * (STACK_SIZE, DISPLAY_NAME, TOOLTIP, CRAFTING_COST).
 *
 * Every struct is a USTRUCT so that FJsonObjectConverter can fill it from JSON by
 * property name (JSON keys are matched case-insensitively, so "stackSize" -> StackSize),
 * and so Blueprints can read it.
 */

/** Well-known ids used by code. Everything else is purely data-driven. */
namespace LRNames
{
	// Actions (actions.json "name")
	inline const FName Perturb(TEXT("perturb"));
	/** Not an action in actions.json: the outside panel's kick-start (FLRSimulation::Ignite). */
	inline const FName Ignite(TEXT("ignite"));
	inline const FName Vent(TEXT("vent"));
	inline const FName Craft(TEXT("craft"));
	inline const FName Use(TEXT("use"));
	inline const FName Dismantle(TEXT("dismantle"));

	// Item categories (items.json "category")
	inline const FName CategoryMaterial(TEXT("material"));
	inline const FName CategoryLootBox(TEXT("lootbox"));
	inline const FName CategoryPlaceable(TEXT("placeable"));
	inline const FName CategorySource(TEXT("source"));
	/** Part of the pocket universe itself (overdensities): created by actions, never held. */
	inline const FName CategoryStructure(TEXT("structure"));

	// Loot modifier kinds (see FLRLootModifier)
	inline const FName ModifierExtraRolls(TEXT("extra_rolls"));
	inline const FName ModifierItemWeightMult(TEXT("item_weight_mult"));
	inline const FName ModifierItemCountMult(TEXT("item_count_mult"));
	inline const FName ModifierAddEntry(TEXT("add_entry"));
	inline const FName ModifierReveal(TEXT("reveal"));

	// Requirement checks
	inline const FName CheckMatter(TEXT("matter"));
	inline const FName CheckPlaced(TEXT("placed"));
	inline const FName CheckStat(TEXT("stat"));
	inline const FName CheckUnlocked(TEXT("unlocked"));
	inline const FName CheckEpoch(TEXT("epoch"));
	inline const FName CheckHost(TEXT("host"));

	// Loot table used when a loot box's own table is missing (mirrors LootTable.for fallback)
	inline const FName DefaultLootTable(TEXT("default"));
}

/** An item id + quantity pair. Used for costs, loot results, etc. */
USTRUCT(BlueprintType)
struct LOOTBOXRECURSION_API FLRItemAmount
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LR")
	FName Item;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LR")
	int32 Count = 0;

	FLRItemAmount() = default;
	FLRItemAmount(FName InItem, int32 InCount) : Item(InItem), Count(InCount) {}
};

/** One item type. Rails: an InventoryItem STI subclass. */
USTRUCT(BlueprintType)
struct LOOTBOXRECURSION_API FLRItemDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	FString DisplayName;

	/** Short label drawn in inventory slots until there are real icons. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	FString Abbrev;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	FString Tooltip;

	/** material | lootbox | placeable | source | structure */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	FName Category;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	int32 StackSize = 100;

	/** If set, this item can be opened by the Use action and rolls on this table. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	FName LootTable;

	/** Hex colour, e.g. "#E8A020". Used for slot swatches and world meshes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	FString Color;

	/** How solid the item draws in the world, 0 (invisible) to 1 (opaque). Below 1 it draws see-through, so irradiators show their contents. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	float Opacity = 1.f;

	/**
	 * Optional material for this item in the world, e.g. "/Game/Materials/M_Ripple". Used if the
	 * asset exists, instead of the shared entity or see-through hook (see docs/MATERIALS.md).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	FString Material;

	/** Radiation sources: which radiation (radiation.json id) this item emits. Sources are built into an irradiator. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	FName Radiation;

	/** Irradiators: highest radiation tier the irradiator can contain. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	int32 MaxRadiationTier = 0;

	/** Irradiators: most modifier stacks a loot box can pick up inside it. 0 = not an irradiator. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	int32 MaxExposureStacks = 0;

	/** Irradiators: seconds of exposure per modifier stack. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	float ExposureSeconds = 10.f;

	/** Overdensities: the deepest a ripple can get. 0 = not an overdensity. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	int32 MaxAmplitude = 0;

	/** Overdensities: seconds between yields (each yield rolls the epoch's yieldTable). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	float YieldSeconds = 10.f;

	bool IsLootBox() const { return !LootTable.IsNone(); }
	bool IsPlaceable() const { return Category == LRNames::CategoryPlaceable; }
	bool IsSource() const { return !Radiation.IsNone(); }
	bool IsIrradiator() const { return IsPlaceable() && MaxExposureStacks > 0; }
	bool IsStructure() const { return Category == LRNames::CategoryStructure; }
	bool IsOverdensity() const { return IsStructure() && MaxAmplitude > 0; }
	bool IsUnique() const { return StackSize <= 1; }
	FLinearColor GetLinearColor() const;
};

/**
 * One condition. Rails: an entry in a player action's requirements / reveal_requirements list.
 * Used for action requirements, and as reveal (unlock) requirements for actions and recipes,
 * which together form the tech tree.
 */
USTRUCT(BlueprintType)
struct LOOTBOXRECURSION_API FLRRequirement
{
	GENERATED_BODY()

	/**
	 * matter    - matter in the whole pocket universe, of Item (or of every item in Category)
	 * placed    - count entities in the world (optionally filtered by Item / Category)
	 * stat      - a lifetime counter named by Id, e.g. "crafted:loot_box", "opened:loot_box",
	 *             "gained:iron", "exposed:x_rays", "done:inject"
	 * unlocked  - 1 if Id ("recipe:<id>" or "action:<name>") is unlocked, else 0
	 * epoch     - 1 if the epoch named by Id has been reached (or passed), else 0
	 * host      - the host black hole's mass, in whole percent of its starting mass
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	FName Check = LRNames::CheckMatter;

	/** For stat / unlocked / epoch checks: the counter, unlock key or epoch id. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	FName Id;

	/** Optional: only count this item id. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	FName Item;

	/** Optional: only count items in this category. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	FName Category;

	/** gt | gte | lt | lte | eq */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	FName Condition = TEXT("gt");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	int32 Value = 0;

	static bool EvaluateCondition(int32 Actual, FName InCondition, int32 Expected);
	static bool IsValidCondition(FName InCondition);
};

/**
 * A buildable thing. Built at a cell, paid from the matter within reach of it. Materials go into
 * the cell's matter; a cache or machine takes the cell (a cache can also go into an irradiator's
 * empty chamber); a source goes into the irradiator in the cell. Dismantling refunds the cost.
 * Rails: User#get_craft_choices + <Class>::CRAFTING_COST.
 */
USTRUCT(BlueprintType)
struct LOOTBOXRECURSION_API FLRRecipeDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	FString Label;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	FString Tooltip;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	FName Output;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	int32 OutputCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	TArray<FLRItemAmount> Cost;

	/** Tech tree: the recipe is hidden until all of these have been met once (then it stays unlocked). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	TArray<FLRRequirement> RevealRequirements;
};

USTRUCT(BlueprintType)
struct LOOTBOXRECURSION_API FLRLootEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	FName Item;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	int32 Weight = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	int32 MinCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	int32 MaxCount = 1;
};

/** Rails: one entry under loot_tables: in loot_tables.yml. */
USTRUCT(BlueprintType)
struct LOOTBOXRECURSION_API FLRLootTableDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	int32 RollsMin = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	int32 RollsMax = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	TArray<FLRLootEntry> Entries;
};

/** Rails: PlayerAction (the static half - the dynamic half is FLRActionState). */
USTRUCT(BlueprintType)
struct LOOTBOXRECURSION_API FLRActionDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	FString Label;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	FString Tooltip;

	/** Seconds before the action can be requested again (starts when requested). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	float Cooldown = 0.f;

	/** Seconds between request and execution. 0 = executes immediately. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	float CastTime = 0.f;

	/** Actions that yield loot directly (inject) roll on this table. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	FName LootTable;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	TArray<FLRRequirement> Requirements;

	/** Tech tree: the action is hidden until all of these have been met once (then it stays unlocked). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	TArray<FLRRequirement> RevealRequirements;

	/**
	 * Tech tree: once all of these have been met, the action is retired for good (hidden and
	 * refused). Empty = never retires. Perturb retires at recombination.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	TArray<FLRRequirement> RetireRequirements;

	/**
	 * Seeding actions (perturb): the overdensity item created in the selected cell. Acting on a
	 * cell that already holds one deepens it by one amplitude instead.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	FName Places;
};

/**
 * Alters a loot table before it is rolled. Rails: LootBoxModifier (which only had the
 * no-op base class). Radiation effects (radiation.json "effect") are modifiers, added to a
 * loot box each time an irradiator completes an exposure.
 *
 * Kind:
 *   extra_rolls      - add Value to rollsMin and rollsMax
 *   item_weight_mult - multiply the weight of Item's entries by Value
 *   item_count_mult  - multiply minCount/maxCount of Item's entries by Value
 *   add_entry        - add Item to the table with weight Value and count 1 (or boost its weight)
 *   reveal           - no table change; the box's contents are rolled now and locked (X-rays)
 */
USTRUCT(BlueprintType)
struct LOOTBOXRECURSION_API FLRLootModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LR")
	FName Kind;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LR")
	FName Item;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LR")
	float Value = 0.f;

	/** Where it came from, for tooltips (e.g. "gamma_rays"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LR")
	FName Source;

	bool IsNone() const { return Kind.IsNone(); }
	static bool IsValidKind(FName InKind);
	static bool KindNeedsItem(FName InKind);
};

/** One kind of radiation from the design notes, and what it does to a loot box. */
USTRUCT(BlueprintType)
struct LOOTBOXRECURSION_API FLRRadiationDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	FName Id;

	/** electromagnetic | particle | acoustic | gravitational */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	FName Family;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	int32 Tier = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	int32 Energy = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	FString Effects;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	FString Notes;

	/** Hex colour for sources and effects in the world. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	FString Color;

	/** What one exposure stack does to a loot box. Kind None = no effect yet. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	FLRLootModifier Effect;

	FLinearColor GetLinearColor() const;
};

/**
 * One era of the pocket universe's history (universe.json). Epochs run in order: the first is
 * where a new game starts, and each later one begins once its advanceRequirements are met.
 * Times are cosmic seconds since the pocket universe's Big Bang.
 */
USTRUCT(BlueprintType)
struct LOOTBOXRECURSION_API FLREpochDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	FString Name;

	/** Announced in the log when the epoch begins. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	FString Description;

	/** Cosmic time at which the epoch begins. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	double StartTime = 0.0;

	/** Where the cosmic clock stops if the next epoch isn't reached. 0 = the next epoch's startTime. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	double EndTime = 0.0;

	/** Simulation seconds the clock takes to sweep from startTime to the end, on a log scale. 0 = the clock stands still. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	float ClockSeconds = 0.f;

	/** What overdensities yield during this epoch. None = nothing (there is no matter yet). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	FName YieldTable;

	/** Seconds for gravity to deepen a ripple by one amplitude on its own. 0 = only Perturb deepens ripples. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	float RippleGrowthSeconds = 0.f;

	/** How opaque the primordial plasma is (0..1). The sky fades to the new value when the epoch begins. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	float Plasma = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	TArray<FLRRequirement> AdvanceRequirements;
};

/**
 * The host black hole that the pocket universe lives in (universe.json "host"), and the
 * facility's mass injectors that feed it. Masses are in kg and rates in kg/s. See
 * docs/DESIGN.md, "Feeding the host".
 *
 * - It evaporates by Hawking radiation: dM/dt = -K / M^2, so M^3 falls linearly. K is set so a
 *   StartMass host lasts LifetimeSeconds unfed (3,500 t and an hour match real physics).
 * - The injectors feed it at a dial-set rate, with inertia, no faster than the rated limit
 *   (EddingtonRate * mass).
 * - The safeties trip when its 1 g radius reaches the chamber wall (ChamberRadius), dumping
 *   the stored charge, which must rebuild before feeding resumes.
 * - At zero the pocket universe freezes until Ignite fires the stored charge.
 */
USTRUCT(BlueprintType)
struct LOOTBOXRECURSION_API FLRHostDef
{
	GENERATED_BODY()

	/** Mass of a new host, kg. 0 = no host: nothing evaporates and the universe never freezes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	double StartMass = 0.0;

	/** Seconds a StartMass host takes to evaporate if it's never fed. 0 = it never evaporates. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	float LifetimeSeconds = 0.f;

	/** Mass (kg) seeding a new ripple draws from the host. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	double SeedCost = 0.0;

	/** Mass (kg) deepening a ripple draws from the host. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	double PerturbCost = 0.0;

	/** The injectors' rated limit per second, as a fraction of the host's mass (a multiple of the real Eddington limit). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	float EddingtonRate = 0.f;

	/** The top of the feed dial, kg/s. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	double InjectorMaxRate = 0.0;

	/** Seconds the injected flow takes to get 90% of the way to a new dial setting (see FLRSimulation::StepInjector). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	float InjectorResponseSeconds = 15.f;

	/** Distance from the host to the containment chamber's wall, m. The safeties trip when the 1 g radius reaches it. 0 = no cap. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	float ChamberRadius = 0.f;

	/** The pull the chamber wall tolerates, m/s^2 (1 g by default). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	float SafetyGravity = 9.80665f;

	/** The full stored charge, kg: dumped when the safeties trip, fired by Ignite. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	double ChargeCapacity = 0.0;

	/** How fast the stored charge rebuilds, kg/s. Feeding is locked out while it recharges. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	double RechargeRate = 0.0;

	/** The log warns when the host, shrinking, has less than this many seconds left unfed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	float WarningSeconds = 300.f;

	bool IsDefined() const { return StartMass > 0.0; }

	/** K in dM/dt = -K / M^2 (kg^3/s); 0 if the host never evaporates. */
	double GetEvaporationConstant() const
	{
		return (IsDefined() && LifetimeSeconds > 0.f) ? StartMass * StartMass * StartMass / (3.0 * LifetimeSeconds) : 0.0;
	}

	/** Mass lost per second by Hawking radiation at a given mass. */
	double GetEvaporationRate(double Mass) const
	{
		return Mass > 0.0 ? GetEvaporationConstant() / (Mass * Mass) : 0.0;
	}

	/** Seconds a host of this mass lasts unfed (a very large number if it never evaporates). */
	double GetUnfedLifetime(double Mass) const
	{
		const double K = GetEvaporationConstant();
		return K > 0.0 ? Mass * Mass * Mass / (3.0 * K) : TNumericLimits<double>::Max();
	}

	/** The most the injectors can feed per second at a given mass; anything above it is blown back out. */
	double GetRatedLimit(double Mass) const { return static_cast<double>(EddingtonRate) * FMath::Max(Mass, 0.0); }

	/** The rated limit as a multiple of the real Eddington limit (about 6e12 in the shipped data). */
	double GetEddingtonMultiple() const;

	/**
	 * The point of no return: below this mass, evaporation outruns even the most the injectors
	 * can feed (the rated limit, or their maximum), so the host can't be saved (only replaced, by Ignite once it's gone).
	 * 0 if the host never evaporates.
	 */
	double GetTippingMass() const;

	/** The mass at which the safeties trip (its 1 g radius reaches the chamber wall), or 0 for no cap. */
	double GetSafetyCap() const;

	/** Radius inside which a host of this mass pulls harder than SafetyGravity, m. */
	double GetGravityRadius(double Mass) const;

	/** Mass (kg) a perturbation draws: seeding a new ripple, or deepening one. */
	double GetPerturbCost(bool bSeedsNew) const { return bSeedsNew ? SeedCost : PerturbCost; }
};

/**
 * The shape of one JSON file. Each file fills in whichever arrays it has
 * (items.json -> Items, recipes.json -> Recipes, ...); the loader merges them.
 */
USTRUCT()
struct LOOTBOXRECURSION_API FLRDataFile
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FLRItemDef> Items;

	UPROPERTY()
	TArray<FLRRecipeDef> Recipes;

	UPROPERTY()
	TArray<FLRLootTableDef> LootTables;

	UPROPERTY()
	TArray<FLRActionDef> Actions;

	UPROPERTY()
	TArray<FLRRadiationDef> Radiation;

	UPROPERTY()
	TArray<FLREpochDef> Epochs;

	UPROPERTY()
	FLRHostDef Host;

	/** universe.json: how many cells away building can draw matter from. -1 = not set in this file. */
	UPROPERTY()
	int32 ReachRadius = -1;
};

/** All definitions, indexed for lookup. Plain C++ (no reflection needed). */
struct LOOTBOXRECURSION_API FLRGameData
{
	TMap<FName, FLRItemDef> Items;
	TArray<FLRRecipeDef> Recipes;       // ordered as authored (UI order)
	TMap<FName, FLRLootTableDef> LootTables;
	TArray<FLRActionDef> Actions;       // ordered as authored (UI order)
	TArray<FLRRadiationDef> Radiation;
	TArray<FLREpochDef> Epochs;         // in cosmic order
	FLRHostDef Host;
	/** Building at a cell pays from matter in cells up to this many steps away (0 = the cell itself). */
	int32 ReachRadius = 0;

	const FLRItemDef* FindItem(FName Id) const { return Items.Find(Id); }
	const FLRLootTableDef* FindLootTable(FName Id) const { return LootTables.Find(Id); }
	const FLRRecipeDef* FindRecipe(FName Id) const;
	const FLRActionDef* FindAction(FName Name) const;
	const FLRRadiationDef* FindRadiation(FName Id) const;
	/** The first recipe that builds Item (what dismantling it refunds), or null. */
	const FLRRecipeDef* FindRecipeFor(FName Item) const;
	/** Position of an epoch in Epochs, or INDEX_NONE. */
	int32 FindEpochIndex(FName Id) const;

	/** Stack size for an item (1 if unknown, so unknown items never merge). */
	int32 GetStackSize(FName Item) const;
	/** Display name, falling back to the id. */
	FString GetDisplayName(FName Item) const;

	/** Merge one parsed file into this data set. Later files override earlier ids. */
	void AddFrom(const FLRDataFile& File);

	/** Unlock keys used by the tech tree: "recipe:<id>" / "action:<name>". */
	static FName RecipeUnlockKey(FName RecipeId);
	static FName ActionUnlockKey(FName ActionName);
	/** Latch key for a retired action: "retired:<name>". */
	static FName ActionRetiredKey(FName ActionName);

	/** Cross-reference checks (unknown ids, bad ranges...). Empty = valid. */
	TArray<FString> Validate() const;

	/** Load and merge every *.json in Directory. Returns false if any file failed to parse or validate. */
	static bool LoadFromDirectory(const FString& Directory, FLRGameData& OutData, TArray<FString>& OutErrors);

	/** Content/Data in the project (works in editor and packaged builds). */
	static FString GetDefaultDataDirectory();
};
