#pragma once

#include "CoreMinimal.h"
#include "Data/LRGameData.h"
#include "LRSimTypes.generated.h"

/** The feed dial's automatic modes: it follows a mark on the dial as the host's mass changes. */
UENUM(BlueprintType)
enum class ELRInjectorAuto : uint8
{
	/** The dial stays where it's put. */
	Off,
	/** The dial follows the HOLD mark (the evaporation rate), so the host keeps its mass. */
	Hold,
	/** The dial follows the LIMIT mark (the rated limit), so the host grows as fast as it can. */
	Limit
};

/**
 * Runtime (mutable) game state. Rails equivalent: the database rows.
 *
 * These are USTRUCTs so they can be saved with USaveGame and read from Blueprints.
 */

/**
 * One held item: what an irradiator's chamber or source slot contains. (Rails: an
 * InventorySlot + its InventoryItem; there is no player inventory any more.) Empty when Item is
 * NAME_None.
 */
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
 * The matter in one cell of the pocket universe: an amount per material. Cells with no matter
 * aren't stored.
 */
USTRUCT(BlueprintType)
struct LOOTBOXRECURSION_API FLRCellMatter
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	FIntVector Cell = FIntVector::ZeroValue;

	/** One entry per material present, each with a positive count. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	TArray<FLRItemAmount> Amounts;

	int32 Get(FName Item) const
	{
		const FLRItemAmount* Found = Amounts.FindByPredicate([Item](const FLRItemAmount& Amount) { return Amount.Item == Item; });
		return Found ? Found->Count : 0;
	}

	/** Add (or with a negative Delta, remove) matter; entries that reach zero are dropped. */
	void Add(FName Item, int32 Delta)
	{
		FLRItemAmount* Found = Amounts.FindByPredicate([Item](const FLRItemAmount& Amount) { return Amount.Item == Item; });
		if (Found)
		{
			Found->Count += Delta;
		}
		else if (Delta > 0)
		{
			Amounts.Emplace(Item, Delta);
		}
		Amounts.RemoveAll([](const FLRItemAmount& Amount) { return Amount.Count <= 0; });
	}

	int32 Total() const
	{
		int32 Sum = 0;
		for (const FLRItemAmount& Amount : Amounts)
		{
			Sum += Amount.Count;
		}
		return Sum;
	}

	bool IsEmpty() const { return Amounts.IsEmpty(); }
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

	/** Set by X-ray inspection: the contents were rolled early and are now fixed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	bool bRevealed = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	TArray<FLRItemAmount> RevealedContents;
};

/** An entity in a grid cell: a ripple, a cache, or a machine such as an irradiator. Rails: a PlaceableEntity row. */
USTRUCT(BlueprintType)
struct LOOTBOXRECURSION_API FLRPlacedEntity
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	int32 InstanceId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	FName Item;

	/** Grid cell (Q, R, Layer); see FLRHexGrid. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	FIntVector Cell = FIntVector::ZeroValue;

	/** Simulation time it was placed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	double PlacedAt = 0.0;

	/** Irradiators: the loot box being irradiated (empty if none). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	FLRInventorySlot Chamber;

	/** Irradiators: the radiation source (empty if none). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	FLRInventorySlot Source;

	/** Seconds of exposure accumulated toward the next modifier stack. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	double ExposureProgress = 0.0;

	/** Overdensities: how deep the ripple is (1..MaxAmplitude). Each yield rolls this many times. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	int32 Amplitude = 0;

	/** Overdensities: seconds accumulated toward the next yield. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	double YieldProgress = 0.0;

	/** Overdensities: seconds accumulated toward gravity deepening the ripple by one. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LR")
	double GrowthProgress = 0.0;
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

	/** The grid cell the action works on (every action but Feed needs one). Only meaningful when bHasCell is true. */
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
	TArray<FLRCellMatter> Matter;

	UPROPERTY()
	TArray<FLRLootBoxInstance> LootBoxes;

	UPROPERTY()
	TArray<FLRPlacedEntity> Placed;

	UPROPERTY()
	TArray<FLRActionState> Actions;

	/** Tech tree: unlocked keys ("recipe:<id>", "action:<name>"). */
	UPROPERTY()
	TArray<FName> Unlocked;

	/** Lifetime counters ("crafted:loot_box", "gained:iron", ...). */
	UPROPERTY()
	TMap<FName, int32> Stats;

	/** Current epoch id (universe.json). */
	UPROPERTY()
	FName Epoch;

	/** Simulation time the current epoch began. */
	UPROPERTY()
	double EpochStartedAt = 0.0;

	/** Cosmic seconds since the pocket universe's Big Bang. */
	UPROPERTY()
	double CosmicTime = 0.0;

	/** The host black hole's mass, kg. */
	UPROPERTY()
	double HostMass = 0.0;

	/** The feed dial's setting, kg/s. */
	UPROPERTY()
	double InjectorTarget = 0.0;

	/** The injectors' actual flow (kg/s) and its rate of change (kg/s^2). */
	UPROPERTY()
	double InjectorFlow = 0.0;

	UPROPERTY()
	double InjectorChange = 0.0;

	/** The storage ring's charge, kg, and whether feeding is locked out until it's full. */
	UPROPERTY()
	double RingCharge = 0.0;

	UPROPERTY()
	bool bRingRecharging = false;

	/** Whether the feed dial follows the HOLD or LIMIT mark by itself. */
	UPROPERTY()
	ELRInjectorAuto InjectorAuto = ELRInjectorAuto::Off;
};

/**
 * The mass injectors' flow: how much they feed the host per second, and how fast that is
 * changing. The flow follows the feed dial with inertia (see FLRSimulation::StepInjector).
 */
struct FLRInjectorState
{
	/** kg/s */
	double Flow = 0.0;
	/** kg/s^2 */
	double Change = 0.0;
};
