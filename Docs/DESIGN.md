# awsim — Game Design (Iteration 1)

> What the game *is* — the vision and the feature scope for the first playable
> iteration. We build off this doc; `ARCHITECTURE.md` covers how it's built.
>
> Status: **drafting** — filled in iteratively from design discussion.

---

## 1. Vision

A single-player city-management simulation in the lineage of **SimCity 4** —
dense, skyscraper cities — reskinned with a **sci-fi** tone. You play as the
**Overlord** (named, e.g. *Overlord John Smith*), and your one job is to **not
get kicked out of office**.

You never command citizens directly. You shape the conditions they live under,
and their collective **satisfaction** decides whether you keep your seat.

## 2. Experience pillars

_(draft — the north stars every feature should serve)_

1. **Stay in power.** Success is political survival, not endless expansion.
   Keep citizens satisfied or lose your office.
2. **Indirect stewardship.** You manage conditions and services, never the
   citizens themselves; the city responds as a system. You act through two levers:
   **building** infrastructure on the map, and **tuning sliders** on the
   interactables you place. (The systems those levers feed are catalogued in
   `Domains.md`.)
3. **Balance under scarcity.** There is no "right" option — higher quality lifts
   ratings but drains resources, cheaper choices conserve budget but cost
   wellbeing. The game *is* the constant trade-off of building a good city with
   limited means.

## 3. Core loop

```
   1. COLLECT  ── tax a % of the city's aggregate wealth, every X days ──► budget
   2. ALLOCATE ── build infrastructure, then tune sliders on it
   3. RIPPLE   ── buildings change citizen needs & the economy
   4. REACT    ── satisfaction moves your Overlord rating; read it, adjust
                                    │
                                    └──► repeat. Rating too low = term ends.
```

## 4. Citizens — measured in aggregate

Citizens are the heart of the game, but the sim does **not** model them one by
one. The city is run **top-down**: a handful of **macro stats** are the source of
truth, and the citizens/cars/planes you see are a *visual projection* of those
stats — not the thing being simulated.

The macro stats:

| Stat                 | Changes | Drives                                  |
|----------------------|---------|-----------------------------------------|
| **Population**       | slowly  | tax base size; how busy the city looks  |
| **Aggregate wealth** | slowly  | the tax base → Overlord budget          |
| **Satisfaction**     | slowly  | → Overlord rating → stay in office?     |

The causal chain:

```
   Buildings & service quality ─► macro stats (population, wealth, satisfaction)
                                          │
                                          ▼
                       satisfaction ─► Overlord rating ─► stay in office?
                                          │
                                          ▼
              agents spawned to represent the state (citizens / cars / planes)
```

**Quality is a trade-off.** How well you provide shapes the macro stats — a better
food source lifts satisfaction more than a cheap one. But quality costs more, so
every provision is a decision: spend to lift the stats (and ratings), or conserve
budget and accept the lower outcome. Balancing this across the whole city under a
limited budget is the core of the game.

> **Why macro, not per-citizen?** The stats *are* the game; individual agents are
> *rendered* from them and are deliberately ephemeral — they appear and disappear
> in a cycle. See `ARCHITECTURE.md` §5 and `ENTITIES.md`.

## 5. Economy & building

- **Revenue:** every X days, tax collects a **% of the city's aggregate wealth**
  into the Overlord's budget.
- **Spending:** the budget buys and upgrades infrastructure and services —
  roads, energy supplies, schools, office buildings, companies, restaurants,
  grocery stores.
- **Feedback:** every building affects citizens (their needs/health) and/or the
  economy (resource output, net-worth growth). Most come in **quality tiers** so
  the player trades cost against benefit.

> Building types and their effects are **data-driven** (see `ARCHITECTURE.md` §2)
> so new buildings can be added without code changes — this is how we keep the
> game easily extensible as it grows.

## 6. Simulation domains — the long game

Under the hood awsim aims to be a **deep, interconnected civilization simulation**:
many systems (economy, energy, health, transport, housing, environment, …) that
the player shapes via the two levers above, and that feed one another. These are
the "conditions" citizens live under — aggregated into the macro stats that drive
satisfaction and your rating.

The full catalogue (18 domains) lives in **`Domains.md`**. It is a long-horizon
target, **not** current scope — almost none of it is built. We will pick a small
vertical slice of domains first (see `Domains.md` open questions) and prove the
build → tune → interconnect loop before widening.

## 7. Iteration 1 — in scope

_(proposed minimal slice — confirm/adjust)_

- One small map to build on.
- Citizens simulated with all six attributes on the fixed tick (§4).
- A starter set of building types with quality tiers — enough to exercise the
  loop: housing, a food source, a workplace, an energy source, roads.
- The tax cycle → budget → placement-spending loop.
- Satisfaction → Overlord rating, shown as a visible meter.
- A lose condition when the rating bottoms out.

## 8. Out of scope (for now)

- Multiplayer.
- Deep Overlord identity/flavor (naming is cosmetic for now).
- Rich economy modelling, advanced pathfinding, polished art.

## 9. Open questions

- What exactly moves **satisfaction** — which services/qualities, and how fast?
- How is **aggregate wealth** generated and grown (jobs, company profits, wages)?
- Tax cadence and rate: what is "X days", and what %?
- Which macro stats beyond satisfaction matter (health, employment, …), and what
  feeds them from the grid?
- Rating thresholds — what counts as "doing well" vs. "term ends"?
- Map model: grid vs. freeform placement (ties to `ARCHITECTURE.md` §8).
