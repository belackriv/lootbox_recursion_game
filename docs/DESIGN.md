# Design notes

This is a living document. Sections marked **(implemented)** match the code. Everything
else is design notes and proposals.

## Core loop (implemented)

1. **Scavenge** for raw materials (wood, iron). It takes a cast time and has a cooldown.
2. **Craft** materials into loot boxes and irradiation enclosures.
3. **Open** loot boxes (the Use action) for randomized loot from weighted loot tables.
   A loot box can contain other loot boxes: that's the recursion.
4. **Deploy** placeable items into a one-dimensional world, and **Recall** them back.
5. **Sort** to compress and order the 50-slot inventory.

Loot boxes carry **modifiers**, which rewrite their loot table before it's rolled:

- extra rolls
- a multiplied weight for one item
- multiplied counts for one item

Nothing creates modifiers yet. Irradiation is meant to.

## Irradiation enclosures

There are two variants from the notes: **Wood Irradiation Enclosure** and **Iron
Irradiation Enclosure** (implemented as craftable and deployable items, with placeholder
costs).

A proposed mechanic, not implemented:

- An enclosure deployed in the world gets a small inventory: a loot box slot plus a
  radiation source.
- Over time, exposure adds `FLRLootModifier`s to the box, with `Source` set to the
  radiation id.
- The enclosure material limits which radiation tiers or energies it can contain. Wood
  might only handle low-energy radiation (radio, microwaves, infrared). Iron could go
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
  transmission medium, which could matter in the 1D world, where neighbouring cells are
  the medium.
- **Gravitational radiation:** gravitational waves, ripples in spacetime. It's a late-game
  or exotic tier.

Tier (the order you unlock it) and energy (how strong it is) are separate on purpose.
Microwaves come fourth but carry little energy.

## The world (partly implemented)

- **One-dimensional:** signed integer coordinates, unbounded both ways, one placed entity
  per cell (implemented).
- The camera and the "Deployed" list show a window of cells around a focus coordinate
  (implemented).
- **Logistics idea:** eventually travel distance matters. Moving items between cells could
  take time proportional to distance, so layout becomes the puzzle: keep the enclosure next
  to the scavenging site, the power source next to the enclosure, and so on. A 1D world keeps
  the logistics readable while still forcing trade-offs.

## Open questions

- Should there be offline or idle progress? Real-time cooldowns were the Rails behaviour.
- Does each loot box type get its own table (the `wood_loot_box` and `iron_loot_box`
  tables exist but are unused)? Maybe the enclosure material chooses the box type?
- What is the goal or ending? A tech ladder up the radiation tiers is one candidate.
- Does anything consume world cells (terrain, resource nodes at coordinates) so that
  position matters before logistics arrive?
