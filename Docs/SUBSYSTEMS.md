# awsim — Subsystems

> **Why this exists.** The simulation is the game: a city of citizens whose
> conditions the Overlord shapes to stay in power (`DESIGN.md`). Subsystems are
> the engine-managed singletons that run that simulation — one clock driving a
> fixed order of phases (`ARCHITECTURE.md` §4, §6). This document is a **living
> reference** to the subsystems that exist *today*, what each does, and where it
> is still a placeholder.
>
> Status: **living document** — kept in step with `Source/awsim/Simulation/`.

---

## Map of what's built

```
USimulationSubsystem            the single clock (ticks per frame)
  └─ steps, in PhaseOrder:
       USimPhase  (abstract)    base class for every phase
         ├─ UPopulationSubsystem   order 400 — citizens' daily state + tax base
         └─ UEconomySubsystem      order 500 — treasury + tax collection
```

Only **Population** and **Economy** are implemented. `Time`, `Grid`, and
`Traffic` from the architecture's phase plan (`ARCHITECTURE.md` §6) do not exist
yet; their roles are currently faked inside the existing phases (see notes
below).

---

## `USimulationSubsystem` — the single clock

`SimulationSubsystem.{h,cpp}`

The one orchestrator that advances the whole sim. It is the **only** thing that
ticks per frame; every phase is stepped from here so phase order — and therefore
the result — is deterministic (`ARCHITECTURE.md` §6, Rule 1).

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
  `Population=400`, `Economy=500` (Economy after Population so it taxes updated
  net worth).
- Marked `UCLASS(Abstract)` so it is never instantiated directly.

**To add a phase:** subclass `USimPhase`, choose a `PhaseOrder`, implement
`Step()`. No orchestrator change needed — it is discovered and sorted
automatically.

---

## `UPopulationSubsystem` — citizens & the tax base

`PopulationSubsystem.{h,cpp}` — `PhaseOrder() = 400`

Owns every citizen's data and advances their daily state each step. It is the
single source of truth for the population (`ARCHITECTURE.md` §5) and the heart of
the sim — citizen satisfaction is ultimately what the Overlord is graded on
(`DESIGN.md` §4).

**Currently features:**

- **Owns `TArray<FCitizen> Citizens`** — the contiguous, POD-friendly data array
  (`Transient`).
- **Seeding.** `OnWorldBeginPlay` seeds **100** citizens if empty;
  `SeedCitizens(Count)` adds citizens with `NetWorth = 1000`. Temporary test
  seeding.
- **Per-step daily drift (`Step`).** A simple loop over all citizens:
  - `Hunger` rises (`+0.02/s`), `Energy` falls (`-0.01/s`), both clamped 0..1.
  - `Mood` eases toward a fed-and-rested target.
  - Long-term `Satisfaction` eases toward a health + net-worth proxy
    (`DESIGN.md` §4), slowly.
- **Readouts.** `NumCitizens()`, `AverageProductivity()` (mean of each citizen's
  `Productivity()` — a loose city-health number), and `TotalNetWorth()` (the tax
  base consumed by Economy).

**Placeholder / not yet:**

- Update is a **plain serial loop** — the per-citizen work is independent and is
  explicitly intended to go data-parallel with double-buffering later
  (`ARCHITECTURE.md` §6, Rule 2). Not done yet.
- Needs drift in a vacuum: **building / service quality** (the thing that should
  offset hunger/energy and shape outcomes) is not modelled, so nothing yet
  *improves* a citizen — they only decay.
- No projection to visuals (no ISM sync) yet.

---

## `UEconomySubsystem` — treasury & tax cycle

`EconomySubsystem.{h,cpp}` — `PhaseOrder() = 500`

The Overlord's treasury and the collect half of the core loop (`DESIGN.md` §5).
Runs *after* Population so it taxes freshly updated net worth. Closes the
"`COLLECT` → budget" step that the rest of the economy will spend from.

**Currently features:**

- **Treasury.** `GetBudget()`, `AddFunds(Amount)`, and `TrySpend(Amount)` which
  refuses (returns `false`, spends nothing) if the amount is negative or
  unaffordable.
- **Tax cycle (`Step`).** Accrues a `TaxTimer`; every `TaxIntervalSeconds`
  (default **10s**) it calls `CollectTaxes()`.
- **Collection.** Reads `UPopulationSubsystem::TotalNetWorth()`, takes
  `TaxRate` (default **2%**) into the budget, and logs the revenue, new budget,
  and citizen count via `LogAwsim`.

**Placeholder / not yet:**

- Tax cadence is **seconds-based** (`TaxIntervalSeconds`), explicitly standing in
  for "every X days" until a `TimeSubsystem` owns the calendar.
- **No spending side** — building/service costs, upgrades, and the `ALLOCATE`
  step of the loop are not implemented; only `COLLECT` exists.
- Rates (`TaxRate`, interval) are loose placeholders, not yet data-driven.

---

## Known gaps vs. the architecture plan

These phases are named in `ARCHITECTURE.md` §6 but are **not built yet**:

- **`TimeSubsystem`** (order ~100) — calendar + speed ownership. Today the day
  clock lives in the orchestrator and tax cadence lives in Economy.
- **`GridSubsystem`** (order ~200) — tiles and placement validity. No placement
  exists yet.
- **`TrafficSubsystem`** — commuting/pathfinding. `ECitizenState::Commuting`
  exists on the data side but nothing drives it.
