# awsim — Architecture

> A management/simulation game built in Unreal Engine 5.8, inspired by
> SimCity (2013), SimCity 4, Planet Coaster 2, and RollerCoaster Tycoon.
>
> Status: **living document** — expect this to change as the project evolves.

---

## 1. Vision & genre

A god-view simulation game. The player is not a character — they are a camera
over a world, placing and managing many entities (buildings, citizens, roads,
attractions). The defining technical challenge is **simulating and rendering
thousands of entities at once** while keeping the game readable, debuggable, and
moddable.

## 2. Guiding principles

1. **Logic in C++, art in the editor.** All game logic, infrastructure, and
   simulation live in C++ (`Source/awsim/`). All art and per-asset tuning live
   in the editor as `Content/` assets and Blueprint subclasses. C++ classes are
   abstract and never hardcode asset paths; Blueprint children bind the art.

2. **Separate data from visuals.** Entity *state* is plain data (structs in
   arrays), simulated independently of how — or whether — it is drawn. Visuals
   are a disposable *projection* of that data, not the source of truth. See §5.

3. **Data-driven content.** Entity *types* (building costs, footprints, stats,
   meshes) are defined in Data Assets / Data Tables edited in the editor, not
   hardcoded. C++ defines the schema and behavior; designers fill in numbers
   and art without recompiling.

4. **Simulation decoupled from rendering frame rate.** The sim advances on a
   fixed timestep with a game-speed multiplier (pause / 1x / 2x / 3x),
   independent of how fast frames render.

## 3. Unreal concepts primer (orientation)

- **Reflection macros** (`UCLASS` / `UPROPERTY` / `UFUNCTION`) power garbage
  collection, editor exposure, and serialization. Every gameplay class is a
  `UCLASS`; pointers that must survive GC use `TObjectPtr` in a `UPROPERTY`.
- **Actor** (`AActor`) = anything placeable in a level. **Component** = composable
  behavior/visual attached to an Actor. Favor composition.
- **Pawn / PlayerController** = the player. In this game the Pawn is a *camera
  rig*; the PlayerController does *mouse-to-world picking* and placement.
- **GameMode / GameState** = session rules and shared state.
- **Subsystems** = engine-managed singletons. `UWorldSubsystem` (per loaded
  world) hosts simulation systems; `UGameInstanceSubsystem` (whole session)
  hosts save/load and settings.

## 4. Layered architecture

```
Input / Camera layer     Camera Pawn + PlayerController (picking, placement)
        |
Simulation layer         WorldSubsystems on a fixed tick:
        |                  - TimeSubsystem (calendar, speed)
        |                  - EconomySubsystem (money, power, water...)
        |                  - PopulationSubsystem (citizens)
        |                  - GridSubsystem (tiles, placement validity)
        |
Data layer               TArray<FCitizen>, TArray<FBuilding>, ...
        |                  + Data Assets / Data Tables for entity *types*
        |
Representation layer      Instanced Static Mesh components project data -> screen
        |                  (promote to full Actors only when needed)
        |
Persistence              GameInstanceSubsystem-backed save/load
```

The seam between the **data layer** and the **representation layer** is the most
important boundary in the project (§5). Keep it clean and the rendering strategy
can be swapped/upgraded without touching simulation.

## 5. Entity strategy: data vs. visuals

### The core idea

A rendered entity is **not** one `UObject`. Per-entity *state* is a plain struct
in a contiguous array; per-entity *visual* is just an `FTransform` inside a
shared instanced-mesh component. Thousands of entities require only a handful of
UObjects (the shared mesh asset + one instancing component per type).

```cpp
USTRUCT()
struct FCitizen            // plain data, NOT a UObject
{
    GENERATED_BODY()
    FVector Position;
    int32   State;         // AtHome / Commuting / Working / ...
    int32   HomeBuilding;
    int32   WorkBuilding;
};
```

```
TArray<FCitizen>  Citizens;     // source of truth (simulation owns this)
HISM Component    CitizenISM;   // 1 component, N instances (the projection)
```

### The loop

1. **Simulate** — a WorldSubsystem updates `Citizens[i]` on the fixed tick.
2. **Project** — push each citizen's transform into the ISM
   (`UpdateInstanceTransform`), maintaining an `entity index <-> instance index`
   map for add/remove.
3. **Render** — the engine draws all instances in ~one draw call.

### Selection without per-entity Actors

ISM per-instance line traces return the **instance index** (`FHitResult::Item`),
which maps back to the data struct. Clicking works without Actors.

### Per entity type

| Entity   | Count      | Strategy                                                  |
|----------|------------|----------------------------------------------------------|
| Citizen  | thousands  | Data struct + ISM always. Never individual Actors.       |
| Building | hundreds   | Data struct + ISM for rendering; **promote to a full Actor only while selected/interacted with.** |

### Known follow-ups (deferred)

- **Animated crowds:** citizens need walk cycles. Plan to use **Vertex Animation
  Textures (VATs)** — baked animation played on static meshes via material — so
  crowds render through ISM at near-zero per-entity cost.
- **Scaling to Mass Entity:** the hand-rolled `TArray + ISM` approach is the
  starting point. Unreal's **Mass Entity** (built-in ECS) is the same idea
  industrialized — fragments (data), processors (systems), and a representation
  system that auto-switches each entity between Actor / ISM / culled by LOD.
  Adopt it later *if* scale demands; the clean data/representation seam (§4)
  makes this an upgrade, not a rewrite.

## 6. Simulation scheduling

How the simulation actually runs over time. Two rules, each with a "why".

### Rule 1 — One clock, ordered phases

A **single orchestrator** advances the sim and steps each subsystem in a **fixed
order** every tick. Subsystems do **not** tick themselves.

*Why:* phases depend on each other (Economy needs the latest population; Traffic
needs where everyone decided to go), and the game must be **deterministic** for
save/load. A fixed order guarantees each phase sees the *finished* output of the
one before it. (Unreal does not guarantee tick order between subsystems, so we
never let them self-tick.)

```
   one fixed step (repeats while the speed-scaled clock has time owed):

   Time  ->  Grid  ->  Economy  ->  Population  ->  Traffic
    \________________________________________________/
        each phase sees the completed result of the previous one

   speed multiplier (pause / 1x / 2x / 3x) only changes how many
   fixed steps run per frame — never the order or the step size.
```

### Rule 2 — Parallel *inside* a phase

A single phase may update thousands of independent entities. That inner loop
fans out across CPU cores.

*Why:* per-entity work is independent and pure data — no UObject or engine calls
(those aren't thread-safe). We **double-buffer**: read the current state, write
the next, then swap — so no two threads touch the same slot. No locks, no races,
and the result is identical no matter how threads are scheduled.

```
   Population phase:

   Citizens[] ──┬─► core 0 ─┐
                ├─► core 1 ─┤
                ├─► core 2 ─┼─► NextCitizens[] ──► swap buffers
                └─► core 3 ─┘
```

After **all** phases finish, visuals are synced to the ISMs (§5) on the game
thread. Simulation is parallel; touching the engine stays single-threaded.

> This is the same shape as Mass Entity (ordered processors + parallel chunks),
> which is why moving to Mass later (§5) is an upgrade, not a rewrite.

## 7. Source layout (proposed)

```
Source/awsim/
  Core/            GameMode, GameState, GameInstance subsystem (save/load)
  Camera/          camera Pawn, PlayerController (picking/placement)
  Simulation/      Time, Economy, Population, Grid subsystems
  Entities/        FCitizen, FBuilding structs; representation components
  Data/            DataAsset/DataTable schema for entity types
```

```
Content/
  Maps/            levels (.umap)
  Buildings/       BP children + meshes/materials
  Citizens/        meshes / VAT assets
  Data/            DataAsset/DataTable instances (the actual numbers)
```

## 8. Open questions / TODO

- [ ] Grid model: square tiles vs. freeform placement (RCT/Planet Coaster lean freeform).
- [ ] Fixed-tick rate and how game-speed multiplier maps to sim steps.
- [ ] Save format and versioning strategy.
- [ ] When (if ever) to adopt Mass Entity.
- [ ] Pathfinding approach for agents (navmesh vs. grid vs. flow fields).
