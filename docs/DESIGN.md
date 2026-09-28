# Design notes

This is a living document. Sections marked **(implemented)** match the code. Everything
else is design notes and proposals.

## Premise

Humanity has learned to harness a black hole as a **universe inside a universe**. The
pocket universe behind the event horizon has its own space, so it can be built in.

You are the **operator**, working from a facility outside the horizon. The horizon itself
hangs in the sky of the pocket universe: the animated black hole backdrop. You can't go in, so
there is no player character and the camera is a free "god view". What you *can* do:

- **Inject** elementary matter (carbon, iron) through the horizon. The link has limited
  bandwidth, which is what the cast time and cooldown represent.
- **Compress** matter into **Quantum Caches** (the Rails "loot boxes"; code and data ids
  still say `loot_box`). A cache's contents are in superposition: nothing is decided until
  it's observed. Opening it collapses it. X-rays observe it early, so you see the result and
  it stays fixed. Gamma mutation means a cache can collapse into more caches, which is the
  recursion.
- **Assemble and place** machines on the pocket universe's grid, starting with irradiation
  enclosures.

The progression arc is Factorio-shaped. Early on, you inject everything by hand. Later,
machines inside the pocket universe produce, transform and move matter for you, and your
direct powers matter less.

[COSMOLOGY.md](COSMOLOGY.md) has the physics behind the theme and the proposed three-phase
arc (Big Bang, structure formation, stars), including the plan to replace Inject Matter with
a perturbation ability and reorder the materials so hydrogen comes first.

## Core loop (implemented)

1. **Inject Matter** to receive carbon or iron. It takes a cast time and has a cooldown.
2. **Craft** materials into loot boxes and irradiation enclosures.
3. **Open** loot boxes (the Use action) for randomized loot from weighted loot tables.
   A loot box can contain other loot boxes: that's the recursion.
4. **Deploy** placeable items into cells of the 3D grid, and **Recall** them back.
5. **Sort** to compress and order the 50-slot inventory.

Loot boxes carry **modifiers**, which rewrite their loot table before it's rolled:

- extra rolls
- a multiplied weight for one item
- multiplied counts for one item

Irradiation enclosures create them (see below).

## The grid (implemented)

- Integer cells `(X, Y, Z)`, unbounded in every direction, one deployed entity per cell.
- You build on one **layer** (Z) at a time. The grid lines, hover outline and placement
  preview show that layer; entities on every layer stay visible.
- There's no support or gravity rule yet: anything can be placed on any layer.

**Ideas, not implemented:**

- **Structural rules:** things above layer 0 need support, or the pocket universe has no
  gravity at all, which is a nice sci-fi excuse.
- **Logistics:** moving matter between cells takes time proportional to distance, so layout
  becomes the puzzle. Keep the enclosure next to the injection point and the power source
  next to the enclosure. Vertical stacking could be a way to keep distances short.
- **Multi-cell machines** (2x2x2 and so on), as the machines get more complex.
- **Gravity as a field on the grid** ([COSMOLOGY.md](COSMOLOGY.md#gravity-on-the-grid)):
  every massive body (overdensity, dark matter halo, star, remnant) adds a softened 1/r
  potential. Loose gas is a per-cell quantity that drifts downhill each tick; bodies don't
  drift. The two most massive bodies (earlier-created wins ties) shape the displayed field
  and its Roche lobe borders. Rendering is staged: a clamped visual warp of the lattice
  first, field lines and equipotentials as the grid near a well second, and transport time
  measured in the warped space last.
- **Binaries** ([COSMOLOGY.md](COSMOLOGY.md#binaries)): a bound pair orbits its barycentre
  on closed-form Kepler ellipses (no integration), sweeps an exclusion zone, and hardens
  through gas drag, common envelopes or gravitational waves until it merges. Triples are
  hierarchical or resolved by ejecting the lightest body.

## Tech tree (implemented)

Recipes and actions can have `revealRequirements`. Until those have all been met once, the
recipe or action is hidden (and refused). After that it is **unlocked for good**, saved,
and announced in the log. There's no separate research system: the tree *is* these
requirements, so it lives entirely in `recipes.json` and `actions.json`.

Requirement checks:

| check | counts | example |
|---|---|---|
| `inventory` | items held (by `item` or `category`) | `{"check":"inventory","category":"lootbox","condition":"gt","value":0}` |
| `placed` | entities deployed in the grid | `{"check":"placed","condition":"gt","value":0}` |
| `stat` | lifetime counters the game records | `{"check":"stat","id":"exposed:x_rays","condition":"gte","value":1}` |
| `unlocked` | 1 if another recipe/action is unlocked | `{"check":"unlocked","id":"recipe:grow_lamp","condition":"eq","value":1}` |

Stats recorded automatically:

- `done:<action>` for each completed action
- `gained:<item>` for items received through actions
- `crafted:<recipe>`
- `opened:<loot box item>`
- `exposed:<radiation>` for each irradiation exposure

Cheat items (`LRGive`) don't count.

**Starter tree** (placeholder pacing):

| Unlocks | When |
|---|---|
| Inject Matter, Loot Box recipe, Sort | from the start |
| Craft | after 2 injections |
| Use | when you first hold a loot box |
| Carbon Irradiation Enclosure | after opening a loot box |
| Deploy, then Recall | when you first hold a deployable, then once Deploy is unlocked |
| Grow Lamp | after crafting a carbon enclosure |
| Load, then Unload | once the Grow Lamp is unlocked, then once Load is |
| Infrared Emitter | after 3 visible-light exposures |
| Microwave Emitter | after 3 infrared exposures |
| Iron Irradiation Enclosure | after 2 microwave exposures |
| X-Ray Tube | after crafting an iron enclosure |
| Gamma Source | after your first X-ray |

The `TechTree.ShippedTreeIsPlayable` automation test plays this tree from a new game to
the last unlock, so a data change that creates a dead end fails a test.

**Later:** a tech tree viewer (locked entries shown as "???" with hints), branching choices,
and requirements with OR.

## Irradiation (implemented, v1)

Irradiation is how loot boxes get better. Numbers are placeholder tuning in
`Content/Data/items.json` and `radiation.json`.

**Enclosures** are deployable. Each one has:

- a **chamber** (one loot box)
- a **source slot** (one radiation source)
- a **max radiation tier** it can safely contain
- a **max number of stacks** it can add to a box
- an **exposure interval** in seconds

| Enclosure | Max tier | Max stacks | Interval |
|---|---|---|---|
| Carbon (graphite-lined) | 4 (up to microwaves) | 3 | 10s |
| Iron (steel-plated) | 7 (up to gamma) | 5 | 12s |

**Sources** are craftable items that emit one radiation type. Each radiation's `effect` in
`radiation.json` is the loot modifier one exposure adds:

| Source | Radiation (tier) | Effect per stack | From the notes |
|---|---|---|---|
| Grow Lamp | Visible light (1) | Carbon amounts ×1.25 | photosynthesis |
| Infrared Emitter | Infrared (2) | +1 roll | heating, curing |
| Microwave Emitter | Microwaves (4) | Iron amounts ×1.35 | ore extraction |
| X-Ray Tube | X-rays (5) | Reveals the contents and **locks** them | inspection |
| Gamma Source | Gamma (7) | Box may contain Loot Boxes (+15 weight per stack) | mutation |

**Balance target** (check with `python Tools/balance.py`):

- A plain cache returns about 90% of its cost: a small gamble, not a farm.
- Each exposure stack adds roughly 15–25% value, so a full carbon enclosure is +40–60% and
  a full iron enclosure roughly doubles a cache.
- Enclosures are the scaling: many of them work in parallel, while Inject is manual.

**How it plays:**

1. Deploy an enclosure, select it, then select a loot box in the inventory and press
   **Load**. Do the same with a source.
2. While both are loaded, the enclosure adds one modifier stack per interval until the
   box reaches the enclosure's cap. The log reports each exposure.
3. X-rays are special. They add no stack; they roll the box's (already modified) table
   right away and lock the result. The box then shows exactly what's inside, and it can't
   be changed any further. Irradiate first, then inspect.
4. **Unload** returns the box and the source. **Recall** returns the enclosure with
   everything in it. Stacks from different sources and enclosures add up on the same box,
   so moving a box between enclosures is a strategy.

**Ideas for later:**

- Sources that decay, or that need power: alpha / Pu-238 RTGs are the natural power source.
- Ultraviolet (lithography → chips), radio, beta and neutron sources once there are items
  for them. Graphite moderating neutrons in the carbon enclosure is a natural fit.
- Gamma's downside: a chance to destroy contents, or to need shielding around the enclosure
  (lead or concrete blocks in neighbouring cells, which uses the 3D grid).
- Enclosure upgrades, and multi-cell enclosures.

## Radiation catalogue (data in `Content/Data/radiation.json`)

### Electromagnetic

| Name | Tier | Energy | Game effect |
|---|---|---|---|
| Visible light | 1 | 4 | Photoelectric, photosynthesis |
| Infrared | 2 | 3 | Heating, curing |
| Ultraviolet | 3 | 5 | Disinfection, lithography (chips) |
| Microwaves | 4 | 2 | Ore extraction, ceramics |
| X-rays | 5 | 6 | Inspection, high-tech lithography (chips). Generated outside the nucleus. |
| Radio waves | 6 | 1 | Long-range comms |
| Gamma rays | 7 | 7 | Mutation, sterilization, huge penetration. Generated inside the nucleus. |

### Particle

| Name | Tier | Energy | Game effect |
|---|---|---|---|
| Alpha (helium nucleus) | 1 | 1 | Plutonium-238 energy (radioisotope thermoelectric generators), anti-static |
| Beta (fast electrons) | 2 | 2 | Gauging the thickness of materials |
| Neutron | 3 | 3 | TBD |

### Other families (no tiers yet)

- **Acoustic radiation:** ultrasound, sound, seismic waves. It needs a physical
  transmission medium, which could matter on the grid, where adjacent filled cells are
  the medium.
- **Gravitational radiation:** gravitational waves, ripples in spacetime. It's a late-game
  or exotic tier. Its source is a compact binary inspiralling (a steady emitter) and its
  merger (a burst); see *Binaries* in [COSMOLOGY.md](COSMOLOGY.md#binaries).

Tier (the order you unlock it) and energy (how strong it is) are separate on purpose.
Microwaves come fourth but carry little energy.

## Open questions

- Should there be offline or idle progress? Real-time cooldowns were the Rails behaviour.
- Does each loot box type get its own table (the `carbon_loot_box` and `iron_loot_box`
  tables exist but are unused)? Maybe the enclosure material chooses the box type?
- What is the goal or ending? [COSMOLOGY.md](COSMOLOGY.md#phase-3-stars-late-game) proposes
  two: reach today's universe (a complete periodic table, maybe a rocky planet), or dive
  into a stellar black hole made inside the pocket universe, which is the recursion.
- Does anything occupy grid cells from the start (resource nodes, "anomalies" in the pocket
  universe) so that position matters before logistics arrive? The perturbation plan answers
  this: perturbations seed overdensity nodes at coordinates that keep growing on their own.
- Where does injected matter appear: at a fixed "injection point" cell, or straight in your
  inventory, as now? With gas as a cell quantity, injected hydrogen fills the target cell and
  drifts from there; primordial materials from Perturb may still land in the inventory.
- How is the game paced across 13.8 billion years? A cosmic clock on a log scale, with epoch
  checks in the tech tree, is the proposal in COSMOLOGY.md.
