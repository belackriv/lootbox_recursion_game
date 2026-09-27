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
	const FName ReasonNoCell(TEXT("no_cell"));
	const FName ReasonOccupied(TEXT("occupied"));
	const FName ReasonNoPlaceable(TEXT("no_placeable_item"));
	const FName ReasonNothingPlaced(TEXT("no_placed_entity"));
	const FName ReasonAlreadySorted(TEXT("already_sorted"));
	const FName ReasonNoEnclosure(TEXT("no_enclosure"));
	const FName ReasonNothingToLoad(TEXT("nothing_to_load"));
	const FName ReasonChamberFull(TEXT("chamber_full"));
	const FName ReasonSourceFull(TEXT("source_full"));
	const FName ReasonTooStrong(TEXT("radiation_too_strong"));
	const FName ReasonEnclosureEmpty(TEXT("enclosure_empty"));

	const FName ReasonRecipeLocked(TEXT("recipe_locked"));
	const FName ReasonNothingSelected(TEXT("nothing_selected"));

	const FName IrradiateEvent(TEXT("irradiate"));
	const FName UnlockEvent(TEXT("unlock"));

	FString DescribeReason(FName Reason)
	{
		if (Reason == ReasonCasting) { return TEXT("already in progress"); }
		if (Reason == ReasonOnCooldown) { return TEXT("on cooldown"); }
		if (Reason == ReasonRequirements) { return TEXT("requirements not met"); }
		if (Reason == ReasonInsufficientMaterials) { return TEXT("not enough materials"); }
		if (Reason == ReasonNoSlot || Reason == ReasonNoInventorySpace) { return TEXT("inventory is full"); }
		if (Reason == ReasonNoLootBox) { return TEXT("nothing to open"); }
		if (Reason == ReasonNoCell) { return TEXT("select a grid cell first"); }
		if (Reason == ReasonOccupied) { return TEXT("that cell is occupied"); }
		if (Reason == ReasonNoPlaceable) { return TEXT("nothing deployable selected"); }
		if (Reason == ReasonNothingPlaced) { return TEXT("nothing deployed in that cell"); }
		if (Reason == ReasonAlreadySorted) { return TEXT("already sorted"); }
		if (Reason == ReasonUnknownRecipe) { return TEXT("pick something to craft"); }
		if (Reason == ReasonRecipeLocked) { return TEXT("that recipe isn't unlocked yet"); }
		if (Reason == ReasonNothingSelected) { return TEXT("select an inventory slot first"); }
		if (Reason == ReasonNoEnclosure) { return TEXT("select a cell with an irradiation enclosure"); }
		if (Reason == ReasonNothingToLoad) { return TEXT("select a cache or radiation source in the inventory"); }
		if (Reason == ReasonChamberFull) { return TEXT("the enclosure already holds a cache"); }
		if (Reason == ReasonSourceFull) { return TEXT("the enclosure already holds a source"); }
		if (Reason == ReasonTooStrong) { return TEXT("this enclosure can't contain radiation that strong"); }
		if (Reason == ReasonEnclosureEmpty) { return TEXT("the enclosure is empty"); }
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
	Unlocked.Reset();
	Stats.Reset();
	RefreshUnlocks(); // starting unlocks, not announced

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
	Out.Unlocked = Unlocked.Array();
	Out.Stats = Stats;
	return Out;
}

bool FLRSimulation::Load(const FLRSaveData& SaveData)
{
	if (SaveData.Version < MinCompatibleSaveVersion || SaveData.Version > SaveVersion)
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
		Placed.Add(Entity.Cell, Entity);
	}
	ActionStates.Reset();
	for (const FLRActionState& State : SaveData.Actions)
	{
		ActionStates.Add(State.Name, State);
	}

	Unlocked.Reset();
	Unlocked.Append(SaveData.Unlocked);
	Stats = SaveData.Stats;
	RefreshUnlocks(); // catch up with data changes, not announced

	// Never hand out an instance id that is already in use, even if the save is inconsistent.
	for (const FLRInventorySlot& Slot : Inventory)
	{
		NextInstanceId = FMath::Max(NextInstanceId, Slot.InstanceId + 1);
	}
	for (const TPair<int32, FLRLootBoxInstance>& Pair : LootBoxes)
	{
		NextInstanceId = FMath::Max(NextInstanceId, Pair.Key + 1);
	}
	for (const TPair<FIntVector, FLRPlacedEntity>& Pair : Placed)
	{
		NextInstanceId = FMath::Max(NextInstanceId, Pair.Value.InstanceId + 1);
		NextInstanceId = FMath::Max(NextInstanceId, Pair.Value.Chamber.InstanceId + 1);
		NextInstanceId = FMath::Max(NextInstanceId, Pair.Value.Source.InstanceId + 1);
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

	AdvanceIrradiation(DeltaSeconds);
}

void FLRSimulation::AdvanceIrradiation(double DeltaSeconds)
{
	TArray<FString> Messages;
	for (TPair<FIntVector, FLRPlacedEntity>& Pair : Placed)
	{
		FLRPlacedEntity& Entity = Pair.Value;
		const FLRItemDef* EnclosureDef = Data.FindItem(Entity.Item);
		if (!EnclosureDef || !EnclosureDef->IsEnclosure() || Entity.Chamber.IsEmpty() || Entity.Source.IsEmpty())
		{
			continue;
		}
		FLRLootBoxInstance* Box = LootBoxes.Find(Entity.Chamber.InstanceId);
		const FLRItemDef* SourceDef = Data.FindItem(Entity.Source.Item);
		const FLRRadiationDef* Radiation = SourceDef ? Data.FindRadiation(SourceDef->Radiation) : nullptr;
		if (!Box || !Radiation || Radiation->Effect.IsNone() || IsExposureComplete(*Box, *EnclosureDef))
		{
			Entity.ExposureProgress = 0.0;
			continue;
		}

		Entity.ExposureProgress += DeltaSeconds;
		const double Interval = FMath::Max(0.1, static_cast<double>(EnclosureDef->ExposureSeconds));
		while (Entity.ExposureProgress >= Interval && !IsExposureComplete(*Box, *EnclosureDef))
		{
			Entity.ExposureProgress -= Interval;
			Messages.Add(FString::Printf(TEXT("%s at %s: %s"),
				*Data.GetDisplayName(Entity.Chamber.Item), *DescribeCell(Entity.Cell), *ApplyExposure(*Box, *Radiation, *EnclosureDef)));
		}
		if (IsExposureComplete(*Box, *EnclosureDef))
		{
			Entity.ExposureProgress = 0.0;
		}
	}

	if (Messages.IsEmpty())
	{
		return;
	}
	OnInventoryChanged.Broadcast();
	OnWorldChanged.Broadcast();
	for (const FString& Message : Messages)
	{
		FLRActionResult Result;
		Result.Action = IrradiateEvent;
		Result.bSuccess = true;
		Result.Message = Message;
		OnActionCompleted.Broadcast(Result);
	}
	AnnounceUnlocks(RefreshUnlocks());
}

bool FLRSimulation::IsExposureComplete(const FLRLootBoxInstance& Box, const FLRItemDef& EnclosureDef)
{
	return Box.bRevealed || Box.Modifiers.Num() >= EnclosureDef.MaxExposureStacks;
}

FString FLRSimulation::ApplyExposure(FLRLootBoxInstance& Box, const FLRRadiationDef& Radiation, const FLRItemDef& EnclosureDef)
{
	if (Radiation.Effect.Kind == LRNames::ModifierReveal)
	{
		// X-ray inspection: roll the (modified) table now and lock the result in.
		const FLRLootTableDef* Table = Data.FindLootTable(Box.LootTable);
		if (!Table)
		{
			Table = Data.FindLootTable(LRNames::DefaultLootTable);
		}
		Box.RevealedContents = Table ? MergeAmounts(RollLootTable(ApplyModifiers(*Table, Box.Modifiers))) : TArray<FLRItemAmount>();
		Box.bRevealed = true;
		AddStat(StatKey(TEXT("exposed"), Radiation.Id));
		return FString::Printf(TEXT("%s observed it and collapsed its contents: %s (fixed)"), *Radiation.Name,
			Box.RevealedContents.IsEmpty() ? TEXT("nothing!") : *DescribeAmounts(Box.RevealedContents));
	}

	AddStat(StatKey(TEXT("exposed"), Radiation.Id));
	FLRLootModifier Modifier = Radiation.Effect;
	Modifier.Source = Radiation.Id;
	Box.Modifiers.Add(Modifier);
	return FString::Printf(TEXT("absorbed %s, %s (%d/%d)"), *Radiation.Name, *DescribeModifier(Modifier, Data),
		Box.Modifiers.Num(), EnclosureDef.MaxExposureStacks);
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
		if (IsRecipeUnlocked(Recipe.Id) && CanAfford(Recipe.Cost))
		{
			return true;
		}
	}
	return false;
}

// ---------------------------------------------------------------------------------------
// Tech tree / stats
// ---------------------------------------------------------------------------------------

FName FLRSimulation::StatKey(const TCHAR* Prefix, FName Id)
{
	return FName(*FString::Printf(TEXT("%s:%s"), Prefix, *Id.ToString()));
}

int32 FLRSimulation::GetStat(FName Key) const
{
	const int32* Value = Stats.Find(Key);
	return Value ? *Value : 0;
}

void FLRSimulation::AddStat(FName Key, int32 Delta)
{
	Stats.FindOrAdd(Key) += Delta;
}

bool FLRSimulation::IsRecipeUnlocked(FName RecipeId) const
{
	const FLRRecipeDef* Recipe = Data.FindRecipe(RecipeId);
	return Recipe && (Recipe->RevealRequirements.IsEmpty() || Unlocked.Contains(FLRGameData::RecipeUnlockKey(RecipeId)));
}

bool FLRSimulation::IsActionUnlocked(FName ActionName) const
{
	const FLRActionDef* Action = Data.FindAction(ActionName);
	return Action && (Action->RevealRequirements.IsEmpty() || Unlocked.Contains(FLRGameData::ActionUnlockKey(ActionName)));
}

TArray<FString> FLRSimulation::RefreshUnlocks()
{
	// Unlocks latch: once met, they stay. Loop so chains ("unlocked" requirements) resolve
	// in one pass no matter how the data is ordered.
	TArray<FString> Messages;
	bool bChanged = true;
	while (bChanged)
	{
		bChanged = false;
		for (const FLRRecipeDef& Recipe : Data.Recipes)
		{
			const FName Key = FLRGameData::RecipeUnlockKey(Recipe.Id);
			if (!Recipe.RevealRequirements.IsEmpty() && !Unlocked.Contains(Key) && CheckRequirements(Recipe.RevealRequirements))
			{
				Unlocked.Add(Key);
				Messages.Add(FString::Printf(TEXT("New recipe unlocked: %s"), *Recipe.Label));
				bChanged = true;
			}
		}
		for (const FLRActionDef& Action : Data.Actions)
		{
			const FName Key = FLRGameData::ActionUnlockKey(Action.Name);
			if (!Action.RevealRequirements.IsEmpty() && !Unlocked.Contains(Key) && CheckRequirements(Action.RevealRequirements))
			{
				Unlocked.Add(Key);
				Messages.Add(FString::Printf(TEXT("New action unlocked: %s"), *Action.Label));
				bChanged = true;
			}
		}
	}
	return Messages;
}

void FLRSimulation::AnnounceUnlocks(const TArray<FString>& Messages)
{
	for (const FString& Message : Messages)
	{
		FLRActionResult Result;
		Result.Action = UnlockEvent;
		Result.bSuccess = true;
		Result.Message = Message;
		OnActionCompleted.Broadcast(Result);
	}
}

bool FLRSimulation::IsSortNeeded() const
{
	return BuildSortedInventory() != Inventory;
}

bool FLRSimulation::CheckRequirement(const FLRRequirement& Requirement) const
{
	int32 Actual = 0;
	if (Requirement.Check == LRNames::CheckStat)
	{
		Actual = GetStat(Requirement.Id);
	}
	else if (Requirement.Check == LRNames::CheckUnlocked)
	{
		Actual = Unlocked.Contains(Requirement.Id) ? 1 : 0;
	}
	else if (Requirement.Check == LRNames::CheckPlaced)
	{
		for (const TPair<FIntVector, FLRPlacedEntity>& Pair : Placed)
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
	Status.bRevealed = IsActionUnlocked(ActionName); // latched tech-tree unlock

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
	TArray<FString> NewUnlocks;
	if (Result.bSuccess && !Result.bStarted)
	{
		AddStat(StatKey(TEXT("done"), Result.Action));
		for (const FLRItemAmount& Amount : Result.Gained)
		{
			AddStat(StatKey(TEXT("gained"), Amount.Item), Amount.Count);
		}
		NewUnlocks = RefreshUnlocks();
	}
	if (Result.bSuccess)
	{
		OnInventoryChanged.Broadcast();
		OnWorldChanged.Broadcast();
	}
	OnActionCompleted.Broadcast(Result);
	AnnounceUnlocks(NewUnlocks);
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
		if (!IsRecipeUnlocked(Recipe->Id)) { return ReasonRecipeLocked; }
		if (!CanAfford(Recipe->Cost)) { return ReasonInsufficientMaterials; }
	}
	else if (Request.Action == LRNames::Use)
	{
		if (ResolveLootBoxSlot(Request.Slot) == INDEX_NONE) { return ReasonNoLootBox; }
	}
	else if (Request.Action == LRNames::Deploy)
	{
		if (!Request.bHasCell) { return ReasonNoCell; }
		if (Placed.Contains(Request.Cell)) { return ReasonOccupied; }
		if (ResolvePlaceableSlot(Request.Slot) == INDEX_NONE) { return ReasonNoPlaceable; }
	}
	else if (Request.Action == LRNames::Recall)
	{
		if (!Request.bHasCell) { return ReasonNoCell; }
		if (!Placed.Contains(Request.Cell)) { return ReasonNothingPlaced; }
	}
	else if (Request.Action == LRNames::Load)
	{
		return ValidateLoad(Request);
	}
	else if (Request.Action == LRNames::Annihilate)
	{
		if (!Inventory.IsValidIndex(Request.Slot) || Inventory[Request.Slot].IsEmpty()) { return ReasonNothingSelected; }
	}
	else if (Request.Action == LRNames::Unload)
	{
		if (!Request.bHasCell) { return ReasonNoCell; }
		const FLRPlacedEntity* Entity = Placed.Find(Request.Cell);
		const FLRItemDef* Def = Entity ? Data.FindItem(Entity->Item) : nullptr;
		if (!Def || !Def->IsEnclosure()) { return ReasonNoEnclosure; }
		if (Entity->Chamber.IsEmpty() && Entity->Source.IsEmpty()) { return ReasonEnclosureEmpty; }
	}
	return NAME_None;
}

FName FLRSimulation::ValidateLoad(const FLRActionRequest& Request) const
{
	if (!Request.bHasCell) { return ReasonNoCell; }
	const FLRPlacedEntity* Entity = Placed.Find(Request.Cell);
	const FLRItemDef* EnclosureDef = Entity ? Data.FindItem(Entity->Item) : nullptr;
	if (!EnclosureDef || !EnclosureDef->IsEnclosure()) { return ReasonNoEnclosure; }

	const FLRInventorySlot* Slot = Inventory.IsValidIndex(Request.Slot) ? &Inventory[Request.Slot] : nullptr;
	const FLRItemDef* ItemDef = (Slot && !Slot->IsEmpty()) ? Data.FindItem(Slot->Item) : nullptr;
	if (!ItemDef || (!ItemDef->IsLootBox() && !ItemDef->IsSource())) { return ReasonNothingToLoad; }

	if (ItemDef->IsLootBox())
	{
		return Entity->Chamber.IsEmpty() ? NAME_None : ReasonChamberFull;
	}
	if (!Entity->Source.IsEmpty()) { return ReasonSourceFull; }
	const FLRRadiationDef* Radiation = Data.FindRadiation(ItemDef->Radiation);
	if (!Radiation || Radiation->Tier > EnclosureDef->MaxRadiationTier) { return ReasonTooStrong; }
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
	if (Request.Action == LRNames::Load)          { return ExecuteLoad(Request); }
	if (Request.Action == LRNames::Unload)        { return ExecuteUnload(Request); }
	if (Request.Action == LRNames::Annihilate)    { return ExecuteAnnihilate(Request); }
	if (Request.Action == LRNames::SortInventory) { return ExecuteSort(); }
	if (!Def->LootTable.IsNone())                 { return ExecuteLootAction(*Def); }

	return MakeFailure(Request.Action, ReasonUnknownAction,
		FString::Printf(TEXT("No behaviour implemented for '%s'"), *Request.Action.ToString()));
}

FLRActionResult FLRSimulation::ExecuteLootAction(const FLRActionDef& Def)
{
	// Rails: InventoryItem.scavenge_item - now just a roll on a loot table (e.g. "inject").
	const FLRLootTableDef* Table = Data.FindLootTable(Def.LootTable);
	if (!Table)
	{
		return MakeFailure(Def.Name, ReasonNoLootTable, FString::Printf(TEXT("%s has no loot table"), *Def.Label));
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
	if (!IsRecipeUnlocked(Recipe->Id))
	{
		return MakeFailure(Request.Action, ReasonRecipeLocked, FString::Printf(TEXT("Can't craft %s: %s"), *Recipe->Label, *DescribeReason(ReasonRecipeLocked)));
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
	AddStat(StatKey(TEXT("crafted"), Recipe->Id));
	return Result;
}

FLRActionResult FLRSimulation::ExecuteUse(const FLRActionRequest& Request)
{
	// Rails: User#use + LootBox#open!
	const int32 SlotIndex = ResolveLootBoxSlot(Request.Slot);
	if (SlotIndex == INDEX_NONE)
	{
		return MakeFailure(Request.Action, ReasonNoLootBox, TEXT("Can't use: nothing to open"));
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

	// X-rayed boxes open to exactly what was revealed.
	const TArray<FLRItemAmount> Rolled = Instance.bRevealed
		? Instance.RevealedContents
		: MergeAmounts(RollLootTable(ApplyModifiers(*Table, Instance.Modifiers)));

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
	AddStat(StatKey(TEXT("opened"), BoxSlot.Item));
	Result.Message = FString::Printf(TEXT("Opened %s: %s"), *BoxDef->DisplayName,
		Rolled.IsEmpty() ? TEXT("nothing!") : *DescribeAmounts(Rolled));
	return Result;
}

FLRActionResult FLRSimulation::ExecuteDeploy(const FLRActionRequest& Request)
{
	// Rails: User#deploy + IrradiationEnclosureInventoryItem#place! + PlaceableEntity#place!
	if (!Request.bHasCell)
	{
		return MakeFailure(Request.Action, ReasonNoCell, TEXT("Can't deploy: select a grid cell first"));
	}
	if (Placed.Contains(Request.Cell))
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
	Entity.Cell = Request.Cell;
	Entity.PlacedAt = Now;

	Inventory[SlotIndex].Clear(); // placeables are unique: one per slot
	Placed.Add(Entity.Cell, Entity);

	FLRActionResult Result;
	Result.Action = Request.Action;
	Result.bSuccess = true;
	Result.Spent.Emplace(Slot.Item, 1);
	Result.Message = FString::Printf(TEXT("Deployed %s at %s"), *Data.GetDisplayName(Slot.Item), *DescribeCell(Entity.Cell));
	return Result;
}

FLRActionResult FLRSimulation::ExecuteRecall(const FLRActionRequest& Request)
{
	// Rails: IrradiationEnclosureInventoryItem.recall! + PlaceableEntity#recall!
	if (!Request.bHasCell)
	{
		return MakeFailure(Request.Action, ReasonNoCell, TEXT("Can't recall: select a grid cell first"));
	}
	const FLRPlacedEntity* Found = Placed.Find(Request.Cell);
	if (!Found)
	{
		return MakeFailure(Request.Action, ReasonNothingPlaced, TEXT("Can't recall: nothing deployed in that cell"));
	}
	const FLRPlacedEntity Entity = *Found;

	// The entity comes back with whatever it was holding, all or nothing.
	FLRInventorySlot EntityItem;
	EntityItem.Item = Entity.Item;
	EntityItem.Count = 1;
	EntityItem.InstanceId = Entity.InstanceId;

	FTransaction Txn(*this);
	FLRActionResult Result;
	for (const FLRInventorySlot& Returned : { EntityItem, Entity.Chamber, Entity.Source })
	{
		if (Returned.IsEmpty())
		{
			continue;
		}
		if (!AddUnique(Returned.Item, Returned.InstanceId))
		{
			return MakeFailure(Request.Action, ReasonNoInventorySpace, TEXT("Can't recall: inventory is full"));
		}
		Result.Gained.Emplace(Returned.Item, 1);
	}
	Placed.Remove(Request.Cell);
	Txn.Commit();

	Result.Action = Request.Action;
	Result.bSuccess = true;
	Result.Message = FString::Printf(TEXT("Recalled %s from %s"), *Data.GetDisplayName(Entity.Item), *DescribeCell(Entity.Cell));
	return Result;
}

FLRActionResult FLRSimulation::ExecuteLoad(const FLRActionRequest& Request)
{
	const FName Invalid = ValidateLoad(Request);
	if (!Invalid.IsNone())
	{
		return MakeFailure(Request.Action, Invalid, FString::Printf(TEXT("Can't load: %s"), *DescribeReason(Invalid)));
	}

	FLRPlacedEntity& Entity = Placed.FindChecked(Request.Cell);
	const FLRInventorySlot Slot = Inventory[Request.Slot];
	const FLRItemDef* ItemDef = Data.FindItem(Slot.Item);
	if (ItemDef && ItemDef->IsLootBox())
	{
		Entity.Chamber = Slot;
		Entity.ExposureProgress = 0.0;
	}
	else
	{
		Entity.Source = Slot;
	}
	Inventory[Request.Slot].Clear();

	FLRActionResult Result;
	Result.Action = Request.Action;
	Result.bSuccess = true;
	Result.Spent.Emplace(Slot.Item, 1);
	Result.Message = FString::Printf(TEXT("Loaded %s into the %s at %s"), *Data.GetDisplayName(Slot.Item),
		*Data.GetDisplayName(Entity.Item), *DescribeCell(Entity.Cell));
	return Result;
}

FLRActionResult FLRSimulation::ExecuteUnload(const FLRActionRequest& Request)
{
	const FName Invalid = ValidateRequest(Request);
	if (!Invalid.IsNone())
	{
		return MakeFailure(Request.Action, Invalid, FString::Printf(TEXT("Can't unload: %s"), *DescribeReason(Invalid)));
	}

	FLRPlacedEntity& Entity = Placed.FindChecked(Request.Cell);
	FTransaction Txn(*this);
	FLRActionResult Result;
	for (const FLRInventorySlot& Returned : { Entity.Chamber, Entity.Source })
	{
		if (Returned.IsEmpty())
		{
			continue;
		}
		if (!AddUnique(Returned.Item, Returned.InstanceId))
		{
			return MakeFailure(Request.Action, ReasonNoInventorySpace, TEXT("Can't unload: inventory is full"));
		}
		Result.Gained.Emplace(Returned.Item, 1);
	}
	// Entity still points into the live Placed map: AddUnique only touches the inventory.
	Entity.Chamber.Clear();
	Entity.Source.Clear();
	Entity.ExposureProgress = 0.0;
	Txn.Commit();

	Result.Action = Request.Action;
	Result.bSuccess = true;
	Result.Message = FString::Printf(TEXT("Unloaded the %s at %s"), *Data.GetDisplayName(Entity.Item), *DescribeCell(Entity.Cell));
	return Result;
}

FLRActionResult FLRSimulation::ExecuteAnnihilate(const FLRActionRequest& Request)
{
	// Destroys the whole stack in one slot. Sci-fi flavour: matter meets antimatter.
	if (!Inventory.IsValidIndex(Request.Slot) || Inventory[Request.Slot].IsEmpty())
	{
		return MakeFailure(Request.Action, ReasonNothingSelected, FString::Printf(TEXT("Can't annihilate: %s"), *DescribeReason(ReasonNothingSelected)));
	}
	const FLRInventorySlot Slot = Inventory[Request.Slot];
	LootBoxes.Remove(Slot.InstanceId); // a destroyed loot box takes its modifiers with it
	Inventory[Request.Slot].Clear();
	AddStat(StatKey(TEXT("annihilated"), Slot.Item), Slot.Count);

	FLRActionResult Result;
	Result.Action = Request.Action;
	Result.bSuccess = true;
	Result.Spent.Emplace(Slot.Item, Slot.Count);
	Result.Message = FString::Printf(TEXT("Annihilated %d %s"), Slot.Count, *Data.GetDisplayName(Slot.Item));
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
		AnnounceUnlocks(RefreshUnlocks()); // inventory-based unlocks (no stats: this is a cheat)
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
		if (Modifier.Kind == LRNames::ModifierExtraRolls)
		{
			const int32 Extra = FMath::RoundToInt(Modifier.Value);
			Out.RollsMin = FMath::Max(0, Out.RollsMin + Extra);
			Out.RollsMax = FMath::Max(Out.RollsMin, Out.RollsMax + Extra);
		}
		else if (Modifier.Kind == LRNames::ModifierItemWeightMult)
		{
			for (FLRLootEntry& Entry : Out.Entries)
			{
				if (Entry.Item == Modifier.Item)
				{
					Entry.Weight = FMath::Max(0, FMath::RoundToInt(Entry.Weight * Modifier.Value));
				}
			}
		}
		else if (Modifier.Kind == LRNames::ModifierAddEntry)
		{
			const int32 Weight = FMath::Max(1, FMath::RoundToInt(Modifier.Value));
			if (FLRLootEntry* Existing = Out.Entries.FindByPredicate([&Modifier](const FLRLootEntry& E) { return E.Item == Modifier.Item; }))
			{
				Existing->Weight += Weight;
			}
			else
			{
				FLRLootEntry Added;
				Added.Item = Modifier.Item;
				Added.Weight = Weight;
				Added.MinCount = 1;
				Added.MaxCount = 1;
				Out.Entries.Add(Added);
			}
		}
		// ModifierReveal changes nothing here: it locks contents instead (see ApplyExposure).
		else if (Modifier.Kind == LRNames::ModifierItemCountMult)
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

FString FLRSimulation::DescribeModifier(const FLRLootModifier& Modifier, const FLRGameData& InData)
{
	const FString ItemName = InData.GetDisplayName(Modifier.Item);
	if (Modifier.Kind == LRNames::ModifierExtraRolls)     { return FString::Printf(TEXT("%+d roll(s)"), FMath::RoundToInt(Modifier.Value)); }
	if (Modifier.Kind == LRNames::ModifierItemWeightMult) { return FString::Printf(TEXT("%s x%.2f as likely"), *ItemName, Modifier.Value); }
	if (Modifier.Kind == LRNames::ModifierItemCountMult)  { return FString::Printf(TEXT("%s amounts x%.2f"), *ItemName, Modifier.Value); }
	if (Modifier.Kind == LRNames::ModifierAddEntry)       { return FString::Printf(TEXT("may contain %s"), *ItemName); }
	if (Modifier.Kind == LRNames::ModifierReveal)         { return TEXT("observes the cache: contents revealed and fixed"); }
	return Modifier.Kind.ToString();
}

FString FLRSimulation::DescribeCell(const FIntVector& Cell)
{
	return FString::Printf(TEXT("(%d, %d, %d)"), Cell.X, Cell.Y, Cell.Z);
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
