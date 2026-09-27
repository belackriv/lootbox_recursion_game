// Automation tests for the simulation. Rails equivalent: test/models/*_test.rb.
//
// Run in the editor: Tools > Session Frontend > Automation, filter "LootboxRecursion".
// Or headless (see README): UnrealEditor-Cmd <project> -ExecCmds="Automation RunTests LootboxRecursion;Quit" ...

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Data/LRGameData.h"
#include "Simulation/LRSimulation.h"

#define LR_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

namespace LRTest
{

	const FName Carbon(TEXT("carbon"));
	const FName Iron(TEXT("iron"));
	const FName Box(TEXT("loot_box"));
	const FName MysteryBox(TEXT("mystery_box"));
	const FName Enclosure(TEXT("enclosure"));

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

		AddTable(Data, TEXT("box"), 2, Carbon, 10);       // always 2 x 10 carbon
		AddTable(Data, TEXT("inject"), 1, Carbon, 30);    // always 30 carbon
		AddTable(Data, TEXT("nested"), 1, Box, 1);        // a box inside a box

		FLRRecipeDef BoxRecipe;
		BoxRecipe.Id = Box;
		BoxRecipe.Label = TEXT("Loot Box");
		BoxRecipe.Output = Box;
		BoxRecipe.Cost = { FLRItemAmount(Carbon, 50), FLRItemAmount(Iron, 50) };
		Data.Recipes.Add(BoxRecipe);

		Data.Actions.Add(MakeAction(LRNames::Inject, 5.f, 5.f, TEXT("inject")));
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
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLRSimInjectTest, "LootboxRecursion.Simulation.InjectCastAndCooldown", LR_TEST_FLAGS)
bool FLRSimInjectTest::RunTest(const FString& Parameters)
{
	FLRSimulation Sim(LRTest::MakeData(), 42);

	const FLRActionResult Started = Sim.RequestAction(FLRActionRequest::Make(LRNames::Inject));
	TestTrue(TEXT("request accepted"), Started.bSuccess && Started.bStarted);
	TestTrue(TEXT("status shows casting"), Sim.GetActionStatus(LRNames::Inject).bCasting);

	const FLRActionResult Again = Sim.RequestAction(FLRActionRequest::Make(LRNames::Inject));
	TestFalse(TEXT("second request rejected while casting"), Again.bSuccess);

	Sim.Advance(4.9);
	TestEqual(TEXT("no carbon before cast completes"), Sim.CountItem(LRTest::Carbon), 0);

	bool bCompleted = false;
	Sim.OnActionCompleted.AddLambda([&bCompleted](const FLRActionResult& Result) { bCompleted = Result.bSuccess; });
	Sim.Advance(0.2);
	TestTrue(TEXT("completion broadcast"), bCompleted);
	TestEqual(TEXT("carbon after cast"), Sim.CountItem(LRTest::Carbon), 30);
	TestFalse(TEXT("cooldown over (cooldown == cast time)"), Sim.GetActionStatus(LRNames::Inject).bOnCooldown);
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
	Sim.RequestAction(FLRActionRequest::Make(LRNames::Inject)); // mid-cast when saved
	Sim.Advance(2.0);

	const FLRSaveData Save = Sim.Save();

	FLRSimulation Loaded(LRTest::MakeData(), 0);
	TestTrue(TEXT("load"), Loaded.Load(Save));
	TestNotNull(TEXT("placed entity restored"), Loaded.FindPlaced(FIntVector(12, -1, 3)));
	TestTrue(TEXT("cast restored"), Loaded.GetActionStatus(LRNames::Inject).bCasting);

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

	for (const FName Action : { LRNames::Inject, LRNames::Craft, LRNames::Use, LRNames::Deploy, LRNames::Recall, LRNames::SortInventory })
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
	return true;
}

#undef LR_TEST_FLAGS

#endif // WITH_DEV_AUTOMATION_TESTS
