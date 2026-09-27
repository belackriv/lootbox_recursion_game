#!/usr/bin/env python3
"""Expected-value calculator for quantum caches (loot tables + irradiation modifiers).

Mirrors FLRSimulation::ApplyModifiers / RollLootTable on the data in Content/Data, so you can
tune loot_tables.json, recipes.json and radiation.json without playing hundreds of rounds.

    python Tools/balance.py                                  # plain cache + common irradiation combos
    python Tools/balance.py infrared infrared microwaves     # a specific combo (radiation ids)
"""
import json
import sys
from pathlib import Path

DATA = Path(__file__).resolve().parent.parent / "Content" / "Data"


def load(name, key):
    return json.loads((DATA / name).read_text(encoding="utf-8"))[key]


def apply_modifiers(table, modifiers):
    rolls_min, rolls_max = table["rollsMin"], table["rollsMax"]
    entries = [dict(e) for e in table["entries"]]
    for mod in modifiers:
        kind, item, value = mod.get("kind"), mod.get("item"), mod.get("value", 0)
        if kind == "extra_rolls":
            rolls_min = max(0, rolls_min + round(value))
            rolls_max = max(rolls_min, rolls_max + round(value))
        elif kind == "item_weight_mult":
            for e in entries:
                if e["item"] == item:
                    e["weight"] = max(0, round(e["weight"] * value))
        elif kind == "item_count_mult":
            for e in entries:
                if e["item"] == item:
                    e["minCount"] = max(0, round(e["minCount"] * value))
                    e["maxCount"] = max(e["minCount"], round(e["maxCount"] * value))
        elif kind == "add_entry":
            existing = next((e for e in entries if e["item"] == item), None)
            if existing:
                existing["weight"] += max(1, round(value))
            else:
                entries.append({"item": item, "weight": max(1, round(value)), "minCount": 1, "maxCount": 1})
    return rolls_min, rolls_max, entries


def expected(table, modifiers):
    rolls_min, rolls_max, entries = apply_modifiers(table, modifiers)
    rolls = (rolls_min + rolls_max) / 2
    total_weight = sum(max(0, e["weight"]) for e in entries)
    out = {}
    for e in entries:
        p = max(0, e["weight"]) / total_weight if total_weight else 0
        out[e["item"]] = out.get(e["item"], 0) + rolls * p * (e["minCount"] + e["maxCount"]) / 2
    return out


def main():
    tables = {t["id"]: t for t in load("loot_tables.json", "lootTables")}
    radiation = {r["id"]: r for r in load("radiation.json", "radiation")}
    items = {i["id"]: i for i in load("items.json", "items")}
    recipes = {r["id"]: r for r in load("recipes.json", "recipes")}

    cache = items["loot_box"]
    table = tables[cache["lootTable"]]
    cost = {c["item"]: c["count"] for c in recipes["loot_box"]["cost"]}
    cost_total = sum(cost.values())

    combos = [sys.argv[1:]] if len(sys.argv) > 1 else [
        [], ["visible_light"] * 3, ["infrared"] * 3, ["microwaves"] * 3,
        ["infrared", "infrared", "microwaves"], ["infrared"] * 5, ["gamma_rays"] * 2,
    ]
    inject = tables["inject"]
    inject_ev = sum(expected(inject, []).values())
    print(f"Cache cost: {cost} = {cost_total}.  Inject Matter: ~{inject_ev:.0f} per use (free, 5s).\n")
    print(f"{'irradiation':<50}{'expected yield':<44}{'total':>7}{'vs cost':>9}")
    for combo in combos:
        mods = [radiation[r]["effect"] for r in combo if radiation[r].get("effect", {}).get("kind") not in (None, "reveal")]
        ev = expected(table, mods)
        total = sum(v for k, v in ev.items() if k in ("carbon", "iron"))
        extra = "".join(f", {v:.2f} {k}" for k, v in ev.items() if k not in ("carbon", "iron"))
        label = " + ".join(combo) or "(plain cache)"
        yield_text = f"{ev.get('carbon', 0):.0f} C, {ev.get('iron', 0):.0f} Fe{extra}"
        print(f"{label:<50}{yield_text:<44}{total:>7.0f}{(total / cost_total - 1) * 100:>+8.0f}%")


if __name__ == "__main__":
    main()
