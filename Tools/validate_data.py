#!/usr/bin/env python3
"""Validate Content/Data/*.json without launching Unreal.

Mirrors FLRGameData::Validate() in Source/LootboxRecursion/Data/LRGameData.cpp - keep the two
in sync. Run it after editing data files (CI runs it too):

    python Tools/validate_data.py
"""
import json
import sys
from pathlib import Path

DATA_DIR = Path(__file__).resolve().parent.parent / "Content" / "Data"
CATEGORIES = {"material", "lootbox", "placeable", "source"}
MODIFIER_KINDS = {"extra_rolls", "item_weight_mult", "item_count_mult", "add_entry", "reveal"}
KINDS_NEEDING_ITEM = {"item_weight_mult", "item_count_mult", "add_entry"}
CONDITIONS = {"gt", "gte", "lt", "lte", "eq"}
CHECKS = {"inventory", "placed", "stat", "unlocked"}
REQUIRED_ACTIONS = {"inject", "craft", "use", "deploy", "recall", "sort_inventory", "load", "unload"}


def main() -> int:
    errors: list[str] = []
    items, recipes, tables, actions, radiation = {}, [], {}, [], []

    files = sorted(DATA_DIR.glob("*.json"))
    if not files:
        print(f"No data files in {DATA_DIR}")
        return 1

    for path in files:
        try:
            doc = json.loads(path.read_text(encoding="utf-8"))
        except json.JSONDecodeError as exc:
            errors.append(f"{path.name}: invalid JSON: {exc}")
            continue
        for item in doc.get("items", []):
            items[item.get("id")] = item
        recipes += doc.get("recipes", [])
        for table in doc.get("lootTables", []):
            tables[table.get("id")] = table
        actions += doc.get("actions", [])
        radiation += doc.get("radiation", [])

    def check_item(item_id, where):
        if not item_id:
            errors.append(f"{where}: missing item id")
        elif item_id not in items:
            errors.append(f"{where}: unknown item '{item_id}'")

    radiation_ids = {rad.get("id") for rad in radiation}

    for item_id, item in items.items():
        where = f"item '{item_id}'"
        stack = item.get("stackSize", 100)
        if not item_id:
            errors.append("item with no id")
        if stack < 1:
            errors.append(f"{where}: stackSize must be >= 1")
        if item.get("category") not in CATEGORIES:
            errors.append(f"{where}: unknown category '{item.get('category')}'")
        table = item.get("lootTable")
        if table and table not in tables:
            errors.append(f"{where}: unknown lootTable '{table}'")
        if item.get("category") == "lootbox" and not table:
            errors.append(f"{where}: lootbox items need a lootTable")
        if (item.get("category") in ("placeable", "source") or table) and stack != 1:
            errors.append(f"{where}: placeable, lootbox and source items must have stackSize 1")
        if item.get("category") == "source" and item.get("radiation") not in radiation_ids:
            errors.append(f"{where}: unknown radiation '{item.get('radiation')}'")
        stacks, tier = item.get("maxExposureStacks", 0), item.get("maxRadiationTier", 0)
        if stacks < 0 or tier < 0 or (stacks > 0 and item.get("exposureSeconds", 10) <= 0):
            errors.append(f"{where}: enclosure needs maxExposureStacks/maxRadiationTier >= 0 and exposureSeconds > 0")

    unlock_keys = {f"recipe:{r.get('id')}" for r in recipes} | {f"action:{a.get('name')}" for a in actions}

    def check_requirements(reqs, where):
        for req in reqs:
            check = req.get("check", "inventory")
            if check not in CHECKS:
                errors.append(f"{where}: unknown requirement check '{check}'")
            if req.get("condition", "gt") not in CONDITIONS:
                errors.append(f"{where}: unknown condition '{req.get('condition')}'")
            if req.get("item"):
                check_item(req["item"], f"{where} requirement")
            if check == "stat" and not req.get("id"):
                errors.append(f"{where}: stat requirement needs an id")
            if check == "unlocked" and req.get("id") not in unlock_keys:
                errors.append(f"{where}: unknown unlock '{req.get('id')}' (use recipe:<id> or action:<name>)")

    for recipe in recipes:
        where = f"recipe '{recipe.get('id')}'"
        check_requirements(recipe.get("revealRequirements", []), where)
        check_item(recipe.get("output"), f"{where} output")
        if recipe.get("outputCount", 1) < 1:
            errors.append(f"{where}: outputCount must be >= 1")
        for cost in recipe.get("cost", []):
            check_item(cost.get("item"), f"{where} cost")
            if cost.get("count", 0) < 0:
                errors.append(f"{where}: negative cost")

    for table_id, table in tables.items():
        where = f"loot table '{table_id}'"
        lo, hi = table.get("rollsMin", 1), table.get("rollsMax", 1)
        if lo < 0 or hi < lo:
            errors.append(f"{where}: need 0 <= rollsMin <= rollsMax")
        if not table.get("entries"):
            errors.append(f"{where}: has no entries")
        for entry in table.get("entries", []):
            check_item(entry.get("item"), where)
            if entry.get("weight", 1) <= 0:
                errors.append(f"{where}: weight must be > 0")
            if entry.get("minCount", 1) < 1 or entry.get("maxCount", 1) < entry.get("minCount", 1):
                errors.append(f"{where}: need 1 <= minCount <= maxCount")

    names = set()
    for action in actions:
        where = f"action '{action.get('name')}'"
        names.add(action.get("name"))
        if action.get("cooldown", 0) < 0 or action.get("castTime", 0) < 0:
            errors.append(f"{where}: cooldown/castTime must be >= 0")
        table = action.get("lootTable")
        if table and table not in tables:
            errors.append(f"{where}: unknown lootTable '{table}'")
        check_requirements(action.get("requirements", []) + action.get("revealRequirements", []), where)
    for missing in sorted(REQUIRED_ACTIONS - names):
        errors.append(f"action '{missing}' is required by the code but not defined")

    seen = set()
    for rad in radiation:
        where = f"radiation '{rad.get('id')}'"
        if not rad.get("id") or not rad.get("family"):
            errors.append(f"radiation entry missing id/family: {rad}")
        if rad.get("id") in seen:
            errors.append(f"{where}: duplicate id")
        seen.add(rad.get("id"))
        effect = rad.get("effect")
        if effect:
            if effect.get("kind") not in MODIFIER_KINDS:
                errors.append(f"{where}: unknown effect kind '{effect.get('kind')}'")
            if effect.get("kind") in KINDS_NEEDING_ITEM:
                check_item(effect.get("item"), f"{where} effect")

    if errors:
        print(f"{len(errors)} problem(s) in {DATA_DIR}:")
        for error in errors:
            print(f"  - {error}")
        return 1

    print(f"OK: {len(items)} items, {len(recipes)} recipes, {len(tables)} loot tables, "
          f"{len(actions)} actions, {len(radiation)} radiation types")
    return 0


if __name__ == "__main__":
    sys.exit(main())
