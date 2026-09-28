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
- [x] Starter tree from Perturb to Gamma, with a playthrough test.
- [ ] Tech tree viewer panel with hints for locked entries.

## M2.5: Game menu

- [ ] Esc opens a pause menu: Resume, New Game, Save, Load, Settings, Quit.
- [ ] Save slots (several named saves instead of the single autosave slot).
- [ ] Settings: key rebinding (Enhanced Input user settings), mouse and free-look
      sensitivity, invert Y, pan speed, UI scale, graphics quality.
- [ ] Settings persist per user (`USaveGame` or `UGameUserSettings`).

## M3: Logistics on the grid

- [ ] Spike: true 3D grid navigation (see *The grid* in [DESIGN.md](DESIGN.md)). Face
      placement on existing entities, a placement plane that follows the cursor with a
      slide-along-normal modifier and a depth stalk, snap camera views. Decide one level
      versus 3D from how it plays; the sim needs no change either way.
- [ ] Resource nodes at coordinates, so scavenging happens *somewhere*.
- [ ] Transport time proportional to distance between cells.
- [ ] Simple automation: an entity that moves items to its neighbour.

## M4: Content and progression

- [ ] More materials and the radiation tier ladder.
- [ ] Carbon and iron loot box variants (their tables already exist).
- [ ] A goal and ending (candidates in [COSMOLOGY.md](COSMOLOGY.md): today's universe, or
      diving into a black hole made inside the pocket universe).

## M6: Genesis (early game as the Big Bang)

See [COSMOLOGY.md](COSMOLOGY.md) for the physics behind each item.

- [x] Cosmic clock: epochs in `universe.json` with a log-scale clock, shown in the HUD by
      epoch name, and an `epoch` check for requirements.
- [x] Reorder the materials: hydrogen and helium first. Carbon and iron come out of
      collapsing caches (fused by the crush) until there are stars.
- [x] Perturb replaces Inject Matter: it seeds or deepens a ripple (an overdensity) in a
      grid cell. Ripples yield matter; after recombination gravity deepens them, and Perturb
      retires (`retireRequirements`).
- [x] Recombination as the phase boundary: a glowing plasma veil in the sky, driven by the
      epoch, clears when it's reached.
- [x] Hawking evaporation on the host black hole, with Feed the Horizon and a `host` check.
      Decision: a soft fail. At zero the universe freezes until fed.
- [ ] Play it in the engine: compile, run the automation tests, and tune the host lifetime,
      perturbation cost, ripple yields and epoch thresholds by feel.
- [ ] A visible hint for the next epoch (what it needs), like the tech tree viewer's hints.

## M7: Structure (mid game as the cosmic web)

Matter moves into the pocket universe first (see *Matter lives in the pocket universe* in
[DESIGN.md](DESIGN.md)), then gravity moves it around.

- [ ] Grid geometry behind one plain C++ type (neighbours, distance, cells within a radius,
      world position), and pick the cell shape: cubes, or hexagons (see *Cell shape*).
- [ ] Cells hold matter: an amount per material next to the entity. Ripples deposit into
      their own cell.
- [ ] Costs are paid from cells within `reachRadius` (data, in cells) of the build site.
- [ ] Caches, enclosures and sources become cell entities, built and dismantled in place.
      Sources may irradiate caches within a radius.
- [ ] Remove the player inventory, Sort and Annihilate. Add a universe totals readout and a
      hovered-cell contents panel.
- [ ] Gas drifts downhill and diffuses. Injecting hydrogen fills the target cell.
- [ ] Conveyors deliver matter into a build site's reach automatically (the radius stays the
      same).
- [ ] Gravity field: softened 1/r potential from every massive body, sampled on the grid.
      The top-two bodies (earlier-created wins ties) shape the displayed field and define the
      Roche lobe borders.
- [ ] Dark matter placement as the gravity-altering power, budgeted about five to one
      against ordinary matter. Filaments are the conveyors, halos the hubs.
- [ ] Expansion: lattice spacing grows with cosmic time except inside bound regions, so
      unbound logistics stretch. Dark energy makes it accelerate late in the phase.
- [ ] Bent grid, stage 1: displace the drawn grid vertices by a clamped copy of the field
      (under half a cell) while cell positions stay put for gameplay and picking.
- [ ] Bent grid, stage 2 (if stage 1 earns it): field lines and equipotentials as the grid
      near a well, then transport time measured in the warped space.

## M8: Stars (late game as today's universe)

- [ ] Gravity wells as Jeans collapse triggers; a well fed past a threshold becomes a star,
      and fed past a higher, visible threshold fragments into a binary.
- [ ] Stars as machines: mass sets spectral class, lifetime, products and remnant (tables in
      COSMOLOGY.md). Absorbing gas raises the class up to the Eddington cap. Metallicity
      of the gas decides whether small, long-lived stars are possible.
- [ ] Supernova yields rolled through the loot table system (a supernova is a very large
      cache), plus a blast that disperses gas and wrecks nearby structures.
- [ ] Binaries: a `Binary` record (members, masses, axis, eccentricity, phase) anchored to a
      barycentre cell, placing both bodies on Kepler orbits each tick. Circular first,
      eccentric later. The swept ellipse is an exclusion zone; the bent grid rotates with
      the pair. Test the Kepler placement in `LRSimulationTests.cpp`.
- [ ] Binary interactions: Roche lobe overflow through L1, Type Ia from a white dwarf and a
      companion, hardening by gas drag, common envelope and gravitational waves (Peters
      1964), then inspiral, merger and ringdown with the merger-product table.
- [ ] Triples: nested `Binary` records for hierarchical systems; a third body too close
      ejects the lightest of the three and tightens the survivors.
- [ ] Gravitational radiation as a radiation tier with binaries as its source.
- [ ] Recursion ending: a stellar black hole inside the pocket universe as a new game plus.

## M5: Ship it

- [ ] Check stars and grid lines in a packaged build: engine materials on instanced meshes
      need `bUsedWithInstancedStaticMeshes`. The editor sets it automatically in PIE; a
      packaged game may show the default checker material instead.
- [ ] Packaging profile (File → Package Project → Windows), and check that
      `Content/Data/*.json` is staged. `DirectoriesToAlwaysStageAsUFS` is already set.
- [ ] CI build on a self-hosted Windows runner with the engine installed (GitHub-hosted
      runners can't fit the engine).
