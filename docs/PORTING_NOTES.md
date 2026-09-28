# Porting notes: Rails → Unreal

This project is a port of [`belackriv/lootbox_recursion`](https://github.com/belackriv/lootbox_recursion)
(Rails 8 + Vue 3 + Inertia + Action Cable). The rules carried over. The architecture changed,
because an online multi-user web app became a local single-player desktop game.

## Where each Rails concept went

| Rails / Vue | Unreal port | Notes |
|---|---|---|
| `app/data/player_actions.yml`, `loot_tables.yml` | `Content/Data/actions.json`, `loot_tables.json` | Same shape, camelCase keys. Also new: `items.json`, `recipes.json`, `radiation.json`. |
| `config/initializers/app_data.rb` | `FLRGameData::LoadFromDirectory` | Called from `ULRGameSubsystem::Initialize`. |
| `InventoryItem` STI subclasses (`STACK_SIZE`, `DISPLAY_NAME`, `TOOLTIP`) | `FLRItemDef` rows in `items.json` | New item types are data, not classes. |
| `ItemCraftingCost` + `CRAFTING_COST` + `User#get_craft_choices` | `FLRRecipeDef` in `recipes.json` | Costs are a list, so any material can be a cost. |
| `LootTable` (PORO) | `FLRLootTableDef` + `FLRSimulation::RollLootTable` | Same algorithm: roll count, then cumulative weight pick. |
| `LootBoxModifier` | `FLRLootModifier` + `FLRSimulation::ApplyModifiers` | Rails had only the no-op base class. The port implements three kinds (`extra_rolls`, `item_weight_mult`, `item_count_mult`) as hooks for irradiation. |
| `LootBox` row | `FLRLootBoxInstance` (by instance id) | |
| `Entity` + `InventorySlot` + `InventoryItem` rows | *(gone)* Matter per grid cell (`FLRCellMatter`) | The port first had a 50-slot inventory; the theme since moved everything into the pocket universe. `FLRInventorySlot` survives as what an enclosure's chamber and source hold. |
| `InventoryItemMutation` ledger | *(dropped)* | The ledger existed to stream deltas to the browser. Here the UI reads state directly. |
| `PlaceableEntity` / `IrradiationEnclosure` rows | `FLRPlacedEntity` in `TMap<coordinate, entity>` | Keeps its instance id across deploy and recall. |
| `PlayerAction` (ActiveModel) | `FLRActionDef` (static) + `FLRActionState` (dynamic) + `FLRActionStatus` (computed for UI) | |
| `PlayerActionState` table | `FLRActionState` inside the save file | |
| `User#perform_action` | `FLRSimulation::RequestAction` | |
| `PerformPlayerActionJob` (Solid Queue `wait: cast_time`) | Pending request in `FLRActionState`, run by `FLRSimulation::Advance` | Runs on simulation time. |
| `ActiveRecord::Base.transaction` + `raise ActiveRecord::Rollback` | `FLRSimulation::FTransaction` (RAII snapshot) | It rolls back automatically unless `Commit()` is called. |
| Action Cable `PlayerInventoryChannel` / `PlayerActionsChannel` | Delegates: `OnMatterChanged`, `OnWorldChanged`, `OnActionCompleted` | Native delegates on the sim, re-broadcast as Blueprint-assignable delegates on the subsystem. |
| Pinia `store/player.ts` (selection, world window) | `ULRGameSubsystem` selection + focus | |
| `MainLayout.vue`, `Index.vue`, `ActionBar.vue`, `InventoryGrid.vue`, ... | `SLRGameHud` (Slate) | `*_Lambda` attributes act like Vue computed properties. |
| `WorldGrid.vue` (virtualized 1D list) | `ALRWorldGridActor` (3D grid) + the Grid panel's deployed list | The grid draws glowing lines only around the camera focus. |
| `TrimButton.vue` | "Home" button / `H` key → `ULRGameSubsystem::FocusHome` | |
| Postgres | `USaveGame` in `Saved/SaveGames/LootboxRecursion.sav` | |
| Users, sessions, auth, mailers | *(dropped)* | Single-player. The save file takes the place of the user. |
| Minitest model tests | `Source/LootboxRecursion/Tests/LRSimulationTests.cpp` | Unreal Automation tests. |
| `application.css` `--color-fac-*` palette | `FLRHudStyle` | Same hex values. |

## Behaviour changes (intentional)

1. **Cooldowns and cast times run on simulation time.** Rails used the wall clock
   (`Time.current`). The game's clock only advances while the game runs, and you can speed
   it up with the `LRTimeScale` console command. Offline progress could come back later as a
   design choice.
2. **An invalid request doesn't burn the cooldown.** Rails set `on_cooldown_until` before
   checking whether the action could succeed. The port validates first, then starts the
   cooldown.
3. **Craft is enabled when any recipe is affordable.** Rails required wood (now carbon) > 50 **and**
   iron > 50. The check was strict, so exactly 50/50 couldn't craft a 50/50 loot box.
4. **Adding items tops up existing stacks before using empty slots.** Rails took the first
   slot in slot order that was either empty or a matching partial stack.
5. **Craft checks for space after paying the cost, inside one transaction.** If paying frees
   the only slot, the craft still succeeds. Rails looked for a slot first.
6. **Scavenge became Inject Matter, and it was a loot table** (`inject` in
   `loot_tables.json`, 25–34 of one material, as in Rails). Since then the theme has
   replaced it: Perturb seeds ripples that gather matter on their own (see *The pocket
   universe* in [DESIGN.md](DESIGN.md)). Any action with a `lootTable` still just rolls it.
7. **Loot tables belong to the item** (`lootTable` on the item def), not to a `LootBox`
   STI subclass. The `carbon_loot_box` (Rails `WoodLootBox`) and `iron_loot_box` tables were ported but no item uses
   them yet. In Rails, crafting made a plain `LootBox`, which used `default`.
8. **Loot boxes can drop loot boxes.** Any loot table entry may name a `lootbox` item, and
   each one gets its own instance. That's the "recursion", and a test covers it
   (`BoxInsideABox`).
9. **The single Irradiation Enclosure became Carbon and Iron variants**, per the design
   notes. Their costs (150/50 and 50/150) are placeholders. The Rails enclosure cost 100
   wood + 100 iron.
10. **Sort keeps unique items' identity.** A sorted loot box keeps its modifiers. Rails needed
    the "displaced item" logic for this. Here it follows from the data model.
11. **The repair and orphan-cleanup code is gone** (`User#use` loot box recovery,
    `cleanup_orphaned_inventory_items!`). Those fixed foreign-key drift, which in-memory
    state doesn't have.

## Changes after the port (new direction)

12. **Wood → carbon everywhere**: item ids, recipes, loot tables and tests. The theme is
    sci-fi now (a pocket universe inside a harnessed black hole), and wood isn't an element.
13. **The world is a 3D grid.** Rails had a signed 1D `world_coordinate`. Cells are now
    `FIntVector` (X, Y, Z), you build on a selectable Z layer, and the camera is a free
    god-mode camera. `SaveVersion` 2 ignores older saves.

## Deliberately not ported

- Auth, users, sessions, password reset, mailers.
- Server/client split, Action Cable, Solid Queue, Solid Cache, Kamal/Docker deployment.
- Per-user world isolation (`placed_by_user_id`). There's only one world.
