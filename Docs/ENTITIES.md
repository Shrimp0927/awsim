# awsim — Entities

> **Why this exists.** Entities are the *things* the data layer holds. Under the
> **macro model** (`DESIGN.md` §4) the simulation's source of truth is the **grid**
> (what's built) plus a handful of **macro stats** — *not* per-citizen state. The
> citizens/cars/planes you see are a *disposable projection* of the macro state and
> are never saved (`ARCHITECTURE.md` §2, §5). This is a **living reference** to the
> entity types that exist today.
>
> Status: **living document** — kept in step with `Source/awsim/Entities/`.
>
> _Reworking in progress: the persistent grid-content entities (previously `FRoad`)
> have been scrapped and are being rebuilt from the ground up — none exist yet._

---

## Map of what's built

```
Value primitives:
  FGridCoord      integer tile coordinate used by the grid

Ephemeral representation (projection of game state, never saved):
  FAgent          one live visual agent on screen (owned by UAgentSubsystem)
    EAgentKind      pedestrian / vehicle / aircraft
  FAgentKindDef   data-driven description of an agent kind (lifespan, anim, speed)
```

Only `FGridCoord` and the agent types exist. There is currently **no persistent
grid-content entity** — roads were scrapped and buildings (`FBuilding`) are not
built yet.

---

## `FGridCoord` — tile coordinate (value primitive)

`Entities/GridCoord.h` (header-only)

Not an entity — a small shared value type (`int32 X, Y`) used by the grid.
Reflection-friendly and hashable (`GetTypeHash`) so it can key the grid's
occupancy map (`UGridSubsystem`, see `SUBSYSTEMS.md`). Tile-based addressing is a
provisional choice (grid model is open — `ARCHITECTURE.md` §8).

---

## Agents — the ephemeral representation

`Entities/Agent.h` (header-only)

The citizens, cars, and planes you *see*. They are **not** the simulation — under
the macro model nobody is individually simulated. Each agent is a **disposable
projection** of the macro stats + grid: `UAgentSubsystem` spawns them, ages them,
and recycles them in a continuous cycle so the screen reflects the city's state
(see `SUBSYSTEMS.md`). Everything here is **pure ephemeral — never saved.**

### `EAgentKind` — what an agent is

`Pedestrian` (a "citizen"), `Vehicle` (a car), `Aircraft` (a plane). Extensible —
add a value per new kind. The kind selects which `FAgentKindDef` to use, and
therefore the mesh, animation, lifespan, and movement behaviour.

### `FAgentKindDef` — the data-driven *kind* description

Defines how a kind looks and behaves — the "type", meant to be authored in a Data
Table / Data Asset in the editor (`ARCHITECTURE.md` §2.3), not hardcoded.

| Field      | Type         | Meaning                                       | Default       |
|------------|--------------|-----------------------------------------------|---------------|
| `Kind`     | `EAgentKind` | which kind this describes                      | `Pedestrian`  |
| `Lifespan` | `float`      | seconds on screen before it despawns          | `8`           |
| `MoveSpeed`| `float`      | world units / second while moving             | `150`         |
| `AnimSet`  | `FName`      | animation set; bound to an asset later         | *(none)*      |

### `FAgent` — one live instance

The minimal per-instance runtime record, held `Transient` in `UAgentSubsystem`'s
pool. This is the *correct* use of `Transient` — ephemeral data rebuilt at runtime
and never saved.

| Field      | Type         | Meaning                                       | Default       |
|------------|--------------|-----------------------------------------------|---------------|
| `Kind`     | `EAgentKind` | which kind this instance is                    | `Pedestrian`  |
| `Position` | `FVector`    | where it is in the world                       | `0,0,0`       |
| `Velocity` | `FVector`    | current movement                               | `0,0,0`       |
| `Age`      | `float`      | seconds alive so far                           | `0`           |
| `Lifespan` | `float`      | once `Age >= Lifespan`, it despawns            | `8`           |

**Placeholder / not yet:**

- **Lifecycle only** — `UAgentSubsystem` spawns/ages/recycles; **movement,
  animation, and ISM rendering are TODO** (no visuals are built yet).
- **`FAgentKindDef` is a schema with hardcoded defaults** — not yet sourced from a
  Data Table/Asset, so per-kind lifespan/anim/speed aren't wired through to spawns.
- **Pedestrians only** — `SpawnAgent` only spawns `Pedestrian`; vehicles/aircraft
  and the rules for how many of each to show aren't implemented.

---

## Known gaps vs. the design/architecture plan

- **Persistent grid content** — there is currently no placed-content entity at all
  (roads scrapped, buildings not built). When rebuilt, this content lives in
  `UGridSubsystem` as plain structs (the `EGridContent::Building` slot is already
  reserved) and is authoritative, saved state — the opposite of the ephemeral
  agents.
- **`FBuilding`** — the planned half of the loop (`ARCHITECTURE.md` §5, §7;
  `DESIGN.md` §5): buildings shape the macro stats (housing capacity, jobs,
  services, wealth growth) via data-driven *quality tiers*, render via ISM, and
  promote to a full Actor only while selected. Not built yet, so the macro stats
  are loose seeded values rather than derived from the city.
