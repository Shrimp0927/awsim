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
   citizens themselves; the city responds as a system.
3. **Balance under scarcity.** There is no "right" option — higher quality lifts
   ratings but drains resources, cheaper choices conserve budget but cost
   wellbeing. The game *is* the constant trade-off of building a good city with
   limited means.

## 3. Core loop

```
   1. COLLECT  ── tax a % of each citizen's net worth, every X days ──► budget
   2. ALLOCATE ── spend budget on buildings & services
   3. RIPPLE   ── buildings change citizen needs & the economy
   4. REACT    ── satisfaction moves your Overlord rating; read it, adjust
                                    │
                                    └──► repeat. Rating too low = term ends.
```

## 4. Citizens — the heart of the sim

Each citizen carries two tiers of state:

| Tier                | Attributes                     | Changes        | Drives                              |
|---------------------|--------------------------------|----------------|-------------------------------------|
| **Long-term**       | net worth, health, satisfaction | slowly         | satisfaction → Overlord rating      |
| **Daily (short)**   | hunger, energy, mood           | every day      | productivity → world resource output |

The two tiers feed two causal chains:

```
   Buildings & service quality ─► hunger / energy / mood (daily)
                                          │
                                          ▼
                              productivity / efficiency
                                          │
                                          ▼
                              world resource generation
   (a tired or hungry citizen works below their max)


   net worth + health ─► satisfaction ─► Overlord rating ─► stay in office?
```

**Quality is a trade-off.** Daily needs are shaped by *how well* you provide —
a better food source keeps hunger lower than a cheap one. But quality costs more,
so every provision is a decision: spend to lift wellbeing (and ratings), or
conserve resources and accept the lower outcome. Balancing this across the whole
city under a limited budget is the core of the game.

## 5. Economy & building

- **Revenue:** every X days, tax collects a **% of each citizen's net worth**
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

## 6. Iteration 1 — in scope

_(proposed minimal slice — confirm/adjust)_

- One small map to build on.
- Citizens simulated with all six attributes on the fixed tick (§4).
- A starter set of building types with quality tiers — enough to exercise the
  loop: housing, a food source, a workplace, an energy source, roads.
- The tax cycle → budget → placement-spending loop.
- Satisfaction → Overlord rating, shown as a visible meter.
- A lose condition when the rating bottoms out.

## 7. Out of scope (for now)

- Multiplayer.
- Deep Overlord identity/flavor (naming is cosmetic for now).
- Rich economy modelling, advanced pathfinding, polished art.

## 8. Open questions

- Does daily **mood** feed long-term satisfaction directly, or only via productivity?
- How is **net worth** generated and grown (jobs, company profits, wages)?
- Tax cadence and rate: what is "X days", and what %?
- What changes a citizen's **health** (food quality, services, pollution)?
- Rating thresholds — what counts as "doing well" vs. "term ends"?
- Map model: grid vs. freeform placement (ties to `ARCHITECTURE.md` §8).
