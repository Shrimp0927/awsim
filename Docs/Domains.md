# awsim — Simulation Domains (Plan)

> **Status: long-horizon vision / backlog — almost none of this is built.**
> Today the sim models only loose placeholders (population, wealth, satisfaction)
> in `UCityStatsSubsystem`, with a grid for placement and an ephemeral agent crowd
> for visuals. This document catalogs the *full* set of interconnected systems the
> macro simulation aims to model over time, and how the player influences them. It
> is a planning doc — a target to build toward, not a description of current code.
>
> See `DESIGN.md` (game vision), `Architecture.md` (how the sim is built), and
> `SUBSYSTEMS.md` / `ENTITIES.md` (what actually exists today).

---

## The interaction model — build + tune

The player never commands citizens (DESIGN.md §2). Everything below is shaped
through exactly **two levers**:

1. **Build the world.** Place infrastructure on the **grid** (`UGridSubsystem`) —
   power plants, hospitals, transit, housing, factories, etc. Placement provides a
   domain's *capacity*.
2. **Tune interactables.** Each placed thing exposes **sliders** (operating level,
   budget allocation, policy settings) that shift its domain's metrics — almost
   always a cost-vs-benefit trade-off (DESIGN.md §2, pillar 3).

```
   Build (grid placement)  ─┐
                            ├─► domain metrics ─► feed OTHER domains
   Tune  (sliders / policy)─┘            │
                                         ▼
                         aggregate wellbeing ─► satisfaction ─► Overlord rating
```

The defining property of the sim is **interconnection**: domains feed each other —
an Energy blackout cripples Healthcare and Industry; good Education lifts the Labor
market and Innovation; Pollution drags down Public Health and Environment. Each
domain below ultimately *contributes* macro stats and *reads* others' outputs.

> **This is the core macro model (DESIGN.md §4):** nobody is simulated per-citizen.
> These are aggregate systems. The citizens/cars/planes on screen are an ephemeral
> *projection* of these numbers (`AGENT` crowd), never the source of truth.

---

## The 18 domains

### 1. Economy & Finance
*The engine of the civilization — production, distribution, and consumption of goods and services.*
- **Market performance & trade:** import/export balance, GDP growth, supply-chain liquidity, stock/commodity markets.
- **Fiscal policy:** tax rates (corporate, property, income brackets), government debt/bonds, public spending.
- **Monetary policy:** inflation, currency valuation, central-bank interest rates.
- **Labor market:** unemployment, wage growth, wealth inequality (Gini), union leverage.

### 2. Transportation & Mobility
*The circulatory system — physical movement of people, goods, and data.*
- **Mass transit:** subway/bus/light-rail capacity and ridership efficiency.
- **Road network & traffic:** congestion, road degradation, flow optimization, parking.
- **Logistics & freight:** rail yards, cargo ports, airports, last-mile delivery.
- **Micro-mobility & pedestrians:** walkability, bike lanes, pedestrian safety.

### 3. Public Health & Healthcare
*The physical well-being and longevity of the population.*
- **Healthcare infrastructure:** hospital bed capacity, specialized clinics, staff-to-citizen ratios.
- **Epidemiology & disease control:** contagion vectors, vaccination rates, pandemic readiness.
- **Mental & preventative health:** addiction, stress, green-space access, fitness.
- **Emergency medical services (EMS):** response times, fleet distribution, trauma-center efficiency.

### 4. Politics, Governance & Law
*The decision-making and regulatory framework.*
- **Legislative & policy framework:** law creation, zoning, bureaucratic red tape.
- **Political stability & factions:** party approval, lobbying influence, corruption index, civil-unrest risk.
- **Geopolitics & external relations:** treaties, border control, diplomatic standing.
- **Electoral systems:** voting demographics, gerrymandering, campaign finance.

### 5. Energy & Power Grid
*The literal current that keeps the civilization running.*
- **Generation mix:** ratio of fossil / nuclear / renewables (solar, wind, hydro).
- **Grid stability & distribution:** transmission lines, substation loads, blackouts, smart-grid efficiency.
- **Storage & reserves:** battery storage, pumped-hydro, fuel stockpiles.

### 6. Water & Waste Management
*The fundamental life-support and sanitation loop.*
- **Water supply:** aquifer levels, desalination, purification, water pressure.
- **Sewage & wastewater:** treatment throughput, pipe integrity, discharge impact.
- **Solid waste & recycling:** landfill lifespan, incineration, sorting efficiency, toxic-waste handling.

### 7. Education & Human Capital
*The system that upgrades the quality and capability of the workforce.*
- **Primary & secondary:** school capacity, teacher retention, graduation, literacy.
- **Higher education & research:** universities, trade schools, research output, innovation index.
- **Lifelong learning:** reskilling for workers displaced by automation/economic shifts.

### 8. Public Safety, Justice & Security
*Enforcement of laws and protection of citizens.*
- **Law enforcement:** police presence, response times, community trust.
- **Crime & rehabilitation:** property vs. violent crime, recidivism, prison/rehab capacity.
- **Judicial system:** court backlogs, sentencing fairness, legal-aid access.
- **Fire & rescue:** station coverage, hazmat handling, structural-fire response.

### 9. Housing & Real Estate
*Where the population lives and accumulates personal wealth.*
- **Zoning & density:** low-density residential vs. high-density mixed-use ratios.
- **Affordability & homelessness:** rent-to-income, public housing, eviction rates.
- **Property markets:** foreclosures, gentrification index, value appreciation.

### 10. Environment, Ecology & Climate
*The natural envelope that constrains and reacts to the civilization.*
- **Pollution levels:** air quality (AQI), microplastics, soil toxicity.
- **Climate & weather:** carbon footprint, extreme-weather frequency, rising sea levels.
- **Biodiversity & conservation:** ecosystem health, deforestation, urban wildlife.

### 11. Agriculture & Food Systems
*Feeding the population and ensuring resource independence.*
- **Production & yield:** arable land use, vertical farming, livestock, crop-rotation health.
- **Food security & distribution:** food deserts, grocery supply chains, emergency reserves.

### 12. Industry & Manufacturing
*The physical production of goods and technology.*
- **Sector ratios:** raw-material extraction (mining) vs. refining (heavy manufacturing).
- **Supply-chain resilience:** foreign-component dependence, factory automation, industrial waste.

### 13. Telecommunications & Digital Infrastructure
*The digital fabric connecting the civilization.*
- **Connectivity:** 5G/6G coverage, fiber penetration, satellite internet.
- **Data & cyber-security:** server-farm capacity, cyberattack frequency, data-privacy metrics.

### 14. Culture, Tourism & Recreation
*The "happiness" and identity multipliers of the populace.*
- **Cultural heritage:** museums, landmarks, arts funding.
- **Entertainment & leisure:** stadiums, nightlife, parks, theme parks.
- **Tourism:** hotel occupancy, visitor spending, landmark strain.

### 15. Science, Technology & Innovation
*The progression tree of the civilization.*
- **R&D investment:** % of GDP to tech, patent-generation rates.
- **Tech adoption curve:** how fast populace/business adopt automation, AI, green tech.

### 16. Social Services & Welfare
*The safety net preventing systemic collapse.*
- **Social security & pensions:** elderly-care funding, disability benefits, pension solvency.
- **Family & child support:** subsidized childcare, foster care, parental leave.

### 17. Defense & Homeland Security
*Protection against existential or external threats.*
- **Military readiness:** personnel, weaponry tier, border fortifications.
- **Intelligence & counter-terrorism:** threat detection, border screening, radicalization tracking.

### 18. Disaster Management & Resilience
*Handling the unpredictable "chaos elements."*
- **Mitigation infrastructure:** seawalls, earthquake retrofitting, early-warning sirens.
- **Emergency response & recovery:** food/medical stockpiles, civil-defense training, reconstruction speed.

---

## How a domain plugs into the current architecture

Each domain is intended to follow the same shape, reusing the existing skeleton:

| Piece                        | Provided by                          | Today |
|------------------------------|--------------------------------------|-------|
| Capacity (built infrastructure) | `UGridSubsystem` content (`FBuilding`, …) | only generic tile occupancy exists |
| Policy (slider settings)     | per-interactable data + a policy store | not built |
| Domain metrics               | the macro stats (`UCityStatsSubsystem` or per-domain phases) | only pop/wealth/satisfaction placeholders |
| Cross-domain effects         | ordered phases reading each other     | not built |
| Visualization                | the agent crowd + map readouts        | crowd lifecycle only, no rendering |

The clean data/representation seam (`Architecture.md` §5) and ordered phases
(`Architecture.md` §6) are meant to absorb these without a rewrite.

---

## Open questions (to resolve before building any domain)

- **Granularity:** is each domain a field-group inside one `CityStatsSubsystem`,
  or its own `USimPhase` (e.g. `UEnergySubsystem`, `UHealthSubsystem`)? Likely a
  hybrid: heavy domains get their own phase; light ones are stats fields.
- **Cross-domain dependencies:** how is the "X affects Y" graph expressed — hard-coded
  in each phase's `Step`, or data-driven coefficients? Determinism + ordering matter.
- **Sliders / interactables:** how is a per-building slider represented and stored,
  and how does it map onto domain metrics? (UI + data schema.)
- **First vertical slice:** we should **not** build all 18 at once. Pick a small set
  with a tight feedback loop (e.g. Energy → Industry → Economy → Housing) and prove
  the build-+-tune-+-interconnect loop end to end before widening.
