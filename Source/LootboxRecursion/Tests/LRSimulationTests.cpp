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
	const FName Lamp(TEXT("lamp"));
	const FName XRayTube(TEXT("xray_tube"));
	const FName GammaSource(TEXT("gamma_source"));

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
		Data.Actions.Add(MakeAction(LRNames::Load, 0.f, 0.f));
		Data.Actions.Add(MakeAction(LRNames::Unload, 0.f, 0.f));

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

		FLRActionDef Scan = MakeAction(TEXT("scan"), 1.f, 0.f, TEXT("inject"));
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
	// Walks the real Content/Data tech tree from a new game to the last unlock, so a data
	// change that creates a dead end fails here. Materials are cheated in (GiveItem doesn't
	// count toward stats); everything else is played through real actions.
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
	const FName Carbon(TEXT("carbon"));
	const FName Iron(TEXT("iron"));
	const FName LootBox(TEXT("loot_box"));

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
		while (Sim.CountItem(LootBox) > 0 && OpenFirstBox()) {}
		Craft(TEXT("loot_box"));
		const bool bBox = Act(AtCell(LRNames::Load, Cell, LRTest::SlotOf(Sim, LootBox)));
		const bool bSource = Act(AtCell(LRNames::Load, Cell, LRTest::SlotOf(Sim, Source)));
		Sim.Advance(Seconds);
		Act(AtCell(LRNames::Unload, Cell));
		return bBox && bSource;
	};

	TestTrue(TEXT("inject available from the start"), Sim.GetActionStatus(LRNames::Inject).bRevealed);
	TestFalse(TEXT("craft hidden at the start"), Sim.GetActionStatus(LRNames::Craft).bRevealed);
	Act(FLRActionRequest::Make(LRNames::Inject));
	Act(FLRActionRequest::Make(LRNames::Inject));
	TestTrue(TEXT("craft revealed after injecting"), Sim.GetActionStatus(LRNames::Craft).bRevealed);

	Sim.GiveItem(Carbon, 1000);
	Sim.GiveItem(Iron, 1000);
	TestTrue(TEXT("craft a loot box"), Craft(TEXT("loot_box")));
	TestTrue(TEXT("use revealed"), Sim.GetActionStatus(LRNames::Use).bRevealed);
	TestTrue(TEXT("open it"), OpenFirstBox());

	TestTrue(TEXT("carbon enclosure"), Craft(TEXT("carbon_irradiation_enclosure")));
	const FIntVector CarbonCell(0, 0, 0);
	TestTrue(TEXT("deploy it"), Act(AtCell(LRNames::Deploy, CarbonCell, LRTest::SlotOf(Sim, TEXT("carbon_irradiation_enclosure")))));

	TestTrue(TEXT("grow lamp"), Craft(TEXT("grow_lamp")));
	TestTrue(TEXT("visible light run"), Irradiate(CarbonCell, TEXT("grow_lamp"), 35.0));
	TestTrue(TEXT("infrared emitter"), Craft(TEXT("infrared_emitter")));
	// Unload returned the lamp too, so the enclosure is free for the next source.
	TestTrue(TEXT("infrared run"), Irradiate(CarbonCell, TEXT("infrared_emitter"), 35.0));
	TestTrue(TEXT("microwave emitter"), Craft(TEXT("microwave_emitter")));
	TestTrue(TEXT("microwave run"), Irradiate(CarbonCell, TEXT("microwave_emitter"), 25.0));

	TestTrue(TEXT("iron enclosure"), Craft(TEXT("iron_irradiation_enclosure")));
	const FIntVector IronCell(1, 0, 0);
	TestTrue(TEXT("deploy iron enclosure"), Act(AtCell(LRNames::Deploy, IronCell, LRTest::SlotOf(Sim, TEXT("iron_irradiation_enclosure")))));
	TestTrue(TEXT("x-ray tube"), Craft(TEXT("xray_tube")));
	TestTrue(TEXT("x-ray run"), Irradiate(IronCell, TEXT("xray_tube"), 13.0));
	TestTrue(TEXT("gamma source"), Craft(TEXT("gamma_source")));

	for (const FLRRecipeDef& Recipe : Data.Recipes)
	{
		TestTrue(*FString::Printf(TEXT("recipe '%s' reachable"), *Recipe.Id.ToString()), Sim.IsRecipeUnlocked(Recipe.Id));
	}
	for (const FLRActionDef& Action : Data.Actions)
	{
		TestTrue(*FString::Printf(TEXT("action '%s' reachable"), *Action.Name.ToString()), Sim.IsActionUnlocked(Action.Name));
	}
	return true;
}

#undef LR_TEST_FLAGS

#endif // WITH_DEV_AUTOMATION_TESTS
