# Roadmap

These are suggested milestones, roughly in order. Each one should be small enough to finish
and play.

## M0: Build it, see it (first session)

- [ ] Install UE 5.8 and Visual Studio, build, and press Play (see README).
- [ ] Fix any first-compile errors. The code has never been compiled against the real engine.
- [ ] Run the automation tests. They should all pass.
- [ ] Create `Content/Maps/Main` as an Empty Level, set it as the default map, and
      commit it via LFS.

## M1: Make it feel like a game

- [ ] Item icons: textures in `Content/UI/Icons`, an `Icon` soft-object path on
      `FLRItemDef`, drawn in the inventory slots instead of the abbreviations.
- [ ] A loot box opening moment: a reveal panel listing what dropped, a Niagara burst on the
      box, and a sound. `OnActionCompleted` already carries `Gained`.
- [ ] Real meshes for the enclosures in the world (a Blueprint subclass of
      `ALRWorldGridActor`, or per-item mesh paths in data).
- [x] Hover highlight and selection on grid cells.
- [x] Cosmic look: grey/amber HUD, a grid of glowing light beams in the void, starfield, and
      an animated, ray-traced black hole backdrop.
- [ ] Grid beams in a real emissive material (they borrow the engine's widget material for
      now), with a smooth distance fade instead of stepped brightness layers.
- [ ] Higher-resolution black hole bake (1024²), lensed stars near the shadow, nebula haze.
- [ ] Verify the black hole's blend: if `Widget3DPassThrough_Translucent` turns out to be
      premultiplied (AlphaComposite), drop the `1/Alpha` scale in `FLRBlackHoleRenderer::RenderRow`
      (symptom: a bright halo or box around the disk).
- [ ] Optional: rebuild one HUD panel in UMG to learn the Widget Blueprint workflow.

## M2: Irradiation (the core idea)

See [DESIGN.md](DESIGN.md#irradiation-implemented-v1).

- [x] Enclosures hold a loot box and a radiation source (Load / Unload; Recall returns
      everything).
- [x] Five source items driven by `radiation.json` effects; enclosure material caps the tier.
- [x] Exposure over simulation time adds modifiers; X-rays reveal and lock the contents.
- [x] Modifiers and revealed contents in the loot box tooltip.
- [ ] Tuning pass, and a visual effect while an enclosure is active (Niagara glow).

## M2.2: Tech tree

- [x] Reveal requirements on recipes and actions latch into permanent unlocks (saved).
- [x] Lifetime stats (`crafted:`, `opened:`, `gained:`, `exposed:`, `done:`) and `unlocked` chains.
- [x] Starter tree from Inject to Gamma, with a playthrough test.
- [ ] Tech tree viewer panel with hints for locked entries.

## M2.5: Game menu

- [ ] Esc opens a pause menu: Resume, New Game, Save, Load, Settings, Quit.
- [ ] Save slots (several named saves instead of the single autosave slot).
- [ ] Settings: key rebinding (Enhanced Input user settings), mouse and free-look
      sensitivity, invert Y, pan speed, UI scale, graphics quality.
- [ ] Settings persist per user (`USaveGame` or `UGameUserSettings`).

## M3: Logistics on the grid

- [ ] Resource nodes at coordinates, so scavenging happens *somewhere*.
- [ ] Transport time proportional to distance between cells.
- [ ] Simple automation: an entity that moves items to its neighbour.

## M4: Content and progression

- [ ] More materials and the radiation tier ladder.
- [ ] Carbon and iron loot box variants (their tables already exist).
- [ ] A goal and ending.

## M5: Ship it

- [ ] Check stars and grid lines in a packaged build: engine materials on instanced meshes
      need `bUsedWithInstancedStaticMeshes`. The editor sets it automatically in PIE; a
      packaged game may show the default checker material instead.
- [ ] Packaging profile (File → Package Project → Windows), and check that
      `Content/Data/*.json` is staged. `DirectoriesToAlwaysStageAsUFS` is already set.
- [ ] CI build on a self-hosted Windows runner with the engine installed (GitHub-hosted
      runners can't fit the engine).
