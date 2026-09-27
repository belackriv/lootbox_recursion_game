# Unreal primer (for a full-stack dev who last shipped mods in the Quake 3 / UT99 era)

This guide covers what you need to work on *this* project and what has changed since the
UnrealScript days. It isn't a general tutorial. The links at the end go deeper.

## 1. The shape of a project

```
LootboxRecursion.uproject   ← the "package.json": engine version, modules, plugins
Config/*.ini                ← project settings (the editor writes these too)
Source/                     ← C++ modules (the equivalent of your app/ code)
Content/                    ← assets: maps, meshes, materials, Blueprints (.uasset/.umap, binary)
Binaries/ Intermediate/ Saved/ DerivedDataCache/   ← build output and caches (gitignored)
```

- **Engine vs editor.** The editor is the engine plus tooling. "Play in Editor" (PIE, Alt+P)
  runs the game in the editor process, which is how you'll iterate 95% of the time.
- **Modules.** C++ lives in modules. Each has a `*.Build.cs` (C#) listing its dependencies,
  like a Gemfile for one engine or library. `*.Target.cs` describes the executables
  (the game, and the editor with your code loaded).
- **UBT and UHT.** UnrealBuildTool is the build system. UnrealHeaderTool scans headers for
  the reflection macros and generates code (`Foo.generated.h`). That's why every reflected
  header ends its include list with `#include "Foo.generated.h"`.

## 2. What happened to UnrealScript?

It's gone (removed in UE4). Game code is now written two ways:

| | C++ | Blueprints |
|---|---|---|
| What | Real C++ with engine macros | Visual scripting graphs, stored as assets |
| Good for | Core rules, systems, anything performance- or logic-heavy, anything you want to diff and test | Wiring content: tweaking values, level scripting, UI animation, fast experiments |
| In this repo | The simulation, subsystem, actors and HUD | None yet, but everything useful is exposed to them |

Some UnrealScript concepts map directly:

| UT99 / UnrealScript | UE5 |
|---|---|
| `class Foo extends Actor;` | `UCLASS() class AFoo : public AActor` |
| `var() int Health;` (editable) | `UPROPERTY(EditAnywhere) int32 Health;` |
| `defaultproperties { }` | The C++ constructor (it sets the Class Default Object) |
| `exec function` | `UFUNCTION(Exec)` (see `ALRPlayerController::LRGive`) |
| `GameInfo` | `AGameModeBase` (server rules) + `AGameStateBase` (replicated state) |
| `PlayerPawn` | `APlayerController` (the player's will) + `APawn` (the body it possesses) |
| `HUD` / canvas drawing | `AHUD`, plus a real UI framework (UMG or Slate) |
| `.u` packages | `.uasset` files, one per asset |
| Console (`~`) | Still `~`. `stat fps`, `stat unit`, and `showdebug` are still there. |

## 3. UObject basics (the things that bite web devs)

- **The prefixes are enforced:** `U` = UObject, `A` = Actor, `F` = plain struct or class,
  `S` = Slate widget, `E` = enum, `I` = interface, `T` = template.
- **Garbage collection.** UObjects are garbage collected, and only `UPROPERTY()` pointers
  keep them alive. A raw `UFoo*` member without `UPROPERTY()` can dangle after the next GC.
  Use `TObjectPtr<UFoo>` in `UPROPERTY` members, and `TWeakObjectPtr<UFoo>` when you
  shouldn't keep the object alive (see `SLRGameHud::Subsystem`).
- **Never `new` or `delete` a UObject.** Use `NewObject<T>()`, `CreateDefaultSubobject<T>()`
  (constructors only), or `GetWorld()->SpawnActor<T>()`.
- **Plain C++ is allowed.** `FLRSimulation` is an ordinary class owned by `TUniquePtr`, so
  the game rules have no engine dependencies and are easy to test.
- **Reflection macros:**
  - `USTRUCT()` / `UCLASS()` / `UENUM()` make a type visible to the engine: serialization,
    Blueprints, the details panel, JSON conversion.
  - `UPROPERTY()` does the same for a field.
  - `UFUNCTION()` does the same for a method. It's required for Blueprint calls, `Exec`, RPCs
    and dynamic delegates.
- **Containers and strings:** `TArray` ≈ `Array`, `TMap` ≈ `Hash`, `TSet` ≈ `Set`.
  - `FString` is a mutable string.
  - `FName` is an interned, case-insensitive identifier, good for ids like `"carbon"`.
  - `FText` is user-facing, localizable text.

## 4. The gameplay framework in this project

```
UGameInstance ─┬─ ULRGameSubsystem      lives for the whole run; owns FLRSimulation, saves and loads
               │
UWorld (level) ├─ ALRGameMode           picks the classes below; spawns lights and the world view
               ├─ ALRPlayerController   input, cursor, console commands
               │    └─ ALRCameraPawn    camera on a spring arm, follows the focus coordinate
               ├─ ALRHud                adds the Slate HUD (SLRGameHud) to the viewport
               └─ ALRWorldGridActor     draws the 3D build grid with engine basic shapes
```

- **Subsystems** are engine-managed singletons scoped to a lifetime (engine, game
  instance, world, local player). You can reach them from anywhere with
  `GetGameInstance()->GetSubsystem<ULRGameSubsystem>()`. They're the closest thing to a
  Rails service object or a Pinia store.
- **Actors vs components.** An actor is a thing in the level. Components give it
  behaviour: mesh, camera, text. Composition over inheritance, as in React.
- **Ticking.** Actors can `Tick(DeltaSeconds)` every frame. The subsystem uses
  `FTSTicker` because it isn't an actor.

## 5. Delegates (events)

- **Native multicast delegates** (`DECLARE_MULTICAST_DELEGATE`) are fast and C++-only, and
  can bind lambdas. `FLRSimulation` uses them.
- **Dynamic multicast delegates** (`DECLARE_DYNAMIC_MULTICAST_DELEGATE`) are slower, can be
  used from Blueprints, and bind only to `UFUNCTION`s with `AddDynamic`. `ULRGameSubsystem`
  re-broadcasts the sim's events this way, so Blueprints can use
  `Bind Event to OnInventoryChanged`.

## 6. UI: Slate vs UMG

- **Slate** is the C++ UI framework the editor itself is built with. Layout is declarative
  (`SNew(SButton)[ ... ]`), and `Text_Lambda` / `IsEnabled_Lambda` re-evaluate every frame,
  like computed properties. The HUD is Slate because it's code-only, diffable, and needs no
  binary assets.
- **UMG (Widget Blueprints)** is the designer-friendly layer on top of Slate: drag-and-drop
  layout, animations, styling in the editor. Most shipped games use it. A natural next step
  is to rebuild panels as Widget Blueprints bound to `ULRGameSubsystem`, since all the data
  is already exposed with `BlueprintPure` and `BlueprintCallable`.

## 7. Input (Enhanced Input)

The old `BindAxis` / `BindAction` system is deprecated. Enhanced Input uses two kinds of
asset:

- **Input Actions** (`IA_*`) say *what* happened.
- **Input Mapping Contexts** (`IMC_*`) map keys to actions.

This project builds both in code (`ALRPlayerController::SetupInputComponent`) so it runs with
zero assets. To move them to assets:

1. In the Content Browser, go to Add > Input > Input Action and Input Mapping Context.
2. Add `UPROPERTY(EditDefaultsOnly)` fields for them on the controller.
3. Make a Blueprint subclass of `ALRPlayerController` and assign the assets.

## 8. The iteration loop

1. **Build from the IDE.** Pick the `Development Editor` configuration, then press F5 to
   launch the editor with the debugger attached. Breakpoints work in the C++.
2. **Live Coding** (Ctrl+Alt+F11 in the editor) recompiles `.cpp` changes while the editor
   runs. If you change a **header** (new `UPROPERTY`, new function, class layout), close the
   editor and rebuild.
3. **Logs.** Use `UE_LOG(LogLootbox, Log, TEXT("..."))` and read them in
   Window > Output Log. Filter by `LogLootbox`.
4. **Console** (`~`): `LRGive carbon 500`, `LRTimeScale 10`, `LRReset`, `LRItems`, `stat fps`.
5. **Tests.** Open Tools > Session Frontend > Automation, filter `LootboxRecursion`, and
   run them.

## 9. Assets, maps and Git

- `.uasset` and `.umap` files are binary and can't be merged. `.gitattributes` routes them
  through **Git LFS**. Install it once with `git lfs install`.
- The repo ships **no maps**. The game runs in the engine's empty `/Engine/Maps/Entry` level
  because the GameMode spawns everything. Your first editor task is to make a real level (see
  README, "First steps in the editor").
- If two people edit the same map, one of them loses. That's why larger teams use
  "One File Per Actor" and file locking.

## 10. Further reading

- Programming guides: https://dev.epicgames.com/documentation/en-us/unreal-engine/programming-with-cplusplus-in-unreal-engine
- Gameplay framework: https://dev.epicgames.com/documentation/en-us/unreal-engine/gameplay-framework-in-unreal-engine
- Enhanced Input: https://dev.epicgames.com/documentation/en-us/unreal-engine/enhanced-input-in-unreal-engine
- UMG: https://dev.epicgames.com/documentation/en-us/unreal-engine/umg-ui-designer-for-unreal-engine
- Slate: https://dev.epicgames.com/documentation/en-us/unreal-engine/slate-user-interface-programming-framework-for-unreal-engine
- Coding standard: https://dev.epicgames.com/documentation/en-us/unreal-engine/epic-cplusplus-coding-standard-for-unreal-engine
- Automation tests: https://dev.epicgames.com/documentation/en-us/unreal-engine/automation-test-framework-in-unreal-engine
