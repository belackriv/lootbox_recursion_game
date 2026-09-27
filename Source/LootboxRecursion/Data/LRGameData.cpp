#include "Data/LRGameData.h"

#include "HAL/FileManager.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

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
			if (Req.Check != LRNames::CheckInventory && Req.Check != LRNames::CheckPlaced
				&& Req.Check != LRNames::CheckStat && Req.Check != LRNames::CheckUnlocked)
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
			&& Item.Category != LRNames::CategoryPlaceable && Item.Category != LRNames::CategorySource)
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
		if ((Item.IsPlaceable() || Item.IsLootBox() || Item.IsSource()) && Item.StackSize != 1)
		{
			Errors.Add(FString::Printf(TEXT("%s: placeable, lootbox and source items must have stackSize 1"), *Where));
		}
		if (Item.Category == LRNames::CategorySource && !FindRadiation(Item.Radiation))
		{
			Errors.Add(FString::Printf(TEXT("%s: unknown radiation '%s'"), *Where, *Item.Radiation.ToString()));
		}
		if (Item.MaxExposureStacks < 0 || Item.MaxRadiationTier < 0 || (Item.MaxExposureStacks > 0 && Item.ExposureSeconds <= 0.f))
		{
			Errors.Add(FString::Printf(TEXT("%s: enclosure needs maxExposureStacks/maxRadiationTier >= 0 and exposureSeconds > 0"), *Where));
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
