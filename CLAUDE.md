# CLAUDE.md

This is an Unreal Engine 5.7 C++ project (module `LootboxRecursion`). It's a port of the
Rails app `belackriv/lootbox_recursion`.

## Layout and architecture

- `Source/LootboxRecursion/Simulation/FLRSimulation` holds all the game rules as plain C++,
  with no UWorld or actors. Keep it engine-light so it stays testable.
- `Data/LRGameData` covers the JSON definitions in `Content/Data/*.json` and their
  validation. `Tools/validate_data.py` mirrors `FLRGameData::Validate()`, so update both
  together.
- `Game/` is the engine glue:
  - `ULRGameSubsystem` owns the sim, ticks it, and saves and loads it.
  - The GameMode, PlayerController, CameraPawn, WorldLineActor and HUD live here too.
- `UI/` is the Slate HUD (`SLRGameHud`) with its style.
- Includes are relative to the module root (`#include "Simulation/LRSimulation.h"`).
  `Build.cs` adds `ModuleDirectory` to the include paths.

## Conventions

- Follow the Epic C++ coding standard: tabs, `F`/`U`/`A`/`S` prefixes, `b` prefix on bools.
- Avoid variable shadowing. UE treats it as an error.
- New gameplay rules go in `FLRSimulation` with an automation test in
  `Tests/LRSimulationTests.cpp`. New content goes in JSON, not code.
- The repo has no binary assets yet. Anything that needs a `.uasset` must be created in the
  editor by the user.

## Checks available without the engine

- `python3 Tools/validate_data.py` validates the data files.
- There's no Unreal toolchain in cloud sessions. The C++ can't be compiled there, so review
  engine API usage carefully and say so when a change is uncompiled.
