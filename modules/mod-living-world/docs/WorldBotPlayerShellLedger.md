# World Bot Player Shell Ledger

This module now treats `living_world_bot_identity` as the canonical truth for a
bot even when that bot is materialized through a leased player shell.

## Canonical Layers

`living_world_bot_identity`
- who the bot is
- persistent player-shell appearance (`skin`, `face`, `hair_style`, `hair_color`, `facial_style`)
- level / progression state
- combat spec and loadout keys
- display loadout key and doctrine profile key
- active shell pointer and shell state version
- task/session/runtime summary fields

`living_world_bot_assigned_gear`
- combat gear truth
- exact item ids, quality, and enchant strings used during rebuild

`living_world_bot_display_loadout`
- visual equipment truth
- appearance-facing item ids and hide flags for player shell rebuilds

`living_world_bot_runtime_snapshot`
- small resumable runtime state only
- position, runtime text, last task keys, home bind point key

`living_world_bot_shell_runtime`
- current shell lease mapping
- which `LedRes_*` shell is wearing this identity right now

`living_world_bot_rebuild_log`
- append-only audit trail for rebuilds and shell transitions

## Rehydrate Model

When a bot is materialized:

1. Reserve an idle player shell.
2. Load identity + assigned gear + display loadout + runtime snapshot.
3. If appearance is still unresolved, roll a valid race/gender look once and persist it back to the ledger.
4. Register the shell as a distinct runtime kind (`LedgerShell`) so doctrine and inventory logic can treat it differently from owner-backed companion bots.
5. Reset disposable player junk on the shell.
6. Prime only real persistent stock before play begins:
   - level-appropriate healing/mana potions
   - conjured class items stay part of startup prep, not DB seeding
7. Rebuild player state from ledger truth:
   - level
   - appearance
   - doctrine/talent/glyph/action bar package
   - assigned gear
   - display loadout
8. Mark the shell lease and append a rebuild log row.

When a bot is dismissed:

1. Capture only meaningful runtime deltas.
2. Persist them back into the ledger/runtime snapshot tables.
3. Release the shell lease.

The shell is a vessel, not the source of truth.

## Current First Pass

The first working hydrator now wires the critical offline steps:

1. resolve the assigned shell from ledger/runtime mapping
2. wipe volatile shell-owned state offline
   - spells
   - talents
   - action buttons
   - cooldowns and auras
   - pet rows
   - non-bag inventory/items
3. rewrite the shell character row from ledger truth
   - race/class/gender
   - level
   - appearance bytes
   - runtime snapshot position when present
   - hide-helm/hide-cloak flags
   - equipped items rebuilt from assigned gear plus display-loadout overrides
4. clear the pending rebuild flag, append a rebuild log row, and let the login
   path do one live sanitize/reseed pass before AI starts

That is the point where the old shell body stops being authoritative. The shell
history can be garbage; the ledger is what the bot becomes.
