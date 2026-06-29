# awsim — Subsystems

> **Why this exists.** The simulation is the game. Under the **macro model**
> (`DESIGN.md` §4) the city is run top-down: the **grid** (what's built) plus a
> few **macro stats** are the source of truth, and the citizens/cars/planes you
> see are a *projection* of that state. Subsystems are the engine-managed
> singletons that run it — one clock driving a fixed order of phases
> (`ARCHITECTURE.md` §4, §6). This is a **living reference** to the subsystems
> that exist today, what each does, and where it is still a placeholder.
>
> Status: **living document** — kept in step with `Source/awsim/Simulation/`.
>
> _Reworking in progress: `EconomySubsystem` has been scrapped (a future economy
> phase will return), and the grid's concrete content types (previously roads) are
> being rebuilt from the ground up._

---

## Map of what's built

```
USimulationSubsystem            the single clock (ticks per frame)
  └─ steps, in PhaseOrder:
       USimPhase  (abstract)    base class for every phase
         ├─ UGridSubsystem         order 200 — placement authority (tile occupancy)
         ├─ UCityStatsSubsystem    order 800 — macro stats (pop, wealth, rating)
         └─ UAgentSubsystem        order 900 — ephemeral visual agents (crowd)
```

These four (plus the `USimPhase` base) are the current skeleton. `Time`,
`Economy`, and `Traffic` phases do not exist; the calendar/cadence and economy
they would own are not implemented.

---

## `USimulationSubsystem` — the single clock

`SimulationSubsystem.{h,cpp}`

The one orchestrator that advances the whole sim. It is the **only** thing that
ticks per frame; every phase is stepped from here so phase order — and therefore
the result — is deterministic (`ARCHITECTURE.md` §6, Rule 1). It runs the sim
**synchronously on the game thread** (the async snapshot pipeline in
`ARCHITECTURE.md` §6 is a deferred optimization, not built).

**Currently features:**

- **Fixed-timestep accumulator.** `FixedStep = 1/30s`. Each frame it adds
  `DeltaSeconds * GameSpeed` to an accumulator and runs whole steps while time is
  owed. A **`MaxStepsPerFrame = 8`** cap stops a frame hitch from spiralling into
  a catch-up freeze.
- **Game-speed control.** `SetGameSpeed()` / `GetGameSpeed()` (clamped ≥ 0) and
  `SetPaused()` / `IsRunning()`. Speed only changes *how many* fixed steps run per
  frame, never the step size or order.
- **Phase ordering.** On `OnWorldBeginPlay` it collects every `USimPhase` in the
  world via `GetSubsystemArrayCopy` and sorts them ascending by `PhaseOrder()`
  into `OrderedPhases`. `StepOnce()` walks that list in order, so each phase sees
  the completed output of the one before it.
- **Loose day clock.** `StepsPerDay = 300` fixed steps == one in-game day;
  `GetDay()` exposes the count.

**Placeholder / not yet:**

- Day clock is a step counter, not a calendar — a dedicated `TimeSubsystem` is
  meant to own the calendar and speed (`ARCHITECTURE.md` §6).
- `RebuildPhaseOrder()` runs once at begin-play; phases added/removed at runtime
  would need a re-sort.
- No save/load hook into the accumulator/day yet.

---

## `USimPhase` — phase base class

`SimPhase.h` (header-only)

The abstract base every simulation phase derives from. Exists to enforce the
"one clock, ordered phases, no self-ticking" rule: phases are `UWorldSubsystem`s
but are stepped by the orchestrator, never by their own tick.

**Currently features:**

- **`Step(float StepSeconds)`** — advance one fixed step. Default no-op; override
  per phase.
- **`PhaseOrder()`** — lower runs first. Documented bands: `Time=100`, `Grid=200`,
  `CityStats=300`, `Agents=900` (representation, reads finished output).
- Marked `UCLASS(Abstract)` so it is never instantiated directly.

**To add a phase:** subclass `USimPhase`, choose a `PhaseOrder`, implement
`Step()`. No orchestrator change needed — it is discovered and sorted
automatically.

---

## `UGridSubsystem` — placement authority

`GridSubsystem.{h,cpp}` — `PhaseOrder() = 200`

The city grid: the placement authority for everything placed on the map
(`ARCHITECTURE.md` §4, §6). Runs early (order 200) so it's settled before
CityStats and the crowd read it.

**Currently a skeleton** (concrete content types are being reworked):

- **Generic occupancy** — `TMap<FGridCoord, EGridContent> Occupancy` mapping each
  tile to what sits on it. Authoritative placement state, so it is **not**
  `Transient` (the save/load layer persists it; see § "Persistence").
- **`EGridContent`** — the content enum, currently `Empty` and a reserved
  `Building`. Concrete types (buildings, transport, …) return with the rework.
- **Tile API** — `IsTileOccupied(Tile)`, `GetContentAt(Tile)`, and
  `SetContent(Tile, Content)` (passing `Empty` clears; returns false on a no-op).

**Placeholder / not yet:**

- **`Step()` is a no-op** — placement is event-driven. The per-step slot is
  reserved for any derived rebuild (e.g. a transport-network graph) once content
  returns.
- **No concrete content** — roads were scrapped; buildings/transport are not built.
- **No placement input/UI** — the API exists but nothing calls it yet.
- **Grid model still provisional** — tile-based is the loose first pass; square
  tiles vs. freeform is an open question (`ARCHITECTURE.md` §8).

---

## `UCityStatsSubsystem` — the macro stats

`CityStatsSubsystem.{h,cpp}` — `PhaseOrder() = 800`

The top-down source of truth for "how's the city doing" (`DESIGN.md` §4). Under
the macro model nobody is simulated individually; a handful of aggregate numbers,
derived from the grid, drive the whole game and everything that gets visualised.
Runs as the **last simulation phase** (order 800) — right before the Agent crowd
(900) projects it — so the visualised crowd reflects the freshest stats.

**Currently features:**

- **Macro readouts** — `GetPopulation()`, `GetAverageSatisfaction()` (0..1),
  `GetOverlordRating()` (0..1), `GetTaxableWealth()`.
- **Wealth deduction** — `DeductWealth(Amount)`, exposed for a future economy
  phase to move taxed wealth into a treasury (the previous Economy phase was
  scrapped pending rework).
- **`RatingFromSatisfaction(s)`** — a pure static mapping (clamped 0..1).
- **Loose seeding** — `OnWorldBeginPlay` seeds 100 population and 100,000 wealth
  if empty. `Step()` recomputes the rating from satisfaction each tick.
- Stats are authoritative game state (**not** `Transient`).

**Placeholder / not yet:**

- **Stats don't derive from the city yet** — population, wealth, and satisfaction
  are seeded/steady placeholders. They're meant to come from the grid's housing
  capacity, jobs, and service quality once buildings exist (`DESIGN.md` §4/§5).
- **Satisfaction is static** — nothing yet moves it, so the rating is flat.
- **No consumer for wealth** — `GetTaxableWealth`/`DeductWealth` await the reworked
  economy phase.

---

## `UAgentSubsystem` — the crowd (representation)

`AgentSubsystem.{h,cpp}` — `PhaseOrder() = 900`

Spawns, ages, and recycles the ephemeral visual agents (citizens/cars/planes)
that represent the macro state on screen. This is the **representation layer, not
simulation** (`ARCHITECTURE.md` §4/§5): agents are a disposable projection of the
macro stats + grid, never the source of truth. Runs last (order 900) so it reads
finished grid/macro output.

**Currently features:**

- **Ephemeral agent pool** — `TArray<FAgent> Agents`, marked `Transient` (the
  *correct* use of `Transient`: this data is rebuilt at runtime and never saved).
- **Lifecycle (`Step`).** Ages every agent, despawns any past its `Lifespan`
  (swap-remove), then tops the pool up toward `DesiredAgentCount()` so agents
  appear and disappear in a continuous cycle.
- **Macro-driven count** — `DesiredAgentCount()` reads
  `UCityStatsSubsystem::GetPopulation()` and divides by `PeoplePerAgent`
  (default 10).
- `NumAgents()` readout.

**Placeholder / not yet:**

- **No rendering** — there is no ISM projection, mesh, or animation yet; the pool
  is just numbers. Movement (`Velocity`/`Position`) is unused.
- **Pedestrians only** — `SpawnAgent` always spawns `Pedestrian`; vehicles/
  aircraft and the kind mix are not implemented.
- **`FAgentKindDef` not wired in** — per-kind lifespan/anim/speed come from a
  hardcoded default, not yet from a Data Table/Asset.
- **Should it be a phase?** It's modelled as a high-order `USimPhase` (900) for
  now; strictly it's representation and may move to a dedicated post-sim
  visual-sync pass (`ARCHITECTURE.md` §6) later.

---

## Persistence

What's saved vs. rebuilt, by design (`ARCHITECTURE.md` §4 — a GameInstance-backed
save/load layer that is **not built yet**):

| Data                                   | Saved? | Why                                       |
|----------------------------------------|--------|-------------------------------------------|
| `UGridSubsystem::Occupancy`            | yes    | authoritative placement state             |
| `UCityStatsSubsystem` macro stats      | yes    | authoritative city state                  |
| `UAgentSubsystem::Agents`              | no     | ephemeral projection; recycled at runtime |

Note: marking data non-`Transient` only keeps it *serializable* — nothing
actually saves until the save/load layer exists. World subsystems aren't
auto-saved the way level Actors are.

---

## Known gaps vs. the architecture plan

Phases named in `ARCHITECTURE.md` §6 but **not built (or scrapped pending
rework)**:

- **`TimeSubsystem`** (order ~100) — calendar + speed ownership. Today the day
  clock lives in the orchestrator.
- **Economy phase** (~order 500) — treasury + tax cycle. Scrapped; will return to
  consume `CityStats`'s taxable wealth.
- **`TrafficSubsystem`** — movement/pathfinding for agents over placed transport.
  Needs concrete grid content (roads) and a network graph first.
