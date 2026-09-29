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
  irradiators.

The progression arc is Factorio-shaped. Early on, you perturb the vacuum by hand. Later,
the pocket universe's own structures and machines produce, transform and move matter for
you, and your direct powers matter less.

[COSMOLOGY.md](COSMOLOGY.md) has the physics behind the theme and the three-phase arc (Big
Bang, structure formation, stars). The first phase is implemented: see *The pocket universe*
below.

## Core loop (implemented)

Everything happens in grid cells: there is no inventory (see *Matter lives in the pocket
universe*).

1. **Perturb** a grid cell to seed a ripple, or deepen one. Ripples gather matter into their
   own cell every few seconds, depending on the epoch: nothing during inflation, then
   hydrogen, then hydrogen and helium.
2. **Build** in a selected cell, paying with matter within reach: caches, irradiation
   irradiators, and radiation sources (built into an irradiator). A cache crushes hydrogen and
   helium, and collapses back into them plus some fused carbon and a little iron. Caches are
   the only source of carbon and iron until there are stars.
3. **Open** a cache where it sits (the `use` action) for randomized loot from weighted loot
   tables; the loot lands in its cell. A cache can contain other caches: that's the recursion.
4. **Dismantle** what's in a cell to get its cost back into that cell.

Loot boxes carry **modifiers**, which rewrite their loot table before it's rolled:

- extra rolls
- a multiplied weight for one item
- multiplied counts for one item

Irradiators create them (see below).

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
  which is what makes the early game self-sustaining. Structures can't be dismantled.
- **Perturb retires** at recombination (`retireRequirements`): it's hidden and refused for
  good, and the ripples carry on without it.
- **The host black hole evaporates** by Hawking radiation. Its mass cubed falls linearly,
  so a full-mass host lasts `lifetimeSeconds` (an hour) and the loss speeds up as it
  shrinks. Seeding a new ripple draws one feed's worth of its mass (15%, `seedFeeds`),
  deepening one draws 2%, and a perturbation is refused if it would take the last of it.
  **Feed the Horizon** (revealed at 90%) restores 15%. Feeding by hand is meant as the
  kick-start (getting a frozen universe going again, or buying ripples early), not upkeep: the
  pocket universe should come to feed itself (see "Feeding the host"). At zero
  the pocket universe freezes: ripples, irradiators and the clock stop until you feed it.
  That's the soft fail; nothing is lost.
- **The plasma** is a glowing veil in the sky whose opacity comes from the epoch, fading over
  a few seconds between epochs. It clears at recombination.

## Feeding the host (planned)

Feeding by hand should be the kick-start, not the upkeep, like hand-feeding coal into
generators to get a dead power grid back up in Satisfactory. It has two jobs:

- **Restarting a frozen universe** when the host has evaporated to nothing.
- **Buying ripples early.** Seeding a ripple costs one feed of host mass (implemented:
  `seedFeeds`), so feeding is how the first few ripples get paid for.

After that, the pocket universe should come to feed itself.

### The accretion disk and the Eddington limit

Nothing feeds the host directly. Fed matter goes into the host's **accretion disk**, and the
disk drains into the host no faster than the **Eddington rate**. In real physics, the most a
black hole can swallow before its own radiation blows the rest away grows in proportion to
its mass. So growth at the limit is exponential: the real e-folding time is the Salpeter time,
about 45 million years.

- **The intake rule.** The disk drains into the host at `eddingtonRate × max(mass,
  eddingtonFloor)` per second, in fractions of the starting mass. The floor keeps a dead host
  from being stuck at zero forever.
- **Restarts spool up.** With a rate of 0.02 and a floor of 0.1, a dead host takes about 50
  s to reach 10%, then speeds up. One feed brings it back in a bit over a minute. The universe
  stirs again once the host passes `restartMass` (e.g. 5%), so the spool-up is felt.
- **Spamming Feed doesn't work.** The disk holds at most `diskCapacity` (e.g. two feeds).
  Anything fed beyond that is blown off as jets and lost, and the log says so. How fast you
  feed matters, not how often you click. That also paces how quickly feeding can buy the
  early ripples.
- **Evaporation and intake meet at a tipping point.** Hawking evaporation is fastest for a
  small host, and Eddington intake is fastest for a big one. Below some mass, the drain beats
  anything the disk can deliver. Above it, a kept-full disk makes the host grow. That point is
  the early game's goal.
- **The host can grow past its starting mass.** Mass above 1 is allowed, so the "host is
  full" refusal goes. Lifetime scales as mass cubed: twice the mass lasts 8 times as long.
- **Feed stays** (it's never retired, unlike Perturb), because a universe can always freeze
  again.
- **Seeds and deepening draw on the host's mass,** not on the disk.

A data sketch for `universe.json`'s `host`:

```json
"eddingtonRate": 0.02, "eddingtonFloor": 0.1, "diskCapacity": 0.3, "restartMass": 0.05
```

**Readout.** The HUD shows the host's mass, what's in the disk, and intake against
evaporation per second (the net rate, so the tipping point is visible). The redone sky should
show the disk too: a fuller disk looks brighter.

### How the universe feeds itself

| Satisfactory | Here | When |
|---|---|---|
| Hand-feeding coal | **Feed the Horizon**: restarts, and the first ripples | whole game, mostly early |
| Coal on a conveyor | **Horizon Siphon** (working name; or Throat Pump) | mid game |
| Late-game free power | **Dark energy** | late game, with expansion |

**Horizon Siphon.** A structure built in a cell from recombination on. It pipes matter from
the cells within its reach through the "throat" back into the host's disk. In lore, that's
the Einstein–Rosen bridge between the pocket universe and the host (Popławski).
- **It's Perturb in reverse, and a trade-off.** Matter siphoned is matter not built with.
- **Mass value.** Each material is worth mass in proportion to its balance value (hydrogen 1,
  helium 2, carbon 4, iron 8, as in `Tools/balance.py`). Some constant, e.g. 1000 value, makes
  one feed.
- **Settings.** A throughput per siphon, and a filter (hydrogen only by default, so the rare
  materials aren't burned by accident).
- **It still goes through the disk,** so it's Eddington-limited. A bigger host can use more
  siphons.

**Dark energy.** Space expanding creates vacuum energy for free. Its density stays constant
while the volume grows, and in general relativity energy isn't conserved globally in an
expanding universe. Alan Guth called inflation "the ultimate free lunch" for this reason.
- **How it works.** With expansion (see "Expansion" below), each grid level passed, or the
  growth of the scale factor, adds mass to the host.
- **When it matters.** It's negligible before the dark-energy era (it took over at about 9.8
  billion years) and dominant after it. The late universe then sustains itself, as the real
  one does.
- **It skips the disk.** It isn't infalling matter, so it isn't Eddington-limited.
- **Before that,** the vacuum energy of inflation is spent making the matter (reheating), not
  the host.

**Later ideas.** Black holes that form inside the pocket universe (from dead stars) could
anchor the host: slowing its evaporation, or adding their mass when they merge.

## Time controls (planned)

The log-scale cosmic clock stays as it is. On top of it the player gets a speed control: a
**speed level** from -1 to 3, which buttons (and keys) step up or down one at a time.

| Level | Speed | Display |
|---|---|---|
| -1 | paused | `\|\|` |
| 0 | 1x | `\|>` |
| 1 | 2x | `\|> >` |
| 2 | 4x | `\|> >>` |
| 3 | 8x | `\|> >>>` |

- Speed is 2^level, and paused is 0. The plumbing exists: `ULRGameSubsystem::SetTimeScale`
  (and the `LRTimeScale` console command) already multiply what the simulation advances each
  frame. The feature is the control and its display in the HUD header, next to the cosmic
  time.
- Everything scales together (the epoch clock, ripple yields and growth, exposures, build
  times, Hawking evaporation), so speed is comfort, not an exploit, and pausing is safe.
- Commands still work while paused: build, perturb and open are accepted, and anything with
  a duration waits for time to run.
- A new game and a loaded game start at 1x.
- Later: higher levels for the late game, when the clock spans billions of years.

## Expansion: an infinite zoom out (planned)

Time expands; space should too. Ripples start out tiny, and as the universe expands the grid
should shrink away from the camera and be replaced by a bigger grid growing out of it, like a
slow, endless zoom out. Expect to iterate on this until it feels right.

**The physics lines up with the log clock.** How big the universe is, its scale factor `a`,
grows as a power of time in each era: `a ∝ t^½` while radiation dominates, `t^⅔` while
matter does, and exponentially during inflation (about e^60 in 10^-32 s). On a log-time clock
a power law is a straight line, so **within an epoch the zoom-out runs at a constant rate**.
Inflation is one fast plunge, and the later eras are steady drifts. That makes the rate a
per-epoch number in `universe.json`, e.g. `zoomLevels`: how many grid levels the epoch
passes through.

**How it looks.**
- The grid (and what's on it) is drawn at a view scale that shrinks continuously.
- When the current cells get small on screen, the next level's larger hexagons fade in over
  them, and the old lines fade out. Every level looks the same, so it loops seamlessly
  (a Droste effect).
- It must rebase rather than truly scale by 7^k, or world coordinates overflow.
- The camera's own zoom stays the player's.

**How the levels nest.** Hexagons don't subdivide into hexagons exactly. There are three
options, and the first two are worth prototyping:
- **Aperture 7:** each big cell is a small cell plus its six neighbours. It's √7 ≈ 2.65 times
  larger and turned about 19.1° each level, so the zoom spirals gently. That could look great
  or be disorienting. Membership is exact (every small cell belongs to exactly one big cell),
  so gameplay can merge cells cleanly.
- **Scale ×2 with a cross-fade:** the lines never line up, but the fade hides that. It's
  simplest, and visual only.
- **Aperture 3:** √3 larger, turned 30°.

Level conversions belong in `FLRHexGrid` (a cell's parent and children), so nothing does the
maths inline.

**What happens to things when the level changes** (the open part):
- **Stage 1, visual only.** The grid zooms and swaps, and entities shrink with it. This
  finds the right rate and look.
- **Stage 2, coarse-graining.** Seven cells become one:
  - Their matter sums.
  - Ripples in the group merge into one deeper ripple, like small structure merging into
    larger (structure in the real universe grows hierarchically).
  - Machines either keep a sub-cell position and are drawn smaller, or are refunded.
- **Reach stays in cells.** Physical reach grows with every level, so logistics scale with
  the universe for free.
- **It tells the story.** Quantum ripples from inflation become the seeds of galaxies, and
  each epoch's structures become the fine detail of the next.

## Matter lives in the pocket universe (implemented)

There is no player inventory: the operator can't reach into the pocket universe, so there is
nowhere outside it for a stockpile to live. Everything is contained in the pocket universe.

- **Cells hold matter.** Each cell holds an amount of each material, alongside at most one
  entity. Ripples deposit their yield into their own cell, and an opened cache spills into
  its cell. The grid draws a disc of gas in every cell that holds matter, sized by how much
  and tinted by the mix. Later that gas drifts along the gravity field.
- **Costs come from within a reach radius.** Building at a cell draws its cost from the
  matter in cells within `reachRadius` steps of it (`universe.json`, currently 2), in its
  layer: the cell itself first, then ring by ring. Distance is the hex grid's own.
- **Where things go.** A cache goes into an empty cell, or into the empty chamber of an
  irradiator there. A radiation source goes into the irradiator in the cell. A machine takes an
  empty cell. Materials can go anywhere.
- **Nested caches** from an opened cache land in its spot, then the nearest empty cell
  within reach. With no room, they're lost (the log says so).
- **Dismantle** returns what something cost to its cell. An irradiator comes apart a layer at
  a time: its source, then its cache, then the irradiator itself. Refunds don't count as
  gained for the tech tree.
- **Conveyors deliver, they don't extend reach** (planned). Once gravity conveyors unlock, the
  radius stays the same; conveyors carry matter into a build site's reach automatically.
- **Readability.** The Universe panel shows each material's total and how much is within
  reach of the selected cell. The Info panel lists a hovered cell's matter.

## The grid (implemented)

- **Hexagonal cells in layers**, unbounded in every direction, one entity per cell (plus
  matter).
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
  becomes the puzzle. Keep the irradiator next to the injection point and the power source
  next to the irradiator.
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
| `matter` | matter in the whole pocket universe (by `item` or `category`) | `{"check":"matter","item":"carbon","condition":"gte","value":100}` |
| `placed` | entities in the grid (by `item` or `category`) | `{"check":"placed","category":"lootbox","condition":"gt","value":0}` |
| `stat` | lifetime counters the game records | `{"check":"stat","id":"exposed:x_rays","condition":"gte","value":1}` |
| `unlocked` | 1 if another recipe/action is unlocked | `{"check":"unlocked","id":"recipe:optical_emitter","condition":"eq","value":1}` |
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
| Perturb | from the start |
| Feed the Horizon | when the host is down to 90% of its mass |
| Build, Quantum Cache recipe | at nucleosynthesis |
| Open, Dismantle | after building your first cache |
| Nebula Irradiator | after opening a cache |
| Optical Emitter | after building a nebula irradiator |
| Infrared Emitter | after 3 visible-light exposures |
| Microwave Emitter | after 3 infrared exposures |
| Corona Irradiator | after 2 microwave exposures |
| X-Ray Emitter | after crafting a corona irradiator |
| Gamma Emitter | after your first X-ray |
| Perturb retires | at recombination |

The `TechTree.ShippedTreeIsPlayable` automation test plays this tree and the epochs from a
new game to the last unlock, so a data change that creates a dead end fails a test.

**Later:** a tech tree viewer (locked entries shown as "???" with hints), branching choices,
and requirements with OR.

## Irradiation (implemented, v1)

Irradiation is how loot boxes get better. Numbers are placeholder tuning in
`Content/Data/items.json` and `radiation.json`.

**Irradiators** are built in a cell. The tiers are named after ever hotter things in the sky:
a nebula glows in infrared, visible light and microwaves, a star's corona gives off X-rays, and a
future tier above could be the **Magnetar Irradiator** (gamma bursts). They draw see-through (`opacity` 0.25 in
`items.json`), so the cache and source inside are visible. Each one has:

- a **chamber** (one loot box)
- a **source slot** (one radiation source)
- a **max radiation tier** it can safely contain
- a **max number of stacks** it can add to a box
- an **exposure interval** in seconds

| Irradiator | Max tier | Max stacks | Interval |
|---|---|---|---|
| Nebula | 4 (up to microwaves) | 3 | 10s |
| Corona | 7 (up to gamma) | 5 | 12s |

**Sources** are built into an irradiator and emit one radiation type. Each radiation's `effect` in
`radiation.json` is the loot modifier one exposure adds:

| Source | Radiation (tier) | Effect per stack | From the notes |
|---|---|---|---|
| Optical Emitter | Visible light (1) | Carbon amounts ×1.35 | photosynthesis |
| Infrared Emitter | Infrared (2) | +1 roll | heating, curing |
| Microwave Emitter | Microwaves (4) | Iron amounts ×1.8 | ore extraction |
| X-Ray Emitter | X-rays (5) | Reveals the contents and **locks** them | inspection |
| Gamma Emitter | Gamma (7) | Box may contain Loot Boxes (+15 weight per stack) | mutation |

**Balance target** (check with `python Tools/balance.py`):

- A plain cache returns about 90% of its cost by value: a small gamble, not a farm. The
  tool values a unit of hydrogen at 1, helium 2, carbon 4 and iron 8, by rarity.
- Each exposure stack adds roughly 15–25% value, so a full nebula irradiator is +40–60% and
  a full corona irradiator roughly doubles a cache.
- Irradiators are the scaling: many of them work in parallel, while Perturb is manual.

**How it plays:**

1. Build an irradiator in a cell, then, with that cell selected, build a cache and a source
   into it.
2. While both are in, the irradiator adds one modifier stack per interval until the cache
   reaches the irradiator's cap. The log reports each exposure.
3. X-rays are special. They add no stack; they roll the cache's (already modified) table
   right away and lock the result. The cache then shows exactly what's inside, and it can't
   be changed any further. Irradiate first, then inspect.
4. **Open** the cache right there; its loot lands in the irradiator's cell. To change sources,
   **Dismantle** once (the source comes out first, refunded) and build the next one in.
   Caches can't move between irradiators any more, so stacking different radiation on one
   cache means swapping sources while it stays in the chamber.

**Ideas for later:**

- Sources that decay, or that need power: alpha / Pu-238 RTGs are the natural power source.
- Ultraviolet (lithography → chips), radio, beta and neutron sources once there are items
  for them. A graphite-moderated neutron irradiator is a natural fit.
- Gamma's downside: a chance to destroy contents, or to need shielding around the irradiator
  (lead or concrete blocks in neighbouring cells, which uses the 3D grid).
- Irradiator upgrades, and multi-cell irradiators.

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
  tables exist but are unused)? Maybe the irradiator material chooses the box type?
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
