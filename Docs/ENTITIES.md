# awsim — Entities

> **Why this exists.** Entities are the *things* the simulation moves around —
> the per-object state that subsystems read and write each tick. Following the
> project's core rule, entity **state is plain data** (structs in arrays),
> simulated independently of how (or whether) it is drawn (`ARCHITECTURE.md` §2,
> §5). This document is a **living reference** to the entity types that exist
> today and the state each one carries.
>
> Status: **living document** — kept in step with `Source/awsim/Entities/`.

---

## Map of what's built

```
FCitizen        the only entity type so far — one simulated citizen
  ECitizenState   its coarse daily activity enum
```

`FBuilding` is referenced throughout the design and architecture docs but does
**not** exist yet. Citizen is the only entity currently implemented.

---

## `FCitizen` — one simulated citizen

`Entities/Citizen.h` (header-only)

The heart of the sim. Citizens carry the state the Overlord is ultimately graded
on: keep them satisfied or lose office (`DESIGN.md` §4). It is a **plain
`USTRUCT`, not a `UObject`** — thousands live contiguously in the
`TArray<FCitizen>` owned by `UPopulationSubsystem`, kept cheap and POD-friendly
so the daily update can go data-parallel later (`ARCHITECTURE.md` §5).

**Currently features two tiers of state** (`DESIGN.md` §4):

| Tier          | Field          | Range / meaning                       | Default |
|---------------|----------------|---------------------------------------|---------|
| **Long-term** | `NetWorth`     | wealth; the tax base                   | `0`     |
| (drives the   | `Health`       | `0..1`                                 | `1`     |
| Overlord rating) | `Satisfaction` | `0..1`                              | `0.5`   |
| **Daily**     | `Hunger`       | `0` = full … `1` = starving            | `0`     |
| (drives       | `Energy`       | `1` = rested … `0` = exhausted         | `1`     |
| productivity) | `Mood`         | `0..1`                                 | `0.5`   |

Plus:

- **`State` (`ECitizenState`)** — coarse daily activity:
  `AtHome` / `Commuting` / `Working` / `Leisure`. Default `AtHome`.
- **`Productivity()`** — derived `0..1` readout of how effectively the citizen
  works *right now*. First-pass formula: the mean of "fed" (`1 - Hunger`),
  `Energy`, and `Mood`. This is the bridge from daily state to world resource
  output described in `DESIGN.md` §4.

**How it's used today:** `UPopulationSubsystem` drifts the daily fields each step
and eases `Satisfaction` toward a health/net-worth proxy; `Economy` taxes
`NetWorth`. See `SUBSYSTEMS.md`.

**Placeholder / not yet:**

- **No position or building links.** The architecture's sketch of `FCitizen`
  includes `Position`, `HomeBuilding`, and `WorkBuilding` (`ARCHITECTURE.md` §5);
  none exist yet, so citizens have no place in the world and `Commuting` /
  `Working` states are never actually entered.
- **No visual projection.** No ISM instance index / transform mapping yet — the
  data exists but is not drawn.
- `Productivity()` and the satisfaction proxy are loose first passes, not tuned
  or data-driven.

---

## Known gaps vs. the design/architecture plan

- **`FBuilding`** — the other half of the loop (`ARCHITECTURE.md` §5, §7;
  `DESIGN.md` §5). Buildings are meant to shape citizen needs and the economy via
  data-driven *quality tiers*, render via ISM, and promote to a full Actor only
  while selected. None of this is built — there are no buildings, no placement,
  and so nothing yet *improves* a citizen's daily needs.
- **Representation components** — the data↔visual seam (ISM projection, selection
  by instance index) named in `ARCHITECTURE.md` §5 has no entity-side code yet.
