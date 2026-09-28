# Design notes

This is a living document. Sections marked **(implemented)** match the code. Everything
else is design notes and proposals.

## Premise

Humanity has learned to harness a black hole as a **universe inside a universe**. The
pocket universe behind the event horizon has its own space, so it can be built in.

You are the **operator**, working from a facility outside the horizon. The horizon itself
hangs in the sky of the pocket universe: the animated black hole backdrop. You can't go in, so
there is no player character and the camera is a free "god view". What you *can* do:

- **Perturb** the vacuum: stretch a quantum fluctuation across a grid cell, seeding a ripple
  (an overdensity) there. Ripples gather matter on their own and send it through the horizon
  link. The link has limited bandwidth, which is what the cast time and cooldown represent.
- **Feed** the host black hole, which is slowly evaporating.
- **Compress** matter into **Quantum Caches** (the Rails "loot boxes"; code and data ids
  still say `loot_box`). A cache's contents are in superposition: nothing is decided until
  it's observed. Opening it collapses it. X-rays observe it early, so you see the result and
  it stays fixed. Gamma mutation means a cache can collapse into more caches, which is the
  recursion.
- **Assemble and place** machines on the pocket universe's grid, starting with irradiation
  enclosures.

The progression arc is Factorio-shaped. Early on, you perturb the vacuum by hand. Later,
the pocket universe's own structures and machines produce, transform and move matter for
you, and your direct powers matter less.

[COSMOLOGY.md](COSMOLOGY.md) has the physics behind the theme and the three-phase arc (Big
Bang, structure formation, stars). The first phase is implemented: see *The pocket universe*
below.

## Core loop (implemented)

1. **Perturb** a grid cell to seed a ripple, or deepen one. Ripples yield matter into the
   inventory every few seconds, depending on the epoch: nothing during inflation, then
   hydrogen, then hydrogen and helium.
2. **Craft** materials into loot boxes and irradiation enclosures. A cache crushes hydrogen
   and helium, and collapses back into them plus some fused carbon and a little iron. Caches
   are the only source of carbon and iron until there are stars.
3. **Open** loot boxes (the Use action) for randomized loot from weighted loot tables.
   A loot box can contain other loot boxes: that's the recursion.
4. **Deploy** placeable items into cells of the hexagonal grid, and **Recall** them back.
5. **Sort** to compress and order the 50-slot inventory.

Loot boxes carry **modifiers**, which rewrite their loot table before it's rolled:

- extra rolls
- a multiplied weight for one item
- multiplied counts for one item

Irradiation enclosures create them (see below).

## The pocket universe (implemented)

The early game follows the real early universe ([COSMOLOGY.md](COSMOLOGY.md)). The data is in
`Content/Data/universe.json`; numbers are placeholder tuning.

**Epochs** run in order. A new game starts in the first, and each later one begins once its
`advanceRequirements` are met (the same requirement format as the tech tree). The log
announces each one.

| Epoch | Cosmic time | Begins when | Ripples gather | Sky |
|---|---|---|---|---|
| Inflation | 10^-36 s | a new game starts | nothing: there is no matter yet | faint plasma |
| Reheating | 10^-32 s | 3 perturbations | hydrogen | opaque, glowing plasma |
| Nucleosynthesis | 3 min | 100 hydrogen gathered | hydrogen and helium; caches unlock | glowing plasma |
| Recombination | 380 thousand years | 3 ripples and 3 opened caches | hydrogen and helium; gravity deepens ripples | clears: the fog lifts |

- **The cosmic clock** sweeps from an epoch's start to the next one's over `clockSeconds` of
  play, on a log scale, then waits there. The HUD shows the epoch and the cosmic time.
- **Ripples** (the `overdensity` structure) are created by Perturb in an empty cell, and
  deepened by Perturb up to amplitude 5. Each yield rolls the epoch's `yieldTable` once per
  amplitude. Before recombination, radiation pressure stops matter clumping, so only Perturb
  deepens them. From recombination on, gravity deepens them every `rippleGrowthSeconds`,
  which is what makes the early game self-sustaining. Structures can't be recalled.
- **Perturb retires** at recombination (`retireRequirements`): it's hidden and refused for
  good, and the ripples carry on without it.
- **The host black hole evaporates** by Hawking radiation. Its mass cubed falls linearly,
  so a full-mass host lasts `lifetimeSeconds` (an hour) and the loss speeds up as it
  shrinks. Each perturbation draws 2% of its mass, and a perturbation is refused if it
  would take the last of it. **Feed the Horizon** (revealed at 90%) restores 15%. At zero
  the pocket universe freezes: ripples, enclosures and the clock stop until you feed it.
  That's the soft fail; nothing is lost.
- **The plasma** is a glowing veil in the sky whose opacity comes from the epoch, fading over
  a few seconds between epochs. It clears at recombination.

Overdensity yields go straight into the inventory, and whatever doesn't fit is lost. That's
the interim until matter lives in cells (next section).

## Matter lives in the pocket universe (planned)

The player inventory is a leftover of the loot-box game. The operator can't reach into the
pocket universe, so there is nowhere outside it for a stockpile to live. Everything will be
contained in the pocket universe instead, and the inventory goes away (roadmap M7).

- **Cells hold matter.** Each cell holds an amount of each material, alongside at most one
  entity. Ripples deposit their yield into their own cell. Later that gas drifts along the
  gravity field.
- **Costs come from within a reach radius.** Crafting or building at a cell draws its cost
  from the matter in cells within `reachRadius` cells of it. The radius is data (planned for
  `universe.json`), so it's easy to tune. Distance is measured in cells, using the grid's own
  distance function.
- **Conveyors deliver, they don't extend reach.** Once gravity conveyors unlock, the radius
  stays the same. Instead, conveyors carry matter into a build site's reach automatically.
- **Caches are cell entities.** Compressing gas at a cell makes a cache there, and collapsing
  it spills the contents into that cell.
- **Enclosures and sources are built in place** rather than crafted and then deployed.
  Dismantling returns their materials to the cell. A source could irradiate every cache
  within a few cells, which would make irradiation spatial.
- **Selection is of cells, not slots.** Load, Deploy and Recall become building and
  dismantling at a cell. Sort and Annihilate go away.
- **Readability.** A readout of the whole universe's totals and a panel for the hovered
  cell's contents replace the inventory grid.

## The grid (implemented)

- **Hexagonal cells in layers**, unbounded in every direction, one deployed entity per cell.
  A cell is `(Q, R, Layer)`: axial hex coordinates plus the layer. `FLRHexGrid` holds all the
  geometry (neighbours, distance, cells within a radius, world position, picking), and the
  simulation and world view both go through it, so the shape can still change in one place.
- Hexagons because all six neighbours are equally far away: a radius is a round ring, and
  flow has six clean directions with no diagonals.
- You build on one **layer** at a time. The grid lines, hover outline and placement preview
  show that layer; entities on every layer stay visible. Layers are stacked like hexagonal
  prisms for now.
- There's no support or gravity rule yet: anything can be placed on any layer.

**Going truly 3D (rhombic dodecahedra).** Each hexagon is the middle slice of a rhombic
dodecahedron standing on a three-faced vertex, so the true-3D version keeps `(Q, R, Layer)`
and the flat game plays exactly the same. Only the stacking changes:

- Shift layer k in the plane by k × (1/3, 1/3) in axial units, so each cell sits over a
  dimple between three cells below, like stacked oranges. The pattern repeats every three
  layers.
- Space the layers `Spacing × √(2/3)` apart instead of `Spacing`.
- Each cell then has 12 neighbours, all the same distance away: 6 in its layer, 3 above at
  `(Q, R, k+1)`, `(Q−1, R, k+1)`, `(Q, R−1, k+1)`, and 3 below at `(Q, R, k−1)`,
  `(Q+1, R, k−1)`, `(Q, R+1, k−1)`.
- Distance, radius and picking become their 3D versions in `FLRHexGrid`. Code written
  against those functions carries over; code that reads raw coordinates doesn't.

Other shapes that tile space, for the record:

| Shape | Neighbours | Why not |
|---|---|---|
| Cube (the old grid) | 6 faces, 26 touching | Diagonals are ambiguous, and a radius is a square. |
| Truncated octahedron | 14: 8 hexagons, 6 squares | Kelvin's foam cell, a nice tie-in with the cosmic web as a foam of voids, but two kinds of neighbour and harder to read. |

**Ideas, not implemented:**

- **One level or a true 3D grid?** Undecided, and worth a prototype. On one level, play
  happens on a single layer and the other Z layers are reserved for a later idea, such as
  extra dimensions. A true 3D grid (no layers, cells in a volume) is what the physics wants:
  orbits have inclination and the cosmic web is a foam, not a sheet. There are no support or
  structural rules either way; nothing rests on anything in space.

  Making 3D navigable, in order of expected payoff:

  1. *Face placement.* Raycast onto an existing entity's cell and place on the face that was
     hit. Most things go next to other things, and this has no depth ambiguity.
  2. *A placement plane that follows you.* The current single-plane cursor, but the plane
     passes through the hovered or last-placed cell and faces the camera's dominant axis. A
     modifier slides along the plane's normal, and a stalk from the cursor to the reference
     plane makes depth readable in empty space.
  3. *Depth cues from the grid.* Beams only near existing entities or in a translucent slab
     around the active plane, with distance fade. Near a well the bent grid becomes shells
     and radial lines, which are landmarks for free.
  4. *Camera.* Orbit around a focus point, with snap views along the three axes.

  The universe helps: anything that spins flattens (solar systems, galaxies, accretion
  discs), so a net spin from the first perturbations settles structure toward a plane. That
  plane is the ecliptic, a natural reference without being a cheat, and most builds sit near
  it while stars and remnants stray. The sim already stores unbounded `(Q, R, Layer)`, and
  the rhombic dodecahedron stacking above is the 3D grid, so the prototype is picking and
  rendering only. Its test: a fresh player builds a solid three-layer cluster and a ten-cell
  line in each direction without misplacing.
- **Logistics:** moving matter between cells takes time proportional to distance, so layout
  becomes the puzzle. Keep the enclosure next to the injection point and the power source
  next to the enclosure.
- **Multi-cell machines** (2x2 or 2x2x2 and so on), as the machines get more complex.
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
and announced in the log. Actions can also have `retireRequirements`: once those are met,
the action is **retired for good** (hidden and refused again). There's no separate research
system: the tree *is* these requirements, so it lives entirely in `recipes.json`,
`actions.json` and the epochs in `universe.json`.

Requirement checks:

| check | counts | example |
|---|---|---|
| `inventory` | items held (by `item` or `category`) | `{"check":"inventory","category":"lootbox","condition":"gt","value":0}` |
| `placed` | entities deployed in the grid | `{"check":"placed","condition":"gt","value":0}` |
| `stat` | lifetime counters the game records | `{"check":"stat","id":"exposed:x_rays","condition":"gte","value":1}` |
| `unlocked` | 1 if another recipe/action is unlocked | `{"check":"unlocked","id":"recipe:grow_lamp","condition":"eq","value":1}` |
| `epoch` | 1 once an epoch has been reached | `{"check":"epoch","id":"nucleosynthesis","condition":"eq","value":1}` |
| `host` | the host black hole's mass, in whole percent | `{"check":"host","condition":"lte","value":90}` |

Stats recorded automatically:

- `done:<action>` for each completed action
- `gained:<item>` for items received through actions and from ripples
- `crafted:<recipe>`
- `opened:<loot box item>`
- `exposed:<radiation>` for each irradiation exposure

Cheat items (`LRGive`) don't count.

**Starter tree** (placeholder pacing):

| Unlocks | When |
|---|---|
| Perturb, Sort, Annihilate | from the start |
| Feed the Horizon | when the host is down to 90% of its mass |
| Craft, Quantum Cache recipe | at nucleosynthesis |
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
| Perturb retires | at recombination |

The `TechTree.ShippedTreeIsPlayable` automation test plays this tree and the epochs from a
new game to the last unlock, so a data change that creates a dead end fails a test.

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
| Grow Lamp | Visible light (1) | Carbon amounts ×1.35 | photosynthesis |
| Infrared Emitter | Infrared (2) | +1 roll | heating, curing |
| Microwave Emitter | Microwaves (4) | Iron amounts ×1.8 | ore extraction |
| X-Ray Tube | X-rays (5) | Reveals the contents and **locks** them | inspection |
| Gamma Source | Gamma (7) | Box may contain Loot Boxes (+15 weight per stack) | mutation |

**Balance target** (check with `python Tools/balance.py`):

- A plain cache returns about 90% of its cost by value: a small gamble, not a farm. The
  tool values a unit of hydrogen at 1, helium 2, carbon 4 and iron 8, by rarity.
- Each exposure stack adds roughly 15–25% value, so a full carbon enclosure is +40–60% and
  a full iron enclosure roughly doubles a cache.
- Enclosures are the scaling: many of them work in parallel, while Perturb is manual.

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
- Where does injected matter appear? Decided: in the pocket universe. Injected hydrogen and
  ripple yields fill cells and drift from there (see *Matter lives in the pocket universe*).
- How is the game paced across 13.8 billion years? A cosmic clock on a log scale, with epoch
  checks in the tech tree, is the proposal in COSMOLOGY.md.
