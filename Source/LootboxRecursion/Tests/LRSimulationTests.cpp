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
	const FName DoubleBox(TEXT("double_box"));
	const FName Irradiator(TEXT("irradiator"));
	const FName Lamp(TEXT("lamp"));
	const FName XRayTube(TEXT("xray_tube"));
	const FName GammaSource(TEXT("gamma_source"));
	/** A plain loot action: any action with a lootTable rolls it into the selected cell. */
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

	FLRRecipeDef& AddRecipe(FLRGameData& Data, FName Id, FName Output, const TArray<FLRItemAmount>& Cost)
	{
		FLRRecipeDef Recipe;
		Recipe.Id = Id;
		Recipe.Label = Id.ToString();
		Recipe.Output = Output;
		Recipe.Cost = Cost;
		return Data.Recipes.Add_GetRef(Recipe);
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

	/**
	 * Small, fully deterministic data set (every loot range is a single value). Building,
	 * opening and dismantling are instant; reach is one cell.
	 */
	FLRGameData MakeData()
	{
		FLRGameData Data;
		Data.ReachRadius = 1;
		AddItemDef(Data, Carbon, TEXT("Carbon"), LRNames::CategoryMaterial, 100);
		AddItemDef(Data, Iron, TEXT("Iron"), LRNames::CategoryMaterial, 100);
		AddItemDef(Data, Box, TEXT("Loot Box"), LRNames::CategoryLootBox, 1, TEXT("box"));
		AddItemDef(Data, MysteryBox, TEXT("Mystery Box"), LRNames::CategoryLootBox, 1, TEXT("nested"));
		AddItemDef(Data, DoubleBox, TEXT("Double Box"), LRNames::CategoryLootBox, 1, TEXT("double"));
		AddItemDef(Data, Irradiator, TEXT("Irradiator"), LRNames::CategoryPlaceable, 1);
		Data.Items[Irradiator].MaxRadiationTier = 5;
		Data.Items[Irradiator].MaxExposureStacks = 2;
		Data.Items[Irradiator].ExposureSeconds = 10.f;

		// Radiation: light doubles carbon amounts, x-rays reveal, gamma is too strong for the irradiator.
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
		AddTable(Data, TEXT("double"), 2, Box, 1);        // two boxes inside a box

		AddRecipe(Data, Box, Box, { FLRItemAmount(Carbon, 50), FLRItemAmount(Iron, 50) }).Label = TEXT("Loot Box");
		AddRecipe(Data, MysteryBox, MysteryBox, { FLRItemAmount(Carbon, 1) });
		AddRecipe(Data, DoubleBox, DoubleBox, { FLRItemAmount(Carbon, 1) });
		AddRecipe(Data, Irradiator, Irradiator, { FLRItemAmount(Carbon, 10) });
		AddRecipe(Data, Lamp, Lamp, { FLRItemAmount(Carbon, 5) });
		AddRecipe(Data, XRayTube, XRayTube, { FLRItemAmount(Carbon, 1) });
		AddRecipe(Data, GammaSource, GammaSource, { FLRItemAmount(Carbon, 1) });

		Data.Actions.Add(MakeAction(Gather, 5.f, 5.f, TEXT("gather")));
		Data.Actions.Add(MakeAction(LRNames::Craft, 0.f, 0.f));
		Data.Actions.Add(MakeAction(LRNames::Use, 0.f, 0.f));
		Data.Actions.Add(MakeAction(LRNames::Dismantle, 0.f, 0.f));

		// Tech tree: a recipe unlocked by building a loot box, one chained off it, and an
		// action revealed by a cache sitting in the world.
		FLRRequirement CraftedABox;
		CraftedABox.Check = LRNames::CheckStat;
		CraftedABox.Id = TEXT("crafted:loot_box");
		CraftedABox.Condition = TEXT("gte");
		CraftedABox.Value = 1;
		AddRecipe(Data, TEXT("gated_irradiator"), Irradiator, { FLRItemAmount(Carbon, 10) }).RevealRequirements.Add(CraftedABox);

		FLRRequirement AfterGated;
		AfterGated.Check = LRNames::CheckUnlocked;
		AfterGated.Id = TEXT("recipe:gated_irradiator");
		AfterGated.Condition = TEXT("eq");
		AfterGated.Value = 1;
		AddRecipe(Data, TEXT("chained_lamp"), Lamp, { FLRItemAmount(Carbon, 5) }).RevealRequirements.Add(AfterGated);

		FLRActionDef Scan = MakeAction(TEXT("scan"), 1.f, 0.f, TEXT("gather"));
		FLRRequirement CacheInWorld;
		CacheInWorld.Check = LRNames::CheckPlaced;
		CacheInWorld.Category = LRNames::CategoryLootBox;
		Scan.RevealRequirements.Add(CacheInWorld);
		Data.Actions.Add(Scan);
		return Data;
	}

	FLRActionRequest AtCell(FName Action, const FIntVector& Cell)
	{
		FLRActionRequest Request = FLRActionRequest::Make(Action);
		Request.Cell = Cell;
		Request.bHasCell = true;
		return Request;
	}

	FLRActionResult Build(FLRSimulation& Sim, FName Recipe, const FIntVector& Cell)
	{
		FLRActionRequest Request = AtCell(LRNames::Craft, Cell);
		Request.Choice = Recipe;
		return Sim.RequestAction(Request);
	}

	bool Open(FLRSimulation& Sim, const FIntVector& Cell)
	{
		return Sim.RequestAction(AtCell(LRNames::Use, Cell)).bSuccess;
	}

	bool Dismantle(FLRSimulation& Sim, const FIntVector& Cell)
	{
		return Sim.RequestAction(AtCell(LRNames::Dismantle, Cell)).bSuccess;
	}

	/** An irradiator at Cell, paid for with matter given to it. */
	void BuildIrradiator(FLRSimulation& Sim, const FIntVector& Cell)
	{
		Sim.GiveMatter(Cell, Carbon, 10);
		Build(Sim, Irradiator, Cell);
	}

	/** Build a cache (or source) into the irradiator at Cell, paying with matter given to it. */
	bool BuildInto(FLRSimulation& Sim, FName Recipe, const FIntVector& Cell)
	{
		const FLRRecipeDef* Def = Sim.GetData().FindRecipe(Recipe);
		for (const FLRItemAmount& Cost : Def ? Def->Cost : TArray<FLRItemAmount>())
		{
			Sim.GiveMatter(Cell, Cost.Item, Cost.Count);
		}
		return Build(Sim, Recipe, Cell).bSuccess;
	}

	/** The instance id of the cache at Cell (an entity or in an irradiator), or 0. */
	int32 CacheIdAt(const FLRSimulation& Sim, const FIntVector& Cell)
	{
		const FLRLootBoxInstance* Cache = Sim.FindCacheAt(Cell);
		return Cache ? Cache->InstanceId : 0;
	}

	int32 StacksAt(const FLRSimulation& Sim, const FIntVector& Cell)
	{
		const FLRLootBoxInstance* Cache = Sim.FindCacheAt(Cell);
		return Cache ? Cache->Modifiers.Num() : -1;
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
	const FIntVector Cell(2, -1, 0);

	TestFalse(TEXT("needs a cell"), Sim.RequestAction(FLRActionRequest::Make(LRTest::Gather)).bSuccess);
	TestFalse(TEXT("a refused request doesn't start the cooldown"), Sim.GetActionStatus(LRTest::Gather).bOnCooldown);

	const FLRActionResult Started = Sim.RequestAction(LRTest::AtCell(LRTest::Gather, Cell));
	TestTrue(TEXT("request accepted"), Started.bSuccess && Started.bStarted);
	TestTrue(TEXT("status shows casting"), Sim.GetActionStatus(LRTest::Gather).bCasting);
	TestFalse(TEXT("second request rejected while casting"), Sim.RequestAction(LRTest::AtCell(LRTest::Gather, Cell)).bSuccess);

	Sim.Advance(4.9);
	TestEqual(TEXT("no carbon before the cast completes"), Sim.GetMatter(Cell, LRTest::Carbon), 0);

	bool bCompleted = false;
	Sim.OnActionCompleted.AddLambda([&bCompleted](const FLRActionResult& Result) { bCompleted = Result.bSuccess; });
	Sim.Advance(0.2);
	TestTrue(TEXT("completion broadcast"), bCompleted);
	TestEqual(TEXT("carbon lands in the cell"), Sim.GetMatter(Cell, LRTest::Carbon), 30);
	TestFalse(TEXT("cooldown over (cooldown == cast time)"), Sim.GetActionStatus(LRTest::Gather).bOnCooldown);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLRMatterCellsTest, "LootboxRecursion.Matter.CellsHoldMatter", LR_TEST_FLAGS)
bool FLRMatterCellsTest::RunTest(const FString& Parameters)
{
	FLRSimulation Sim(LRTest::MakeData(), 1); // reach 1
	const FIntVector Here(0, 0, 0);
	TestTrue(TEXT("give carbon"), Sim.GiveMatter(Here, LRTest::Carbon, 30));
	Sim.GiveMatter(FIntVector(1, 0, 0), LRTest::Carbon, 40);   // a neighbour
	Sim.GiveMatter(FIntVector(3, 0, 0), LRTest::Carbon, 100);  // out of reach
	Sim.GiveMatter(FIntVector(0, 0, 1), LRTest::Carbon, 500);  // the layer above
	TestFalse(TEXT("only materials are matter"), Sim.GiveMatter(Here, LRTest::Box, 1));
	TestFalse(TEXT("unknown items aren't"), Sim.GiveMatter(Here, TEXT("unobtainium"), 1));

	TestEqual(TEXT("per cell"), Sim.GetMatter(Here, LRTest::Carbon), 30);
	TestEqual(TEXT("whole universe"), Sim.GetTotalMatter(LRTest::Carbon), 670);
	TestEqual(TEXT("within reach: this cell and its neighbours, this layer only"), Sim.GetMatterInReach(Here, LRTest::Carbon), 70);
	TestEqual(TEXT("nothing of other materials"), Sim.GetMatterInReach(Here, LRTest::Iron), 0);

	FLRSimulation Loaded(LRTest::MakeData(), 0);
	TestTrue(TEXT("load"), Loaded.Load(Sim.Save()));
	TestEqual(TEXT("matter survives save/load"), Loaded.GetMatter(FIntVector(3, 0, 0), LRTest::Carbon), 100);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLRMatterReachTest, "LootboxRecursion.Matter.CostsArePaidFromReach", LR_TEST_FLAGS)
bool FLRMatterReachTest::RunTest(const FString& Parameters)
{
	FLRSimulation Sim(LRTest::MakeData(), 1); // reach 1
	const FIntVector Here(0, 0, 0);
	const FIntVector Next(1, 0, 0);
	const FIntVector Far(3, 0, 0);
	Sim.GiveMatter(Here, LRTest::Carbon, 30);
	Sim.GiveMatter(Next, LRTest::Carbon, 40);
	Sim.GiveMatter(Next, LRTest::Iron, 50);
	Sim.GiveMatter(Far, LRTest::Carbon, 100);

	const FLRActionResult Built = LRTest::Build(Sim, LRTest::Box, Here);
	TestTrue(TEXT("a box costs 50 carbon and 50 iron, all within reach"), Built.bSuccess);
	TestTrue(TEXT("it sits in the cell"), LRTest::CacheIdAt(Sim, Here) != 0);
	TestEqual(TEXT("the build cell pays first"), Sim.GetMatter(Here, LRTest::Carbon), 0);
	TestEqual(TEXT("then its neighbours"), Sim.GetMatter(Next, LRTest::Carbon), 20);
	TestEqual(TEXT("iron from the neighbour"), Sim.GetMatter(Next, LRTest::Iron), 0);
	TestEqual(TEXT("out of reach is untouched"), Sim.GetMatter(Far, LRTest::Carbon), 100);
	TestEqual(TEXT("empty cells drop out"), Sim.GetAllMatter().Num(), 2);

	const FLRActionResult Poor = LRTest::Build(Sim, LRTest::Box, FIntVector(0, 1, 0));
	TestFalse(TEXT("not enough within reach"), Poor.bSuccess);
	TestTrue(TEXT("reason"), Poor.Reason == FName(TEXT("insufficient_materials")));
	TestFalse(TEXT("no cell, no build"), Sim.RequestAction(FLRActionRequest::Make(LRNames::Craft)).bSuccess);

	Sim.GiveMatter(Here, LRTest::Carbon, 50);
	Sim.GiveMatter(Here, LRTest::Iron, 50);
	const FLRActionResult Occupied = LRTest::Build(Sim, LRTest::Box, Here);
	TestTrue(TEXT("a cell holds one cache"), !Occupied.bSuccess && Occupied.Reason == FName(TEXT("occupied")));
	TestTrue(TEXT("matter lives alongside the cache"), Sim.GetMatter(Here, LRTest::Carbon) == 50);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLRSimOpenTest, "LootboxRecursion.Simulation.OpenCacheSpillsIntoItsCell", LR_TEST_FLAGS)
bool FLRSimOpenTest::RunTest(const FString& Parameters)
{
	FLRSimulation Sim(LRTest::MakeData(), 1);
	const FIntVector Cell(0, 0, 0);
	TestFalse(TEXT("nothing to open"), LRTest::Open(Sim, Cell));

	Sim.GiveMatter(Cell, LRTest::Carbon, 50);
	Sim.GiveMatter(Cell, LRTest::Iron, 50);
	LRTest::Build(Sim, LRTest::Box, Cell);
	const int32 BoxId = LRTest::CacheIdAt(Sim, Cell);
	TestTrue(TEXT("built"), BoxId != 0);

	TestTrue(TEXT("open"), LRTest::Open(Sim, Cell));
	TestNull(TEXT("the cache is gone"), Sim.FindPlaced(Cell));
	TestNull(TEXT("and its instance"), Sim.FindLootBox(BoxId));
	TestEqual(TEXT("2 rolls x 10 carbon, in its cell"), Sim.GetMatter(Cell, LRTest::Carbon), 20);
	TestEqual(TEXT("opened stat"), Sim.GetStat(TEXT("opened:loot_box")), 1);
	TestEqual(TEXT("gained stat"), Sim.GetStat(TEXT("gained:carbon")), 20);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLRSimRecursionTest, "LootboxRecursion.Simulation.BoxInsideABox", LR_TEST_FLAGS)
bool FLRSimRecursionTest::RunTest(const FString& Parameters)
{
	FLRSimulation Sim(LRTest::MakeData(), 1); // reach 1
	const FIntVector Cell(0, 0, 0);
	Sim.GiveMatter(Cell, LRTest::Carbon, 1);
	LRTest::Build(Sim, LRTest::MysteryBox, Cell);
	TestTrue(TEXT("open the mystery box"), LRTest::Open(Sim, Cell));

	const FLRPlacedEntity* Inner = Sim.FindPlaced(Cell);
	TestTrue(TEXT("a new cache took its place"), Inner && Inner->Item == LRTest::Box);
	TestTrue(TEXT("with a real instance"), LRTest::CacheIdAt(Sim, Cell) != 0);
	TestTrue(TEXT("open that too"), LRTest::Open(Sim, Cell));
	TestEqual(TEXT("inner box opened into the cell"), Sim.GetMatter(Cell, LRTest::Carbon), 20);

	// Two caches in one: the second lands in the nearest empty cell within reach...
	Sim.GiveMatter(Cell, LRTest::Carbon, 1);
	LRTest::Build(Sim, LRTest::DoubleBox, Cell);
	LRTest::Open(Sim, Cell);
	int32 Caches = 0;
	for (const TPair<FIntVector, FLRPlacedEntity>& Pair : Sim.GetPlaced())
	{
		Caches += (Pair.Value.Item == LRTest::Box && FLRHexGrid::Distance(Pair.Key, Cell) <= 1) ? 1 : 0;
	}
	TestEqual(TEXT("both caches landed nearby"), Caches, 2);

	// ...and with no room in reach, it's lost.
	FLRGameData Cramped = LRTest::MakeData();
	Cramped.ReachRadius = 0;
	FLRSimulation Tight(Cramped, 1);
	LRTest::FMessageLog Log(Tight);
	Tight.GiveMatter(Cell, LRTest::Carbon, 1);
	LRTest::Build(Tight, LRTest::DoubleBox, Cell);
	LRTest::Open(Tight, Cell);
	TestEqual(TEXT("one cache kept"), Tight.GetPlaced().Num(), 1);
	TestTrue(TEXT("the other reported lost"), Log.Contains(TEXT("lost")));
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLRSimBuildDismantleTest, "LootboxRecursion.Simulation.BuildInPlaceAndDismantle", LR_TEST_FLAGS)
bool FLRSimBuildDismantleTest::RunTest(const FString& Parameters)
{
	FLRSimulation Sim(LRTest::MakeData(), 1);
	const FIntVector Cell(4, 2, 0);
	const FIntVector Empty(-3, 1, 0);
	Sim.GiveMatter(Empty, LRTest::Carbon, 10);
	const FLRActionResult Loose = LRTest::Build(Sim, LRTest::Lamp, Empty);
	TestTrue(TEXT("a source needs an irradiator"), !Loose.bSuccess && Loose.Reason == FName(TEXT("needs_irradiator")));

	LRTest::BuildIrradiator(Sim, Cell);
	const FLRPlacedEntity* Built = Sim.FindPlaced(Cell);
	TestTrue(TEXT("irradiator built in the cell"), Built && Built->Item == LRTest::Irradiator);
	Sim.GiveMatter(Cell, LRTest::Carbon, 10);
	TestFalse(TEXT("a machine needs an empty cell"), LRTest::Build(Sim, LRTest::Irradiator, Cell).bSuccess);
	TestEqual(TEXT("the refused build cost nothing"), Sim.GetMatter(Cell, LRTest::Carbon), 10);

	TestTrue(TEXT("lamp into the irradiator"), LRTest::BuildInto(Sim, LRTest::Lamp, Cell));
	TestTrue(TEXT("box into its chamber"), LRTest::BuildInto(Sim, LRTest::Box, Cell));
	const FLRPlacedEntity* Loaded = Sim.FindPlaced(Cell);
	TestTrue(TEXT("both inside"), Loaded && Loaded->Source.Item == LRTest::Lamp && Loaded->Chamber.Item == LRTest::Box);
	const int32 BoxId = LRTest::CacheIdAt(Sim, Cell);

	// It comes apart a layer at a time, and each layer's cost comes back to the cell.
	TestTrue(TEXT("dismantle the source"), LRTest::Dismantle(Sim, Cell));
	TestEqual(TEXT("lamp refunded"), Sim.GetMatter(Cell, LRTest::Carbon), 15);
	TestTrue(TEXT("dismantle the cache"), LRTest::Dismantle(Sim, Cell));
	TestEqual(TEXT("box refunded"), Sim.GetMatter(Cell, LRTest::Iron), 50);
	TestNull(TEXT("its instance is gone"), Sim.FindLootBox(BoxId));
	TestTrue(TEXT("dismantle the irradiator"), LRTest::Dismantle(Sim, Cell));
	TestNull(TEXT("the cell is empty"), Sim.FindPlaced(Cell));
	TestEqual(TEXT("all of it back"), Sim.GetMatter(Cell, LRTest::Carbon), 75);
	TestFalse(TEXT("nothing left to dismantle"), LRTest::Dismantle(Sim, Cell));
	TestEqual(TEXT("refunds don't count as gained"), Sim.GetStat(TEXT("gained:carbon")), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLRSimSaveLoadTest, "LootboxRecursion.Simulation.SaveLoadRoundTrip", LR_TEST_FLAGS)
bool FLRSimSaveLoadTest::RunTest(const FString& Parameters)
{
	FLRSimulation Sim(LRTest::MakeData(), 7);
	const FIntVector Cell(12, -1, 3);
	LRTest::BuildIrradiator(Sim, Cell);
	Sim.GiveMatter(Cell, LRTest::Iron, 7);
	Sim.RequestAction(LRTest::AtCell(LRTest::Gather, Cell)); // mid-cast when saved
	Sim.Advance(2.0);

	const FLRSaveData Save = Sim.Save();
	FLRSimulation Loaded(LRTest::MakeData(), 0);
	TestTrue(TEXT("load"), Loaded.Load(Save));
	TestNotNull(TEXT("entity restored"), Loaded.FindPlaced(Cell));
	TestEqual(TEXT("matter restored"), Loaded.GetMatter(Cell, LRTest::Iron), 7);
	TestTrue(TEXT("cast restored"), Loaded.GetActionStatus(LRTest::Gather).bCasting);

	Loaded.Advance(3.0);
	TestEqual(TEXT("pending cast completes after load"), Loaded.GetMatter(Cell, LRTest::Carbon), 30);

	FLRSaveData Newer = Save;
	Newer.Version = FLRSimulation::SaveVersion + 1;
	TestFalse(TEXT("incompatible save rejected"), Loaded.Load(Newer));
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

	for (const FName Action : { LRNames::Perturb, LRNames::Feed, LRNames::Craft, LRNames::Use, LRNames::Dismantle })
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

	FLRGameData BuildsStructure = Cosmos;
	LRTest::AddRecipe(BuildsStructure, TEXT("ripple_kit"), LRTest::Ripple, { FLRItemAmount(LRTest::Carbon, 1) });
	TestTrue(TEXT("recipes can't build structures"), BuildsStructure.Validate().Num() > 0);

	FLRGameData RollsASource = Cosmos;
	LRTest::AddTable(RollsASource, TEXT("lamps"), 1, LRTest::Lamp, 1);
	TestTrue(TEXT("rolls can't produce sources"), RollsASource.Validate().Num() > 0);

	FLRGameData VagueMatter = Cosmos;
	FLRRequirement Anything;
	Anything.Check = LRNames::CheckMatter;
	VagueMatter.Recipes[0].RevealRequirements.Add(Anything);
	TestTrue(TEXT("a matter check needs an item or category"), VagueMatter.Validate().Num() > 0);

	FLRGameData PlacesMaterial = Cosmos;
	for (FLRActionDef& Action : PlacesMaterial.Actions)
	{
		if (Action.Name == LRTest::Seed)
		{
			Action.Places = LRTest::Hydrogen;
		}
	}
	TestTrue(TEXT("seeding something that isn't an overdensity reported"), PlacesMaterial.Validate().Num() > 0);

	FLRGameData SeeThrough = Cosmos;
	SeeThrough.Items.FindChecked(LRTest::Ripple).Opacity = 0.15f;
	TestEqual(TEXT("a see-through item is valid"), SeeThrough.Validate().Num(), 0);
	SeeThrough.Items.FindChecked(LRTest::Ripple).Opacity = 0.f;
	TestTrue(TEXT("an invisible item reported"), SeeThrough.Validate().Num() > 0);
	SeeThrough.Items.FindChecked(LRTest::Ripple).Opacity = 1.5f;
	TestTrue(TEXT("opacity above 1 reported"), SeeThrough.Validate().Num() > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLRSimIrradiateTest, "LootboxRecursion.Irradiation.StacksUpToCapThenOpens", LR_TEST_FLAGS)
bool FLRSimIrradiateTest::RunTest(const FString& Parameters)
{
	FLRSimulation Sim(LRTest::MakeData(), 1);
	const FIntVector Cell(0, 0, 0);
	LRTest::BuildIrradiator(Sim, Cell);
	TestTrue(TEXT("box into the chamber"), LRTest::BuildInto(Sim, LRTest::Box, Cell));
	TestTrue(TEXT("lamp into the source slot"), LRTest::BuildInto(Sim, LRTest::Lamp, Cell));
	TestTrue(TEXT("all the matter was spent"), Sim.GetAllMatter().IsEmpty());

	Sim.Advance(9.9);
	TestEqual(TEXT("no stack before the interval"), LRTest::StacksAt(Sim, Cell), 0);
	Sim.Advance(0.2);
	TestEqual(TEXT("first stack"), LRTest::StacksAt(Sim, Cell), 1);
	Sim.Advance(100.0);
	TestEqual(TEXT("capped at the irradiator's max stacks"), LRTest::StacksAt(Sim, Cell), 2);

	// 2 rolls x 10 carbon, amounts doubled twice = 2 x 40, opened right in the irradiator.
	TestTrue(TEXT("open in place"), LRTest::Open(Sim, Cell));
	TestEqual(TEXT("irradiated loot in the cell"), Sim.GetMatter(Cell, LRTest::Carbon), 80);
	const FLRPlacedEntity* After = Sim.FindPlaced(Cell);
	TestTrue(TEXT("the irradiator keeps its source"), After && After->Chamber.IsEmpty() && After->Source.Item == LRTest::Lamp);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLRSimIrradiateRulesTest, "LootboxRecursion.Irradiation.BuildRules", LR_TEST_FLAGS)
bool FLRSimIrradiateRulesTest::RunTest(const FString& Parameters)
{
	FLRSimulation Sim(LRTest::MakeData(), 1);
	const FIntVector Cell(0, 0, 0);
	LRTest::BuildIrradiator(Sim, Cell);

	Sim.GiveMatter(Cell, LRTest::Carbon, 1);
	const FLRActionResult TooStrong = LRTest::Build(Sim, LRTest::GammaSource, Cell);
	TestFalse(TEXT("gamma is too strong for a tier-5 irradiator"), TooStrong.bSuccess);
	TestTrue(TEXT("reason"), TooStrong.Reason == FName(TEXT("radiation_too_strong")));

	TestTrue(TEXT("first box"), LRTest::BuildInto(Sim, LRTest::Box, Cell));
	const FLRActionResult Full = LRTest::Build(Sim, LRTest::Box, Cell);
	TestTrue(TEXT("the chamber holds one box"), !Full.bSuccess && Full.Reason == FName(TEXT("chamber_full")));
	Sim.Advance(60.0);
	TestEqual(TEXT("no stacks without a source"), LRTest::StacksAt(Sim, Cell), 0);

	TestTrue(TEXT("lamp"), LRTest::BuildInto(Sim, LRTest::Lamp, Cell));
	Sim.GiveMatter(Cell, LRTest::Carbon, 5);
	const FLRActionResult Second = LRTest::Build(Sim, LRTest::Lamp, Cell);
	TestTrue(TEXT("one source per irradiator"), !Second.bSuccess && Second.Reason == FName(TEXT("source_full")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLRSimXRayTest, "LootboxRecursion.Irradiation.XRayRevealsAndLocks", LR_TEST_FLAGS)
bool FLRSimXRayTest::RunTest(const FString& Parameters)
{
	FLRSimulation Sim(LRTest::MakeData(), 3);
	const FIntVector Cell(0, 0, 0);
	LRTest::BuildIrradiator(Sim, Cell);
	LRTest::BuildInto(Sim, LRTest::Box, Cell);
	LRTest::BuildInto(Sim, LRTest::XRayTube, Cell);
	const int32 BoxId = LRTest::CacheIdAt(Sim, Cell);

	Sim.Advance(10.0);
	const FLRLootBoxInstance* BoxInstance = Sim.FindLootBox(BoxId);
	TestTrue(TEXT("revealed"), BoxInstance && BoxInstance->bRevealed);
	TestEqual(TEXT("contents known: 2 x 10 carbon"),
		(BoxInstance && BoxInstance->RevealedContents.Num() == 1) ? BoxInstance->RevealedContents[0].Count : -1, 20);

	Sim.Advance(100.0);
	TestEqual(TEXT("locked: no further stacks"), LRTest::StacksAt(Sim, Cell), 0);
	LRTest::Open(Sim, Cell);
	TestEqual(TEXT("opens to exactly what was revealed"), Sim.GetMatter(Cell, LRTest::Carbon), 20);
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
	const FIntVector Cell(0, 0, 0);
	LRTest::BuildIrradiator(Sim, Cell);
	LRTest::BuildInto(Sim, LRTest::Box, Cell);
	LRTest::BuildInto(Sim, LRTest::Lamp, Cell);
	const int32 BoxId = LRTest::CacheIdAt(Sim, Cell);
	Sim.Advance(6.0);

	FLRSimulation Loaded(LRTest::MakeData(), 0);
	TestTrue(TEXT("load"), Loaded.Load(Sim.Save()));
	const FLRPlacedEntity* Entity = Loaded.FindPlaced(Cell);
	TestTrue(TEXT("chamber restored"), Entity && Entity->Chamber.InstanceId == BoxId);
	Loaded.Advance(4.0);
	TestEqual(TEXT("progress carried over"), LRTest::StacksAt(Loaded, Cell), 1);

	// New caches must not reuse the id of the box inside the irradiator.
	const FIntVector Elsewhere(5, 0, 0);
	LRTest::BuildInto(Loaded, LRTest::Box, Elsewhere);
	const int32 NewId = LRTest::CacheIdAt(Loaded, Elsewhere);
	TestTrue(TEXT("no id collision"), NewId != 0 && NewId != BoxId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLRTechTreeRecipeTest, "LootboxRecursion.TechTree.RecipesUnlockAndLatch", LR_TEST_FLAGS)
bool FLRTechTreeRecipeTest::RunTest(const FString& Parameters)
{
	FLRSimulation Sim(LRTest::MakeData(), 1);
	const FName Gated(TEXT("gated_irradiator"));
	const FName Chained(TEXT("chained_lamp"));
	const FIntVector Cell(0, 0, 0);
	TestTrue(TEXT("no reveal requirements = unlocked"), Sim.IsRecipeUnlocked(LRTest::Box));
	TestFalse(TEXT("gated starts locked"), Sim.IsRecipeUnlocked(Gated));
	TestFalse(TEXT("chained starts locked"), Sim.IsRecipeUnlocked(Chained));

	Sim.GiveMatter(Cell, LRTest::Carbon, 200);
	Sim.GiveMatter(Cell, LRTest::Iron, 100);
	const FLRActionResult Locked = LRTest::Build(Sim, Gated, FIntVector(1, 0, 0));
	TestFalse(TEXT("a locked recipe can't be built"), Locked.bSuccess);
	TestTrue(TEXT("reason"), Locked.Reason == FName(TEXT("recipe_locked")));

	int32 UnlockMessages = 0;
	Sim.OnActionCompleted.AddLambda([&UnlockMessages](const FLRActionResult& Result)
	{
		UnlockMessages += Result.Action == FName(TEXT("unlock")) ? 1 : 0;
	});
	LRTest::Build(Sim, LRTest::Box, Cell);

	TestEqual(TEXT("stat counted"), Sim.GetStat(TEXT("crafted:loot_box")), 1);
	TestEqual(TEXT("gained stat counted"), Sim.GetStat(TEXT("gained:loot_box")), 1);
	TestTrue(TEXT("gated unlocked"), Sim.IsRecipeUnlocked(Gated));
	TestTrue(TEXT("chain resolved in the same pass"), Sim.IsRecipeUnlocked(Chained));
	// Gated + Chained recipes, plus the "scan" action (revealed by a cache now sitting in the world).
	TestEqual(TEXT("all three unlocks announced"), UnlockMessages, 3);
	TestTrue(TEXT("now buildable"), LRTest::Build(Sim, Gated, FIntVector(1, 0, 0)).bSuccess);

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
	const FIntVector Cell(0, 0, 0);
	TestFalse(TEXT("hidden at first"), Sim.GetActionStatus(Scan).bRevealed);
	TestFalse(TEXT("hidden actions can't be used"), Sim.RequestAction(LRTest::AtCell(Scan, Cell)).bSuccess);

	LRTest::BuildInto(Sim, LRTest::Box, Cell);
	TestTrue(TEXT("revealed once a cache is in the world"), Sim.GetActionStatus(Scan).bRevealed);

	LRTest::Open(Sim, Cell);
	TestNull(TEXT("cache gone"), Sim.FindCacheAt(Cell));
	TestTrue(TEXT("stays revealed (latched)"), Sim.GetActionStatus(Scan).bRevealed);
	TestEqual(TEXT("opened stat"), Sim.GetStat(TEXT("opened:loot_box")), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLRTechTreePlaythroughTest, "LootboxRecursion.TechTree.ShippedTreeIsPlayable", LR_TEST_FLAGS)
bool FLRTechTreePlaythroughTest::RunTest(const FString& Parameters)
{
	// Walks the real Content/Data tech tree and epochs from a new game to the last unlock, so a
	// data change that creates a dead end fails here. The early game is played for real: the
	// ripples' yields are what reach nucleosynthesis. After that, matter is cheated into a stock
	// cell (GiveMatter doesn't count toward stats) and everything else is played through real
	// actions, building within reach of the stock and the ripples.
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
	TestTrue(TEXT("the layout below assumes a reach of at least 2"), Sim.GetReachRadius() >= 2);

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
	auto Craft = [&Act](const TCHAR* Recipe, const FIntVector& Cell)
	{
		FLRActionRequest Request = LRTest::AtCell(LRNames::Craft, Cell);
		Request.Choice = Recipe;
		return Act(Request);
	};
	auto Irradiate = [&Sim, &Act, &Craft](const FIntVector& Cell, double Seconds)
	{
		// Open the last cache in the chamber, then irradiate a fresh one.
		if (Sim.FindCacheAt(Cell))
		{
			Act(LRTest::AtCell(LRNames::Use, Cell));
		}
		Craft(TEXT("loot_box"), Cell);
		const bool bLoaded = Sim.FindCacheAt(Cell) != nullptr;
		Sim.Advance(Seconds);
		return bLoaded;
	};

	// Genesis: three ripples in a row end inflation, and their yields reach nucleosynthesis.
	TestTrue(TEXT("starts in inflation"), EpochIs(TEXT("inflation")));
	TestTrue(TEXT("perturb available from the start"), Sim.GetActionStatus(LRNames::Perturb).bRevealed);
	TestFalse(TEXT("build hidden at the start"), Sim.GetActionStatus(LRNames::Craft).bRevealed);
	for (int32 X = 0; X < 3; ++X)
	{
		TestTrue(*FString::Printf(TEXT("perturb ripple %d"), X + 1), Act(LRTest::AtCell(LRNames::Perturb, FIntVector(X, 0, 0))));
	}
	TestTrue(TEXT("reheating after three perturbations"), EpochIs(TEXT("reheating")));
	Sim.Advance(120.0);
	TestTrue(TEXT("nucleosynthesis once the ripples gathered enough hydrogen"), EpochIs(TEXT("nucleosynthesis")));
	TestTrue(TEXT("the hydrogen is in the ripples' cells"), Sim.GetMatter(FIntVector(0, 0, 0), TEXT("hydrogen")) > 0);
	TestTrue(TEXT("build revealed"), Sim.GetActionStatus(LRNames::Craft).bRevealed);

	// The host evaporates, and perturbing drew on it too: Feed appears once it's below 90%.
	for (int32 Wait = 0; Wait < 100 && !Sim.GetActionStatus(LRNames::Feed).bRevealed; ++Wait)
	{
		Sim.Advance(30.0);
	}
	TestTrue(TEXT("feed revealed"), Sim.GetActionStatus(LRNames::Feed).bRevealed);
	TestTrue(TEXT("feed the horizon"), Act(FLRActionRequest::Make(LRNames::Feed)));

	// The row above the ripples: a stock of matter with build sites around it, all within reach
	// of the stock (and mostly of the ripples too).
	const FIntVector Stock(1, 1, 0);
	const FIntVector CacheSite(0, 2, 0);
	const FIntVector CarbonCell(0, 1, 0);
	const FIntVector IronCell(2, 1, 0);
	TestTrue(TEXT("cheat in matter"), Sim.GiveMatter(Stock, TEXT("hydrogen"), 300) && Sim.GiveMatter(Stock, TEXT("helium"), 400)
		&& Sim.GiveMatter(Stock, TEXT("carbon"), 400) && Sim.GiveMatter(Stock, TEXT("iron"), 200));

	TestTrue(TEXT("build a cache"), Craft(TEXT("loot_box"), CacheSite));
	TestTrue(TEXT("open revealed"), Sim.GetActionStatus(LRNames::Use).bRevealed);
	TestTrue(TEXT("dismantle revealed"), Sim.GetActionStatus(LRNames::Dismantle).bRevealed);
	TestTrue(TEXT("open it"), Act(LRTest::AtCell(LRNames::Use, CacheSite)));

	TestTrue(TEXT("nebula irradiator"), Craft(TEXT("nebula_irradiator"), CarbonCell));
	TestTrue(TEXT("grow lamp into it"), Craft(TEXT("grow_lamp"), CarbonCell));
	TestTrue(TEXT("visible light run"), Irradiate(CarbonCell, 35.0));
	// Swap sources: dismantling takes the source out first (and refunds it).
	TestTrue(TEXT("take the lamp out"), Act(LRTest::AtCell(LRNames::Dismantle, CarbonCell)));
	TestTrue(TEXT("infrared emitter"), Craft(TEXT("infrared_emitter"), CarbonCell));
	TestTrue(TEXT("infrared run"), Irradiate(CarbonCell, 35.0));
	TestTrue(TEXT("take the emitter out"), Act(LRTest::AtCell(LRNames::Dismantle, CarbonCell)));
	TestTrue(TEXT("microwave emitter"), Craft(TEXT("microwave_emitter"), CarbonCell));
	TestTrue(TEXT("microwave run"), Irradiate(CarbonCell, 25.0));

	TestTrue(TEXT("corona irradiator"), Craft(TEXT("corona_irradiator"), IronCell));
	TestTrue(TEXT("x-ray tube into it"), Craft(TEXT("xray_tube"), IronCell));
	TestTrue(TEXT("x-ray run"), Irradiate(IronCell, 13.0));
	TestTrue(TEXT("take the tube out"), Act(LRTest::AtCell(LRNames::Dismantle, IronCell)));
	TestTrue(TEXT("gamma source"), Craft(TEXT("gamma_source"), IronCell));

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

	LRTest::BuildIrradiator(Sim, FIntVector(1, 0, 0));
	TestNotNull(TEXT("an irradiator next door"), Sim.FindPlaced(FIntVector(1, 0, 0)));
	TestFalse(TEXT("can't seed into an occupied cell"), LRTest::SeedAt(Sim, FIntVector(1, 0, 0)));
	const FLRActionResult Dismantled = Sim.RequestAction(LRTest::AtCell(LRNames::Dismantle, Cell));
	TestTrue(TEXT("ripples can't be dismantled"), !Dismantled.bSuccess && Dismantled.Reason == FName(TEXT("cannot_dismantle")));
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
	TestEqual(TEXT("no matter yet in the first epoch"), Sim.GetMatter(FIntVector(0, 0, 0), LRTest::Hydrogen), 0);

	TestTrue(TEXT("deepen it (the second seed begins the hot epoch)"), LRTest::SeedAt(Sim, Cell));
	TestEqual(TEXT("hot epoch"), Sim.GetEpochIndex(), 1);
	Sim.Advance(10.0);
	TestEqual(TEXT("one yield, one roll per amplitude"), Sim.GetMatter(FIntVector(0, 0, 0), LRTest::Hydrogen), 10);
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
	TestEqual(TEXT("three yields at amplitude 3"), Sim.GetMatter(FIntVector(0, 0, 0), LRTest::Hydrogen), 20 + 3 * 15);
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
	TestEqual(TEXT("nothing yields while frozen"), Sim.GetMatter(FIntVector(0, 0, 0), LRTest::Hydrogen), 0);
	Sim.Advance(50.0);
	TestEqual(TEXT("still nothing"), Sim.GetMatter(FIntVector(0, 0, 0), LRTest::Hydrogen), 0);
	TestFalse(TEXT("perturbing needs a host"), LRTest::SeedAt(Sim, FIntVector(1, 0, 0)));

	TestTrue(TEXT("feed"), Sim.RequestAction(FLRActionRequest::Make(LRNames::Feed)).bSuccess);
	TestFalse(TEXT("no longer frozen"), Sim.IsFrozen());
	TestTrue(TEXT("the universe stirs"), Log.Contains(TEXT("stirs again")));
	Sim.Advance(10.0);
	TestEqual(TEXT("yields resume"), Sim.GetMatter(FIntVector(0, 0, 0), LRTest::Hydrogen), 10);

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
