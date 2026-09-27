# Roadmap

These are suggested milestones, roughly in order. Each one should be small enough to finish
and play.

## M0: Build it, see it (first session)

- [ ] Install UE 5.8 and Visual Studio, build, and press Play (see README).
- [ ] Fix any first-compile errors. The code has never been compiled against the real engine.
- [ ] Run the automation tests. They should all pass.
- [ ] Create `Content/Maps/Main` from the Basic template, set it as the default map, and
      commit it via LFS.

## M1: Make it feel like a game

- [ ] Item icons: textures in `Content/UI/Icons`, an `Icon` soft-object path on
      `FLRItemDef`, drawn in the inventory slots instead of the abbreviations.
- [ ] A loot box opening moment: a reveal panel listing what dropped, a Niagara burst on the
      box, and a sound. `OnActionCompleted` already carries `Gained`.
- [ ] Real meshes for the enclosures in the world (a Blueprint subclass of
      `ALRWorldLineActor`, or per-item mesh paths in data).
- [ ] Hover highlight on 3D cells, and a selection outline.
- [ ] Optional: rebuild one HUD panel in UMG to learn the Widget Blueprint workflow.

## M2: Irradiation (the core idea)

See [DESIGN.md](DESIGN.md#irradiation-enclosures).

- [ ] Give placed entities an inventory (one loot box slot plus a source slot).
- [ ] Radiation source items, which draw from `radiation.json`.
- [ ] Exposure over simulation time adds `FLRLootModifier`s, and enclosure material limits
      the tier.
- [ ] Show modifiers on the loot box tooltip (the HUD already lists them).

## M3: Logistics on the line

- [ ] Resource nodes at coordinates, so scavenging happens *somewhere*.
- [ ] Transport time proportional to distance between cells.
- [ ] Simple automation: an entity that moves items to its neighbour.

## M4: Content and progression

- [ ] More materials and the radiation tier ladder.
- [ ] Wood and iron loot box variants (their tables already exist).
- [ ] A goal and ending.

## M5: Ship it

- [ ] Packaging profile (File → Package Project → Windows), and check that
      `Content/Data/*.json` is staged. `DirectoriesToAlwaysStageAsUFS` is already set.
- [ ] Settings menu, key rebinding (Enhanced Input user settings), save slots.
- [ ] CI build on a self-hosted Windows runner with the engine installed (GitHub-hosted
      runners can't fit the engine).
