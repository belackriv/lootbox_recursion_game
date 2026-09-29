# Cosmology background

Design notes for the theme: the physics the game borrows and how each piece maps onto a
mechanic. Phase 1 (genesis) and Hawking evaporation are implemented; *The pocket universe* in
[DESIGN.md](DESIGN.md) describes what the code does today. The rest is still design notes.

The arc: the player is a scientist who has made a mini black hole and nucleated a pocket
universe inside it. The early game plays like the Big Bang, the mid game like structure
formation during rapid expansion, and the late game like the universe as it is now.

## The premise, and the papers behind it

**A universe inside a black hole.** Nikodem Popławski's 2010 papers model the interior of a
black hole as a new, expanding universe. Infalling matter reaches a minimum size and
bounces instead of hitting a singularity, and that bounce is the interior's Big Bang. Lee
Smolin's *cosmological natural selection* (1992) has universes reproducing through black
holes, each child with slightly varied physical constants.

**Making one in a lab.** Farhi and Guth, "An obstacle to creating a universe in the
laboratory" (1987): classically, a new universe can't be inflated from a small region
without a singularity in the past. Farhi, Guth and Guven (1990) get around it with quantum
tunneling. Lore for the game: the scientist doesn't build the universe, they *nucleate* it,
and the obstacle is why they can only nudge it afterwards.

**Hawking evaporation.** A black hole radiates with temperature inversely proportional to
its mass and loses mass at a rate proportional to 1/M², so its remaining lifetime scales as
M³. A stellar-mass black hole takes 10⁶⁷ years; one the mass of a mountain (10¹² kg) lasts
about the current age of the universe; anything smaller is short-lived and hot. A "mini"
black hole therefore has to be fed or it disappears, and the drain accelerates as it
shrinks. See *Pressure mechanics* below.

**Penrose process.** Up to 29% of a maximally spinning black hole's mass can be extracted
as usable energy from its ergosphere. That is the facility's power source, if it ever needs
one.

**Time dilation.** A distant observer sees clocks near the horizon run slow. The game
inverts it as lore: the pocket universe has its own time, and 13.8 billion years pass inside
while the operator watches. It also justifies idle or offline progress if we want it.

## The real timeline

| Epoch | Cosmic time | What happens | Game |
|---|---|---|---|
| Inflation | 10⁻³⁶ to 10⁻³² s | Space expands exponentially. Quantum fluctuations of the vacuum are stretched to cosmic size and frozen in as density ripples of about 1 part in 100,000. Every galaxy traces back to one of them. | Early: the Perturb ability |
| Reheating and baryogenesis | 10⁻³² to 10⁻⁶ s | The inflation field's energy decays into a hot plasma. A matter-over-antimatter excess of about one part in a billion survives annihilation. That excess is all the matter there is. | Early: a perturbation's yield |
| Big Bang nucleosynthesis | 3 to 20 minutes | Protons and neutrons fuse. By mass: about 75% hydrogen, 25% helium-4, traces of deuterium, helium-3 and lithium-7. Nothing heavier. | Early: the primordial materials |
| Recombination | 380,000 years | The plasma cools to about 3000 K and becomes neutral gas. The universe turns transparent and releases the cosmic microwave background (CMB). Before this, light can't travel. | End of early game: the fog lifts |
| Dark ages | to about 100 million years | Neutral gas, no stars. Gravity slowly amplifies the inflation ripples into clumps, with dark matter doing most of the pulling. | Transition |
| First stars, cosmic web | 100 Myr to a few Gyr | Gas falls along dark matter filaments into halos and forms stars. Population III stars are huge, metal-free and short-lived; their deaths make the first carbon, oxygen and iron. Star formation peaks at "cosmic noon", about 3.5 Gyr in. | Mid: gravity and structure |
| Dark energy era | about 6 Gyr ago to now | Expansion accelerates. Star formation declines. Bound structures hold together; unbound ones drift apart forever. | Late: today's universe |

Today's inventory: 5% ordinary matter, 27% dark matter, 68% dark energy. The player only
ever handles the 5%, and everything they build is scaffolded on the other 95%.

## Phase 1: genesis (early game)

**Perturb replaces Inject Matter.** The player stretches a quantum fluctuation into an
overdense region. The universe's own vacuum energy supplies the mass through reheating, so
the player never conjures matter; they decide where and how strongly the ripples land. This
also suits the caches: a Quantum Cache is a region whose contents are undecided until
observed, and a perturbation is a quantum event by nature.

**Primordial materials.** A perturbation yields hydrogen, helium and energy (photons).
Carbon and iron, the two base materials today, don't exist until the first stars die, and
iron is the end of the fusion road, which is the late game's whole point. So the materials
get reordered:

| Tier | Materials | Where they come from |
|---|---|---|
| Primordial | hydrogen, helium, (lithium) | Perturb, then self-sustaining overdensities |
| First metals | carbon, nitrogen, oxygen | dying low-mass stars, first supernovae |
| Alpha elements | neon, magnesium, silicon, sulphur, calcium | core-collapse supernovae |
| Iron peak | iron, nickel, chromium, manganese | Type Ia supernovae (mostly), core collapse |
| Heavy | strontium, barium, lead | slow neutron capture in giant stars |
| Heaviest | gold, platinum, uranium | rapid neutron capture in neutron star mergers |

Early caches are compressed hydrogen and helium. The Nebula and Corona Irradiators become
mid-game unlocks, and the irradiator's tier still caps the radiation it can hold.

**Self-sustaining by the end of the phase.** A perturbation seeds an overdensity node on the
grid that keeps accreting on its own (the resource nodes at coordinates already on the
roadmap). Once a few nodes are growing, gravity has taken over from the player and Perturb
retires.

**Recombination is the phase boundary.** Visually, the pocket universe starts as an opaque
glowing plasma; at recombination it clears and the player sees the structure their ripples
produced. That moment is also the reveal of the god view.

## Phase 2: structure (mid game)

**The cosmic web is a conveyor system.** Matter flows along dark matter filaments into halos
at the nodes. Filaments are the conveyor belts and halos the hubs.

**Dark matter is the gravity-altering power.** The player places dark matter, invisible
scaffolding that ordinary matter follows. Budget it by the real ratio, about five units of
dark matter per unit of ordinary matter created, so the power is limited by how much matter
the universe has so far.

**Expansion is the pressure.** Cells drift apart over cosmic time, so transport time between
unbound cells grows, and only gravitationally bound layouts stay compact. Bind your
production lines or watch them stretch. Dark energy arriving late in the phase turns that
drift into acceleration.

## Phase 3: stars (late game)

**A gravity well is a Jeans collapse trigger.** Enough mass in a small enough, cold enough
region falls in on itself. Injecting hydrogen into a well concentrates it; note primordial
hydrogen is already 75% of everything, so the well's real job is gathering what's there.

**Cooling and metallicity.** Metal-free gas cools badly (only molecular hydrogen radiates),
which is why Population III stars came out at hundreds of solar masses. Carbon and oxygen
let gas cool, so smaller, longer-lived stars become possible. Above about 1.3 solar masses,
stars fuse hydrogen through the CNO cycle, which needs carbon as a catalyst. The player's
earlier chemistry changes what kind of stars they can make.

**Feeding a star changes its class.** This is realistic. Protostars set their final mass by
accretion, and stars in binaries that gain mass move up the main sequence (the Algol paradox,
blue stragglers). More mass means hotter, bluer, more luminous and a shorter remaining life.
Luminosity scales roughly as M³·⁵ and main-sequence lifetime as 10 Gyr × M⁻²·⁵.

| Class | Mass (Sun = 1) | Colour | Main-sequence life |
|---|---|---|---|
| M | 0.08 to 0.45 | red | 100s of Gyr |
| K | 0.45 to 0.8 | orange | 20 to 70 Gyr |
| G | 0.8 to 1.04 | yellow | 10 Gyr |
| F | 1.04 to 1.4 | yellow-white | 3 to 8 Gyr |
| A | 1.4 to 2.1 | white | 1 to 3 Gyr |
| B | 2.1 to 16 | blue-white | 10 Myr to 1 Gyr |
| O | over 16 | blue | 1 to 10 Myr |

The real cap is the Eddington limit: above roughly 150 solar masses, radiation pressure
blows incoming gas back out. That's the natural "this star won't take any more" rule.

**Mass sets the fate.** A star is a machine whose mass decides its lifetime, its products and
what it leaves behind:

| Mass (Sun = 1) | Fate | Products | Remnant |
|---|---|---|---|
| under 0.08 | brown dwarf, never ignites | nothing | itself |
| 0.08 to 0.5 | red dwarf, trillions of years | nothing on any useful timescale | white dwarf, eventually |
| 0.5 to 8 | red giant, planetary nebula | carbon, nitrogen, slow-process heavies | carbon-oxygen white dwarf |
| 8 to about 20 | core-collapse supernova | oxygen, neon, magnesium, silicon, some iron | neutron star |
| over about 20 | core-collapse or direct collapse | as above | black hole |
| 140 to 260 (metal-free only) | pair-instability supernova | large iron and nickel yield | nothing |

**Recipes from pairs.** A white dwarf pulling gas from a companion passes 1.4 solar masses
(the Chandrasekhar limit) and goes Type Ia, which makes most of the universe's iron. Two
neutron stars merging run the rapid neutron-capture process that makes gold, platinum and
uranium. Feeding a neutron star past about 2 to 3 solar masses collapses it into a black
hole. Feeding a black hole just grows it.

**Why iron is the wall.** Binding energy per nucleon peaks around iron-56 and nickel-62, so
fusing iron consumes energy instead of releasing it. A star that builds an iron core has no
way to hold itself up, and once the core passes the Chandrasekhar mass it collapses in under
a second. Iron is the top of the fusion tree, and a supernova is the only way past it.

**Endings.** Two candidates:

- **Today's universe.** Dark energy dominant, star formation fading, a chemically complete
  periodic table, and perhaps a rocky planet, which needs the iron, silicon, oxygen and carbon
  the player spent the game making.
- **Recursion.** A stellar black hole inside the pocket universe is a candidate for a new
  pocket universe. Diving in is a prestige mechanic, and the thematic payoff of the title.

## Gravity on the grid

The grid stays a regular integer lattice in the simulation. Gravity is a field sampled on
it, and the "bendy" grid is a rendering of that field.

**The field.** Each massive entity (an overdensity, a dark matter halo, a star, a remnant)
has a mass. The potential at a cell is the sum of −M / (r + s) over the bodies, with a
softening length s of about one cell so nothing blows up at a body's own cell. The pull is
the potential's gradient. Two bodies is the readable case, and the two most massive bodies
can be the only ones that contribute to the *displayed* field and to the well boundaries
below. The restricted three-body problem (two masses plus massless test particles) is well
understood, and it's what "the top two masses control gravity" is.

**Gas drifts, bodies orbit.** Loose matter moves at a speed proportional to the pull
(overdamped, or "Aristotelian" gravity): each tick it steps to the neighbouring cell best
aligned with the downhill direction, with probability proportional to the slope. Gas has no
velocity, so it never orbits, and that is what makes the many-body case safe for gas:
three-body chaos comes from orbital dynamics, not from the potential. Gradient descent on a
potential with N wells just sorts everything into N basins.

Massive bodies are different: a bound pair of them orbits for real. Two bodies are the one
case Newton solved in closed form, so a binary's positions at any time come from Kepler's
laws with no integration and no drift. See *Binaries*.

**Wells have borders.** In the two-body field the boundary between basins is the L1 saddle
point, and each body's basin is its Roche lobe. Hydrogen injected inside a star's Roche lobe
falls into that star; injected outside it, it falls into the other one. Deterministic, and
the player can see the border because the grid warps towards each body.

**Gas is a cell property, not an entity.** A cell already holds at most one deployed
entity. Loose matter becomes a per-cell quantity (hydrogen density, later others) that
advects downhill along the field and diffuses slightly. Injecting hydrogen adds to the
target cell's quantity; the drift then carries it downhill from there. A star's cell absorbs
gas from its neighbours each tick; the absorbed mass raises its class per the table above,
until the Eddington cap. This is the mid-game conveyor and the late-game accretion with one
mechanism. Massive bodies (stars, remnants, halos) don't drift: they are the wells, and gas
is what falls. See *Binaries* for the one case where bodies do move.

**Which two bodies.** The two most massive bodies shape the displayed field and define the
well borders. When they are bound, they are the binary and the whole figure-eight rotates
with them. When masses tie, the body created earlier wins, which keeps the grid from
flickering when a growing star overtakes an older one only briefly.

**Rendering the bend.** Stage it:

1. *Visual warp.* Keep cell positions where they are for gameplay and picking. Displace the
   drawn grid vertices by a scaled, clamped copy of the field, so lines bow towards masses
   and cells near a well look squeezed. Clamp the displacement under half a cell and the
   warped grid stays unambiguous. The hover outline and placement preview draw at the warped
   position of the picked cell.
2. *Field lines as the grid.* Near a well, draw streamlines of the pull and equipotential
   rings instead of the lattice, like a polar grid around one star and a figure-eight around
   two. Cells become curvilinear (field-line index, potential index). This is the end-game
   look, and a bigger change, so only do it if stage 1 earns it.
3. *Make the warp physical.* Transport time uses distance in the warped space, so moving
   towards a well is quicker and moving away slower. At this point the grid is the gravity.

Expansion fits the same rendering: the lattice spacing grows with cosmic time, and bound
regions (inside a well) don't.

## Binaries

Most massive stars are in binaries (over 70% of O stars, about half of Sun-like ones), and
the two-body field is the game's excuse to make them. Binaries form when a collapsing cloud
fragments rather than making one star, so a gravity well fed past a threshold can split into
two stars instead of one bigger one: over-feed the cloud and you get a pair.

**Orbits come for free.** Two bodies orbit their common centre of mass on ellipses, and
their positions at any time are a closed-form function of six numbers: the two masses, the
semi-major axis, the eccentricity, the orientation and the phase. Nothing is integrated, so
nothing drifts or blows up, and the pair is exactly as stable after a billion years as on
day one. The sim keeps a `Binary` record (members, masses, axis, eccentricity, phase) whose
barycentre is anchored to a grid cell. Each tick it advances the phase and places each body
at its Kepler position, on the far side of the barycentre from its partner, with the lighter
body on the wider circle. Whichever cell a body sits in right now is the cell that absorbs
gas. Version 1 uses circular orbits (an angle that advances at a constant rate, no Kepler
equation to solve); eccentric orbits are a Newton solve of Kepler's equation and can come
later for the visual variety.

Orbital periods are for show. Real periods run from hours to centuries, and the cosmic clock
runs on a log scale, so tune the on-screen period for readability (seconds to tens of
seconds) and let Kepler's third law set the ratios: a tighter orbit is a faster one, and
that ratio is what sells the inspiral below.

**The pair claims a region.** The ellipse the orbits sweep is an exclusion zone on the grid,
like a multi-cell machine: nothing can be deployed inside it, and anything already there is
destroyed or thrown clear when the pair forms. The bent grid rotates with the pair, so the
Roche lobes, the L1 saddle between them and the L4/L5 points sixty degrees ahead of and
behind each body are visible as the field turns. Gas drifting in the field of a rotating pair
naturally leaks through L1 from the fuller lobe to the emptier one, which is Roche lobe
overflow, and piles up around L4 and L5, which is where Trojans live.

**What a binary does:**

- **Mass transfer.** When one star swells into a giant it overflows its Roche lobe, and gas
  streams through L1 to the companion, raising its class. This is the Algol paradox (the
  less massive star of a pair being the more evolved one) and it's how the player moves mass
  between stars without touching it.
- **Type Ia.** A white dwarf with a giant companion accretes past the Chandrasekhar limit and
  detonates, making iron. Two white dwarfs merging does the same.
- **Hardening.** The semi-major axis shrinks over time, and by Kepler's third law the orbit
  speeds up as it does, which is the chirp. Three things harden a binary, and all three are
  real:
  - *Gas drag.* A binary embedded in gas loses orbital energy to it. Dumping hydrogen on the
    pair is how the player pushes it towards a merger, which is the most useful lever.
  - *Common envelope.* A giant's envelope engulfs the companion and drags it in fast.
  - *Gravitational waves.* Compact remnants lose orbital energy to gravitational radiation.
    Peters (1964): the axis shrinks at a rate proportional to the product of the masses
    times their sum, divided by the axis cubed, so the last stretch is a runaway. Time to
    merge scales as axis to the fourth power over the mass product, which is why only
    close, heavy pairs merge on their own.
- **Merger.** Inspiral, merger, ringdown. The two bodies become one at the barycentre with
  the combined mass, less what was radiated (the first black hole merger seen by LIGO,
  GW150914, turned three solar masses out of sixty-five into gravitational waves). An
  optional recoil kick can throw the remnant a few cells, which is real for black holes.

Merger products by type:

| Pair | Result | Bonus |
|---|---|---|
| two main-sequence stars | one heavier, rejuvenated star (a blue straggler) | jumps a class |
| two white dwarfs | Type Ia supernova, or a heavy white dwarf | iron |
| two neutron stars | kilonova, then a black hole | gold, platinum, uranium; gravitational waves |
| neutron star and black hole | black hole | some rapid-process elements; gravitational waves |
| two black holes | heavier black hole | gravitational waves only |

Gravitational waves are the source for the *gravitational radiation* tier in the radiation
catalogue in [DESIGN.md](DESIGN.md), which had no source until now. A compact binary
inspiralling is a gravitational wave emitter for as long as it lasts, and the merger is a
burst. What that radiation does to a cache is open; it's the exotic top tier.

**A third body.** Real triples survive only when hierarchical: a tight inner pair and a third
body orbiting the pair's barycentre from well outside, at roughly four or more times the
inner axis (Alpha Centauri A and B with Proxima far out is the local example). That is
two-body all the way down: the inner pair is one point mass as far as the outer orbit is
concerned, so the sim's `Binary` record can nest. A third body that gets closer than that
is resolved the way nature resolves it, without simulating the chaos: the lightest of the
three is flung out to a far cell (or out of the universe) and the surviving pair is left
tighter. So three-body encounters are an ejection rule, not a physics problem.

Player control comes from placement and feeding. Where hydrogen lands decides which star
grows, feeding a well past the fragmentation threshold makes a pair, dumping gas on a pair
hardens it, and a third star placed too close ejects the lightest of the three.

## Pressure mechanics

- **Hawking evaporation.** The host black hole loses mass continuously, faster as it shrinks
  (lifetime scales as M³). The pocket universe's total mass-energy is the host's mass, so
  every perturbation and injection draws on it. Early on the operator feeds the host from
  outside; later the universe feeds it itself (a Horizon Siphon, then dark energy from
  expansion; see DESIGN.md, "Feeding the host"). Late game, the rate matters: it's the clock the endings
  race. Only micro black holes evaporate on game timescales, so the stellar black holes the
  player makes inside are effectively permanent.
- **Expansion.** Unbound cells drift apart, so logistics decay unless bound (phase 2).
- **Eddington limit.** Stars refuse mass above about 150 solar masses and blow gas back out.
  The host obeys the same limit, scaled up: the feed dial marks it, and anything injected
  above it is blown back out as jets. A real mini black hole couldn't be fed at all, because
  its Hawking glow is about 10¹² times its Eddington luminosity. The facility's beamed
  injection is the game's way round that (DESIGN.md, "Feeding the host").
- **Metallicity.** Metal-poor gas only makes monster stars, which die fast and violently.
  Pollute the gas first if you want long-lived stars.
- **Supernova blast.** A supernova enriches the neighbourhood but also disperses gas and can
  wreck nearby structures. Shielding with grid cells, already an idea for gamma, applies.

## Which process makes which element

From Jennifer Johnson's "periodic table of nucleosynthesis origins", simplified. Use this to
decide what each machine produces.

| Process | Elements | In game |
|---|---|---|
| Big Bang nucleosynthesis | hydrogen, helium, a little lithium | Perturb, overdensities |
| Cosmic ray spallation | lithium, beryllium, boron | radiation byproduct |
| Dying low-mass stars | carbon, nitrogen, strontium, barium, lead (slow neutron capture) | planetary nebula |
| Core-collapse supernovae | oxygen, neon, magnesium, silicon, sulphur, argon, calcium, some iron | supernova |
| Type Ia supernovae | iron, nickel, chromium, manganese | white dwarf + companion |
| Neutron star mergers | gold, platinum, europium, uranium (rapid neutron capture) | neutron star pair |

## A cosmic clock

Drive the phases off an epoch counter on a log scale rather than only off stats: seconds to
minutes of cosmic time in the early game, millions of years in the mid game, billions in the
late game. It's a new `revealRequirements` check in `FLRSimulation`, tested like the others,
and the HUD shows the current epoch by name.

## Open questions

- How fast does the host black hole evaporate, and can the player lose? A soft fail (the
  universe pauses until fed) is kinder than a hard one.
- Does dark matter get placed directly, or does the player only choose where filaments
  anchor and the sim lays them out?
- Does a fragmentation threshold on wells make binaries too easy or too random? A visible
  "this cloud will fragment above N" hint on the well keeps it a choice rather than a roll.
- How many binaries at once? Version 1 is one (the top-two pair); the rest are static
  wells. Several independent pairs, each dominating its own neighbourhood, is the next step,
  and nested pairs after that.
- Do the sweeping bodies destroy machines, or does the exclusion zone simply refuse
  deployment? Destroying is more dramatic and matches a supernova's blast.
- Do caches stay the loot mechanic throughout, or does the late game roll supernova yields
  through the same loot table system? (Probably yes: a supernova is a very large cache.)

## Reading

- Wikipedia: *Chronology of the universe*, *Big Bang nucleosynthesis*, *Stellar
  nucleosynthesis*, *Population III star*, *Jeans instability*, *Roche lobe*,
  *Hawking radiation*, *Cosmological natural selection*, *Algol paradox*, *Common envelope*,
  *Blue straggler*, *GW170817* (the neutron star merger seen in gravitational waves and
  light, which confirmed where gold comes from).
- P. C. Peters, "Gravitational radiation and the motion of two point masses" (1964), for the
  inspiral rates.
- N. Popławski, "Radial motion into an Einstein–Rosen bridge" (2010) and "Cosmology with
  torsion" (2010).
- E. Farhi and A. Guth, "An obstacle to creating a universe in the laboratory" (1987);
  Farhi, Guth and Guven, "Is it possible to create a universe in the laboratory by quantum
  tunneling?" (1990).
- L. Smolin, *The Life of the Cosmos* (1997), for cosmological natural selection.
- J. Johnson, "Origin of the elements in the solar system" (the nucleosynthesis periodic
  table), Ohio State University.
