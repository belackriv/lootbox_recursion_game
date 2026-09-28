// Automation tests for the simulation. Rails equivalent: test/models/*_test.rb.
//
// Run in the editor: Tools > Session Frontend > Automation, filter "LootboxRecursion".
// Or headless (see README): UnrealEditor-Cmd <project> -ExecCmds="Automation RunTests LootboxRecursion;Quit" ...

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Data/LRGameData.h"
#include "Cosmos/LRBlackHoleRenderer.h"
#include "Misc/Paths.h"
#include "Simulation/LRHexGrid.h"
#include "Simulation/LRSimulation.h"

#define LR_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

namespace LRTest
{

	const FName Carbon(TEXT("carbon"));
	const FName Iron(TEXT("iron"));
	const FName Box(TEXT("loot_box"));
	const FName MysteryBox(TEXT("mystery_box"));
	const FName Enclosure(TEXT("enclosure"));
	const FName Lamp(TEXT("lamp"));
	const FName XRayTube(TEXT("xray_tube"));
	const FName GammaSource(TEXT("gamma_source"));
	/** A plain loot action: any action with a lootTable just rolls it. */
	const FName Gather(TEXT("gather"));

	void AddItemDef(FLRGameData& Data, FName Id, const TCHAR* Name, FName Category, int32 StackSize, FName LootTable = NAME_None)
	{
		FLRItemDef Item;
		Item.Id = Id;
		Item.DisplayName = Name;
		Item.Category = Category;
		Item.StackSize = StackSize;
		Item.LootTable = LootTable;
		Data.Items.Add(Id, Item);
	}

	void AddTable(FLRGameData& Data, FName Id, int32 Rolls, FName Item, int32 Count)
	{
		FLRLootTableDef Table;
		Table.Id = Id;
		Table.RollsMin = Rolls;
		Table.RollsMax = Rolls;
		FLRLootEntry Entry;
		Entry.Item = Item;
		Entry.Weight = 1;
		Entry.MinCount = Count;
		Entry.MaxCount = Count;
		Table.Entries.Add(Entry);
		Data.LootTables.Add(Id, Table);
	}

	FLRActionDef MakeAction(FName Name, float Cooldown, float CastTime, FName LootTable = NAME_None)
	{
		FLRActionDef Action;
		Action.Name = Name;
		Action.Label = Name.ToString();
		Action.Cooldown = Cooldown;
		Action.CastTime = CastTime;
		Action.LootTable = LootTable;
		return Action;
	}

	/** Small, fully deterministic data set (every loot range is a single value). */
	FLRGameData MakeData()
	{
		FLRGameData Data;
		AddItemDef(Data, Carbon, TEXT("Carbon"), LRNames::CategoryMaterial, 100);
		AddItemDef(Data, Iron, TEXT("Iron"), LRNames::CategoryMaterial, 100);
		AddItemDef(Data, Box, TEXT("Loot Box"), LRNames::CategoryLootBox, 1, TEXT("box"));
		AddItemDef(Data, MysteryBox, TEXT("Mystery Box"), LRNames::CategoryLootBox, 1, TEXT("nested"));
		AddItemDef(Data, Enclosure, TEXT("Enclosure"), LRNames::CategoryPlaceable, 1);
		Data.Items[Enclosure].MaxRadiationTier = 5;
		Data.Items[Enclosure].MaxExposureStacks = 2;
		Data.Items[Enclosure].ExposureSeconds = 10.f;

		// Radiation: light doubles carbon amounts, x-rays reveal, gamma is too strong for the enclosure.
		auto AddRadiation = [&Data](FName Id, int32 Tier, FName Kind, FName Item, float Value)
		{
			FLRRadiationDef Radiation;
			Radiation.Id = Id;
			Radiation.Name = Id.ToString();
			Radiation.Tier = Tier;
			Radiation.Effect.Kind = Kind;
			Radiation.Effect.Item = Item;
			Radiation.Effect.Value = Value;
			Data.Radiation.Add(Radiation);
		};
		AddRadiation(TEXT("light"), 1, LRNames::ModifierItemCountMult, Carbon, 2.f);
		AddRadiation(TEXT("xray"), 5, LRNames::ModifierReveal, NAME_None, 0.f);
		AddRadiation(TEXT("gamma"), 7, LRNames::ModifierAddEntry, Box, 10.f);
		AddItemDef(Data, Lamp, TEXT("Lamp"), LRNames::CategorySource, 1);
		Data.Items[Lamp].Radiation = TEXT("light");
		AddItemDef(Data, XRayTube, TEXT("X-Ray Tube"), LRNames::CategorySource, 1);
		Data.Items[XRayTube].Radiation = TEXT("xray");
		AddItemDef(Data, GammaSource, TEXT("Gamma Source"), LRNames::CategorySource, 1);
		Data.Items[GammaSource].Radiation = TEXT("gamma");

		AddTable(Data, TEXT("box"), 2, Carbon, 10);       // always 2 x 10 carbon
		AddTable(Data, TEXT("gather"), 1, Carbon, 30);    // always 30 carbon
		AddTable(Data, TEXT("nested"), 1, Box, 1);        // a box inside a box

		FLRRecipeDef BoxRecipe;
		BoxRecipe.Id = Box;
		BoxRecipe.Label = TEXT("Loot Box");
		BoxRecipe.Output = Box;
		BoxRecipe.Cost = { FLRItemAmount(Carbon, 50), FLRItemAmount(Iron, 50) };
		Data.Recipes.Add(BoxRecipe);

		Data.Actions.Add(MakeAction(Gather, 5.f, 5.f, TEXT("gather")));
		Data.Actions.Add(MakeAction(LRNames::Craft, 5.f, 5.f));

		FLRActionDef Use = MakeAction(LRNames::Use, 5.f, 5.f);
		FLRRequirement HasBox;
		HasBox.Category = LRNames::CategoryLootBox;
		HasBox.Condition = TEXT("gt");
		HasBox.Value = 0;
		Use.Requirements.Add(HasBox);
		Data.Actions.Add(Use);

		FLRActionDef Deploy = MakeAction(LRNames::Deploy, 1.f, 0.f);
		FLRRequirement HasPlaceable;
		HasPlaceable.Category = LRNames::CategoryPlaceable;
		Deploy.Requirements.Add(HasPlaceable);
		Data.Actions.Add(Deploy);

		FLRActionDef Recall = MakeAction(LRNames::Recall, 1.f, 0.f);
		FLRRequirement HasPlaced;
		HasPlaced.Check = LRNames::CheckPlaced;
		Recall.Requirements.Add(HasPlaced);
		Data.Actions.Add(Recall);
		Data.Actions.Add(MakeAction(LRNames::SortInventory, 1.f, 0.f));
		Data.Actions.Add(MakeAction(LRNames::Load, 0.f, 0.f));
		Data.Actions.Add(MakeAction(LRNames::Unload, 0.f, 0.f));
		Data.Actions.Add(MakeAction(LRNames::Annihilate, 0.f, 0.f));

		// Tech tree: a recipe unlocked by crafting a loot box, one chained off it, and an
		// action revealed by holding a loot box.
		FLRRequirement CraftedABox;
		CraftedABox.Check = LRNames::CheckStat;
		CraftedABox.Id = TEXT("crafted:loot_box");
		CraftedABox.Condition = TEXT("gte");
		CraftedABox.Value = 1;
		FLRRecipeDef Gated;
		Gated.Id = TEXT("gated_enclosure");
		Gated.Label = TEXT("Gated Enclosure");
		Gated.Output = Enclosure;
		Gated.Cost = { FLRItemAmount(Carbon, 10) };
		Gated.RevealRequirements.Add(CraftedABox);
		Data.Recipes.Add(Gated);

		FLRRequirement AfterGated;
		AfterGated.Check = LRNames::CheckUnlocked;
		AfterGated.Id = TEXT("recipe:gated_enclosure");
		AfterGated.Condition = TEXT("eq");
		AfterGated.Value = 1;
		FLRRecipeDef Chained;
		Chained.Id = TEXT("chained_lamp");
		Chained.Label = TEXT("Chained Lamp");
		Chained.Output = Lamp;
		Chained.Cost = { FLRItemAmount(Carbon, 5) };
		Chained.RevealRequirements.Add(AfterGated);
		Data.Recipes.Add(Chained);

		FLRActionDef Scan = MakeAction(TEXT("scan"), 1.f, 0.f, TEXT("gather"));
		FLRRequirement HoldsBox;
		HoldsBox.Category = LRNames::CategoryLootBox;
		Scan.RevealRequirements.Add(HoldsBox);
		Data.Actions.Add(Scan);
		return Data;
	}

	FLRActionRequest UseSlot(int32 Slot)
	{
		FLRActionRequest Request = FLRActionRequest::Make(LRNames::Use);
		Request.Slot = Slot;
		return Request;
	}

	FLRActionRequest AtCell(FName Action, const FIntVector& Cell)
	{
		FLRActionRequest Request = FLRActionRequest::Make(Action);
		Request.Cell = Cell;
		Request.bHasCell = true;
		return Request;
	}

	int32 SlotOf(const FLRSimulation& Sim, FName Item)
	{
		return Sim.GetInventory().IndexOfByPredicate([Item](const FLRInventorySlot& Slot) { return Slot.Item == Item; });
	}

	/** Load the (first) given item from the inventory into the enclosure at Cell. */
	FLRActionResult LoadInto(FLRSimulation& Sim, FName Item, const FIntVector& Cell)
	{
		FLRActionRequest Request = AtCell(LRNames::Load, Cell);
		Request.Slot = SlotOf(Sim, Item);
		return Sim.RequestAction(Request);
	}

	/** A deployed enclosure at (0, 0, 0). */
	void DeployEnclosure(FLRSimulation& Sim)
	{
		Sim.GiveItem(Enclosure, 1);
		FLRActionRequest Deploy = AtCell(LRNames::Deploy, FIntVector::ZeroValue);
		Deploy.Slot = SlotOf(Sim, Enclosure);
		Sim.RequestAction(Deploy);
	}

	const FName Hydrogen(TEXT("hydrogen"));
	const FName Ripple(TEXT("ripple"));
	/** An instant seeding action (like perturb) that places Ripple and retires in the last epoch. */
	const FName Seed(TEXT("seed"));

	/**
	 * MakeData plus a pocket universe: a ripple that yields 5 hydrogen per roll, three epochs and
	 * a host black hole. Epochs: "empty" (no yield) -> "hot" after two seeds (yields) -> "clear"
	 * after 20 hydrogen gained (yields, ripples grow every 30s, the seed action retires).
	 */
	FLRGameData MakeCosmosData()
	{
		FLRGameData Data = MakeData();
		AddItemDef(Data, Hydrogen, TEXT("Hydrogen"), LRNames::CategoryMaterial, 100);
		AddItemDef(Data, Ripple, TEXT("Ripple"), LRNames::CategoryStructure, 1);
		Data.Items[Ripple].MaxAmplitude = 3;
		Data.Items[Ripple].YieldSeconds = 10.f;
		AddTable(Data, TEXT("plasma"), 1, Hydrogen, 5); // always 5 hydrogen per roll

		auto MakeStat = [](const TCHAR* Id, int32 Value)
		{
			FLRRequirement Requirement;
			Requirement.Check = LRNames::CheckStat;
			Requirement.Id = Id;
			Requirement.Condition = TEXT("gte");
			Requirement.Value = Value;
			return Requirement;
		};
		auto AddEpoch = [&Data](const TCHAR* Id, double Start, FName YieldTable, float Growth, float Plasma) -> FLREpochDef&
		{
			FLREpochDef Epoch;
			Epoch.Id = Id;
			Epoch.Name = Id;
			Epoch.Description = TEXT("Test epoch.");
			Epoch.StartTime = Start;
			Epoch.ClockSeconds = 10.f;
			Epoch.YieldTable = YieldTable;
			Epoch.RippleGrowthSeconds = Growth;
			Epoch.Plasma = Plasma;
			return Data.Epochs.Add_GetRef(Epoch);
		};
		AddEpoch(TEXT("empty"), 1e-36, NAME_None, 0.f, 0.2f);
		AddEpoch(TEXT("hot"), 1e-32, TEXT("plasma"), 0.f, 1.f).AdvanceRequirements.Add(MakeStat(TEXT("done:seed"), 2));
		FLREpochDef& Clear = AddEpoch(TEXT("clear"), 100.0, TEXT("plasma"), 30.f, 0.f);
		Clear.EndTime = 1e4;
		Clear.AdvanceRequirements.Add(MakeStat(TEXT("gained:hydrogen"), 20));

		FLRActionDef SeedAction = MakeAction(Seed, 0.f, 0.f);
		SeedAction.Places = Ripple;
		FLRRequirement InClear;
		InClear.Check = LRNames::CheckEpoch;
		InClear.Id = TEXT("clear");
		InClear.Condition = TEXT("eq");
		InClear.Value = 1;
		SeedAction.RetireRequirements.Add(InClear);
		Data.Actions.Add(SeedAction);
		Data.Actions.Add(MakeAction(LRNames::Feed, 0.f, 0.f));

		Data.Host.LifetimeSeconds = 1000.f;
		Data.Host.PerturbCost = 0.1f;
		Data.Host.FeedAmount = 0.3f;
		Data.Host.WarningMass = 0.25f;
		return Data;
	}

	/** The cosmos data with a host that never evaporates (perturbations still cost mass). */
	FLRGameData MakeStableCosmosData()
	{
		FLRGameData Data = MakeCosmosData();
		Data.Host.LifetimeSeconds = 0.f;
		return Data;
	}

	bool SeedAt(FLRSimulation& Sim, const FIntVector& Cell)
	{
		return Sim.RequestAction(AtCell(Seed, Cell)).bSuccess;
	}

	/** Collects every message the sim broadcasts while it is alive (it unbinds itself). */
	struct FMessageLog
	{
		explicit FMessageLog(FLRSimulation& InSim) : Sim(InSim)
		{
			Handle = Sim.OnActionCompleted.AddLambda([this](const FLRActionResult& Result) { Messages.Add(Result.Message); });
		}
		~FMessageLog() { Sim.OnActionCompleted.Remove(Handle); }
		bool Contains(const TCHAR* Text) const
		{
			return Messages.ContainsByPredicate([Text](const FString& Message) { return Message.Contains(Text); });
		}

		FLRSimulation& Sim;
		FDelegateHandle Handle;
		TArray<FString> Messages;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLRSimLootActionTest, "LootboxRecursion.Simulation.LootActionCastAndCooldown", LR_TEST_FLAGS)
bool FLRSimLootActionTest::RunTest(const FString& Parameters)
{
	FLRSimulation Sim(LRTest::MakeData(), 42);

	const FLRActionResult Started = Sim.RequestAction(FLRActionRequest::Make(LRTest::Gather));
	TestTrue(TEXT("request accepted"), Started.bSuccess && Started.bStarted);
	TestTrue(TEXT("status shows casting"), Sim.GetActionStatus(LRTest::Gather).bCasting);

	const FLRActionResult Again = Sim.RequestAction(FLRActionRequest::Make(LRTest::Gather));
	TestFalse(TEXT("second request rejected while casting"), Again.bSuccess);

	Sim.Advance(4.9);
	TestEqual(TEXT("no carbon before cast completes"), Sim.CountItem(LRTest::Carbon), 0);

	bool bCompleted = false;
	Sim.OnActionCompleted.AddLambda([&bCompleted](const FLRActionResult& Result) { bCompleted = Result.bSuccess; });
	Sim.Advance(0.2);
	TestTrue(TEXT("completion broadcast"), bCompleted);
	TestEqual(TEXT("carbon after cast"), Sim.CountItem(LRTest::Carbon), 30);
	TestFalse(TEXT("cooldown over (cooldown == cast time)"), Sim.GetActionStatus(LRTest::Gather).bOnCooldown);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLRSimStackingTest, "LootboxRecursion.Simulation.StackingIsAllOrNothing", LR_TEST_FLAGS)
bool FLRSimStackingTest::RunTest(const FString& Parameters)
{
	FLRSimulation Sim(LRTest::MakeData(), 1);

	TestTrue(TEXT("give 250 carbon"), Sim.GiveItem(LRTest::Carbon, 250));
	TestEqual(TEXT("slot 0"), Sim.GetInventory()[0].Count, 100);
	TestEqual(TEXT("slot 1"), Sim.GetInventory()[1].Count, 100);
	TestEqual(TEXT("slot 2"), Sim.GetInventory()[2].Count, 50);

	TestTrue(TEXT("give 60 more carbon"), Sim.GiveItem(LRTest::Carbon, 60));
	TestEqual(TEXT("partial stack topped up first"), Sim.GetInventory()[2].Count, 100);
	TestEqual(TEXT("remainder in next slot"), Sim.GetInventory()[3].Count, 10);

	// Fill the remaining 46 slots with iron.
	TestTrue(TEXT("fill with iron"), Sim.GiveItem(LRTest::Iron, 4600));
	TestFalse(TEXT("iron no longer fits"), Sim.GiveItem(LRTest::Iron, 1));
	TestTrue(TEXT("carbon still fits in the partial stack"), Sim.GiveItem(LRTest::Carbon, 90));
	TestFalse(TEXT("but not more than that"), Sim.GiveItem(LRTest::Carbon, 1000));
	TestEqual(TEXT("failed add changed nothing"), Sim.CountItem(LRTest::Carbon), 400);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLRSimCraftTest, "LootboxRecursion.Simulation.CraftLootBox", LR_TEST_FLAGS)
bool FLRSimCraftTest::RunTest(const FString& Parameters)
{
	FLRSimulation Sim(LRTest::MakeData(), 1);

	FLRActionRequest Craft = FLRActionRequest::Make(LRNames::Craft);
	Craft.Choice = LRTest::Box;

	const FLRActionResult Poor = Sim.RequestAction(Craft);
	TestFalse(TEXT("can't craft without materials"), Poor.bSuccess);
	TestFalse(TEXT("failed request does not start a cooldown"), Sim.GetActionStatus(LRNames::Craft).bOnCooldown);

	Sim.GiveItem(LRTest::Carbon, 60);
	Sim.GiveItem(LRTest::Iron, 55);
	TestTrue(TEXT("craft accepted"), Sim.RequestAction(Craft).bStarted);
	Sim.Advance(5.0);

	TestEqual(TEXT("carbon spent"), Sim.CountItem(LRTest::Carbon), 10);
	TestEqual(TEXT("iron spent"), Sim.CountItem(LRTest::Iron), 5);
	TestEqual(TEXT("box crafted"), Sim.CountItem(LRTest::Box), 1);

	const int32 BoxSlot = Sim.GetInventory().IndexOfByPredicate([](const FLRInventorySlot& Slot) { return Slot.Item == LRTest::Box; });
	TestTrue(TEXT("box has a slot"), BoxSlot != INDEX_NONE);
	if (BoxSlot != INDEX_NONE)
	{
		const int32 InstanceId = Sim.GetInventory()[BoxSlot].InstanceId;
		TestTrue(TEXT("box has an instance id"), InstanceId != 0);
		TestNotNull(TEXT("box instance registered"), Sim.FindLootBox(InstanceId));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLRSimOpenTest, "LootboxRecursion.Simulation.OpenLootBox", LR_TEST_FLAGS)
bool FLRSimOpenTest::RunTest(const FString& Parameters)
{
	FLRSimulation Sim(LRTest::MakeData(), 1);
	TestFalse(TEXT("use disabled without a box"), Sim.GetActionStatus(LRNames::Use).bEnabled);

	Sim.GiveItem(LRTest::Box, 1);
	const int32 InstanceId = Sim.GetInventory()[0].InstanceId;
	TestTrue(TEXT("use enabled with a box"), Sim.GetActionStatus(LRNames::Use).bEnabled);

	// Slot 7 is empty: Use should fall back to the first loot box (Rails behaviour).
	TestTrue(TEXT("use accepted"), Sim.RequestAction(LRTest::UseSlot(7)).bStarted);
	Sim.Advance(5.0);

	TestEqual(TEXT("box consumed"), Sim.CountItem(LRTest::Box), 0);
	TestNull(TEXT("box instance removed"), Sim.FindLootBox(InstanceId));
	TestEqual(TEXT("2 rolls x 10 carbon"), Sim.CountItem(LRTest::Carbon), 20);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLRSimOpenRollbackTest, "LootboxRecursion.Simulation.OpenRollsBackWhenFull", LR_TEST_FLAGS)
bool FLRSimOpenRollbackTest::RunTest(const FString& Parameters)
{
	FLRSimulation Sim(LRTest::MakeData(), 1);
	Sim.GiveItem(LRTest::Box, 1);
	Sim.GiveItem(LRTest::Iron, 4900); // the other 49 slots

	const int32 InstanceId = Sim.GetInventory()[0].InstanceId;
	FLRLootModifier Modifier;
	Modifier.Kind = TEXT("item_count_mult");
	Modifier.Item = LRTest::Carbon;
	Modifier.Value = 20.f; // 2 x 200 carbon: needs 4 slots, only the box's own slot frees up
	TestTrue(TEXT("modifier attached"), Sim.AddLootBoxModifier(InstanceId, Modifier));

	Sim.RequestAction(LRTest::UseSlot(0));
	Sim.Advance(5.0);

	TestEqual(TEXT("box still there"), Sim.CountItem(LRTest::Box), 1);
	TestNotNull(TEXT("box instance restored"), Sim.FindLootBox(InstanceId));
	TestEqual(TEXT("modifiers kept"), Sim.FindLootBox(InstanceId) ? Sim.FindLootBox(InstanceId)->Modifiers.Num() : 0, 1);
	TestEqual(TEXT("no carbon leaked"), Sim.CountItem(LRTest::Carbon), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLRSimRecursionTest, "LootboxRecursion.Simulation.BoxInsideABox", LR_TEST_FLAGS)
bool FLRSimRecursionTest::RunTest(const FString& Parameters)
{
	FLRSimulation Sim(LRTest::MakeData(), 1);
	Sim.GiveItem(LRTest::MysteryBox, 1);

	Sim.RequestAction(LRTest::UseSlot(0));
	Sim.Advance(5.0);

	TestEqual(TEXT("mystery box consumed"), Sim.CountItem(LRTest::MysteryBox), 0);
	TestEqual(TEXT("contained a loot box"), Sim.CountItem(LRTest::Box), 1);
	const FLRInventorySlot& Slot = Sim.GetInventory()[0];
	TestNotNull(TEXT("inner box is a real instance"), Sim.FindLootBox(Slot.InstanceId));

	Sim.RequestAction(LRTest::UseSlot(0));
	Sim.Advance(5.0);
	TestEqual(TEXT("inner box opened too"), Sim.CountItem(LRTest::Carbon), 20);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLRSimModifierTest, "LootboxRecursion.Simulation.LootModifiers", LR_TEST_FLAGS)
bool FLRSimModifierTest::RunTest(const FString& Parameters)
{
	FLRLootTableDef Table;
	Table.RollsMin = 2;
	Table.RollsMax = 4;
	FLRLootEntry Entry;
	Entry.Item = LRTest::Carbon;
	Entry.Weight = 10;
	Entry.MinCount = 5;
	Entry.MaxCount = 8;
	Table.Entries.Add(Entry);

	FLRLootModifier ExtraRolls;
	ExtraRolls.Kind = TEXT("extra_rolls");
	ExtraRolls.Value = 2.f;
	FLRLootModifier Weight;
	Weight.Kind = TEXT("item_weight_mult");
	Weight.Item = LRTest::Carbon;
	Weight.Value = 3.f;
	FLRLootModifier Count;
	Count.Kind = TEXT("item_count_mult");
	Count.Item = LRTest::Carbon;
	Count.Value = 2.f;

	const FLRLootTableDef Out = FLRSimulation::ApplyModifiers(Table, { ExtraRolls, Weight, Count });
	TestEqual(TEXT("rollsMin"), Out.RollsMin, 4);
	TestEqual(TEXT("rollsMax"), Out.RollsMax, 6);
	TestEqual(TEXT("weight"), Out.Entries[0].Weight, 30);
	TestEqual(TEXT("minCount"), Out.Entries[0].MinCount, 10);
	TestEqual(TEXT("maxCount"), Out.Entries[0].MaxCount, 16);
	TestEqual(TEXT("original untouched"), Table.RollsMin, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLRSimDeployRecallTest, "LootboxRecursion.Simulation.DeployAndRecall", LR_TEST_FLAGS)
bool FLRSimDeployRecallTest::RunTest(const FString& Parameters)
{
	FLRSimulation Sim(LRTest::MakeData(), 1);
	TestFalse(TEXT("deploy disabled without a placeable"), Sim.GetActionStatus(LRNames::Deploy).bRequirementsMet);

	Sim.GiveItem(LRTest::Enclosure, 2);
	const int32 FirstId = Sim.GetInventory()[0].InstanceId;

	TestFalse(TEXT("deploy needs a coordinate"), Sim.RequestAction(FLRActionRequest::Make(LRNames::Deploy)).bSuccess);

	const FIntVector Cell(-3, 4, 2);
	FLRActionRequest Deploy = LRTest::AtCell(LRNames::Deploy, Cell);
	Deploy.Slot = 0;
	TestTrue(TEXT("deploy at (-3, 4, 2) (instant)"), Sim.RequestAction(Deploy).bSuccess);
	const FLRPlacedEntity* Placed = Sim.FindPlaced(Cell);
	TestNull(TEXT("neighbouring layer still empty"), Sim.FindPlaced(FIntVector(-3, 4, 1)));
	TestNotNull(TEXT("entity placed"), Placed);
	TestEqual(TEXT("keeps its identity"), Placed ? Placed->InstanceId : 0, FirstId);
	TestEqual(TEXT("one enclosure left in inventory"), Sim.CountItem(LRTest::Enclosure), 1);

	Sim.Advance(1.0); // deploy cooldown
	TestFalse(TEXT("occupied cell rejected"), Sim.RequestAction(LRTest::AtCell(LRNames::Deploy, Cell)).bSuccess);

	Sim.Advance(1.0);
	TestTrue(TEXT("recall"), Sim.RequestAction(LRTest::AtCell(LRNames::Recall, Cell)).bSuccess);
	TestNull(TEXT("cell cleared"), Sim.FindPlaced(Cell));
	TestEqual(TEXT("back in inventory"), Sim.CountItem(LRTest::Enclosure), 2);
	TestTrue(TEXT("same instance came back"),
		Sim.GetInventory().ContainsByPredicate([FirstId](const FLRInventorySlot& Slot) { return Slot.InstanceId == FirstId; }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLRSimSortTest, "LootboxRecursion.Simulation.SortAndCompress", LR_TEST_FLAGS)
bool FLRSimSortTest::RunTest(const FString& Parameters)
{
	FLRSimulation Sim(LRTest::MakeData(), 1);

	// Build a messy inventory via a save file.
	FLRSaveData Save = Sim.Save();
	auto SetSlot = [&Save](int32 Index, FName Item, int32 Count, int32 InstanceId = 0)
	{
		Save.Inventory[Index].Item = Item;
		Save.Inventory[Index].Count = Count;
		Save.Inventory[Index].InstanceId = InstanceId;
	};
	SetSlot(0, LRTest::Carbon, 40);
	SetSlot(2, LRTest::Iron, 30);
	SetSlot(3, LRTest::Carbon, 70);
	SetSlot(5, LRTest::Box, 1, 9);
	FLRLootBoxInstance BoxInstance;
	BoxInstance.InstanceId = 9;
	BoxInstance.LootTable = TEXT("box");
	Save.LootBoxes.Add(BoxInstance);
	TestTrue(TEXT("load"), Sim.Load(Save));

	TestTrue(TEXT("sort needed"), Sim.IsSortNeeded());
	TestTrue(TEXT("sort"), Sim.RequestAction(FLRActionRequest::Make(LRNames::SortInventory)).bSuccess);

	// Alphabetical by display name: Carbon (x2 stacks), Iron, Loot Box.
	const TArray<FLRInventorySlot>& Inv = Sim.GetInventory();
	TestTrue(TEXT("0 = carbon 100"), Inv[0].Item == LRTest::Carbon && Inv[0].Count == 100);
	TestTrue(TEXT("1 = carbon 10"), Inv[1].Item == LRTest::Carbon && Inv[1].Count == 10);
	TestTrue(TEXT("2 = iron 30"), Inv[2].Item == LRTest::Iron && Inv[2].Count == 30);
	TestTrue(TEXT("3 = loot box, same instance"), Inv[3].Item == LRTest::Box && Inv[3].InstanceId == 9);
	TestTrue(TEXT("4 empty"), Inv[4].IsEmpty());
	TestFalse(TEXT("sorted now"), Sim.IsSortNeeded());

	// The save said NextInstanceId = 1 but box 9 exists: new unique items must not collide.
	TestTrue(TEXT("give another box"), Sim.GiveItem(LRTest::Box, 1));
	TestNotNull(TEXT("original box instance intact"), Sim.FindLootBox(9));
	TestEqual(TEXT("two boxes"), Sim.CountItem(LRTest::Box), 2);
	TestFalse(TEXT("new box did not reuse id 9"),
		Sim.GetInventory().ContainsByPredicate([](const FLRInventorySlot& Slot) { return Slot.Item == LRTest::Box && Slot.InstanceId != 9 && Slot.InstanceId <= 9; }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLRSimSaveLoadTest, "LootboxRecursion.Simulation.SaveLoadRoundTrip", LR_TEST_FLAGS)
bool FLRSimSaveLoadTest::RunTest(const FString& Parameters)
{
	FLRSimulation Sim(LRTest::MakeData(), 7);
	Sim.GiveItem(LRTest::Enclosure, 1);
	Sim.RequestAction(LRTest::AtCell(LRNames::Deploy, FIntVector(12, -1, 3)));
	Sim.RequestAction(FLRActionRequest::Make(LRTest::Gather)); // mid-cast when saved
	Sim.Advance(2.0);

	const FLRSaveData Save = Sim.Save();

	FLRSimulation Loaded(LRTest::MakeData(), 0);
	TestTrue(TEXT("load"), Loaded.Load(Save));
	TestNotNull(TEXT("placed entity restored"), Loaded.FindPlaced(FIntVector(12, -1, 3)));
	TestTrue(TEXT("cast restored"), Loaded.GetActionStatus(LRTest::Gather).bCasting);

	Loaded.Advance(3.0);
	TestEqual(TEXT("pending cast completes after load"), Loaded.CountItem(LRTest::Carbon), 30);

	FLRSaveData Old = Save;
	Old.Version = FLRSimulation::SaveVersion + 1;
	TestFalse(TEXT("incompatible save rejected"), Loaded.Load(Old));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLRGameDataShippedTest, "LootboxRecursion.Data.ShippedDataIsValid", LR_TEST_FLAGS)
bool FLRGameDataShippedTest::RunTest(const FString& Parameters)
{
	FLRGameData Data;
	TArray<FString> Errors;
	const bool bLoaded = FLRGameData::LoadFromDirectory(FLRGameData::GetDefaultDataDirectory(), Data, Errors);
	for (const FString& Error : Errors)
	{
		AddError(Error);
	}
	TestTrue(TEXT("Content/Data loads cleanly"), bLoaded);

	for (const FName Action : { LRNames::Perturb, LRNames::Feed, LRNames::Craft, LRNames::Use, LRNames::Deploy, LRNames::Recall,
		LRNames::SortInventory, LRNames::Load, LRNames::Unload, LRNames::Annihilate })
	{
		TestNotNull(*FString::Printf(TEXT("action '%s' defined"), *Action.ToString()), Data.FindAction(Action));
	}
	TestTrue(TEXT("has recipes"), Data.Recipes.Num() > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLRGameDataValidationTest, "LootboxRecursion.Data.ValidationCatchesBadReferences", LR_TEST_FLAGS)
bool FLRGameDataValidationTest::RunTest(const FString& Parameters)
{
	FLRGameData Data = LRTest::MakeData();
	TestEqual(TEXT("test data is valid"), Data.Validate().Num(), 0);

	FLRRecipeDef Broken;
	Broken.Id = TEXT("broken");
	Broken.Output = TEXT("unobtainium");
	Data.Recipes.Add(Broken);
	TestTrue(TEXT("unknown output reported"), Data.Validate().Num() > 0);

	const FLRGameData Cosmos = LRTest::MakeCosmosData();
	TestEqual(TEXT("cosmos test data is valid"), Cosmos.Validate().Num(), 0);

	FLRGameData UnknownEpoch = Cosmos;
	FLRRequirement& Requirement = UnknownEpoch.Epochs[1].AdvanceRequirements[0];
	Requirement.Check = LRNames::CheckEpoch;
	Requirement.Id = TEXT("big_crunch");
	TestTrue(TEXT("unknown epoch reported"), UnknownEpoch.Validate().Num() > 0);

	FLRGameData OutOfOrder = Cosmos;
	OutOfOrder.Epochs[2].StartTime = 1e-40;
	TestTrue(TEXT("epochs out of order reported"), OutOfOrder.Validate().Num() > 0);

	FLRGameData PlacesMaterial = Cosmos;
	for (FLRActionDef& Action : PlacesMaterial.Actions)
	{
		if (Action.Name == LRTest::Seed)
		{
			Action.Places = LRTest::Hydrogen;
		}
	}
	TestTrue(TEXT("seeding something that isn't an overdensity reported"), PlacesMaterial.Validate().Num() > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLRSimIrradiateTest, "LootboxRecursion.Irradiation.StacksUpToCapThenOpens", LR_TEST_FLAGS)
bool FLRSimIrradiateTest::RunTest(const FString& Parameters)
{
	FLRSimulation Sim(LRTest::MakeData(), 1);
	LRTest::DeployEnclosure(Sim);
	const FIntVector Cell = FIntVector::ZeroValue;

	Sim.GiveItem(LRTest::Box, 1);
	Sim.GiveItem(LRTest::Lamp, 1);
	const int32 BoxId = Sim.GetInventory()[LRTest::SlotOf(Sim, LRTest::Box)].InstanceId;

	TestFalse(TEXT("load needs a selected item"), Sim.RequestAction(LRTest::AtCell(LRNames::Load, Cell)).bSuccess);
	TestTrue(TEXT("load box"), LRTest::LoadInto(Sim, LRTest::Box, Cell).bSuccess);
	TestTrue(TEXT("load lamp"), LRTest::LoadInto(Sim, LRTest::Lamp, Cell).bSuccess);
	TestEqual(TEXT("box left the inventory"), Sim.CountItem(LRTest::Box), 0);

	Sim.Advance(9.9);
	TestEqual(TEXT("no stack before the interval"), Sim.FindLootBox(BoxId) ? Sim.FindLootBox(BoxId)->Modifiers.Num() : -1, 0);
	Sim.Advance(0.2);
	TestEqual(TEXT("first stack"), Sim.FindLootBox(BoxId) ? Sim.FindLootBox(BoxId)->Modifiers.Num() : -1, 1);
	Sim.Advance(100.0);
	TestEqual(TEXT("capped at the enclosure's max stacks"), Sim.FindLootBox(BoxId) ? Sim.FindLootBox(BoxId)->Modifiers.Num() : -1, 2);

	TestTrue(TEXT("unload"), Sim.RequestAction(LRTest::AtCell(LRNames::Unload, Cell)).bSuccess);
	TestEqual(TEXT("box back"), Sim.CountItem(LRTest::Box), 1);
	TestEqual(TEXT("lamp back"), Sim.CountItem(LRTest::Lamp), 1);

	// 2 rolls x 10 carbon, amounts doubled twice = 2 x 40.
	Sim.RequestAction(LRTest::UseSlot(LRTest::SlotOf(Sim, LRTest::Box)));
	Sim.Advance(5.0);
	TestEqual(TEXT("irradiated loot"), Sim.CountItem(LRTest::Carbon), 80);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLRSimIrradiateRulesTest, "LootboxRecursion.Irradiation.LoadRules", LR_TEST_FLAGS)
bool FLRSimIrradiateRulesTest::RunTest(const FString& Parameters)
{
	FLRSimulation Sim(LRTest::MakeData(), 1);
	LRTest::DeployEnclosure(Sim);
	const FIntVector Cell = FIntVector::ZeroValue;
	Sim.GiveItem(LRTest::GammaSource, 1);
	Sim.GiveItem(LRTest::Box, 2);

	const FLRActionResult TooStrong = LRTest::LoadInto(Sim, LRTest::GammaSource, Cell);
	TestFalse(TEXT("gamma is too strong for a tier-5 enclosure"), TooStrong.bSuccess);
	TestTrue(TEXT("reason"), TooStrong.Reason == FName(TEXT("radiation_too_strong")));

	TestFalse(TEXT("no enclosure at an empty cell"), LRTest::LoadInto(Sim, LRTest::Box, FIntVector(5, 5, 0)).bSuccess);
	TestTrue(TEXT("first box"), LRTest::LoadInto(Sim, LRTest::Box, Cell).bSuccess);
	TestFalse(TEXT("chamber holds one box"), LRTest::LoadInto(Sim, LRTest::Box, Cell).bSuccess);
	Sim.Advance(60.0);
	const FLRPlacedEntity* Loaded = Sim.FindPlaced(Cell);
	const FLRLootBoxInstance* Inside = Loaded ? Sim.FindLootBox(Loaded->Chamber.InstanceId) : nullptr;
	TestEqual(TEXT("no stacks without a source"), Inside ? Inside->Modifiers.Num() : -1, 0);

	// Recall brings the enclosure and its contents home.
	TestTrue(TEXT("recall"), Sim.RequestAction(LRTest::AtCell(LRNames::Recall, Cell)).bSuccess);
	TestEqual(TEXT("enclosure back"), Sim.CountItem(LRTest::Enclosure), 1);
	TestEqual(TEXT("both boxes back"), Sim.CountItem(LRTest::Box), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLRSimXRayTest, "LootboxRecursion.Irradiation.XRayRevealsAndLocks", LR_TEST_FLAGS)
bool FLRSimXRayTest::RunTest(const FString& Parameters)
{
	FLRSimulation Sim(LRTest::MakeData(), 3);
	LRTest::DeployEnclosure(Sim);
	const FIntVector Cell = FIntVector::ZeroValue;
	Sim.GiveItem(LRTest::Box, 1);
	Sim.GiveItem(LRTest::XRayTube, 1);
	const int32 BoxId = Sim.GetInventory()[LRTest::SlotOf(Sim, LRTest::Box)].InstanceId;
	LRTest::LoadInto(Sim, LRTest::Box, Cell);
	LRTest::LoadInto(Sim, LRTest::XRayTube, Cell);

	Sim.Advance(10.0);
	const FLRLootBoxInstance* BoxInstance = Sim.FindLootBox(BoxId);
	TestTrue(TEXT("revealed"), BoxInstance && BoxInstance->bRevealed);
	TestEqual(TEXT("contents known: 2 x 10 carbon"),
		(BoxInstance && BoxInstance->RevealedContents.Num() == 1) ? BoxInstance->RevealedContents[0].Count : -1, 20);

	Sim.Advance(100.0);
	TestEqual(TEXT("locked: no further stacks"), Sim.FindLootBox(BoxId) ? Sim.FindLootBox(BoxId)->Modifiers.Num() : -1, 0);

	Sim.RequestAction(LRTest::AtCell(LRNames::Unload, Cell));
	Sim.RequestAction(LRTest::UseSlot(LRTest::SlotOf(Sim, LRTest::Box)));
	Sim.Advance(5.0);
	TestEqual(TEXT("opens to exactly what was revealed"), Sim.CountItem(LRTest::Carbon), 20);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLRSimGammaTest, "LootboxRecursion.Irradiation.AddEntryModifier", LR_TEST_FLAGS)
bool FLRSimGammaTest::RunTest(const FString& Parameters)
{
	FLRLootTableDef Table;
	Table.RollsMin = 1;
	Table.RollsMax = 1;
	FLRLootEntry Entry;
	Entry.Item = LRTest::Carbon;
	Entry.Weight = 10;
	Table.Entries.Add(Entry);

	FLRLootModifier Mutation;
	Mutation.Kind = LRNames::ModifierAddEntry;
	Mutation.Item = LRTest::Box;
	Mutation.Value = 5.f;

	const FLRLootTableDef Once = FLRSimulation::ApplyModifiers(Table, { Mutation });
	TestEqual(TEXT("entry added"), Once.Entries.Num(), 2);
	TestEqual(TEXT("with weight 5"), Once.Entries.Num() == 2 ? Once.Entries[1].Weight : -1, 5);

	const FLRLootTableDef Twice = FLRSimulation::ApplyModifiers(Table, { Mutation, Mutation });
	TestEqual(TEXT("stacks by weight, not duplicate entries"), Twice.Entries.Num(), 2);
	TestEqual(TEXT("weight 10"), Twice.Entries.Num() == 2 ? Twice.Entries[1].Weight : -1, 10);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLRSimIrradiateSaveTest, "LootboxRecursion.Irradiation.SurvivesSaveLoad", LR_TEST_FLAGS)
bool FLRSimIrradiateSaveTest::RunTest(const FString& Parameters)
{
	FLRSimulation Sim(LRTest::MakeData(), 1);
	LRTest::DeployEnclosure(Sim);
	const FIntVector Cell = FIntVector::ZeroValue;
	Sim.GiveItem(LRTest::Box, 1);
	Sim.GiveItem(LRTest::Lamp, 1);
	const int32 BoxId = Sim.GetInventory()[LRTest::SlotOf(Sim, LRTest::Box)].InstanceId;
	LRTest::LoadInto(Sim, LRTest::Box, Cell);
	LRTest::LoadInto(Sim, LRTest::Lamp, Cell);
	Sim.Advance(6.0);

	FLRSimulation Loaded(LRTest::MakeData(), 0);
	TestTrue(TEXT("load"), Loaded.Load(Sim.Save()));
	const FLRPlacedEntity* Entity = Loaded.FindPlaced(Cell);
	TestTrue(TEXT("chamber restored"), Entity && Entity->Chamber.InstanceId == BoxId);
	Loaded.Advance(4.0);
	TestEqual(TEXT("progress carried over"), Loaded.FindLootBox(BoxId) ? Loaded.FindLootBox(BoxId)->Modifiers.Num() : -1, 1);

	// New unique items must not reuse the id of the box inside the enclosure.
	Loaded.GiveItem(LRTest::Box, 1);
	TestFalse(TEXT("no id collision"),
		Loaded.GetInventory().ContainsByPredicate([BoxId](const FLRInventorySlot& Slot) { return Slot.InstanceId == BoxId; }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLRTechTreeRecipeTest, "LootboxRecursion.TechTree.RecipesUnlockAndLatch", LR_TEST_FLAGS)
bool FLRTechTreeRecipeTest::RunTest(const FString& Parameters)
{
	FLRSimulation Sim(LRTest::MakeData(), 1);
	const FName Gated(TEXT("gated_enclosure"));
	const FName Chained(TEXT("chained_lamp"));
	TestTrue(TEXT("no reveal requirements = unlocked"), Sim.IsRecipeUnlocked(LRTest::Box));
	TestFalse(TEXT("gated starts locked"), Sim.IsRecipeUnlocked(Gated));
	TestFalse(TEXT("chained starts locked"), Sim.IsRecipeUnlocked(Chained));

	Sim.GiveItem(LRTest::Carbon, 200);
	Sim.GiveItem(LRTest::Iron, 100);
	FLRActionRequest CraftGated = FLRActionRequest::Make(LRNames::Craft);
	CraftGated.Choice = Gated;
	const FLRActionResult Locked = Sim.RequestAction(CraftGated);
	TestFalse(TEXT("locked recipe can't be crafted"), Locked.bSuccess);
	TestTrue(TEXT("reason"), Locked.Reason == FName(TEXT("recipe_locked")));

	int32 UnlockMessages = 0;
	Sim.OnActionCompleted.AddLambda([&UnlockMessages](const FLRActionResult& Result)
	{
		UnlockMessages += Result.Action == FName(TEXT("unlock")) ? 1 : 0;
	});
	FLRActionRequest CraftBox = FLRActionRequest::Make(LRNames::Craft);
	CraftBox.Choice = LRTest::Box;
	Sim.RequestAction(CraftBox);
	Sim.Advance(5.0);

	TestEqual(TEXT("stat counted"), Sim.GetStat(TEXT("crafted:loot_box")), 1);
	TestEqual(TEXT("gained stat counted"), Sim.GetStat(TEXT("gained:loot_box")), 1);
	TestTrue(TEXT("gated unlocked"), Sim.IsRecipeUnlocked(Gated));
	TestTrue(TEXT("chain resolved in the same pass"), Sim.IsRecipeUnlocked(Chained));
	// Gated + Chained recipes, plus the "scan" action (revealed by now holding a loot box).
	TestEqual(TEXT("all three unlocks announced"), UnlockMessages, 3);

	Sim.Advance(5.0); // craft cooldown
	TestTrue(TEXT("now craftable"), Sim.RequestAction(CraftGated).bStarted);

	FLRSimulation Loaded(LRTest::MakeData(), 0);
	TestTrue(TEXT("load"), Loaded.Load(Sim.Save()));
	TestTrue(TEXT("unlock survives save/load"), Loaded.IsRecipeUnlocked(Gated));
	TestEqual(TEXT("stats survive save/load"), Loaded.GetStat(TEXT("crafted:loot_box")), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLRTechTreeActionTest, "LootboxRecursion.TechTree.ActionsRevealAndLatch", LR_TEST_FLAGS)
bool FLRTechTreeActionTest::RunTest(const FString& Parameters)
{
	FLRSimulation Sim(LRTest::MakeData(), 1);
	const FName Scan(TEXT("scan"));
	TestFalse(TEXT("hidden at first"), Sim.GetActionStatus(Scan).bRevealed);
	TestFalse(TEXT("hidden actions can't be used"), Sim.RequestAction(FLRActionRequest::Make(Scan)).bSuccess);

	Sim.GiveItem(LRTest::Box, 1);
	TestTrue(TEXT("revealed once a box is held"), Sim.GetActionStatus(Scan).bRevealed);

	Sim.RequestAction(LRTest::UseSlot(0));
	Sim.Advance(5.0);
	TestEqual(TEXT("box gone"), Sim.CountItem(LRTest::Box), 0);
	TestTrue(TEXT("stays revealed (latched)"), Sim.GetActionStatus(Scan).bRevealed);
	TestEqual(TEXT("opened stat"), Sim.GetStat(TEXT("opened:loot_box")), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLRTechTreePlaythroughTest, "LootboxRecursion.TechTree.ShippedTreeIsPlayable", LR_TEST_FLAGS)
bool FLRTechTreePlaythroughTest::RunTest(const FString& Parameters)
{
	// Walks the real Content/Data tech tree and epochs from a new game to the last unlock, so a
	// data change that creates a dead end fails here. The early game is played for real: the
	// ripples' yields are what reach nucleosynthesis. After that, materials are cheated in
	// (GiveItem doesn't count toward stats) and everything else is played through real actions.
	FLRGameData Data;
	TArray<FString> Errors;
	if (!FLRGameData::LoadFromDirectory(FLRGameData::GetDefaultDataDirectory(), Data, Errors))
	{
		for (const FString& Error : Errors)
		{
			AddError(Error);
		}
		return false;
	}
	FLRSimulation Sim(Data, 7);
	const FName Hydrogen(TEXT("hydrogen"));
	const FName Helium(TEXT("helium"));
	const FName Carbon(TEXT("carbon"));
	const FName Iron(TEXT("iron"));
	const FName LootBox(TEXT("loot_box"));

	auto EpochIs = [&Sim](const TCHAR* Id)
	{
		const FLREpochDef* Epoch = Sim.GetEpoch();
		return Epoch && Epoch->Id == FName(Id);
	};
	auto Act = [&Sim](const FLRActionRequest& Request)
	{
		const bool bOk = Sim.RequestAction(Request).bSuccess;
		Sim.Advance(6.0); // past any cast time and cooldown
		return bOk;
	};
	auto Craft = [&Act](const TCHAR* Recipe)
	{
		FLRActionRequest Request = FLRActionRequest::Make(LRNames::Craft);
		Request.Choice = Recipe;
		return Act(Request);
	};
	auto AtCell = [](FName Action, const FIntVector& Cell, int32 Slot = INDEX_NONE)
	{
		FLRActionRequest Request = LRTest::AtCell(Action, Cell);
		Request.Slot = Slot;
		return Request;
	};
	auto OpenFirstBox = [&Sim, &Act, LootBox]()
	{
		FLRActionRequest Request = FLRActionRequest::Make(LRNames::Use);
		Request.Slot = LRTest::SlotOf(Sim, LootBox);
		return Act(Request);
	};
	auto Irradiate = [&Sim, &Act, &Craft, &AtCell, &OpenFirstBox, LootBox](const FIntVector& Cell, FName Source, double Seconds)
	{
		// A fresh box: open any leftover one first so the new one is the only box.
		for (int32 Guard = 0; Guard < 10 && Sim.CountItem(LootBox) > 0 && OpenFirstBox(); ++Guard) {}
		Craft(TEXT("loot_box"));
		const bool bBox = Act(AtCell(LRNames::Load, Cell, LRTest::SlotOf(Sim, LootBox)));
		const bool bSource = Act(AtCell(LRNames::Load, Cell, LRTest::SlotOf(Sim, Source)));
		Sim.Advance(Seconds);
		Act(AtCell(LRNames::Unload, Cell));
		return bBox && bSource;
	};

	// Genesis: three perturbations end inflation, and the ripples' yields reach nucleosynthesis.
	TestTrue(TEXT("starts in inflation"), EpochIs(TEXT("inflation")));
	TestTrue(TEXT("perturb available from the start"), Sim.GetActionStatus(LRNames::Perturb).bRevealed);
	TestFalse(TEXT("craft hidden at the start"), Sim.GetActionStatus(LRNames::Craft).bRevealed);
	for (int32 X = 0; X < 3; ++X)
	{
		TestTrue(*FString::Printf(TEXT("perturb ripple %d"), X + 1), Act(AtCell(LRNames::Perturb, FIntVector(X, 0, 0))));
	}
	TestTrue(TEXT("reheating after three perturbations"), EpochIs(TEXT("reheating")));
	Sim.Advance(120.0);
	TestTrue(TEXT("nucleosynthesis once the ripples gathered enough hydrogen"), EpochIs(TEXT("nucleosynthesis")));
	TestTrue(TEXT("craft revealed"), Sim.GetActionStatus(LRNames::Craft).bRevealed);

	// The host evaporates, and perturbing drew on it too: Feed appears once it's below 90%.
	for (int32 Wait = 0; Wait < 100 && !Sim.GetActionStatus(LRNames::Feed).bRevealed; ++Wait)
	{
		Sim.Advance(30.0);
	}
	TestTrue(TEXT("feed revealed"), Sim.GetActionStatus(LRNames::Feed).bRevealed);
	TestTrue(TEXT("feed the horizon"), Act(FLRActionRequest::Make(LRNames::Feed)));

	TestTrue(TEXT("cheat in materials"), Sim.GiveItem(Hydrogen, 300) && Sim.GiveItem(Helium, 400)
		&& Sim.GiveItem(Carbon, 400) && Sim.GiveItem(Iron, 200));
	TestTrue(TEXT("craft a cache"), Craft(TEXT("loot_box")));
	TestTrue(TEXT("use revealed"), Sim.GetActionStatus(LRNames::Use).bRevealed);
	TestTrue(TEXT("open it"), OpenFirstBox());

	// The ripples sit at (0..2, 0, 0), so the enclosures go on the next row.
	TestTrue(TEXT("carbon enclosure"), Craft(TEXT("carbon_irradiation_enclosure")));
	const FIntVector CarbonCell(0, 2, 0);
	TestTrue(TEXT("deploy it"), Act(AtCell(LRNames::Deploy, CarbonCell, LRTest::SlotOf(Sim, TEXT("carbon_irradiation_enclosure")))));

	TestTrue(TEXT("grow lamp"), Craft(TEXT("grow_lamp")));
	TestTrue(TEXT("visible light run"), Irradiate(CarbonCell, TEXT("grow_lamp"), 35.0));
	TestTrue(TEXT("infrared emitter"), Craft(TEXT("infrared_emitter")));
	// Unload returned the lamp too, so the enclosure is free for the next source.
	TestTrue(TEXT("infrared run"), Irradiate(CarbonCell, TEXT("infrared_emitter"), 35.0));
	TestTrue(TEXT("microwave emitter"), Craft(TEXT("microwave_emitter")));
	TestTrue(TEXT("microwave run"), Irradiate(CarbonCell, TEXT("microwave_emitter"), 25.0));

	TestTrue(TEXT("iron enclosure"), Craft(TEXT("iron_irradiation_enclosure")));
	const FIntVector IronCell(1, 2, 0);
	TestTrue(TEXT("deploy iron enclosure"), Act(AtCell(LRNames::Deploy, IronCell, LRTest::SlotOf(Sim, TEXT("iron_irradiation_enclosure")))));
	TestTrue(TEXT("x-ray tube"), Craft(TEXT("xray_tube")));
	TestTrue(TEXT("x-ray run"), Irradiate(IronCell, TEXT("xray_tube"), 13.0));
	TestTrue(TEXT("gamma source"), Craft(TEXT("gamma_source")));

	// Three ripples and three opened caches: recombination, and the ripples grow on their own.
	TestTrue(TEXT("recombination"), EpochIs(TEXT("recombination")));
	TestTrue(TEXT("perturb retired"), Sim.IsActionRetired(LRNames::Perturb));
	TestFalse(TEXT("perturb hidden"), Sim.GetActionStatus(LRNames::Perturb).bRevealed);
	Sim.Advance(61.0);
	const FLRPlacedEntity* Ripple = Sim.FindPlaced(FIntVector(0, 0, 0));
	TestTrue(TEXT("gravity deepens the ripples after recombination"), Ripple && Ripple->Amplitude >= 2);
	TestFalse(TEXT("the host never ran out"), Sim.IsFrozen());

	for (const FLRRecipeDef& Recipe : Data.Recipes)
	{
		TestTrue(*FString::Printf(TEXT("recipe '%s' reachable"), *Recipe.Id.ToString()), Sim.IsRecipeUnlocked(Recipe.Id));
	}
	for (const FLRActionDef& Action : Data.Actions)
	{
		TestTrue(*FString::Printf(TEXT("action '%s' reachable"), *Action.Name.ToString()), Sim.IsActionUnlocked(Action.Name));
	}
	for (const FLREpochDef& Epoch : Data.Epochs)
	{
		TestTrue(*FString::Printf(TEXT("epoch '%s' reached"), *Epoch.Id.ToString()), Sim.GetEpochIndex() >= Data.FindEpochIndex(Epoch.Id));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLRSimAnnihilateTest, "LootboxRecursion.Simulation.Annihilate", LR_TEST_FLAGS)
bool FLRSimAnnihilateTest::RunTest(const FString& Parameters)
{
	FLRSimulation Sim(LRTest::MakeData(), 1);
	Sim.GiveItem(LRTest::Carbon, 150); // slots 0 (100) and 1 (50)
	Sim.GiveItem(LRTest::Box, 1);      // slot 2
	const int32 BoxId = Sim.GetInventory()[2].InstanceId;

	TestFalse(TEXT("needs a selected slot"), Sim.RequestAction(FLRActionRequest::Make(LRNames::Annihilate)).bSuccess);

	FLRActionRequest Request = FLRActionRequest::Make(LRNames::Annihilate);
	Request.Slot = 0;
	TestTrue(TEXT("annihilate the first stack"), Sim.RequestAction(Request).bSuccess);
	TestEqual(TEXT("only that stack is gone"), Sim.CountItem(LRTest::Carbon), 50);
	TestTrue(TEXT("slot empty"), Sim.GetInventory()[0].IsEmpty());
	TestEqual(TEXT("stat"), Sim.GetStat(TEXT("annihilated:carbon")), 100);

	Request.Slot = 2;
	TestTrue(TEXT("annihilate the box"), Sim.RequestAction(Request).bSuccess);
	TestNull(TEXT("box instance gone too"), Sim.FindLootBox(BoxId));

	Request.Slot = 0;
	TestFalse(TEXT("empty slot refused"), Sim.RequestAction(Request).bSuccess);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLRCosmosEpochTest, "LootboxRecursion.Cosmos.EpochsAdvanceAndTheClockRuns", LR_TEST_FLAGS)
bool FLRCosmosEpochTest::RunTest(const FString& Parameters)
{
	FLRSimulation Sim(LRTest::MakeStableCosmosData(), 1);
	LRTest::FMessageLog Log(Sim);
	TestEqual(TEXT("starts in the first epoch"), Sim.GetEpochIndex(), 0);
	TestTrue(TEXT("the clock starts at the first epoch's start"), Sim.GetCosmicTime() == 1e-36);

	// The clock sweeps four decades (1e-36 to 1e-32 s) in 10 seconds, on a log scale.
	Sim.Advance(5.0);
	TestTrue(TEXT("halfway there is 1e-34 s"), FMath::IsNearlyEqual(FMath::Loge(Sim.GetCosmicTime()) / FMath::Loge(10.0), -34.0, 1e-6));
	Sim.Advance(60.0);
	TestTrue(TEXT("then it waits at the next epoch's start"), Sim.GetCosmicTime() == 1e-32);
	TestEqual(TEXT("still the first epoch"), Sim.GetEpochIndex(), 0);

	TestTrue(TEXT("seed a ripple"), LRTest::SeedAt(Sim, FIntVector(0, 0, 0)));
	TestTrue(TEXT("seed another"), LRTest::SeedAt(Sim, FIntVector(1, 0, 0)));
	TestEqual(TEXT("two seeds begin the second epoch"), Sim.GetEpochIndex(), 1);
	TestTrue(TEXT("the new epoch is announced"), Log.Contains(TEXT("New epoch: hot")));
	TestTrue(TEXT("the clock is at the new epoch's start or later"), Sim.GetCosmicTime() >= 1e-32);
	TestFalse(TEXT("the seed action retires only in the last epoch"), Sim.IsActionRetired(LRTest::Seed));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLRCosmosTimeFormatTest, "LootboxRecursion.Cosmos.FormatCosmicTime", LR_TEST_FLAGS)
bool FLRCosmosTimeFormatTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("inflation"), FLRSimulation::FormatCosmicTime(1e-36), FString(TEXT("10^-36 s")));
	TestEqual(TEXT("reheating"), FLRSimulation::FormatCosmicTime(1e-32), FString(TEXT("10^-32 s")));
	TestEqual(TEXT("under a second"), FLRSimulation::FormatCosmicTime(0.5), FString(TEXT("0.50 s")));
	TestEqual(TEXT("nucleosynthesis"), FLRSimulation::FormatCosmicTime(180.0), FString(TEXT("3 min")));
	TestEqual(TEXT("recombination"), FLRSimulation::FormatCosmicTime(1.2e13), FString(TEXT("380 thousand years")));
	TestEqual(TEXT("first stars"), FLRSimulation::FormatCosmicTime(3.156e15), FString(TEXT("100 million years")));
	TestEqual(TEXT("today"), FLRSimulation::FormatCosmicTime(4.35e17), FString(TEXT("13.8 billion years")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLRStructureSeedTest, "LootboxRecursion.Structures.SeedingCreatesAndDeepensRipples", LR_TEST_FLAGS)
bool FLRStructureSeedTest::RunTest(const FString& Parameters)
{
	FLRSimulation Sim(LRTest::MakeStableCosmosData(), 1);
	const FIntVector Cell(0, 0, 0);
	auto AmplitudeAt = [&Sim](const FIntVector& At)
	{
		const FLRPlacedEntity* Entity = Sim.FindPlaced(At);
		return Entity ? Entity->Amplitude : 0;
	};

	TestFalse(TEXT("needs a cell"), Sim.RequestAction(FLRActionRequest::Make(LRTest::Seed)).bSuccess);
	TestTrue(TEXT("seed a ripple"), LRTest::SeedAt(Sim, Cell));
	const FLRPlacedEntity* Seeded = Sim.FindPlaced(Cell);
	TestTrue(TEXT("a ripple at amplitude 1"), Seeded && Seeded->Item == LRTest::Ripple && Seeded->Amplitude == 1);
	TestTrue(TEXT("deepen it"), LRTest::SeedAt(Sim, Cell));
	TestTrue(TEXT("and again"), LRTest::SeedAt(Sim, Cell));
	TestEqual(TEXT("amplitude 3"), AmplitudeAt(Cell), 3);
	TestFalse(TEXT("no deeper than maxAmplitude"), LRTest::SeedAt(Sim, Cell));
	TestTrue(TEXT("each perturbation drew on the host"), FMath::IsNearlyEqual(Sim.GetHostMass(), 0.7, 1e-6));

	Sim.GiveItem(LRTest::Enclosure, 1);
	FLRActionRequest Deploy = LRTest::AtCell(LRNames::Deploy, FIntVector(1, 0, 0));
	Deploy.Slot = LRTest::SlotOf(Sim, LRTest::Enclosure);
	TestTrue(TEXT("deploy an enclosure next door"), Sim.RequestAction(Deploy).bSuccess);
	TestFalse(TEXT("can't seed into an occupied cell"), LRTest::SeedAt(Sim, FIntVector(1, 0, 0)));
	TestFalse(TEXT("ripples can't be recalled"), Sim.RequestAction(LRTest::AtCell(LRNames::Recall, Cell)).bSuccess);
	TestNotNull(TEXT("the ripple is still there"), Sim.FindPlaced(Cell));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLRStructureYieldTest, "LootboxRecursion.Structures.RipplesYieldPerEpochAndGrowLater", LR_TEST_FLAGS)
bool FLRStructureYieldTest::RunTest(const FString& Parameters)
{
	FLRSimulation Sim(LRTest::MakeStableCosmosData(), 1);
	const FIntVector Cell(0, 0, 0);
	auto AmplitudeAt = [&Sim](const FIntVector& At)
	{
		const FLRPlacedEntity* Entity = Sim.FindPlaced(At);
		return Entity ? Entity->Amplitude : 0;
	};

	TestTrue(TEXT("seed"), LRTest::SeedAt(Sim, Cell));
	Sim.Advance(30.0);
	TestEqual(TEXT("no matter yet in the first epoch"), Sim.CountItem(LRTest::Hydrogen), 0);

	TestTrue(TEXT("deepen it (the second seed begins the hot epoch)"), LRTest::SeedAt(Sim, Cell));
	TestEqual(TEXT("hot epoch"), Sim.GetEpochIndex(), 1);
	Sim.Advance(10.0);
	TestEqual(TEXT("one yield, one roll per amplitude"), Sim.CountItem(LRTest::Hydrogen), 10);
	TestEqual(TEXT("yields count as gained"), Sim.GetStat(TEXT("gained:hydrogen")), 10);
	TestEqual(TEXT("no growth before the clear epoch"), AmplitudeAt(Cell), 2);

	Sim.Advance(10.0);
	TestEqual(TEXT("20 hydrogen gained: the clear epoch"), Sim.GetEpochIndex(), 2);
	TestTrue(TEXT("the seed action retired"), Sim.IsActionRetired(LRTest::Seed));
	TestFalse(TEXT("retired actions are hidden"), Sim.GetActionStatus(LRTest::Seed).bRevealed);
	TestFalse(TEXT("and refused"), LRTest::SeedAt(Sim, FIntVector(2, 0, 0)));

	// Growth comes first in a tick, so all three yields here are at amplitude 3 (15 each).
	Sim.Advance(30.0);
	TestEqual(TEXT("gravity deepened the ripple"), AmplitudeAt(Cell), 3);
	TestEqual(TEXT("three yields at amplitude 3"), Sim.CountItem(LRTest::Hydrogen), 20 + 3 * 15);
	Sim.Advance(60.0);
	TestEqual(TEXT("no deeper than maxAmplitude"), AmplitudeAt(Cell), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLRHostTest, "LootboxRecursion.Cosmos.HostEvaporatesFreezesAndFeeds", LR_TEST_FLAGS)
bool FLRHostTest::RunTest(const FString& Parameters)
{
	FLRGameData Data = LRTest::MakeCosmosData(); // lifetime 1000s, perturb 0.1, feed 0.3
	FLRRequirement Low;
	Low.Check = LRNames::CheckHost;
	Low.Condition = TEXT("lte");
	Low.Value = 90;
	FLRActionDef* FeedDef = Data.Actions.FindByPredicate([](const FLRActionDef& Action) { return Action.Name == LRNames::Feed; });
	if (!TestNotNull(TEXT("fixture has feed"), FeedDef))
	{
		return false;
	}
	FeedDef->RevealRequirements.Add(Low);

	// Mass cubed falls linearly: 27.1% of the lifetime leaves 0.729 = 0.9^3, whatever the step size.
	FLRSimulation Stepped(Data, 1);
	for (int32 Step = 0; Step < 271; ++Step)
	{
		Stepped.Advance(1.0);
	}
	FLRSimulation Sim(Data, 1);
	LRTest::FMessageLog Log(Sim);
	TestFalse(TEXT("feed hidden at full mass"), Sim.GetActionStatus(LRNames::Feed).bRevealed);
	Sim.Advance(271.0);
	TestTrue(TEXT("0.9 of the mass left"), FMath::IsNearlyEqual(Sim.GetHostMass(), 0.9, 1e-9));
	TestTrue(TEXT("independent of the step size"), FMath::IsNearlyEqual(Stepped.GetHostMass(), Sim.GetHostMass(), 1e-9));
	TestTrue(TEXT("the host check reveals feed at 90%"), Sim.GetActionStatus(LRNames::Feed).bRevealed);

	TestTrue(TEXT("seed"), LRTest::SeedAt(Sim, FIntVector(0, 0, 0)));
	TestTrue(TEXT("seed again (hot epoch: the ripple yields)"), LRTest::SeedAt(Sim, FIntVector(0, 0, 0)));
	Sim.Advance(400.0); // 0.7^3 = 0.343 is less than 400/1000
	TestTrue(TEXT("evaporated"), Sim.IsFrozen());
	TestTrue(TEXT("announced"), Log.Contains(TEXT("evaporated")));
	TestEqual(TEXT("nothing yields while frozen"), Sim.CountItem(LRTest::Hydrogen), 0);
	Sim.Advance(50.0);
	TestEqual(TEXT("still nothing"), Sim.CountItem(LRTest::Hydrogen), 0);
	TestFalse(TEXT("perturbing needs a host"), LRTest::SeedAt(Sim, FIntVector(1, 0, 0)));

	TestTrue(TEXT("feed"), Sim.RequestAction(FLRActionRequest::Make(LRNames::Feed)).bSuccess);
	TestFalse(TEXT("no longer frozen"), Sim.IsFrozen());
	TestTrue(TEXT("the universe stirs"), Log.Contains(TEXT("stirs again")));
	Sim.Advance(10.0);
	TestEqual(TEXT("yields resume"), Sim.CountItem(LRTest::Hydrogen), 10);

	for (int32 Index = 0; Index < 3; ++Index)
	{
		Sim.RequestAction(FLRActionRequest::Make(LRNames::Feed));
	}
	TestTrue(TEXT("never above full mass"), Sim.GetHostMass() == 1.0);
	TestFalse(TEXT("feeding a full host is refused"), Sim.RequestAction(FLRActionRequest::Make(LRNames::Feed)).bSuccess);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLRCosmosPlasmaTest, "LootboxRecursion.Cosmos.PlasmaFadesBetweenEpochs", LR_TEST_FLAGS)
bool FLRCosmosPlasmaTest::RunTest(const FString& Parameters)
{
	FLRSimulation Sim(LRTest::MakeStableCosmosData(), 1);
	TestEqual(TEXT("the first epoch's plasma"), Sim.GetPlasmaOpacity(), 0.2f);
	LRTest::SeedAt(Sim, FIntVector(0, 0, 0));
	LRTest::SeedAt(Sim, FIntVector(0, 0, 0));
	TestEqual(TEXT("hot epoch"), Sim.GetEpochIndex(), 1);
	TestEqual(TEXT("the fade starts from the old value"), Sim.GetPlasmaOpacity(), 0.2f);
	Sim.Advance(FLRSimulation::PlasmaFadeSeconds / 2.0);
	TestEqual(TEXT("halfway"), Sim.GetPlasmaOpacity(), 0.6f, 1e-4f);
	Sim.Advance(FLRSimulation::PlasmaFadeSeconds);
	TestEqual(TEXT("then the new epoch's plasma"), Sim.GetPlasmaOpacity(), 1.f, 1e-4f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLRCosmosSaveTest, "LootboxRecursion.Cosmos.SurvivesSaveLoad", LR_TEST_FLAGS)
bool FLRCosmosSaveTest::RunTest(const FString& Parameters)
{
	const FLRGameData Data = LRTest::MakeCosmosData();
	FLRSimulation Sim(Data, 3);
	LRTest::SeedAt(Sim, FIntVector(0, 0, 0));
	LRTest::SeedAt(Sim, FIntVector(0, 0, 0));
	Sim.Advance(5.0);

	FLRSimulation Loaded(Data, 0);
	TestTrue(TEXT("load"), Loaded.Load(Sim.Save()));
	TestEqual(TEXT("epoch"), Loaded.GetEpochIndex(), 1);
	TestTrue(TEXT("host mass"), Loaded.GetHostMass() == Sim.GetHostMass());
	TestTrue(TEXT("cosmic time"), Loaded.GetCosmicTime() == Sim.GetCosmicTime());
	TestEqual(TEXT("plasma fade"), Loaded.GetPlasmaOpacity(), Sim.GetPlasmaOpacity());
	const FLRPlacedEntity* Ripple = Loaded.FindPlaced(FIntVector(0, 0, 0));
	TestTrue(TEXT("ripple amplitude"), Ripple && Ripple->Amplitude == 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLRHexDistanceTest, "LootboxRecursion.Grid.HexDistanceAndRadius", LR_TEST_FLAGS)
bool FLRHexDistanceTest::RunTest(const FString& Parameters)
{
	const FIntVector Origin(0, 0, 0);
	const TArray<FIntVector> Neighbors = FLRHexGrid::Neighbors(Origin);
	TestEqual(TEXT("six neighbours"), Neighbors.Num(), 6);
	for (const FIntVector& Neighbor : Neighbors)
	{
		TestEqual(TEXT("each one step away"), FLRHexGrid::Distance(Origin, Neighbor), 1);
	}
	TestEqual(TEXT("a known distance"), FLRHexGrid::Distance(Origin, FIntVector(3, -1, 0)), 3);
	TestEqual(TEXT("distance is symmetric"), FLRHexGrid::Distance(FIntVector(3, -1, 0), Origin), 3);
	TestEqual(TEXT("layers add steps"), FLRHexGrid::Distance(Origin, FIntVector(1, 0, 2)), 3);

	for (int32 Radius = 0; Radius <= 4; ++Radius)
	{
		const FIntVector Center(5, -2, 1);
		const TArray<FIntVector> Cells = FLRHexGrid::CellsInRadius(Center, Radius);
		TestEqual(*FString::Printf(TEXT("radius %d count"), Radius), Cells.Num(), FLRHexGrid::CountInRadius(Radius));
		TSet<FIntVector> Unique(Cells);
		TestEqual(*FString::Printf(TEXT("radius %d has no repeats"), Radius), Unique.Num(), Cells.Num());
		const bool bAllInside = !Cells.ContainsByPredicate([&Center, Radius](const FIntVector& Cell)
		{
			return Cell.Z != Center.Z || FLRHexGrid::Distance(Center, Cell) > Radius;
		});
		TestTrue(*FString::Printf(TEXT("radius %d stays in range and in the layer"), Radius), bAllInside);
	}
	TestEqual(TEXT("radius 2 is 19 cells"), FLRHexGrid::CountInRadius(2), 19);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLRHexLayoutTest, "LootboxRecursion.Grid.HexLayoutAndPicking", LR_TEST_FLAGS)
bool FLRHexLayoutTest::RunTest(const FString& Parameters)
{
	constexpr double Spacing = 100.0;
	const FIntVector Origin(0, 0, 0);
	for (const FIntVector& Neighbor : FLRHexGrid::Neighbors(Origin))
	{
		const double Gap = FVector::Dist(FLRHexGrid::ToLocal(Origin, Spacing), FLRHexGrid::ToLocal(Neighbor, Spacing));
		TestTrue(TEXT("neighbouring centres are Spacing apart"), FMath::IsNearlyEqual(Gap, Spacing, 1e-6));
	}
	TestTrue(TEXT("+Q runs along +X"), FLRHexGrid::ToLocal(FIntVector(1, 0, 0), Spacing).Equals(FVector(Spacing, 0.0, 0.0), 1e-6));
	TestTrue(TEXT("layers are Spacing apart"), FMath::IsNearlyEqual(FLRHexGrid::ToLocal(FIntVector(0, 0, 3), Spacing).Z, 3.0 * Spacing, 1e-6));
	for (int32 Index = 0; Index < 6; ++Index)
	{
		TestTrue(TEXT("corners are one edge length from the centre"),
			FMath::IsNearlyEqual(FLRHexGrid::Corner(Index, Spacing).Size(), FLRHexGrid::CornerRadius(Spacing), 1e-6));
	}
	TestTrue(TEXT("corner indices wrap"), FLRHexGrid::Corner(7, Spacing).Equals(FLRHexGrid::Corner(1, Spacing), 1e-6));

	// Every cell's centre, and points well inside its hexagon, pick that cell.
	const double Apothem = 0.5 * Spacing;
	for (const FIntVector& Cell : FLRHexGrid::CellsInRadius(FIntVector(0, 0, 2), 5))
	{
		const FVector Centre = FLRHexGrid::ToLocal(Cell, Spacing);
		bool bAllPicked = FLRHexGrid::FromLocal(Centre, Spacing, Cell.Z) == Cell;
		for (int32 Index = 0; Index < 6; ++Index)
		{
			const FVector Towards = FLRHexGrid::ToLocal(Cell + FLRHexGrid::Directions[Index], Spacing) - Centre;
			bAllPicked &= FLRHexGrid::FromLocal(Centre + Towards * 0.45, Spacing, Cell.Z) == Cell;
			bAllPicked &= FLRHexGrid::FromLocal(Centre + FLRHexGrid::Corner(Index, Spacing) * 0.9, Spacing, Cell.Z) == Cell;
		}
		TestTrue(*FString::Printf(TEXT("cell %s picks itself"), *FLRSimulation::DescribeCell(Cell)), bAllPicked);
	}

	// Just past a flat side is the neighbour on the other side.
	const FVector PastEdge = FLRHexGrid::ToLocal(Origin, Spacing) + FVector(Apothem + 1.0, 0.0, 0.0);
	TestTrue(TEXT("past the +X side is the +Q neighbour"), FLRHexGrid::FromLocal(PastEdge, Spacing, 0) == FIntVector(1, 0, 0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLRBlackHoleRenderTest, "LootboxRecursion.Cosmos.BlackHoleRendersAndAnimates", LR_TEST_FLAGS)
bool FLRBlackHoleRenderTest::RunTest(const FString& Parameters)
{
	FLRBlackHoleRenderer Renderer;
	FString Error;
	if (!Renderer.LoadFromFile(FPaths::ProjectContentDir() / TEXT("Cosmos/BlackHole.lrbh"), Error))
	{
		AddError(Error);
		return false;
	}
	const int32 W = Renderer.GetWidth();
	const int32 H = Renderer.GetHeight();

	TArray<FColor> First;
	TArray<FColor> Later;
	Renderer.Render(0.0, First);
	Renderer.Render(1.0, Later);
	TestEqual(TEXT("pixel count"), First.Num(), W * H);

	const FColor Centre = First[(H / 2) * W + W / 2];
	TestTrue(TEXT("the shadow is opaque black"), Centre.A == 255 && Centre.R < 8 && Centre.G < 8 && Centre.B < 8);
	TestEqual(TEXT("empty space in the corner is transparent"), static_cast<int32>(First[0].A), 0);

	// Visibly animated, not just changing by a shade: in one second, most of the bright disk
	// moves by more than 12/255 on screen (colour times alpha). The old, smoother bake managed
	// about a quarter, which looked static in game; the current one manages about two thirds.
	auto OnScreen = [](const FColor& Pixel, int32 Channel)
	{
		const int32 Value = Channel == 0 ? Pixel.R : (Channel == 1 ? Pixel.G : Pixel.B);
		return Value * Pixel.A / 255;
	};
	int32 Lit = 0;
	int32 Moved = 0;
	for (int32 Index = 0; Index < First.Num(); ++Index)
	{
		if (First[Index].R <= 60 || First[Index].A <= 100) // warm, mostly opaque
		{
			continue;
		}
		++Lit;
		for (int32 Channel = 0; Channel < 3; ++Channel)
		{
			if (FMath::Abs(OnScreen(First[Index], Channel) - OnScreen(Later[Index], Channel)) > 12)
			{
				++Moved;
				break;
			}
		}
	}
	TestTrue(TEXT("the disk is visible"), Lit > W * H / 50);
	TestTrue(TEXT("the disk visibly animates"), Moved * 10 > Lit * 4);

	TArray<uint8> Garbage = { 'N', 'O', 'P', 'E' };
	FLRBlackHoleRenderer Bad;
	TestFalse(TEXT("rejects a bad file"), Bad.LoadFromBytes(Garbage, Error));
	return true;
}

#undef LR_TEST_FLAGS

#endif // WITH_DEV_AUTOMATION_TESTS
