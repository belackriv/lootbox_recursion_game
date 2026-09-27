# Design notes

This is a living document. Sections marked **(implemented)** match the code. Everything
else is design notes and proposals.

## Premise

Humanity has learned to harness a black hole as a **universe inside a universe**. The
pocket universe behind the event horizon has its own space, so it can be built in.

You are the **operator**, working from a facility outside the horizon. You can't go in, so
there is no player character and the camera is a free "god view". What you *can* do:

- **Inject** elementary matter (carbon, iron) through the horizon. The link has limited
  bandwidth, which is what the cast time and cooldown represent.
- **Compress** matter into loot boxes. The pocket universe decides what comes back when you
  open one, and sometimes that's another box.
- **Assemble and place** machines on the pocket universe's grid, starting with irradiation
  enclosures.

The progression arc is Factorio-shaped. Early on, you inject everything by hand. Later,
machines inside the pocket universe produce, transform and move matter for you, and your
direct powers matter less.

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

Nothing creates modifiers yet. Irradiation is meant to.

## The grid (implemented)

- Integer cells `(X, Y, Z)`, unbounded in every direction, one deployed entity per cell.
- You build on one **layer** (Z) at a time. The floor tiles, hover marker and placement
  preview show that layer; entities on every layer stay visible.
- There's no support or gravity rule yet: anything can be placed on any layer.

**Ideas, not implemented:**

- **Structural rules:** things above layer 0 need support, or the pocket universe has no
  gravity at all, which is a nice sci-fi excuse.
- **Logistics:** moving matter between cells takes time proportional to distance, so layout
  becomes the puzzle. Keep the enclosure next to the injection point and the power source
  next to the enclosure. Vertical stacking could be a way to keep distances short.
- **Multi-cell machines** (2x2x2 and so on), as the machines get more complex.

## Irradiation enclosures

There are two variants: a **Carbon Irradiation Enclosure** (graphite-lined; graphite is a
real neutron moderator) and an **Iron Irradiation Enclosure** (implemented as craftable and deployable items, with placeholder
costs).

A proposed mechanic, not implemented:

- An enclosure deployed in the grid gets a small inventory: a loot box slot plus a
  radiation source.
- Over time, exposure adds `FLRLootModifier`s to the box, with `Source` set to the
  radiation id.
- The enclosure material limits which radiation tiers or energies it can contain. Carbon
  (graphite) might suit neutron work and low-energy radiation (radio, microwaves, infrared). Iron could go
  higher, and gamma would need something heavier (lead? concrete?).
- Higher-energy radiation gives stronger but riskier modifiers. Gamma "mutation" could
  swap an entry for a different item. That would be a new modifier kind.

The code hooks for this already exist: `FLRSimulation::AddLootBoxModifier`,
`FLRLootModifier.Source`, `FLRRadiationDef` data, and the enclosure's persistent
`InstanceId`.

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
  or exotic tier.

Tier (the order you unlock it) and energy (how strong it is) are separate on purpose.
Microwaves come fourth but carry little energy.

## Open questions

- Should there be offline or idle progress? Real-time cooldowns were the Rails behaviour.
- Does each loot box type get its own table (the `carbon_loot_box` and `iron_loot_box`
  tables exist but are unused)? Maybe the enclosure material chooses the box type?
- What is the goal or ending? A tech ladder up the radiation tiers is one candidate.
- Does anything occupy grid cells from the start (resource nodes, "anomalies" in the pocket
  universe) so that position matters before logistics arrive?
- Where does injected matter appear: at a fixed "injection point" cell, or straight in your
  inventory, as now?
