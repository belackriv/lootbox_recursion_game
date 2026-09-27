#pragma once

#include "CoreMinimal.h"
#include "Data/LRGameData.h"
#include "LRSimTypes.generated.h"

/**
 * Runtime (mutable) game state. Rails equivalent: the database rows.
 *
 * These are USTRUCTs so they can be saved with USaveGame and read from Blueprints.
 */

/** Rails: InventorySlot + its InventoryItem. An empty slot has Item == NAME_None. */
USTRUCT(BlueprintType)
struct LOOTBOXRECURSION_API FLRInventorySlot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	FName Item;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	int32 Count = 0;

	/**
	 * Non-zero for unique (stackSize 1) items. Links the slot to per-instance state,
	 * e.g. a loot box's modifiers or a placeable entity that keeps its identity across
	 * deploy/recall. Rails: loot_box_id / irradiation_enclosure_id on the inventory item.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	int32 InstanceId = 0;

	bool IsEmpty() const { return Item.IsNone() || Count <= 0; }
	void Clear() { *this = FLRInventorySlot(); }

	bool operator==(const FLRInventorySlot& Other) const
	{
		if (IsEmpty() && Other.IsEmpty())
		{
			return true;
		}
		return Item == Other.Item && Count == Other.Count && InstanceId == Other.InstanceId;
	}
	bool operator!=(const FLRInventorySlot& Other) const { return !(*this == Other); }
};

/**
 * Alters a loot table before it is rolled. Rails: LootBoxModifier (which only had the
 * no-op base class). Irradiating a loot box is the intended source of these - see docs/DESIGN.md.
 *
 * Kind:
 *   extra_rolls      - add Value to rollsMin and rollsMax
 *   item_weight_mult - multiply the weight of Item's entries by Value
 *   item_count_mult  - multiply minCount/maxCount of Item's entries by Value
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
};

/** Rails: a LootBox row (+ its loot_box_modifiers). */
USTRUCT(BlueprintType)
struct LOOTBOXRECURSION_API FLRLootBoxInstance
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	int32 InstanceId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	FName LootTable;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	TArray<FLRLootModifier> Modifiers;
};

/** Rails: a PlaceableEntity row with placed_at / world_coordinate set (now a 3D grid cell). */
USTRUCT(BlueprintType)
struct LOOTBOXRECURSION_API FLRPlacedEntity
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	int32 InstanceId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	FName Item;

	/** Grid cell (X, Y, Z = layer). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	FIntVector Cell = FIntVector::ZeroValue;

	/** Simulation time it was placed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	double PlacedAt = 0.0;
};

/**
 * Everything needed to perform one action. Rails: the action_data hash sent over
 * PlayerActionsChannel ({ slot_number:, cell_coordinate:, class_name: }).
 */
USTRUCT(BlueprintType)
struct LOOTBOXRECURSION_API FLRActionRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LR")
	FName Action;

	/** Craft: recipe id. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LR")
	FName Choice;

	/** Use / Deploy: inventory slot index, or INDEX_NONE to pick the first suitable slot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LR")
	int32 Slot = INDEX_NONE;

	/** Deploy / Recall: grid cell. Only meaningful when bHasCell is true. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LR")
	FIntVector Cell = FIntVector::ZeroValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LR")
	bool bHasCell = false;

	static FLRActionRequest Make(FName InAction)
	{
		FLRActionRequest Request;
		Request.Action = InAction;
		return Request;
	}
};

/** Rails: PlayerActionState (on_cooldown_until) + the queued PerformPlayerActionJob. */
USTRUCT(BlueprintType)
struct LOOTBOXRECURSION_API FLRActionState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	double CooldownUntil = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	bool bCasting = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	double CastStartedAt = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	double CastEndsAt = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	FLRActionRequest PendingRequest;
};

/** Computed view of an action for UI. Rails: PlayerAction#to_jbuilder after #update. */
USTRUCT(BlueprintType)
struct LOOTBOXRECURSION_API FLRActionStatus
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "LR")
	FName Name;

	UPROPERTY(BlueprintReadOnly, Category = "LR")
	FString Label;

	UPROPERTY(BlueprintReadOnly, Category = "LR")
	FString Tooltip;

	UPROPERTY(BlueprintReadOnly, Category = "LR")
	bool bRevealed = false;

	UPROPERTY(BlueprintReadOnly, Category = "LR")
	bool bRequirementsMet = false;

	UPROPERTY(BlueprintReadOnly, Category = "LR")
	bool bOnCooldown = false;

	UPROPERTY(BlueprintReadOnly, Category = "LR")
	float CooldownRemaining = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "LR")
	float Cooldown = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "LR")
	float CastTime = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "LR")
	bool bCasting = false;

	/** 0..1 while casting. */
	UPROPERTY(BlueprintReadOnly, Category = "LR")
	float CastProgress = 0.f;

	/** Revealed, requirements met, not casting and not on cooldown. */
	UPROPERTY(BlueprintReadOnly, Category = "LR")
	bool bEnabled = false;
};

/** Outcome of requesting or executing an action. Rails: the { success:, mutations:, reason: } hashes. */
USTRUCT(BlueprintType)
struct LOOTBOXRECURSION_API FLRActionResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "LR")
	FName Action;

	UPROPERTY(BlueprintReadOnly, Category = "LR")
	bool bSuccess = false;

	/** True when the request was accepted and a cast started; the real result comes later. */
	UPROPERTY(BlueprintReadOnly, Category = "LR")
	bool bStarted = false;

	/** Machine-readable failure reason, e.g. "insufficient_materials". */
	UPROPERTY(BlueprintReadOnly, Category = "LR")
	FName Reason;

	/** Human-readable summary for the log. */
	UPROPERTY(BlueprintReadOnly, Category = "LR")
	FString Message;

	UPROPERTY(BlueprintReadOnly, Category = "LR")
	TArray<FLRItemAmount> Gained;

	UPROPERTY(BlueprintReadOnly, Category = "LR")
	TArray<FLRItemAmount> Spent;
};

/** The whole game state, as saved to disk. */
USTRUCT(BlueprintType)
struct LOOTBOXRECURSION_API FLRSaveData
{
	GENERATED_BODY()

	/** Bump when the layout changes incompatibly (see FLRSimulation::SaveVersion). */
	UPROPERTY()
	int32 Version = 0;

	UPROPERTY()
	double Now = 0.0;

	UPROPERTY()
	int32 RandomSeed = 0;

	UPROPERTY()
	int32 NextInstanceId = 1;

	UPROPERTY()
	TArray<FLRInventorySlot> Inventory;

	UPROPERTY()
	TArray<FLRLootBoxInstance> LootBoxes;

	UPROPERTY()
	TArray<FLRPlacedEntity> Placed;

	UPROPERTY()
	TArray<FLRActionState> Actions;
};
