**Point Keys**

Use task-point keys in two layers:

- canonical city/service anchors:
  - `stormwind_bank`
  - `ironforge_mailbox`
  - `exodar_ah`
- specific world-coordinate PoIs:
  - `stormwind_auctioneer_chilton_ne`
  - `ironforge_banker_bailey_stonemantle_n`
  - `darnassus_mailbox_inn_ne`

For future city waves, follow these rules:

- keep broad shared service points short and stable:
  - `city_bank`
  - `city_mailbox`
  - `city_ah`
  - `city_inn`
  - `city_taxi`
- for named NPC-backed points, prefer:
  - `city_<role>_<npc_name>_<dir>`
- for unlabeled fixtures like mailboxes, prefer:
  - `city_<fixture>_<landmark_or_district>_<dir>`
  - or `city_<fixture>_<dir>` when no better landmark exists
- for one-off landmarks, use semantic names instead of numeric suffixes:
  - `stormwind_cathedral_square`
  - `stormwind_old_town_walk`
  - `stormwind_mage_quarter_plaza`

Direction suffixes should describe the point relative to the city center or
obvious city district footprint:

- `n`, `ne`, `e`, `se`, `s`, `sw`, `w`, `nw`
- use richer landmark words when they are more helpful than another compass hop

Avoid bare insertion-order suffixes like `_01` and `_02` unless the point is a
temporary authoring stub that will be renamed before it becomes a durable PoI.
