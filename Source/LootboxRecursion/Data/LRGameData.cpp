#include "Data/LRGameData.h"

#include "HAL/FileManager.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Simulation/LRPhysics.h"

FLinearColor FLRItemDef::GetLinearColor() const
{
	if (Color.IsEmpty())
	{
		return FLinearColor(0.5f, 0.5f, 0.5f);
	}
	// FColor is sRGB; the FLinearColor(FColor) constructor converts to linear space.
	return FLinearColor(FColor::FromHex(Color));
}

FLinearColor FLRRadiationDef::GetLinearColor() const
{
	return Color.IsEmpty() ? FLinearColor::White : FLinearColor(FColor::FromHex(Color));
}

bool FLRLootModifier::IsValidKind(FName InKind)
{
	return InKind == LRNames::ModifierExtraRolls || InKind == LRNames::ModifierItemWeightMult
		|| InKind == LRNames::ModifierItemCountMult || InKind == LRNames::ModifierAddEntry
		|| InKind == LRNames::ModifierReveal;
}

bool FLRLootModifier::KindNeedsItem(FName InKind)
{
	return InKind == LRNames::ModifierItemWeightMult || InKind == LRNames::ModifierItemCountMult
		|| InKind == LRNames::ModifierAddEntry;
}

bool FLRRequirement::EvaluateCondition(int32 Actual, FName InCondition, int32 Expected)
{
	if (InCondition == TEXT("gt"))  { return Actual > Expected; }
	if (InCondition == TEXT("gte")) { return Actual >= Expected; }
	if (InCondition == TEXT("lt"))  { return Actual < Expected; }
	if (InCondition == TEXT("lte")) { return Actual <= Expected; }
	if (InCondition == TEXT("eq"))  { return Actual == Expected; }
	return false;
}

bool FLRRequirement::IsValidCondition(FName InCondition)
{
	return InCondition == TEXT("gt") || InCondition == TEXT("gte") || InCondition == TEXT("lt")
		|| InCondition == TEXT("lte") || InCondition == TEXT("eq");
}

FName FLRGameData::RecipeUnlockKey(FName RecipeId)
{
	return FName(*FString::Printf(TEXT("recipe:%s"), *RecipeId.ToString()));
}

FName FLRGameData::ActionUnlockKey(FName ActionName)
{
	return FName(*FString::Printf(TEXT("action:%s"), *ActionName.ToString()));
}

FName FLRGameData::ActionRetiredKey(FName ActionName)
{
	return FName(*FString::Printf(TEXT("retired:%s"), *ActionName.ToString()));
}

int32 FLRGameData::FindEpochIndex(FName Id) const
{
	return Epochs.IndexOfByPredicate([Id](const FLREpochDef& Epoch) { return Epoch.Id == Id; });
}

const FLRRecipeDef* FLRGameData::FindRecipeFor(FName Item) const
{
	return Recipes.FindByPredicate([Item](const FLRRecipeDef& Recipe) { return Recipe.Output == Item; });
}

const FLRRecipeDef* FLRGameData::FindRecipe(FName Id) const
{
	return Recipes.FindByPredicate([Id](const FLRRecipeDef& Recipe) { return Recipe.Id == Id; });
}

const FLRActionDef* FLRGameData::FindAction(FName Name) const
{
	return Actions.FindByPredicate([Name](const FLRActionDef& Action) { return Action.Name == Name; });
}

const FLRRadiationDef* FLRGameData::FindRadiation(FName Id) const
{
	return Radiation.FindByPredicate([Id](const FLRRadiationDef& Def) { return Def.Id == Id; });
}

int32 FLRGameData::GetStackSize(FName Item) const
{
	const FLRItemDef* Def = FindItem(Item);
	return Def ? FMath::Max(1, Def->StackSize) : 1;
}

FString FLRGameData::GetDisplayName(FName Item) const
{
	const FLRItemDef* Def = FindItem(Item);
	return (Def && !Def->DisplayName.IsEmpty()) ? Def->DisplayName : Item.ToString();
}

void FLRGameData::AddFrom(const FLRDataFile& File)
{
	for (const FLRItemDef& Item : File.Items)
	{
		Items.Add(Item.Id, Item);
	}
	for (const FLRRecipeDef& Recipe : File.Recipes)
	{
		Recipes.RemoveAll([&Recipe](const FLRRecipeDef& Existing) { return Existing.Id == Recipe.Id; });
		Recipes.Add(Recipe);
	}
	for (const FLRLootTableDef& Table : File.LootTables)
	{
		LootTables.Add(Table.Id, Table);
	}
	for (const FLRActionDef& Action : File.Actions)
	{
		Actions.RemoveAll([&Action](const FLRActionDef& Existing) { return Existing.Name == Action.Name; });
		Actions.Add(Action);
	}
	Radiation.Append(File.Radiation);
	for (const FLREpochDef& Epoch : File.Epochs)
	{
		const int32 Existing = FindEpochIndex(Epoch.Id);
		if (Existing != INDEX_NONE)
		{
			Epochs[Existing] = Epoch;
		}
		else
		{
			Epochs.Add(Epoch);
		}
	}
	if (File.Host.IsDefined())
	{
		Host = File.Host;
	}
	if (File.Gas.SpreadRate >= 0.f)
	{
		Gas = File.Gas;
	}
	if (File.ReachRadius >= 0)
	{
		ReachRadius = File.ReachRadius;
	}
	if (File.GlobalCooldown >= 0.f)
	{
		GlobalCooldown = File.GlobalCooldown;
	}
	if (File.CancelRefund >= 0.f)
	{
		CancelRefund = File.CancelRefund;
	}
}

TArray<FString> FLRGameData::Validate() const
{
	TArray<FString> Errors;

	auto CheckItemRef = [this, &Errors](FName Item, const FString& Where)
	{
		if (Item.IsNone())
		{
			Errors.Add(FString::Printf(TEXT("%s: missing item id"), *Where));
		}
		else if (!Items.Contains(Item))
		{
			Errors.Add(FString::Printf(TEXT("%s: unknown item '%s'"), *Where, *Item.ToString()));
		}
	};

	auto CheckRequirements = [this, &Errors, &CheckItemRef](const TArray<FLRRequirement>& Requirements, const FString& Where)
	{
		for (const FLRRequirement& Req : Requirements)
		{
			if (Req.Check != LRNames::CheckMatter && Req.Check != LRNames::CheckPlaced
				&& Req.Check != LRNames::CheckStat && Req.Check != LRNames::CheckUnlocked
				&& Req.Check != LRNames::CheckEpoch && Req.Check != LRNames::CheckHost)
			{
				Errors.Add(FString::Printf(TEXT("%s: unknown requirement check '%s'"), *Where, *Req.Check.ToString()));
			}
			if (!FLRRequirement::IsValidCondition(Req.Condition))
			{
				Errors.Add(FString::Printf(TEXT("%s: unknown condition '%s'"), *Where, *Req.Condition.ToString()));
			}
			if (!Req.Item.IsNone())
			{
				CheckItemRef(Req.Item, Where + TEXT(" requirement"));
			}
			if (Req.Check == LRNames::CheckMatter && Req.Item.IsNone() && Req.Category.IsNone())
			{
				Errors.Add(FString::Printf(TEXT("%s: matter requirement needs an item or category"), *Where));
			}
			if (Req.Check == LRNames::CheckStat && Req.Id.IsNone())
			{
				Errors.Add(FString::Printf(TEXT("%s: stat requirement needs an id"), *Where));
			}
			if (Req.Check == LRNames::CheckUnlocked)
			{
				const bool bKnown = Recipes.ContainsByPredicate([&Req](const FLRRecipeDef& R) { return RecipeUnlockKey(R.Id) == Req.Id; })
					|| Actions.ContainsByPredicate([&Req](const FLRActionDef& A) { return ActionUnlockKey(A.Name) == Req.Id; });
				if (!bKnown)
				{
					Errors.Add(FString::Printf(TEXT("%s: unknown unlock '%s' (use recipe:<id> or action:<name>)"), *Where, *Req.Id.ToString()));
				}
			}
			if (Req.Check == LRNames::CheckEpoch && FindEpochIndex(Req.Id) == INDEX_NONE)
			{
				Errors.Add(FString::Printf(TEXT("%s: unknown epoch '%s'"), *Where, *Req.Id.ToString()));
			}
		}
	};

	for (const TPair<FName, FLRItemDef>& Pair : Items)
	{
		const FLRItemDef& Item = Pair.Value;
		const FString Where = FString::Printf(TEXT("item '%s'"), *Item.Id.ToString());
		if (Item.Id.IsNone())
		{
			Errors.Add(TEXT("item with no id"));
		}
		if (Item.StackSize < 1)
		{
			Errors.Add(FString::Printf(TEXT("%s: stackSize must be >= 1"), *Where));
		}
		if (Item.Category != LRNames::CategoryMaterial && Item.Category != LRNames::CategoryLootBox
			&& Item.Category != LRNames::CategoryPlaceable && Item.Category != LRNames::CategorySource
			&& Item.Category != LRNames::CategoryStructure)
		{
			Errors.Add(FString::Printf(TEXT("%s: unknown category '%s'"), *Where, *Item.Category.ToString()));
		}
		if (Item.IsLootBox() && !LootTables.Contains(Item.LootTable))
		{
			Errors.Add(FString::Printf(TEXT("%s: unknown lootTable '%s'"), *Where, *Item.LootTable.ToString()));
		}
		if (Item.Category == LRNames::CategoryLootBox && !Item.IsLootBox())
		{
			Errors.Add(FString::Printf(TEXT("%s: lootbox items need a lootTable"), *Where));
		}
		if ((Item.IsPlaceable() || Item.IsLootBox() || Item.IsSource() || Item.IsStructure()) && Item.StackSize != 1)
		{
			Errors.Add(FString::Printf(TEXT("%s: placeable, lootbox, source and structure items must have stackSize 1"), *Where));
		}
		if (!Item.Material.IsEmpty() && !Item.Material.StartsWith(TEXT("/")))
		{
			Errors.Add(FString::Printf(TEXT("%s: material must be an asset path such as /Game/Materials/M_Name"), *Where));
		}
		if (Item.Opacity <= 0.f || Item.Opacity > 1.f)
		{
			Errors.Add(FString::Printf(TEXT("%s: opacity must be above 0 and at most 1"), *Where));
		}
		if (Item.MaxAmplitude < 0 || (Item.MaxAmplitude > 0 && (!Item.IsStructure() || Item.YieldSeconds <= 0.f)))
		{
			Errors.Add(FString::Printf(TEXT("%s: maxAmplitude is for structure items, and needs yieldSeconds > 0"), *Where));
		}
		if (Item.Category == LRNames::CategorySource && !FindRadiation(Item.Radiation))
		{
			Errors.Add(FString::Printf(TEXT("%s: unknown radiation '%s'"), *Where, *Item.Radiation.ToString()));
		}
		if (Item.AtomicMass < 0.f || (Item.AtomicMass > 0.f && Item.Category != LRNames::CategoryMaterial))
		{
			Errors.Add(FString::Printf(TEXT("%s: atomicMass is for materials, and must be >= 0"), *Where));
		}
		if (Item.MaxExposureStacks < 0 || Item.MaxRadiationTier < 0 || (Item.MaxExposureStacks > 0 && Item.ExposureSeconds <= 0.f))
		{
			Errors.Add(FString::Printf(TEXT("%s: irradiator needs maxExposureStacks/maxRadiationTier >= 0 and exposureSeconds > 0"), *Where));
		}
	}

	for (const FLRRecipeDef& Recipe : Recipes)
	{
		const FString Where = FString::Printf(TEXT("recipe '%s'"), *Recipe.Id.ToString());
		CheckItemRef(Recipe.Output, Where + TEXT(" output"));
		if (Recipe.OutputCount < 1)
		{
			Errors.Add(FString::Printf(TEXT("%s: outputCount must be >= 1"), *Where));
		}
		if (const FLRItemDef* Output = FindItem(Recipe.Output))
		{
			if (Output->IsStructure())
			{
				Errors.Add(FString::Printf(TEXT("%s: structures can't be built, only seeded"), *Where));
			}
			else if (Output->Category != LRNames::CategoryMaterial && Recipe.OutputCount != 1)
			{
				Errors.Add(FString::Printf(TEXT("%s: only materials can be built more than one at a time"), *Where));
			}
		}
		for (const FLRItemAmount& Cost : Recipe.Cost)
		{
			CheckItemRef(Cost.Item, Where + TEXT(" cost"));
			if (Cost.Count < 0)
			{
				Errors.Add(FString::Printf(TEXT("%s: negative cost"), *Where));
			}
		}
		CheckRequirements(Recipe.RevealRequirements, Where);
	}

	for (const TPair<FName, FLRLootTableDef>& Pair : LootTables)
	{
		const FLRLootTableDef& Table = Pair.Value;
		const FString Where = FString::Printf(TEXT("loot table '%s'"), *Table.Id.ToString());
		if (Table.RollsMin < 0 || Table.RollsMax < Table.RollsMin)
		{
			Errors.Add(FString::Printf(TEXT("%s: need 0 <= rollsMin <= rollsMax"), *Where));
		}
		if (Table.Entries.IsEmpty())
		{
			Errors.Add(FString::Printf(TEXT("%s: has no entries"), *Where));
		}
		for (const FLRLootEntry& Entry : Table.Entries)
		{
			CheckItemRef(Entry.Item, Where);
			const FLRItemDef* EntryDef = FindItem(Entry.Item);
			if (EntryDef && EntryDef->Category != LRNames::CategoryMaterial && !EntryDef->IsLootBox())
			{
				Errors.Add(FString::Printf(TEXT("%s: '%s' can't come out of a roll (only materials and caches can)"), *Where, *Entry.Item.ToString()));
			}
			if (Entry.Weight <= 0)
			{
				Errors.Add(FString::Printf(TEXT("%s: weight must be > 0"), *Where));
			}
			if (Entry.MinCount < 1 || Entry.MaxCount < Entry.MinCount)
			{
				Errors.Add(FString::Printf(TEXT("%s: need 1 <= minCount <= maxCount"), *Where));
			}
		}
	}

	for (const FLRActionDef& Action : Actions)
	{
		const FString Where = FString::Printf(TEXT("action '%s'"), *Action.Name.ToString());
		if (Action.Cooldown < 0.f || Action.CastTime < 0.f)
		{
			Errors.Add(FString::Printf(TEXT("%s: cooldown/castTime must be >= 0"), *Where));
		}
		if (!Action.LootTable.IsNone() && !LootTables.Contains(Action.LootTable))
		{
			Errors.Add(FString::Printf(TEXT("%s: unknown lootTable '%s'"), *Where, *Action.LootTable.ToString()));
		}
		CheckRequirements(Action.Requirements, Where);
		CheckRequirements(Action.RevealRequirements, Where);
		CheckRequirements(Action.RetireRequirements, Where);
		if (!Action.Places.IsNone())
		{
			const FLRItemDef* Placed = FindItem(Action.Places);
			if (!Placed || !Placed->IsOverdensity())
			{
				Errors.Add(FString::Printf(TEXT("%s: places '%s', which is not an overdensity (a structure with maxAmplitude > 0)"),
					*Where, *Action.Places.ToString()));
			}
		}
	}

	for (int32 Index = 0; Index < Epochs.Num(); ++Index)
	{
		const FLREpochDef& Epoch = Epochs[Index];
		const FString Where = FString::Printf(TEXT("epoch '%s'"), *Epoch.Id.ToString());
		if (Epoch.Id.IsNone())
		{
			Errors.Add(TEXT("epoch with no id"));
		}
		else if (FindEpochIndex(Epoch.Id) != Index)
		{
			Errors.Add(FString::Printf(TEXT("%s: duplicate id"), *Where));
		}
		if (Epoch.StartTime <= 0.0 || (Index > 0 && Epoch.StartTime <= Epochs[Index - 1].StartTime))
		{
			Errors.Add(FString::Printf(TEXT("%s: startTime must be > 0 and later than the previous epoch's"), *Where));
		}
		if (Epoch.EndTime != 0.0 && Epoch.EndTime <= Epoch.StartTime)
		{
			Errors.Add(FString::Printf(TEXT("%s: endTime must be 0 or later than startTime"), *Where));
		}
		if (Epoch.ClockSeconds < 0.f || Epoch.RippleGrowthSeconds < 0.f || Epoch.Plasma < 0.f || Epoch.Plasma > 1.f)
		{
			Errors.Add(FString::Printf(TEXT("%s: need clockSeconds >= 0, rippleGrowthSeconds >= 0 and 0 <= plasma <= 1"), *Where));
		}
		if (!Epoch.YieldTable.IsNone() && !LootTables.Contains(Epoch.YieldTable))
		{
			Errors.Add(FString::Printf(TEXT("%s: unknown yieldTable '%s'"), *Where, *Epoch.YieldTable.ToString()));
		}
		if (Index == 0 && !Epoch.AdvanceRequirements.IsEmpty())
		{
			Errors.Add(FString::Printf(TEXT("%s: the first epoch is where a game starts, so it can't have advanceRequirements"), *Where));
		}
		CheckRequirements(Epoch.AdvanceRequirements, Where);
	}

	if (ReachRadius < 0)
	{
		Errors.Add(TEXT("reachRadius must be >= 0"));
	}
	if (CancelRefund > 1.f)
	{
		Errors.Add(TEXT("cancelRefund must be between 0 and 1"));
	}
	if (Gas.IsDefined())
	{
		if (Gas.ReferenceTemperature <= 0.f || Gas.TemperatureExponent < 0.f || Gas.MaxSpeedup < 1.f || Gas.StepSeconds <= 0.f
			|| Gas.IonizationTemperature < 0.f || Gas.JeansMass < 0.f || Gas.InfallRate < 0.f)
		{
			Errors.Add(TEXT("gas: referenceTemperature and stepSeconds must be > 0, temperatureExponent >= 0, maxSpeedup >= 1, and ionizationTemperature, jeansMass and infallRate >= 0"));
		}
		// Explicit steps: a cell loses at most this much to each of its six neighbours per step,
		// so keep it well under 1/12 or the spread overshoots and oscillates.
		else if (Gas.SpreadRate * Gas.MaxSpeedup * Gas.StepSeconds > 1.f / 12.f || Gas.InfallRate * Gas.StepSeconds > 1.f / 12.f)
		{
			Errors.Add(TEXT("gas: spreadRate x maxSpeedup x stepSeconds and infallRate x stepSeconds must be at most 1/12"));
		}
	}

	if (Host.StartMass < 0.0)
	{
		Errors.Add(TEXT("host: startMass must be >= 0"));
	}
	if (Host.IsDefined())
	{
		if (Host.LifetimeSeconds < 0.f || Host.EddingtonRate < 0.f || Host.InjectorMaxRate < 0.0 || Host.ChamberRadius < 0.f
			|| Host.ChargeCapacity < 0.0 || Host.RechargeRate < 0.0 || Host.WarningSeconds < 0.f)
		{
			Errors.Add(TEXT("host: lifetimeSeconds, eddingtonRate, injectorMaxRate, chamberRadius, chargeCapacity, rechargeRate and warningSeconds must be >= 0"));
		}
		if (Host.SeedCost < 0.0 || Host.PerturbCost < 0.0 || Host.SeedCost >= Host.StartMass || Host.PerturbCost >= Host.StartMass)
		{
			Errors.Add(TEXT("host: seedCost and perturbCost must be >= 0 and less than startMass"));
		}
		if (Host.InjectorResponseSeconds <= 0.f || Host.SafetyGravity <= 0.f)
		{
			Errors.Add(TEXT("host: injectorResponseSeconds and safetyGravity must be > 0"));
		}
		if (Host.ChargeCapacity > 0.0 && Host.RechargeRate <= 0.0)
		{
			Errors.Add(TEXT("host: a stored charge (chargeCapacity > 0) needs rechargeRate > 0"));
		}
		if (Host.GetSafetyCap() > 0.0 && Host.GetSafetyCap() <= Host.StartMass)
		{
			Errors.Add(FString::Printf(TEXT("host: the chamber caps the host at %.0f kg, not above its startMass"), Host.GetSafetyCap()));
		}
	}

	TSet<FName> RadiationIds;
	for (const FLRRadiationDef& Def : Radiation)
	{
		const FString Where = FString::Printf(TEXT("radiation '%s'"), *Def.Id.ToString());
		if (Def.Id.IsNone())
		{
			Errors.Add(TEXT("radiation entry with no id"));
		}
		else if (RadiationIds.Contains(Def.Id))
		{
			Errors.Add(FString::Printf(TEXT("%s: duplicate id"), *Where));
		}
		RadiationIds.Add(Def.Id);
		if (!Def.Effect.IsNone())
		{
			if (!FLRLootModifier::IsValidKind(Def.Effect.Kind))
			{
				Errors.Add(FString::Printf(TEXT("%s: unknown effect kind '%s'"), *Where, *Def.Effect.Kind.ToString()));
			}
			if (FLRLootModifier::KindNeedsItem(Def.Effect.Kind))
			{
				CheckItemRef(Def.Effect.Item, Where + TEXT(" effect"));
			}
		}
	}

	return Errors;
}

bool FLRGameData::LoadFromDirectory(const FString& Directory, FLRGameData& OutData, TArray<FString>& OutErrors)
{
	OutData = FLRGameData();

	TArray<FString> FileNames;
	IFileManager::Get().FindFiles(FileNames, *(Directory / TEXT("*.json")), /*Files*/ true, /*Directories*/ false);
	FileNames.Sort();

	if (FileNames.IsEmpty())
	{
		OutErrors.Add(FString::Printf(TEXT("No *.json data files found in %s"), *Directory));
		return false;
	}

	bool bAllParsed = true;
	for (const FString& FileName : FileNames)
	{
		const FString Path = Directory / FileName;
		FString Json;
		if (!FFileHelper::LoadFileToString(Json, *Path))
		{
			OutErrors.Add(FString::Printf(TEXT("%s: could not read file"), *FileName));
			bAllParsed = false;
			continue;
		}

		FLRDataFile File;
		if (!FJsonObjectConverter::JsonObjectStringToUStruct(Json, &File))
		{
			OutErrors.Add(FString::Printf(TEXT("%s: invalid JSON or unexpected shape"), *FileName));
			bAllParsed = false;
			continue;
		}
		OutData.AddFrom(File);
	}

	const TArray<FString> ValidationErrors = OutData.Validate();
	OutErrors.Append(ValidationErrors);
	return bAllParsed && ValidationErrors.IsEmpty();
}

FString FLRGameData::GetDefaultDataDirectory()
{
	return FPaths::ProjectContentDir() / TEXT("Data");
}

// ---------------------------------------------------------------------------------------
// Host black hole
// ---------------------------------------------------------------------------------------

double FLRHostDef::GetEddingtonMultiple() const
{
	return static_cast<double>(EddingtonRate) / LRPhysics::EddingtonRatePerSecond;
}

double FLRHostDef::GetTippingMass() const
{
	const double K = GetEvaporationConstant();
	if (K <= 0.0)
	{
		return 0.0;
	}
	if (EddingtonRate <= 0.f || InjectorMaxRate <= 0.0)
	{
		return TNumericLimits<double>::Max(); // nothing can be fed: no host can be saved
	}
	// Evaporation K / M^2 equals the best intake: the rated limit, EddingtonRate * M (at
	// M = (K / rate)^(1/3)), or the injectors' maximum (at M = sqrt(K / max)), whichever binds
	// higher up.
	const double FromLimit = FMath::Pow(K / static_cast<double>(EddingtonRate), 1.0 / 3.0);
	const double FromMax = FMath::Sqrt(K / InjectorMaxRate);
	return FMath::Max(FromLimit, FromMax);
}

double FLRHostDef::GetSafetyCap() const
{
	return ChamberRadius > 0.f ? LRPhysics::MassForGravity(ChamberRadius, SafetyGravity) : 0.0;
}

double FLRHostDef::GetGravityRadius(double Mass) const
{
	return LRPhysics::RadiusOfGravity(Mass, SafetyGravity);
}

