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
  link. Instructions go through the link at once, but the work takes time inside: an action
  with a cast time is a job in its cell (see *Actions take time in their cells*).
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
- **The host black hole** has a real mass (3,500 t at the start) and evaporates by Hawking
  radiation. You feed it from outside with the feed dial (see "Feeding the host"). Seeding a
  ripple costs 500 t and deepening one 70 t, and a perturbation is refused if it would take
  the last of it. At zero the pocket universe freezes: ripples, irradiators and the clock
  stop until you Ignite a new host. That's the soft fail; nothing is lost.
- **The plasma** is a glowing veil in the sky whose opacity comes from the epoch, fading over
  a few seconds between epochs. It clears at recombination.

## Feeding the host (implemented)

The host has a real mass, and feeding is a **dial** on the facility's mass injectors. It
replaced the Feed button and the host-as-a-percentage.
- **Code:** the rules are in `FLRSimulation` (`AdvanceHost`, `StepInjector`, `Ignite`).
- **Settings:** `universe.json`'s `host`.
- **UI:** the outside panel in `SLRGameHud`, with `SLRFeedDial` and `SLRChamberView`.
- **Not built yet:** chamber and injector upgrades in the tech tree, and showing the flow in
  the redone sky. The challenge is keeping three things in balance:

- Hawking evaporation, which takes mass away, fastest when the host is small.
- Feeding, which adds mass, but no faster than the injectors' rated limit (a multiple of the
  Eddington limit).
- The host's gravity well, which must stay inside the containment chamber. That's the real
  cap on its size.

**The arc.** Early on the host is small and hot, and ripples cost it mass, so you run the dial
aggressively, near the rated limit, to grow it. As it grows, evaporation slows and its gravity
well swells toward the chamber wall, so you (literally) dial it back.

### The facility (lore)

- **Why feeding is possible at all.** A black hole's glow pushes infalling gas away. That's
  the Eddington limit, and for a mini black hole it's crippling: its Hawking glow is about
  10¹² times its Eddington luminosity. The limit comes from radiation scattering off the
  electrons in ionized gas.
- **Neutronium injectors.** The facility injects cold, degenerate **neutronium**: neutral
  matter, so radiation has almost nothing to push on. Scattering off a neutron is weaker than
  off an electron by (electron mass / neutron mass)², which alone raises the limit about
  3×10⁶ times.
- **Graviton lens.** A (made-up) field focuses the stream through the glow onto a target
  100,000 times smaller than a proton. It gains the remaining factor of about 10⁶.
- **The rating.** Together the injectors are rated at **6×10¹² × the Eddington limit**, and
  the panel says so. That's the point beyond which even they can't push, and flow above it is
  blown back out as jets. Better injectors (tech tree) could raise the multiple.
- **Why the flow can't change instantly.** The graviton lens runs on superconducting magnets,
  and a magnet's current can't change instantly. Inductance limits how fast it can ramp, just
  as the LHC's magnets take minutes to ramp. So the flow follows the dial with inertia.
- **The stored charge.** The neutronium waiting to be injected is held in a magnetic trap:
  the mass "bucket". The panel calls it the **stored charge**, measured in tonnes.
- **Emergency shutdown.** The safeties do what particle accelerators do. A **beam dump**
  sends the whole stored charge into an absorber instantly, and the magnets **quench**
  (shed their stored energy). Before feeding can resume, the charge has to be rebuilt and the
  magnets ramped back up. That's the lockout, and the ramp afterwards is the inertia again.

### The dial

- **What it sets.** The dial sets a **target** injection rate, in mass per second. It's
  logarithmic, because useful rates span several orders of magnitude (tens of kg/s at a large
  host, tens of t/s at full throttle).
- **The rated limit mark.** The dial shows where the injectors' rated limit is (6×10¹² ×
  Eddington). The mark moves as the host grows and shrinks, because the limit is
  proportional to its mass. Anything injected above the mark is blown back out as jets and
  never reaches the host: wasted.
- **The break-even mark.** A second mark shows the evaporation rate, which is the flow that
  just holds the mass steady. Between the two marks the host grows. Below the break-even
  mark it shrinks.
- **Two needles.** The dial shows the setting and, as a second needle, the actual flow, which
  lags behind it (see "Injector inertia" below).

### Injector inertia

The dial moves instantly, but the lens magnets ramp, so the actual flow follows the setting
smoothly: it starts slowly (a parabolic start), then settles, without overshooting.

**No "time since the last dial change" is needed.** The flow needs one extra piece of state, its
current rate of change, and then it can be worked out from the dial setting, the current
flow and that rate of change alone. With only the setting and the current flow you'd get a
plain exponential approach, which jumps straight away rather than starting slowly. With
the rate of change as well, you get the smooth start.

The model is a critically damped spring. With `flow` and `change` (its rate of change) as
state, `target` the dial setting, and ω set from `injectorResponseSeconds`:

```
change' = ω² (target - flow) - 2ω change
flow'   = change
```

- **It steps exactly.** The closed form advances any time step exactly, so it doesn't depend
  on the frame rate. With e = flow - target at the start of a step of length t:
  `e(t) = (e + (change + ω e) t) e^(-ωt)` and
  `change(t) = (change - ω (change + ω e) t) e^(-ωt)`.
- **Its shape.** From rest, the flow moves by about ½ ω² e t² at first (parabolic), and it
  gets 90% of the way at about 3.9 / ω.
- **Clamping.** The flow is clamped at 0, and the rate of change is zeroed when the clamp
  bites.
- **One easy-to-tweak function.** It lives in `FLRSimulation` (e.g. `StepInjector(State,
  Target, DeltaSeconds, Params)`), so it's easy to swap the curve. Other options are a
  constant-acceleration ramp with a top speed, or doing the same in log space so every
  decade takes equally long.

### The safety cap: the gravity well

The host is held in a containment chamber. What limits its size is its **gravity well**:
the sphere around it inside which its pull is stronger than 1 g. That radius is
√(G·M / g), so it grows with the square root of the mass:

| Host mass | 1 g radius |
|---|---|
| 1,000 t | 2.6 mm |
| 2,000 t | 3.7 mm |
| 3,500 t | 4.9 mm |
| 14,700 t | 10 mm |

- **The chamber rating.** The first chamber's field emitters sit 1 cm from the host, so the
  safeties trip when the 1 g sphere reaches them, at 14,700 t. Each larger chamber is a
  tech-tree upgrade, and 10 times the radius allows 100 times the mass: 10 cm allows
  1.5 million t, and 1 m about 150 million t (a small mountain).
- **The trip.** Injection stops **instantly** (the beam dump), with no inertia, and the
  stored charge is lost.
- **Locked out.** Nothing can be fed until the charge has rebuilt (`chargeCapacity /
  rechargeRate`). After that, the flow ramps up again from zero, with inertia.
- **Why it's a skill.** Because of the inertia, turning the dial down near the cap takes
  effect late. At 20 t/s, 15 s of lag is about 300 t more mass, so you have to dial back
  before the cap, not at it.
- **Later game.** Evaporation is negligible for a big host. In the later game the gravity well
  is the constraint, and chamber upgrades are how the host (and the injectors' flow) keeps
  growing.

### Seeing the host

The outside panel shows the host itself, not only numbers:

- **A chamber cross-section, drawn to scale.** It shows the chamber wall (the trip line), and
  the 1 g sphere as a glowing bubble that swells and shrinks with the mass. At the start
  (3,500 t) it's about halfway to the wall.
- **The horizon,** as an inset with a scale bar: 5×10⁻²¹ m, far too small to draw otherwise.
- **The glow.** The host's temperature and Hawking output, shown as a colour or glow that's
  hottest and brightest when it's small and in danger (3.5×10¹⁶ K and 3×10¹⁹ W at the start).
- **Size is readable at a glance.** How close the bubble is to the wall shows how close the
  trip is, with no arithmetic.
- **A camera into the chamber** (built as a placeholder), labelled CAM 1. The outside panel
  takes 80% of the viewport's height (and at most 90% of its width); the dial and the
  chamber view keep their size, and the camera fills the space under them at 4:3 (at least
  320 × 240). On a viewport too small for that, the whole panel scales down.
  Until the 3D scene exists it shows a cheap painted feed (`SLRChamberCamera`, a choppy 12 fps,
  like an animated GIF): a slowly turning wireframe cage that reddens near the wall, the
  singularity glowing in its Hawking colour (sized by its 1 g sphere), injector beams
  streaming in (or violet and out while venting), scanlines, a blinking REC and a timestamp. For now it's black, and shows static while the instruments
  are down. Later it becomes an elaborate 3D scene of the chamber:
  - the containment field, flaring as the 1 g sphere nears the wall;
  - the injectors, firing the neutronium beam through the graviton lens;
  - the singularity with its glow, hotter and brighter as it shrinks;
  - venting: the injectors running in reverse, the horizon blazing with stimulated emission,
    and the radiation streaming back up the beamline to the beam dump;
  - a trip: the beam dump firing and the magnets quenching.

### The instruments go down (built)

Playtesting found that growing too big cost nothing: at the cap the host has three days of
unfed life, so a trip only paused the injectors. Now reaching the wall hurts.

- **The fiction.** The scientists observe and manipulate the pocket universe through
  instruments that run off the stored charge. When the host's gravity well reaches the
  containment barrier, the safeties trip, the magnets quench, and the charge is dumped. The
  instruments stop working correctly until the charge is full again.
- **The rule.** The instruments are down whenever the stored charge is rebuilding: after a
  trip (20 s at the start), and after Ignite. Meanwhile every inside action (Perturb, Build,
  Open, Dismantle) is refused. The universe itself keeps running: ripples gather and
  irradiators irradiate.
- **What you see.** Old-school TV static over the 3D view (and NO SIGNAL on the chamber camera),
  with a banner saying the instruments are down and how full the charge is. The status bar
  says INSTRUMENTS DOWN. The outside controls keep working, so you can dial back.
- **The static** is drawn by `SLRStaticNoise`: grey noise redrawn 30 times a second, each row
  a little brighter or darker, with a brighter band rolling down. It has a material hook,
  `M_Static` (docs/MATERIALS.md).

### Venting mass (built)

A way to shed mass on purpose, faster than evaporation. At the cap the host evaporates only
18 kg/s, so shedding even 300 t would take over 4 hours.

- **The fiction.** The injectors run in reverse. Driven backwards, the graviton lens pumps the
  horizon into **stimulated Hawking emission**: it radiates far more than it would on its own,
  and the lens focuses that radiation back up the beamline and off to the beam dump. In short,
  instead of forcing mass in, the injectors draw radiation out. All that radiation floods the
  chamber, so the instruments are down while venting, as after a trip.
- **The rule.**
  - VENT (on the outside panel, next to Ignite) needs a **full stored charge** to start. It
    doesn't use the charge up.
  - Starting it sets the dial to OFF and turns auto off. The dial then sets how hard the
    injectors pull, and they pull no harder than the rated limit (the lens pulls only as hard
    as it can push). At the cap, full reverse (20 t/s) sheds about 1,000 t a minute.
  - The flow keeps its inertia: it ramps through zero into reverse, and winds back down when
    venting stops.
  - **Only the dial works.** The presets and auto buttons under it are disabled.
  - The instruments are down while venting, and until the reversed flow has wound down.
  - **No interlock.** Nothing stops venting at the point of no return: vent too far and the
    host can't be saved (the log warns as it crosses). Like over-perturbing, it's the
    player's mistake to make. (An interlock at 1.25 × the point of no return was tried and
    dropped.)
  - STOP VENTING sets the dial to OFF, and the reversed flow winds down (about 15 s).
- **What you see.** The dial's screen turns violet and reads REVERSED. The zone up to LIMIT is
  violet (everything up to it is drawn out), HOLD is hidden, and the flow needle is violet.
  The readouts say "Venting X out", and the status bar says VENTING.
- **Why it's interesting.** It trades time without instruments for size, so it's a choice
  rather than a free undo. Later the radiation could feed something (a resource).

### Losing the host, and the kick-start

When the host evaporates completely (in a final flash as its last tonnes go), the pocket
universe freezes. Nothing is lost, as now.
- **Why the dial can't restart it.** The rated limit is proportional to mass, so an empty
  host can't be fed at all. And a tiny new host evaporates faster than anything can reach
  it.
- **Ignite.** The kick-start is a button that fires the whole stored charge at the
  singularity in one go. A charge that big collapses straight into a new horizon, with no
  Eddington limit. This is hand-feeding coal to restart a dead power grid (as in
  Satisfactory).
- **Size the charge above the tipping point.** Then a freshly ignited host survives, as long
  as you open the dial right away.

### The point of no return

Below the tipping point (about 1,000 t), evaporation outruns even the rated limit, so no dial
setting can save the host. It's shown, so the player can see it coming:
- The chamber view draws the point of no return as a faint red ring (the 1 g radius it would
  have). A bubble inside it is doomed.
- The outside readouts give it in tonnes.
- The status bar says so when the host is below it.
- The log warns when the host crosses it.

**You can shoot yourself in the foot.** Nothing stops seeding from spending the host below
the line: over-perturbing early is a mistake the player is allowed to make, and learns from.
The way back is to let the host go and Ignite a new one. (A playtest tried an "emergency
charge", firing the stored charge into a living host, plus a guard on seeding. Both were
dropped: the charge didn't fit the fiction, and the guard took the lesson away.)

### The dial's buttons

Under the dial:
- **Off | Hold | Limit | Max** set it once. Hold is a little over the HOLD mark (the
  evaporation rate), Limit is the LIMIT mark (the rated limit), and Max opens the injectors
  all the way.
- **Auto Hold | Auto Limit** keep it on that mark as the host's mass changes. Auto Hold keeps
  the host's mass steady (it loses a little while the injectors ramp up), and Auto Limit grows
  it as fast as it can. Both stop at the injectors' maximum. Touching the dial or a preset
  turns auto off, and so does clicking the lit toggle.
- **Auto Limit doesn't stop at the wall.** It follows the limit right up to the safety cap and
  trips it. Stopping short would be an upgrade (a tech-tree governor), if we want one.

### Ripples cost host mass

Seeding a new ripple costs a fixed mass (`seedCost`, 500 t), and
deepening one costs a smaller mass.
- **Early,** that's what makes feeding urgent.
- **Near the cap,** seeding becomes a useful way to spend mass instead of tripping the
  safeties.

### Starting numbers (real physics where it's playable)

**Hawking evaporation (real).** Lifetime is t = 8.41×10⁻¹⁷ s × (M/kg)³, and mass is lost at
3.96×10¹⁵ / M² kg/s. A host that lasts an hour unfed weighs about 3,500 t. For comparison:

| Host mass | Lifetime unfed | Evaporation | Temperature | Horizon radius | 1 g radius |
|---|---|---|---|---|---|
| 1,000 t | 84 s | 4.0 t/s | 1.2×10¹⁷ K | 1.5×10⁻²¹ m | 2.6 mm |
| 2,000 t | 11 min | 1.0 t/s | 6×10¹⁶ K | 3×10⁻²¹ m | 3.7 mm |
| **3,500 t** (start) | **1 hour** | 0.32 t/s | 3.5×10¹⁶ K | 5×10⁻²¹ m | 4.9 mm |
| **14,700 t** (first chamber) | 74 hours | 18 kg/s | 8×10¹⁵ K | 2.2×10⁻²⁰ m | 10 mm |

**The Eddington limit (real, then rated).** The real limit is 6.3 W per kg of black hole.
Accreting at 10% efficiency, that allows 7×10⁻¹⁶ of the host's mass per second: an
e-folding time (the Salpeter time) of 45 million years. For the 3,500 t host that's 2.5×10⁻⁹
kg/s, while it evaporates 324 kg/s.
- **The injectors' rating** (6×10¹² ×, see "The facility") keeps the physics' shape, a
  limit proportional to mass.
- **In game units** that's `eddingtonRate` = 0.004 per second (an e-folding of 250 s).
  It was 0.002 (3×10¹²) at first, which made growing a small host a slog, so it was doubled.

| Setting | Value | Why |
|---|---|---|
| Starting host mass | 3,500 t | 1 hour unfed |
| Evaporation | 3.96×10¹⁵ / M² kg/s | real |
| Rated limit (`eddingtonRate`) | 0.004 /s × M (6×10¹² × Eddington) | 14 t/s at the start; above 5,000 t the injectors' 20 t/s maximum is the real cap |
| Tipping point | about 1,000 t | where the rated limit just equals evaporation: below it, nothing can save the host |
| Safety cap | the 1 g sphere reaches the chamber wall | first chamber 1 cm: 14,700 t (74 hours unfed) |
| Injector maximum | 20 t/s | the top of the dial, until injector upgrades |
| `injectorResponseSeconds` (to 90%) | 15 s | ω ≈ 0.26 /s |
| Stored charge (Ignite) | 2,000 t | above the tipping point |
| Recharge rate | 100 t/s | 20 s lockout after a trip, or to recharge after Ignite (it was 200 s, which felt too long) |
| Seed a ripple | 500 t | three seeds take a fresh host from 3,500 t to 2,000 t, 11 minutes from death |
| Deepen a ripple | 70 t | |

**How the opening plays out:**
1. Seed three ripples, leaving 2,000 t, with evaporation at 1 t/s and the rated limit at
   8 t/s. The 1 g bubble shrinks to 3.7 mm.
2. Open the dial toward the mark. At the limit, and then at the injectors' 20 t/s maximum,
   2,000 t grows to 3,000 t in about 2 minutes and to the 14,700 t cap in about 12, with the
   bubble swelling toward the wall.
3. Start dialing back around 14,000 t to avoid tripping, and settle a little above the
   break-even mark (18 kg/s at the cap).

### Two views: inside and outside

The game has two UIs, one for each side of the horizon.

- **Inside** (the current HUD) is the view into the pocket universe: the grid, the build and
  action panels, the log, and the universe's matter.
- **Outside** is the facility's control panel for the host. It holds only the injector
  controls:
  - the dial, with the Eddington and break-even marks and the setting and flow needles;
  - the stored charge (the mass "bucket"): how full it is, and any lockout;
  - VENT and Ignite;
  - the host's mass and net rate next to the dial.

**How it looks.** The outside is the inverse of the inside: a light grey console with dark
text, with its instruments (the dial and the chamber side by side, the same size, and the
chamber camera under both) set in dark screens.

**How you open it.** The outside panel drops down from the top of the screen when you press a
button on the top status bar, or a hotkey (Tab, say; any free key will do). The same button or
key raises it again.
- The inside view stays live behind it, and the simulation keeps running while it's open.
- Keep it compact, so you can watch a ripple while nudging the dial.

**The status bar is the link between the two.** It always shows the host's mass (in tonnes),
its net rate and, while it shrinks, how long it has left at that rate, whichever view is up.
The controls are behind its (?) button. Its outside button lights up or pulses when the outside
needs attention:
- the host is below break-even (shrinking);
- the safeties have tripped, or the host is closing on the cap;
- the container is recharging;
- the host is gone and Ignite is ready.

The `host` requirement check compares tonnes. The redone sky can show the flow, e.g. the disk
brightening as more is injected.

### How the universe comes to feed itself

| Satisfactory | Here | When |
|---|---|---|
| Hand-feeding coal to restart | **Ignite**, then the dial | whole game, mostly early |
| Coal on a conveyor | **Horizon Siphon** (working name; or Throat Pump) | mid game |
| Late-game free power | **Dark energy** | late game, with expansion |

**Horizon Siphon.** A structure built in a cell from recombination on. It pipes matter from
the cells within its reach through the "throat" into the host. In lore, that's the
Einstein–Rosen bridge between the pocket universe and the host (Popławski).
- **It adds to the injectors' flow.** Together they share the Eddington limit, and the
  safeties trip it too.
- **It's Perturb in reverse, and a trade-off.** Matter siphoned is matter not built with.
- **Mass value.** Materials count by their balance value (hydrogen 1, helium 2, carbon 4,
  iron 8, as in `Tools/balance.py`). Some constant turns value into tonnes.
- **Settings.** A throughput, and a material filter (hydrogen only by default).

**Dark energy.** Space expanding creates vacuum energy for free. Its density stays constant
while the volume grows, and in general relativity energy isn't conserved globally in an
expanding universe. Alan Guth called inflation "the ultimate free lunch" for this reason.
- **How it works.** With expansion (see "Expansion" below), the growth of the universe adds
  mass to the host directly.
- **When it matters.** It's negligible before the dark-energy era (it took over at about 9.8
  billion years) and dominant after.
- **It bypasses the injectors,** so neither the Eddington limit nor the safeties apply. Late
  on, the host outgrows the facility's cap and the universe sustains itself.
- **Before that,** the vacuum energy of inflation is spent making the matter (reheating), not
  the host.

**Later ideas.**
- Black holes that form inside the pocket universe (from dead stars) could anchor the host.
- **Hawking power.** The host's glow (3×10¹⁹ W at the start) could be the facility's power
  supply. A small, hot host gives plenty of power but dies fast, and a big, cool one is safe
  but gives little. Power could then limit the injectors or the tech.
- A buffer in the stored charge, so the injectors can briefly run faster than it rebuilds.

## Time controls (planned)

The log-scale cosmic clock stays as it is. On top of it the player gets a speed control: a
**speed level** from -1 to 3, which buttons (and keys) step up or down one at a time
(These are placeholder for real graphics / icons)

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

## The command card and the menu (implemented)

**The command card.** The Actions panel (top left) works like StarCraft's command card: a
vertical list of ten **slots**, each with a fixed hotkey (1-9, 0 by default, rebindable). The
key belongs to the slot, not to an action: what a slot does depends on what's selected.
- **The main page** lists what the selected cell allows, from the top, always in the same
  order: Perturb (an empty cell or a ripple), Build... (an empty cell or an irradiator),
  Open (a cache, or an irradiator), Dismantle (anything but a structure). A slot that can't be
  done right now is disabled and says why on its right, in red, in a word or two ("busy",
  "host too small", "occupied"; a recipe shows its cost, red when there isn't enough matter
  in reach). Hovering it gives the full reason in Info. The reasons are the simulation's own
  (`FLRSimulation::CheckRequest`), so the card never disagrees with what would happen.
- **The Build page.** Build... turns the card into a page of the unlocked recipes, on the
  same keys, with Back in the last slot (Esc also backs out). Building something, or
  selecting another cell, goes back to the main page. (More than nine recipes will need
  paging.) If no unlocked recipe could go in the cell (a full irradiator, say), Build... is
  disabled with "no builds"; a recipe that's only short of matter still counts as an option.
- **Around it.** Under the card, a strip for the build layer (- Layer, Z, Layer +, Home).
  The Grid panel's list of everything placed is gone; the selected cell, the cursor and an
  irradiator's progress are in Info, which sits at the bottom right above the log. The
  Universe panel is top right (Info changes size, so the Universe panel stays put above it).
- **Next.** The same idea for the rest of the UI: hotkeys bound to slots in each panel, so
  the key stays put while what it does follows the context.

**Actions take time in their cells.** There are no per-action cooldowns. After any action
starts, a short global cooldown (`globalCooldown` in `actions.json`, 0.25 s) stops double
presses; that's all that holds the player back.
- **Jobs.** An action with a `castTime` (Perturb 3 s, Build 5 s, Open 5 s) is a **job in its
  cell**: it's carried out when its time is up, and that cell is busy until then (anything
  else there is refused as "busy"), but every other cell is free, so you can start jobs in
  several cells at once. Dismantle is instant.
- **Seen on the cell.** A job traces its cell's hex outline in amber, from one corner round
  to the same corner as it runs (drawn with the grid beam hook). Amber is kept for jobs: the
  selected cell is outlined in pale blue. The card shows it too: the
  selected cell's job fills its slot's bar, and the card's heading says what's under way.
- **Paid up front, refunded exactly.** A job pays when it starts: a recipe's matter comes out
  of the cells in reach (nearest first) and a seed's mass out of the host, so two jobs can't
  spend the same matter. What it took, and from which cell, is kept with the job as a **cost
  transaction** (`FLRCostTransaction`, saved with it). If the job fails when it ends (it's
  checked again then), the transaction is refunded **in full**: each cell gets back exactly
  its own share, and the host its mass if it's still there. If the player **cancels** it
  (`CancelJob`; no button for it yet), they get **75%** back (`cancelRefund` in
  `actions.json`): each material's refund is rounded down to whole units and shared between
  the cells in proportion to what each gave, so the shares add up exactly. The transaction
  is discarded when the job ends; later these could go to a ledger.
- **Kept in saves**, and they carry on while the instruments are down: the universe keeps
  running.
- An action can still have its own `cooldown` on top (optional, none do now).

**Room in the cells.** Entities are drawn smaller (`EntityScale` 0.225 of a cell, was 0.7, then 0.45): a
cell holds a whole nebula, so its contents shouldn't crowd it.

**The outside panel's keys.** While the outside panel is down, the slot keys (1-8) press its
buttons instead, in order: Off, Hold, Limit, Max, Auto Hold, Auto Limit, VENT, IGNITE (each
button shows its key). The camera's pan keys turn the dial: left/right (A/D) in fine steps,
up/down (W/S) in coarse ones, along the dial's arc like dragging it. That's manual tuning, so
it also works while venting and turns auto off.

**The menu.** Esc (F10 in the editor, where Esc stops Play; or MENU on the status bar) opens
it and pauses the game. Esc first backs out of whatever else is open: the Build page, help,
the outside panel.
- **Resume, New Game** (confirmed), **Save** (a new named save, or over an existing one),
  **Load** (the autosave first, then the newest), **Settings**, **Quit** (it autosaves on the
  way out).
- **Settings.** Every keyboard command, two keys each: click one, press the new key (Esc
  cancels, Backspace clears). A key does one thing, so binding it takes it off anything
  else. Mouse controls are fixed. Also free-look speed, invert look, pan speed, UI scale and
  graphics quality. Changes apply at once and are saved.
- **Where it's kept.** Keys use the engine's rebinding: Enhanced Input's user settings
  (enabled in `DefaultInput.ini`) store each player's keys in their own save and apply them.
  The commands and their defaults are in `Game/LRInputCommands.h`; the player controller
  makes each command's action player mappable under the command's name and registers the
  mapping context, and `LRKeyBindings` wraps the engine calls (plus our one-key-one-command
  rule). Graphics quality is the engine's `UGameUserSettings`; the camera and UI settings
  are `ULRUserSettings` (a `USaveGame`, `Settings.sav`). This also leaves the door open to
  gamepad keys and key profiles later.

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
  reach of the selected cell. The Info panel lists the matter in the hovered cell (or the
  selected one).

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
| The feed dial and Ignite (outside panel, F) | from the start |
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
