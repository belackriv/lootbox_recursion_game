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

	// Item categories (items.json "category")
	inline const FName CategoryMaterial(TEXT("material"));
	inline const FName CategoryLootBox(TEXT("lootbox"));
	inline const FName CategoryPlaceable(TEXT("placeable"));

	// Requirement checks
	inline const FName CheckInventory(TEXT("inventory"));
	inline const FName CheckPlaced(TEXT("placed"));

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

	bool IsLootBox() const { return !LootTable.IsNone(); }
	bool IsPlaceable() const { return Category == LRNames::CategoryPlaceable; }
	bool IsUnique() const { return StackSize <= 1; }
	FLinearColor GetLinearColor() const;
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

/** Rails: an entry in a player action's requirements / reveal_requirements list. */
USTRUCT(BlueprintType)
struct LOOTBOXRECURSION_API FLRRequirement
{
	GENERATED_BODY()

	/** inventory (count items you hold) | placed (count entities deployed in the world) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	FName Check = LRNames::CheckInventory;

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	TArray<FLRRequirement> RevealRequirements;
};

/** One kind of radiation from the design notes. Data only for now. */
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

	/** Stack size for an item (1 if unknown, so unknown items never merge). */
	int32 GetStackSize(FName Item) const;
	/** Display name, falling back to the id. */
	FString GetDisplayName(FName Item) const;

	/** Merge one parsed file into this data set. Later files override earlier ids. */
	void AddFrom(const FLRDataFile& File);

	/** Cross-reference checks (unknown ids, bad ranges...). Empty = valid. */
	TArray<FString> Validate() const;

	/** Load and merge every *.json in Directory. Returns false if any file failed to parse or validate. */
	static bool LoadFromDirectory(const FString& Directory, FLRGameData& OutData, TArray<FString>& OutErrors);

	/** Content/Data in the project (works in editor and packaged builds). */
	static FString GetDefaultDataDirectory();
};
