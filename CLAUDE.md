# CLAUDE.md

This is **Quantum Recursion**, an Unreal Engine 5.8 C++ project (module `LootboxRecursion`, named after the original prototype). It's a port of the
Rails app `belackriv/lootbox_recursion`.

## Layout and architecture

- `Source/LootboxRecursion/Simulation/FLRSimulation` holds all the game rules as plain C++,
  with no UWorld or actors. Keep it engine-light so it stays testable.
- `Simulation/FLRHexGrid` is the grid's geometry: hexagonal cells `(Q, R, Layer)`. Go through
  it for neighbours, distance, radius and world positions rather than doing cell maths inline.
- `Data/LRGameData` covers the JSON definitions in `Content/Data/*.json` and their
  validation. `Tools/validate_data.py` mirrors `FLRGameData::Validate()`, so update both
  together.
- `Game/` is the engine glue:
  - `ULRGameSubsystem` owns the sim, ticks it, and saves and loads it.
  - The GameMode, PlayerController, CameraPawn, WorldGridActor and HUD live here too.
- `UI/` is the Slate HUD (`SLRGameHud`) with its style.
- `Cosmos/` is the backdrop. `FLRBlackHoleRenderer` (plain C++, tested) composites frames from
  `Content/Cosmos/BlackHole.lrbh`, which is baked by `Tools/cosmos/generate_black_hole.py`.
  The Python `composite()` mirrors the C++, so keep them in sync. `ALRCosmosActor` draws it
  with engine-only content.
- Players see "Quantum Cache" for the `loot_box` item. Keep ids and code names as `LootBox`,
  and keep player-facing strings generic ("cache").
- Includes are relative to the module root (`#include "Simulation/LRSimulation.h"`).
  `Build.cs` adds `ModuleDirectory` to the include paths.

## Conventions

- Follow the Epic C++ coding standard: tabs, `F`/`U`/`A`/`S` prefixes, `b` prefix on bools.
- Avoid variable shadowing. UE treats it as an error.
- New gameplay rules go in `FLRSimulation` with an automation test in
  `Tests/LRSimulationTests.cpp`. New content goes in JSON, not code.
- Binary assets are few and live in Git LFS (so cloud sessions only see pointer files):
  so far just `Content/Materials/M_GridBeam` (the grid beams; the code sets its `Color`
  parameter). Anything that needs a `.uasset` must be created in the editor by the user.
- Every visual goes through a material hook (`Rendering/LRMaterialHooks`, docs/MATERIALS.md):
  an optional asset path with an engine-only fallback. New visuals get a hook too, and the
  doc's table is kept up to date.

## Checks available without the engine

- `python3 Tools/validate_data.py` validates the data files.
- There's no Unreal toolchain in cloud sessions. The C++ can't be compiled there, so review
  engine API usage carefully and say so when a change is uncompiled.
