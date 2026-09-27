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
	inline const FName Inject(TEXT("inject"));
	inline const FName Craft(TEXT("craft"));
	inline const FName Use(TEXT("use"));
	inline const FName Deploy(TEXT("deploy"));
	inline const FName Recall(TEXT("recall"));
	inline const FName SortInventory(TEXT("sort_inventory"));

	inline const FName Load(TEXT("load"));
	inline const FName Unload(TEXT("unload"));
	inline const FName Annihilate(TEXT("annihilate"));

	// Item categories (items.json "category")
	inline const FName CategoryMaterial(TEXT("material"));
	inline const FName CategoryLootBox(TEXT("lootbox"));
	inline const FName CategoryPlaceable(TEXT("placeable"));
	inline const FName CategorySource(TEXT("source"));

	// Loot modifier kinds (see FLRLootModifier)
	inline const FName ModifierExtraRolls(TEXT("extra_rolls"));
	inline const FName ModifierItemWeightMult(TEXT("item_weight_mult"));
	inline const FName ModifierItemCountMult(TEXT("item_count_mult"));
	inline const FName ModifierAddEntry(TEXT("add_entry"));
	inline const FName ModifierReveal(TEXT("reveal"));

	// Requirement checks
	inline const FName CheckInventory(TEXT("inventory"));
	inline const FName CheckPlaced(TEXT("placed"));
	inline const FName CheckStat(TEXT("stat"));
	inline const FName CheckUnlocked(TEXT("unlocked"));

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

	/** material | lootbox | placeable */
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

	/** Radiation sources: which radiation (radiation.json id) this item emits. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	FName Radiation;

	/** Irradiation enclosures: highest radiation tier the enclosure can contain. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	int32 MaxRadiationTier = 0;

	/** Irradiation enclosures: most modifier stacks a loot box can pick up inside it. 0 = not an enclosure. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	int32 MaxExposureStacks = 0;

	/** Irradiation enclosures: seconds of exposure per modifier stack. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	float ExposureSeconds = 10.f;

	bool IsLootBox() const { return !LootTable.IsNone(); }
	bool IsPlaceable() const { return Category == LRNames::CategoryPlaceable; }
	bool IsSource() const { return !Radiation.IsNone(); }
	bool IsEnclosure() const { return IsPlaceable() && MaxExposureStacks > 0; }
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
	 * inventory - count items you hold (optionally filtered by Item / Category)
	 * placed    - count entities deployed in the world (optionally filtered by Item / Category)
	 * stat      - a lifetime counter named by Id, e.g. "crafted:loot_box", "opened:loot_box",
	 *             "gained:iron", "exposed:x_rays", "done:inject"
	 * unlocked  - 1 if Id ("recipe:<id>" or "action:<name>") is unlocked, else 0
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	FName Check = LRNames::CheckInventory;

	/** For stat / unlocked checks: the counter or unlock key. */
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

/** A craftable thing. Rails: User#get_craft_choices + <Class>::CRAFTING_COST. */
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
};

/**
 * Alters a loot table before it is rolled. Rails: LootBoxModifier (which only had the
 * no-op base class). Radiation effects (radiation.json "effect") are modifiers, added to a
 * loot box each time an irradiation enclosure completes an exposure.
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
};

/** All definitions, indexed for lookup. Plain C++ (no reflection needed). */
struct LOOTBOXRECURSION_API FLRGameData
{
	TMap<FName, FLRItemDef> Items;
	TArray<FLRRecipeDef> Recipes;       // ordered as authored (UI order)
	TMap<FName, FLRLootTableDef> LootTables;
	TArray<FLRActionDef> Actions;       // ordered as authored (UI order)
	TArray<FLRRadiationDef> Radiation;

	const FLRItemDef* FindItem(FName Id) const { return Items.Find(Id); }
	const FLRLootTableDef* FindLootTable(FName Id) const { return LootTables.Find(Id); }
	const FLRRecipeDef* FindRecipe(FName Id) const;
	const FLRActionDef* FindAction(FName Name) const;
	const FLRRadiationDef* FindRadiation(FName Id) const;

	/** Stack size for an item (1 if unknown, so unknown items never merge). */
	int32 GetStackSize(FName Item) const;
	/** Display name, falling back to the id. */
	FString GetDisplayName(FName Item) const;

	/** Merge one parsed file into this data set. Later files override earlier ids. */
	void AddFrom(const FLRDataFile& File);

	/** Unlock keys used by the tech tree: "recipe:<id>" / "action:<name>". */
	static FName RecipeUnlockKey(FName RecipeId);
	static FName ActionUnlockKey(FName ActionName);

	/** Cross-reference checks (unknown ids, bad ranges...). Empty = valid. */
	TArray<FString> Validate() const;

	/** Load and merge every *.json in Directory. Returns false if any file failed to parse or validate. */
	static bool LoadFromDirectory(const FString& Directory, FLRGameData& OutData, TArray<FString>& OutErrors);

	/** Content/Data in the project (works in editor and packaged builds). */
	static FString GetDefaultDataDirectory();
};
