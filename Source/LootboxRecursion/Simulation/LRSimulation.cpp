#include "Simulation/LRSimulation.h"

namespace
{
	const FName ReasonUnknownAction(TEXT("unknown_action"));
	const FName ReasonNotRevealed(TEXT("not_revealed"));
	const FName ReasonCasting(TEXT("casting"));
	const FName ReasonOnCooldown(TEXT("on_cooldown"));
	const FName ReasonRequirements(TEXT("requirements_not_met"));
	const FName ReasonUnknownRecipe(TEXT("unknown_recipe"));
	const FName ReasonInsufficientMaterials(TEXT("insufficient_materials"));
	const FName ReasonNoSlot(TEXT("no_slot"));
	const FName ReasonNoLootBox(TEXT("no_loot_box"));
	const FName ReasonNoLootTable(TEXT("no_loot_table"));
	const FName ReasonNoInventorySpace(TEXT("insufficient_inventory_space"));
	const FName ReasonNoCoordinate(TEXT("no_coordinate"));
	const FName ReasonOccupied(TEXT("occupied"));
	const FName ReasonNoPlaceable(TEXT("no_placeable_item"));
	const FName ReasonNothingPlaced(TEXT("no_placed_entity"));
	const FName ReasonAlreadySorted(TEXT("already_sorted"));

	const FName ModifierExtraRolls(TEXT("extra_rolls"));
	const FName ModifierItemWeightMult(TEXT("item_weight_mult"));
	const FName ModifierItemCountMult(TEXT("item_count_mult"));

	FString DescribeReason(FName Reason)
	{
		if (Reason == ReasonCasting) { return TEXT("already in progress"); }
		if (Reason == ReasonOnCooldown) { return TEXT("on cooldown"); }
		if (Reason == ReasonRequirements) { return TEXT("requirements not met"); }
		if (Reason == ReasonInsufficientMaterials) { return TEXT("not enough materials"); }
		if (Reason == ReasonNoSlot || Reason == ReasonNoInventorySpace) { return TEXT("inventory is full"); }
		if (Reason == ReasonNoLootBox) { return TEXT("no loot box to open"); }
		if (Reason == ReasonNoCoordinate) { return TEXT("select a world cell first"); }
		if (Reason == ReasonOccupied) { return TEXT("that cell is occupied"); }
		if (Reason == ReasonNoPlaceable) { return TEXT("nothing deployable selected"); }
		if (Reason == ReasonNothingPlaced) { return TEXT("nothing deployed in that cell"); }
		if (Reason == ReasonAlreadySorted) { return TEXT("already sorted"); }
		if (Reason == ReasonUnknownRecipe) { return TEXT("pick something to craft"); }
		return Reason.ToString();
	}
}

// ---------------------------------------------------------------------------------------
// Transaction
// ---------------------------------------------------------------------------------------

FLRSimulation::FTransaction::FTransaction(FLRSimulation& InSim)
	: Sim(InSim)
	, Inventory(InSim.Inventory)
	, LootBoxes(InSim.LootBoxes)
	, Placed(InSim.Placed)
	, NextInstanceId(InSim.NextInstanceId)
{
}

FLRSimulation::FTransaction::~FTransaction()
{
	if (!bCommitted)
	{
		Sim.Inventory = MoveTemp(Inventory);
		Sim.LootBoxes = MoveTemp(LootBoxes);
		Sim.Placed = MoveTemp(Placed);
		Sim.NextInstanceId = NextInstanceId;
	}
}

// ---------------------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------------------

FLRSimulation::FLRSimulation(const FLRGameData& InData, int32 Seed)
	: Data(InData)
{
	Reset(Seed);
}

void FLRSimulation::Reset(int32 Seed)
{
	Now = 0.0;
	Rng.Initialize(Seed);
	NextInstanceId = 1;
	Inventory.Reset();
	Inventory.SetNum(PlayerInventorySlots); // Rails: Entity#ensure_inventory_slots
	LootBoxes.Reset();
	Placed.Reset();
	ActionStates.Reset();

	OnInventoryChanged.Broadcast();
	OnWorldChanged.Broadcast();
}

FLRSaveData FLRSimulation::Save() const
{
	FLRSaveData Out;
	Out.Version = SaveVersion;
	Out.Now = Now;
	Out.RandomSeed = Rng.GetCurrentSeed();
	Out.NextInstanceId = NextInstanceId;
	Out.Inventory = Inventory;
	LootBoxes.GenerateValueArray(Out.LootBoxes);
	Placed.GenerateValueArray(Out.Placed);
	ActionStates.GenerateValueArray(Out.Actions);
	return Out;
}

bool FLRSimulation::Load(const FLRSaveData& SaveData)
{
	if (SaveData.Version != SaveVersion)
	{
		return false;
	}

	Now = SaveData.Now;
	Rng.Initialize(SaveData.RandomSeed);
	NextInstanceId = FMath::Max(1, SaveData.NextInstanceId);

	Inventory = SaveData.Inventory;
	Inventory.SetNum(PlayerInventorySlots);

	LootBoxes.Reset();
	for (const FLRLootBoxInstance& Box : SaveData.LootBoxes)
	{
		LootBoxes.Add(Box.InstanceId, Box);
	}
	Placed.Reset();
	for (const FLRPlacedEntity& Entity : SaveData.Placed)
	{
		Placed.Add(Entity.Coordinate, Entity);
	}
	ActionStates.Reset();
	for (const FLRActionState& State : SaveData.Actions)
	{
		ActionStates.Add(State.Name, State);
	}

	// Never hand out an instance id that is already in use, even if the save is inconsistent.
	for (const FLRInventorySlot& Slot : Inventory)
	{
		NextInstanceId = FMath::Max(NextInstanceId, Slot.InstanceId + 1);
	}
	for (const TPair<int32, FLRLootBoxInstance>& Pair : LootBoxes)
	{
		NextInstanceId = FMath::Max(NextInstanceId, Pair.Key + 1);
	}
	for (const TPair<int32, FLRPlacedEntity>& Pair : Placed)
	{
		NextInstanceId = FMath::Max(NextInstanceId, Pair.Value.InstanceId + 1);
	}

	OnInventoryChanged.Broadcast();
	OnWorldChanged.Broadcast();
	return true;
}

// ---------------------------------------------------------------------------------------
// Clock
// ---------------------------------------------------------------------------------------

void FLRSimulation::Advance(double DeltaSeconds)
{
	if (DeltaSeconds <= 0.0)
	{
		return;
	}
	Now += DeltaSeconds;

	// Rails: Solid Queue running PerformPlayerActionJob once its `wait:` elapsed.
	TArray<FLRActionState*> Due;
	for (TPair<FName, FLRActionState>& Pair : ActionStates)
	{
		if (Pair.Value.bCasting && Pair.Value.CastEndsAt <= Now)
		{
			Due.Add(&Pair.Value);
		}
	}
	Due.Sort([](const FLRActionState& A, const FLRActionState& B) { return A.CastEndsAt < B.CastEndsAt; });

	// Copy requests out first: executing an action never adds to ActionStates, but
	// keeping pointers into the map across calls would be fragile.
	TArray<FLRActionRequest> Requests;
	for (FLRActionState* State : Due)
	{
		State->bCasting = false;
		Requests.Add(State->PendingRequest);
	}
	for (const FLRActionRequest& Request : Requests)
	{
		Complete(Execute(Request));
	}
}

// ---------------------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------------------

int32 FLRSimulation::CountItem(FName Item) const
{
	int32 Total = 0;
	for (const FLRInventorySlot& Slot : Inventory)
	{
		if (!Slot.IsEmpty() && Slot.Item == Item)
		{
			Total += Slot.Count;
		}
	}
	return Total;
}

int32 FLRSimulation::CountCategory(FName Category) const
{
	int32 Total = 0;
	for (const FLRInventorySlot& Slot : Inventory)
	{
		if (Slot.IsEmpty())
		{
			continue;
		}
		const FLRItemDef* Def = Data.FindItem(Slot.Item);
		if (Def && Def->Category == Category)
		{
			Total += Slot.Count;
		}
	}
	return Total;
}

bool FLRSimulation::CanAfford(const TArray<FLRItemAmount>& Cost) const
{
	for (const FLRItemAmount& Amount : Cost)
	{
		if (CountItem(Amount.Item) < Amount.Count)
		{
			return false;
		}
	}
	return true;
}

bool FLRSimulation::CanAffordAnyRecipe() const
{
	for (const FLRRecipeDef& Recipe : Data.Recipes)
	{
		if (CanAfford(Recipe.Cost))
		{
			return true;
		}
	}
	return false;
}

bool FLRSimulation::IsSortNeeded() const
{
	return BuildSortedInventory() != Inventory;
}

bool FLRSimulation::CheckRequirement(const FLRRequirement& Requirement) const
{
	int32 Actual = 0;
	if (Requirement.Check == LRNames::CheckPlaced)
	{
		for (const TPair<int32, FLRPlacedEntity>& Pair : Placed)
		{
			const FLRItemDef* Def = Data.FindItem(Pair.Value.Item);
			const bool bItemMatches = Requirement.Item.IsNone() || Pair.Value.Item == Requirement.Item;
			const bool bCategoryMatches = Requirement.Category.IsNone() || (Def && Def->Category == Requirement.Category);
			if (bItemMatches && bCategoryMatches)
			{
				++Actual;
			}
		}
	}
	else if (!Requirement.Item.IsNone())
	{
		Actual = CountItem(Requirement.Item);
	}
	else if (!Requirement.Category.IsNone())
	{
		Actual = CountCategory(Requirement.Category);
	}
	return FLRRequirement::EvaluateCondition(Actual, Requirement.Condition, Requirement.Value);
}

bool FLRSimulation::CheckRequirements(const TArray<FLRRequirement>& Requirements) const
{
	for (const FLRRequirement& Requirement : Requirements)
	{
		if (!CheckRequirement(Requirement))
		{
			return false;
		}
	}
	return true;
}

FLRActionStatus FLRSimulation::GetActionStatus(FName ActionName) const
{
	FLRActionStatus Status;
	Status.Name = ActionName;

	const FLRActionDef* Def = Data.FindAction(ActionName);
	if (!Def)
	{
		return Status;
	}

	Status.Label = Def->Label;
	Status.Tooltip = Def->Tooltip;
	Status.Cooldown = Def->Cooldown;
	Status.CastTime = Def->CastTime;
	Status.bRevealed = CheckRequirements(Def->RevealRequirements);

	// Rails: PlayerAction#update_disabled special-cased sort_inventory the same way.
	if (ActionName == LRNames::Craft)
	{
		Status.bRequirementsMet = CanAffordAnyRecipe() && CheckRequirements(Def->Requirements);
	}
	else if (ActionName == LRNames::SortInventory)
	{
		Status.bRequirementsMet = IsSortNeeded() && CheckRequirements(Def->Requirements);
	}
	else
	{
		Status.bRequirementsMet = CheckRequirements(Def->Requirements);
	}

	if (const FLRActionState* State = ActionStates.Find(ActionName))
	{
		Status.CooldownRemaining = static_cast<float>(FMath::Max(0.0, State->CooldownUntil - Now));
		Status.bOnCooldown = Status.CooldownRemaining > 0.f;
		Status.bCasting = State->bCasting;
		if (State->bCasting)
		{
			const double Duration = FMath::Max(State->CastEndsAt - State->CastStartedAt, UE_DOUBLE_SMALL_NUMBER);
			Status.CastProgress = static_cast<float>(FMath::Clamp((Now - State->CastStartedAt) / Duration, 0.0, 1.0));
		}
	}

	Status.bEnabled = Status.bRevealed && Status.bRequirementsMet && !Status.bOnCooldown && !Status.bCasting;
	return Status;
}

// ---------------------------------------------------------------------------------------
// Commands
// ---------------------------------------------------------------------------------------

FLRActionResult FLRSimulation::MakeFailure(FName Action, FName Reason, const FString& Message)
{
	FLRActionResult Result;
	Result.Action = Action;
	Result.bSuccess = false;
	Result.Reason = Reason;
	Result.Message = Message;
	return Result;
}

void FLRSimulation::Complete(const FLRActionResult& Result)
{
	if (Result.bSuccess)
	{
		OnInventoryChanged.Broadcast();
		OnWorldChanged.Broadcast();
	}
	OnActionCompleted.Broadcast(Result);
}

FLRActionResult FLRSimulation::RequestAction(const FLRActionRequest& Request)
{
	const FLRActionDef* Def = Data.FindAction(Request.Action);
	if (!Def)
	{
		FLRActionResult Result = MakeFailure(Request.Action, ReasonUnknownAction,
			FString::Printf(TEXT("Unknown action '%s'"), *Request.Action.ToString()));
		Complete(Result);
		return Result;
	}

	const FLRActionStatus Status = GetActionStatus(Request.Action);
	FName Reason;
	if (!Status.bRevealed)             { Reason = ReasonNotRevealed; }
	else if (Status.bCasting)          { Reason = ReasonCasting; }
	else if (Status.bOnCooldown)       { Reason = ReasonOnCooldown; }
	else if (!Status.bRequirementsMet) { Reason = ReasonRequirements; }
	else                               { Reason = ValidateRequest(Request); }

	if (!Reason.IsNone())
	{
		FLRActionResult Result = MakeFailure(Request.Action, Reason,
			FString::Printf(TEXT("Can't %s: %s"), *Def->Label.ToLower(), *DescribeReason(Reason)));
		Complete(Result);
		return Result;
	}

	FLRActionState& State = ActionStates.FindOrAdd(Request.Action);
	State.Name = Request.Action;
	State.CooldownUntil = Now + Def->Cooldown;

	if (Def->CastTime <= 0.f)
	{
		// Rails: PerformPlayerActionJob.perform_now
		FLRActionResult Result = Execute(Request);
		Complete(Result);
		return Result;
	}

	// Rails: PerformPlayerActionJob.set(wait: cast_time).perform_later
	State.bCasting = true;
	State.CastStartedAt = Now;
	State.CastEndsAt = Now + Def->CastTime;
	State.PendingRequest = Request;

	FLRActionResult Result;
	Result.Action = Request.Action;
	Result.bSuccess = true;
	Result.bStarted = true;
	Result.Message = FString::Printf(TEXT("%s..."), *Def->Label);
	return Result;
}

FName FLRSimulation::ValidateRequest(const FLRActionRequest& Request) const
{
	if (Request.Action == LRNames::Craft)
	{
		const FLRRecipeDef* Recipe = Data.FindRecipe(Request.Choice);
		if (!Recipe) { return ReasonUnknownRecipe; }
		if (!CanAfford(Recipe->Cost)) { return ReasonInsufficientMaterials; }
	}
	else if (Request.Action == LRNames::Use)
	{
		if (ResolveLootBoxSlot(Request.Slot) == INDEX_NONE) { return ReasonNoLootBox; }
	}
	else if (Request.Action == LRNames::Deploy)
	{
		if (!Request.bHasCoordinate) { return ReasonNoCoordinate; }
		if (Placed.Contains(Request.Coordinate)) { return ReasonOccupied; }
		if (ResolvePlaceableSlot(Request.Slot) == INDEX_NONE) { return ReasonNoPlaceable; }
	}
	else if (Request.Action == LRNames::Recall)
	{
		if (!Request.bHasCoordinate) { return ReasonNoCoordinate; }
		if (!Placed.Contains(Request.Coordinate)) { return ReasonNothingPlaced; }
	}
	return NAME_None;
}

FLRActionResult FLRSimulation::Execute(const FLRActionRequest& Request)
{
	// Rails: PerformPlayerActionJob -> user.send(action_name, action_data)
	const FLRActionDef* Def = Data.FindAction(Request.Action);
	if (!Def)
	{
		return MakeFailure(Request.Action, ReasonUnknownAction, TEXT("Unknown action"));
	}
	if (Request.Action == LRNames::Craft)         { return ExecuteCraft(Request); }
	if (Request.Action == LRNames::Use)           { return ExecuteUse(Request); }
	if (Request.Action == LRNames::Deploy)        { return ExecuteDeploy(Request); }
	if (Request.Action == LRNames::Recall)        { return ExecuteRecall(Request); }
	if (Request.Action == LRNames::SortInventory) { return ExecuteSort(); }
	if (!Def->LootTable.IsNone())                 { return ExecuteScavenge(*Def); }

	return MakeFailure(Request.Action, ReasonUnknownAction,
		FString::Printf(TEXT("No behaviour implemented for '%s'"), *Request.Action.ToString()));
}

FLRActionResult FLRSimulation::ExecuteScavenge(const FLRActionDef& Def)
{
	// Rails: InventoryItem.scavenge_item - now just a roll on a loot table.
	const FLRLootTableDef* Table = Data.FindLootTable(Def.LootTable);
	if (!Table)
	{
		return MakeFailure(Def.Name, ReasonNoLootTable, TEXT("Scavenge has no loot table"));
	}

	const TArray<FLRItemAmount> Rolled = MergeAmounts(RollLootTable(*Table));

	FTransaction Txn(*this);
	for (const FLRItemAmount& Amount : Rolled)
	{
		if (!AddItem(Amount.Item, Amount.Count))
		{
			return MakeFailure(Def.Name, ReasonNoInventorySpace,
				FString::Printf(TEXT("%s failed: %s"), *Def.Label, *DescribeReason(ReasonNoInventorySpace)));
		}
	}
	Txn.Commit();

	FLRActionResult Result;
	Result.Action = Def.Name;
	Result.bSuccess = true;
	Result.Gained = Rolled;
	Result.Message = FString::Printf(TEXT("%s: %s"), *Def.Label, *DescribeAmounts(Rolled));
	return Result;
}

FLRActionResult FLRSimulation::ExecuteCraft(const FLRActionRequest& Request)
{
	// Rails: Entity.craft_item
	const FLRRecipeDef* Recipe = Data.FindRecipe(Request.Choice);
	if (!Recipe)
	{
		return MakeFailure(Request.Action, ReasonUnknownRecipe, TEXT("Can't craft: pick something to craft"));
	}
	if (!CanAfford(Recipe->Cost))
	{
		return MakeFailure(Request.Action, ReasonInsufficientMaterials,
			FString::Printf(TEXT("Can't craft %s: %s"), *Recipe->Label, *DescribeReason(ReasonInsufficientMaterials)));
	}

	FTransaction Txn(*this);
	for (const FLRItemAmount& Cost : Recipe->Cost)
	{
		if (!RemoveItem(Cost.Item, Cost.Count))
		{
			return MakeFailure(Request.Action, ReasonInsufficientMaterials,
				FString::Printf(TEXT("Can't craft %s: %s"), *Recipe->Label, *DescribeReason(ReasonInsufficientMaterials)));
		}
	}
	const int32 OutputCount = FMath::Max(1, Recipe->OutputCount);
	if (!AddItem(Recipe->Output, OutputCount))
	{
		return MakeFailure(Request.Action, ReasonNoSlot,
			FString::Printf(TEXT("Can't craft %s: %s"), *Recipe->Label, *DescribeReason(ReasonNoSlot)));
	}
	Txn.Commit();

	FLRActionResult Result;
	Result.Action = Request.Action;
	Result.bSuccess = true;
	Result.Spent = Recipe->Cost;
	Result.Gained.Emplace(Recipe->Output, OutputCount);
	Result.Message = FString::Printf(TEXT("Crafted %s"), *DescribeAmounts(Result.Gained));
	return Result;
}

FLRActionResult FLRSimulation::ExecuteUse(const FLRActionRequest& Request)
{
	// Rails: User#use + LootBox#open!
	const int32 SlotIndex = ResolveLootBoxSlot(Request.Slot);
	if (SlotIndex == INDEX_NONE)
	{
		return MakeFailure(Request.Action, ReasonNoLootBox, TEXT("Can't use: no loot box to open"));
	}

	const FLRInventorySlot BoxSlot = Inventory[SlotIndex];
	const FLRItemDef* BoxDef = Data.FindItem(BoxSlot.Item);
	check(BoxDef); // ResolveLootBoxSlot only returns slots with a known loot box item

	FLRLootBoxInstance Instance;
	if (const FLRLootBoxInstance* Found = LootBoxes.Find(BoxSlot.InstanceId))
	{
		Instance = *Found;
	}
	else
	{
		// Rails had repair logic for LootBoxInventoryItems with a nil loot_box_id. Here a
		// missing instance simply falls back to the item's own table with no modifiers.
		Instance.InstanceId = BoxSlot.InstanceId;
		Instance.LootTable = BoxDef->LootTable;
	}

	const FLRLootTableDef* Table = Data.FindLootTable(Instance.LootTable);
	if (!Table)
	{
		Table = Data.FindLootTable(LRNames::DefaultLootTable);
	}
	if (!Table)
	{
		return MakeFailure(Request.Action, ReasonNoLootTable, TEXT("Can't open: loot table missing"));
	}

	const TArray<FLRItemAmount> Rolled = MergeAmounts(RollLootTable(ApplyModifiers(*Table, Instance.Modifiers)));

	FTransaction Txn(*this);

	// Remove the box itself (unique item: one per slot).
	Inventory[SlotIndex].Clear();
	LootBoxes.Remove(BoxSlot.InstanceId);

	for (const FLRItemAmount& Amount : Rolled)
	{
		if (!AddItem(Amount.Item, Amount.Count))
		{
			return MakeFailure(Request.Action, ReasonNoInventorySpace,
				FString::Printf(TEXT("Can't open %s: %s"), *BoxDef->DisplayName, *DescribeReason(ReasonNoInventorySpace)));
		}
	}
	Txn.Commit();

	FLRActionResult Result;
	Result.Action = Request.Action;
	Result.bSuccess = true;
	Result.Spent.Emplace(BoxSlot.Item, 1);
	Result.Gained = Rolled;
	Result.Message = FString::Printf(TEXT("Opened %s: %s"), *BoxDef->DisplayName,
		Rolled.IsEmpty() ? TEXT("nothing!") : *DescribeAmounts(Rolled));
	return Result;
}

FLRActionResult FLRSimulation::ExecuteDeploy(const FLRActionRequest& Request)
{
	// Rails: User#deploy + IrradiationEnclosureInventoryItem#place! + PlaceableEntity#place!
	if (!Request.bHasCoordinate)
	{
		return MakeFailure(Request.Action, ReasonNoCoordinate, TEXT("Can't deploy: select a world cell first"));
	}
	if (Placed.Contains(Request.Coordinate))
	{
		return MakeFailure(Request.Action, ReasonOccupied, TEXT("Can't deploy: that cell is occupied"));
	}
	const int32 SlotIndex = ResolvePlaceableSlot(Request.Slot);
	if (SlotIndex == INDEX_NONE)
	{
		return MakeFailure(Request.Action, ReasonNoPlaceable, TEXT("Can't deploy: nothing deployable selected"));
	}

	const FLRInventorySlot Slot = Inventory[SlotIndex];

	FLRPlacedEntity Entity;
	Entity.InstanceId = Slot.InstanceId != 0 ? Slot.InstanceId : AllocateInstanceId();
	Entity.Item = Slot.Item;
	Entity.Coordinate = Request.Coordinate;
	Entity.PlacedAt = Now;

	Inventory[SlotIndex].Clear(); // placeables are unique: one per slot
	Placed.Add(Entity.Coordinate, Entity);

	FLRActionResult Result;
	Result.Action = Request.Action;
	Result.bSuccess = true;
	Result.Spent.Emplace(Slot.Item, 1);
	Result.Message = FString::Printf(TEXT("Deployed %s at %d"), *Data.GetDisplayName(Slot.Item), Entity.Coordinate);
	return Result;
}

FLRActionResult FLRSimulation::ExecuteRecall(const FLRActionRequest& Request)
{
	// Rails: IrradiationEnclosureInventoryItem.recall! + PlaceableEntity#recall!
	if (!Request.bHasCoordinate)
	{
		return MakeFailure(Request.Action, ReasonNoCoordinate, TEXT("Can't recall: select a world cell first"));
	}
	const FLRPlacedEntity* Found = Placed.Find(Request.Coordinate);
	if (!Found)
	{
		return MakeFailure(Request.Action, ReasonNothingPlaced, TEXT("Can't recall: nothing deployed in that cell"));
	}
	const FLRPlacedEntity Entity = *Found;

	if (!AddUnique(Entity.Item, Entity.InstanceId))
	{
		return MakeFailure(Request.Action, ReasonNoInventorySpace, TEXT("Can't recall: inventory is full"));
	}
	Placed.Remove(Request.Coordinate);

	FLRActionResult Result;
	Result.Action = Request.Action;
	Result.bSuccess = true;
	Result.Gained.Emplace(Entity.Item, 1);
	Result.Message = FString::Printf(TEXT("Recalled %s from %d"), *Data.GetDisplayName(Entity.Item), Entity.Coordinate);
	return Result;
}

FLRActionResult FLRSimulation::ExecuteSort()
{
	// Rails: Entity#sort_and_compress_inventory!
	TArray<FLRInventorySlot> Sorted = BuildSortedInventory();

	TSet<FName> Types;
	int32 SlotsUsed = 0;
	for (const FLRInventorySlot& Slot : Sorted)
	{
		if (!Slot.IsEmpty())
		{
			Types.Add(Slot.Item);
			++SlotsUsed;
		}
	}
	Inventory = MoveTemp(Sorted);

	FLRActionResult Result;
	Result.Action = LRNames::SortInventory;
	Result.bSuccess = true;
	Result.Message = FString::Printf(TEXT("Sorted inventory: %d item types in %d slots"), Types.Num(), SlotsUsed);
	return Result;
}

bool FLRSimulation::GiveItem(FName Item, int32 Count)
{
	if (!Data.FindItem(Item) || Count <= 0)
	{
		return false;
	}
	const bool bAdded = AddItem(Item, Count);
	if (bAdded)
	{
		OnInventoryChanged.Broadcast();
	}
	return bAdded;
}

bool FLRSimulation::AddLootBoxModifier(int32 InstanceId, const FLRLootModifier& Modifier)
{
	FLRLootBoxInstance* Box = LootBoxes.Find(InstanceId);
	if (!Box)
	{
		return false;
	}
	Box->Modifiers.Add(Modifier);
	OnInventoryChanged.Broadcast();
	return true;
}

// ---------------------------------------------------------------------------------------
// Loot
// ---------------------------------------------------------------------------------------

TArray<FLRItemAmount> FLRSimulation::RollLootTable(const FLRLootTableDef& Table)
{
	TArray<FLRItemAmount> Out;

	int32 TotalWeight = 0;
	for (const FLRLootEntry& Entry : Table.Entries)
	{
		TotalWeight += FMath::Max(0, Entry.Weight);
	}
	if (TotalWeight <= 0)
	{
		return Out;
	}

	const int32 MinRolls = FMath::Max(0, FMath::Min(Table.RollsMin, Table.RollsMax));
	const int32 MaxRolls = FMath::Max(0, FMath::Max(Table.RollsMin, Table.RollsMax));
	const int32 RollCount = Rng.RandRange(MinRolls, MaxRolls);

	for (int32 RollIndex = 0; RollIndex < RollCount; ++RollIndex)
	{
		// Cumulative-weight pick, same as LootTable#weighted_pick.
		const int32 Pick = Rng.RandRange(0, TotalWeight - 1);
		int32 Cumulative = 0;
		for (const FLRLootEntry& Entry : Table.Entries)
		{
			Cumulative += FMath::Max(0, Entry.Weight);
			if (Pick < Cumulative)
			{
				const int32 Low = FMath::Min(Entry.MinCount, Entry.MaxCount);
				const int32 High = FMath::Max(Entry.MinCount, Entry.MaxCount);
				const int32 Count = Rng.RandRange(Low, High);
				if (Count > 0)
				{
					Out.Emplace(Entry.Item, Count);
				}
				break;
			}
		}
	}
	return Out;
}

FLRLootTableDef FLRSimulation::ApplyModifiers(const FLRLootTableDef& Table, const TArray<FLRLootModifier>& Modifiers)
{
	FLRLootTableDef Out = Table;
	for (const FLRLootModifier& Modifier : Modifiers)
	{
		if (Modifier.Kind == ModifierExtraRolls)
		{
			const int32 Extra = FMath::RoundToInt(Modifier.Value);
			Out.RollsMin = FMath::Max(0, Out.RollsMin + Extra);
			Out.RollsMax = FMath::Max(Out.RollsMin, Out.RollsMax + Extra);
		}
		else if (Modifier.Kind == ModifierItemWeightMult)
		{
			for (FLRLootEntry& Entry : Out.Entries)
			{
				if (Entry.Item == Modifier.Item)
				{
					Entry.Weight = FMath::Max(0, FMath::RoundToInt(Entry.Weight * Modifier.Value));
				}
			}
		}
		else if (Modifier.Kind == ModifierItemCountMult)
		{
			for (FLRLootEntry& Entry : Out.Entries)
			{
				if (Entry.Item == Modifier.Item)
				{
					Entry.MinCount = FMath::Max(0, FMath::RoundToInt(Entry.MinCount * Modifier.Value));
					Entry.MaxCount = FMath::Max(Entry.MinCount, FMath::RoundToInt(Entry.MaxCount * Modifier.Value));
				}
			}
		}
	}
	return Out;
}

// ---------------------------------------------------------------------------------------
// Inventory primitives
// ---------------------------------------------------------------------------------------

int32 FLRSimulation::FindFirstEmptySlot() const
{
	return Inventory.IndexOfByPredicate([](const FLRInventorySlot& Slot) { return Slot.IsEmpty(); });
}

bool FLRSimulation::AddItem(FName Item, int32 Count)
{
	if (Count <= 0)
	{
		return true;
	}
	const FLRItemDef* Def = Data.FindItem(Item);
	if (!Def)
	{
		return false;
	}

	if (Def->IsUnique())
	{
		// One slot per unit, each with its own instance (Rails: one LootBox row per box).
		TArray<int32> EmptySlots;
		for (int32 Index = 0; Index < Inventory.Num() && EmptySlots.Num() < Count; ++Index)
		{
			if (Inventory[Index].IsEmpty())
			{
				EmptySlots.Add(Index);
			}
		}
		if (EmptySlots.Num() < Count)
		{
			return false;
		}
		for (const int32 Index : EmptySlots)
		{
			const int32 InstanceId = AllocateInstanceId();
			Inventory[Index].Item = Item;
			Inventory[Index].Count = 1;
			Inventory[Index].InstanceId = InstanceId;
			if (Def->IsLootBox())
			{
				FLRLootBoxInstance Box;
				Box.InstanceId = InstanceId;
				Box.LootTable = Def->LootTable;
				LootBoxes.Add(InstanceId, Box);
			}
		}
		return true;
	}

	// Plan first, apply only if everything fits (Rails: build mutations, apply if count == 0).
	const int32 StackSize = FMath::Max(1, Def->StackSize);
	TArray<TPair<int32, int32>> Plan; // slot index, amount to add
	int32 Remaining = Count;

	// Top up existing partial stacks first...
	for (int32 Index = 0; Index < Inventory.Num() && Remaining > 0; ++Index)
	{
		const FLRInventorySlot& Slot = Inventory[Index];
		if (!Slot.IsEmpty() && Slot.Item == Item && Slot.Count < StackSize)
		{
			const int32 Add = FMath::Min(StackSize - Slot.Count, Remaining);
			Plan.Emplace(Index, Add);
			Remaining -= Add;
		}
	}
	// ...then fill empty slots.
	for (int32 Index = 0; Index < Inventory.Num() && Remaining > 0; ++Index)
	{
		if (Inventory[Index].IsEmpty())
		{
			const int32 Add = FMath::Min(StackSize, Remaining);
			Plan.Emplace(Index, Add);
			Remaining -= Add;
		}
	}
	if (Remaining > 0)
	{
		return false;
	}

	for (const TPair<int32, int32>& Step : Plan)
	{
		FLRInventorySlot& Slot = Inventory[Step.Key];
		if (Slot.IsEmpty())
		{
			Slot.Clear();
			Slot.Item = Item;
		}
		Slot.Count += Step.Value;
	}
	return true;
}

bool FLRSimulation::AddUnique(FName Item, int32 InstanceId)
{
	const int32 Index = FindFirstEmptySlot();
	if (Index == INDEX_NONE)
	{
		return false;
	}
	Inventory[Index].Item = Item;
	Inventory[Index].Count = 1;
	Inventory[Index].InstanceId = InstanceId;
	return true;
}

bool FLRSimulation::RemoveItem(FName Item, int32 Count)
{
	if (Count <= 0)
	{
		return true;
	}
	if (CountItem(Item) < Count)
	{
		return false;
	}

	int32 Remaining = Count;
	for (int32 Index = Inventory.Num() - 1; Index >= 0 && Remaining > 0; --Index)
	{
		FLRInventorySlot& Slot = Inventory[Index];
		if (Slot.IsEmpty() || Slot.Item != Item)
		{
			continue;
		}
		const int32 Take = FMath::Min(Slot.Count, Remaining);
		Slot.Count -= Take;
		Remaining -= Take;
		if (Slot.Count <= 0)
		{
			LootBoxes.Remove(Slot.InstanceId);
			Slot.Clear();
		}
	}
	return true;
}

int32 FLRSimulation::ResolveLootBoxSlot(int32 PreferredSlot) const
{
	auto IsLootBoxSlot = [this](const FLRInventorySlot& Slot)
	{
		const FLRItemDef* Def = Slot.IsEmpty() ? nullptr : Data.FindItem(Slot.Item);
		return Def && Def->IsLootBox();
	};
	if (Inventory.IsValidIndex(PreferredSlot) && IsLootBoxSlot(Inventory[PreferredSlot]))
	{
		return PreferredSlot;
	}
	// Rails: fall back to the first unopened loot box.
	return Inventory.IndexOfByPredicate(IsLootBoxSlot);
}

int32 FLRSimulation::ResolvePlaceableSlot(int32 PreferredSlot) const
{
	auto IsPlaceableSlot = [this](const FLRInventorySlot& Slot)
	{
		const FLRItemDef* Def = Slot.IsEmpty() ? nullptr : Data.FindItem(Slot.Item);
		return Def && Def->IsPlaceable();
	};
	if (Inventory.IsValidIndex(PreferredSlot) && IsPlaceableSlot(Inventory[PreferredSlot]))
	{
		return PreferredSlot;
	}
	// Rails: User#deploy falls back to the first slot containing a placeable item.
	return Inventory.IndexOfByPredicate(IsPlaceableSlot);
}

TArray<FLRInventorySlot> FLRSimulation::BuildSortedInventory() const
{
	TMap<FName, int32> StackableTotals;
	TArray<FLRInventorySlot> UniqueSlots;
	TArray<FName> Types;

	for (const FLRInventorySlot& Slot : Inventory)
	{
		if (Slot.IsEmpty())
		{
			continue;
		}
		Types.AddUnique(Slot.Item);
		if (Data.GetStackSize(Slot.Item) <= 1)
		{
			UniqueSlots.Add(Slot);
		}
		else
		{
			StackableTotals.FindOrAdd(Slot.Item) += Slot.Count;
		}
	}

	// Sort by display name, then id (Rails: sort_by { [display_name.downcase, type] }).
	Types.Sort([this](const FName& A, const FName& B)
	{
		const FString NameA = Data.GetDisplayName(A);
		const FString NameB = Data.GetDisplayName(B);
		const int32 Compare = NameA.Compare(NameB, ESearchCase::IgnoreCase);
		return Compare != 0 ? Compare < 0 : A.LexicalLess(B);
	});
	UniqueSlots.Sort([](const FLRInventorySlot& A, const FLRInventorySlot& B) { return A.InstanceId < B.InstanceId; });

	TArray<FLRInventorySlot> Out;
	Out.Reserve(Inventory.Num());
	for (const FName Type : Types)
	{
		if (const int32* Total = StackableTotals.Find(Type))
		{
			const int32 StackSize = Data.GetStackSize(Type);
			int32 Remaining = *Total;
			while (Remaining > 0)
			{
				FLRInventorySlot Stack;
				Stack.Item = Type;
				Stack.Count = FMath::Min(StackSize, Remaining);
				Out.Add(Stack);
				Remaining -= Stack.Count;
			}
		}
		else
		{
			for (const FLRInventorySlot& Slot : UniqueSlots)
			{
				if (Slot.Item == Type)
				{
					Out.Add(Slot);
				}
			}
		}
	}
	// Normally compressing never needs more slots than we started with. It can if a save
	// holds stacks larger than the current stackSize (data changed since) - then leave the
	// inventory as-is rather than dropping items.
	if (Out.Num() > Inventory.Num())
	{
		return Inventory;
	}
	Out.SetNum(Inventory.Num()); // pad with empties
	return Out;
}

// ---------------------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------------------

TArray<FLRItemAmount> FLRSimulation::MergeAmounts(const TArray<FLRItemAmount>& Amounts)
{
	TArray<FLRItemAmount> Out;
	for (const FLRItemAmount& Amount : Amounts)
	{
		if (FLRItemAmount* Existing = Out.FindByPredicate([&Amount](const FLRItemAmount& E) { return E.Item == Amount.Item; }))
		{
			Existing->Count += Amount.Count;
		}
		else
		{
			Out.Add(Amount);
		}
	}
	return Out;
}

FString FLRSimulation::DescribeAmounts(const TArray<FLRItemAmount>& Amounts) const
{
	TArray<FString> Parts;
	for (const FLRItemAmount& Amount : Amounts)
	{
		Parts.Add(FString::Printf(TEXT("+%d %s"), Amount.Count, *Data.GetDisplayName(Amount.Item)));
	}
	return FString::Join(Parts, TEXT(", "));
}
