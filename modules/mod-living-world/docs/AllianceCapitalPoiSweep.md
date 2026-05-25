## Alliance Capital PoI Sweep

This checklist tracks the city-navigation PoI cleanup for Alliance capitals.

### Coverage Rules

For each city we want:

- auction houses
- banks
- flight masters
- portals and portal access points
- class trainers
- profession trainers
- piers/docks near boats
- zeppelin/ship-adjacent access points when applicable
- elevators/lifts/tram entrances when applicable
- mailboxes
- inns / innkeepers
- landmark or district anchors that help routing

### Wave 1 Status

#### Stormwind City

- Done:
  - auction houses
  - banks
  - flight master
  - Outland portal point
  - class trainers
  - profession trainers
  - portal trainer
  - weapon master
  - mailboxes
  - inn / innkeeper
  - tram entrance
  - mage tower lower/mid access anchors
  - ship point for Northrend already exists in transit SQL
  - harbor walk access helpers
  - ship-adjacent harbor access helper
- Remaining:
  - any extra portal-room or tower-top anchors we decide to support

#### Ironforge

- Done:
  - auction houses
  - banks
  - flight master
  - class trainers
  - profession trainers
  - portal trainer
  - weapon master
  - mailboxes
  - inn / innkeeper
  - tram entrance
- Remaining:
  - district anchors if we want finer routing inside the city

#### Darnassus

- Done:
  - auction houses
  - banks
  - class trainers
  - profession trainers found in-city
  - weapon master
  - mailboxes
  - inn / innkeeper
- Intentional omission:
  - no `darnassus_taxi`; the real flight master is in Rut'theran Village
- Remaining:
  - any city landmark anchors we decide we need

#### Rut'theran Village

- Done:
  - hippogryph master / taxi point
  - ferry dock helper
  - portal to Darnassus
  - Darnassus-side portal endpoint
  - Auberdine taxi linkage
  - Auberdine ferry linkage
- Remaining:
  - extra local landmark anchors only if routing needs them

#### Exodar

- Done:
  - auction houses
  - banks
  - flight master
  - class trainers
  - profession trainers found in-city
  - portal trainer
  - mailboxes
  - inn / innkeeper
- Remaining:
  - any missing profession trainers not found in the current city bounds
  - extra landmark anchors if needed

### SQL Files

- `data/sql/updates/pending_db_world/rev_living_world_016_task_points_city_services.sql`
- `data/sql/updates/pending_db_world/rev_living_world_017_taxi_routes.sql`
- `data/sql/updates/pending_db_world/rev_living_world_018_transit_routes.sql`
- `data/sql/updates/pending_db_world/rev_living_world_032_city_poi_task_expansion.sql`
- `data/sql/updates/pending_db_world/rev_living_world_072_alliance_capital_poi_wave1.sql`
- `data/sql/updates/pending_db_world/rev_living_world_073_alliance_capital_poi_wave2_trainers_and_transit.sql`
- `data/sql/updates/pending_db_world/rev_living_world_074_alliance_capital_transit_helpers.sql`
- `data/sql/updates/pending_db_world/rev_living_world_075_ruttheran_auberdine_transit.sql`

### Next Suggested Waves

1. District / landmark anchor pass for capitals that still feel sparse
2. Horde-capital naming cleanup with the same conventions
3. Additional city-specific transit helpers only where real routing pain shows up
