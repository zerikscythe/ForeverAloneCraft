# Early Bot Validation

This document tracks the first validation pass for the LivingWorld
account-alt and bot-session pipeline.

It separates work into three buckets:

- server-assisted / headless validation
- in-game required validation
- DB verification after each scenario

## Scope

This checklist is for early safety validation of:

- bot-session spawn
- account-alt runtime row lifecycle
- clone name lease / reclaim
- dismiss and logout recovery
- progress sync
- equipment sync
- inventory sync
- bank sync
- restart / interrupted-state recovery
- player-facing command and chat behavior

## Important rule

Do not treat DB writes alone as proof that the feature works.

A scenario is only considered complete when:

1. the expected server lifecycle happened
2. the DB state is correct
3. any required in-game behavior was also observed

---

## Test Fixture

Use one controlled fixture for repeatable runs.

### Accounts and characters

- [ ] One owner account exists for live testing
- [ ] One source alt exists on that owner account
- [ ] One reserved bot-pool account exists
- [ ] One clone target can be materialized on the bot-pool account

### Logging

- [ ] Enable strong LivingWorld server logging
- [ ] Keep player `.lw loglevel` at `4` during manual validation
- [ ] Capture worldserver log for each test run
- [ ] Record timestamps for restart and dismiss actions

### Baseline snapshots

Before each scenario, record:

- [ ] source character name
- [ ] source level
- [ ] source XP
- [ ] source money
- [ ] source equipped items
- [ ] source inventory layout
- [ ] source bank layout
- [ ] current runtime row state
- [ ] current bot-pool reservation row

### Tables to snapshot

- [ ] `characters`
- [ ] `character_inventory`
- [ ] `item_instance`
- [ ] `living_world_account_alt_runtime`
- [ ] `living_world_bot_account_pool`

---

# 1. Server-Assisted / Headless Validation

These tasks can be done with server code, debug commands, controlled SQL setup,
restart testing, and log inspection. They do not require full manual gameplay.

## 1.1 Spawn lifecycle

- [ ] Request a roster entry and confirm a runtime row is created or reused
- [ ] Confirm source account + source character are correct in runtime row
- [ ] Confirm reserved bot account selection is correct
- [ ] Confirm clone character is created or reused correctly
- [ ] Confirm source alt is parked to hidden reserved name
- [ ] Confirm clone receives the visible source name
- [ ] Confirm bot session reaches login successfully
- [ ] Confirm owner-to-bot registry entry exists
- [ ] Confirm no duplicate active runtime rows are created

## 1.2 Pending login and race protection

- [ ] Request the same roster entry twice quickly
- [ ] Confirm pending-login protection blocks the duplicate request
- [ ] Request a different roster entry while one login is pending
- [ ] Confirm the second request is rejected
- [ ] Confirm no second bot session is created
- [ ] Confirm no second active runtime row is created

## 1.3 Dismiss lifecycle

- [ ] Dismiss an active bot through the authoritative dismiss path
- [ ] Confirm bot session uses `LogoutPlayer(true)`
- [ ] Confirm bot registry entry is removed
- [ ] Confirm runtime recovery starts from the logout path
- [ ] Confirm source live name is restored
- [ ] Confirm runtime row is retired or moved to the expected state
- [ ] Confirm bot-pool reservation remains correct
- [ ] Confirm no stale active clone body remains

## 1.4 Owner logout lifecycle

- [ ] Log out the owner while a controlled clone is active
- [ ] Confirm owner `OnPlayerBeforeLogout` starts controlled dismiss
- [ ] Confirm clone logout path executes before owner cleanup completes
- [ ] Confirm group cleanup and runtime recovery both run
- [ ] Confirm no stale bot remains registered after owner logout

## 1.5 Progress sync

### Allowed progress deltas

- [ ] Make clone XP differ from source within allowed range
- [ ] Make clone level differ from source within allowed range
- [ ] Make clone money differ from source within allowed range
- [ ] Dismiss clone
- [ ] Confirm sync plan selects approved progress domains only
- [ ] Confirm source `characters` row is updated correctly
- [ ] Confirm runtime leaves `SyncingBack` cleanly
- [ ] Confirm repeating the same dismiss does not double-apply progress

### Blocked or manual-review deltas

- [ ] Force level delta above allowed threshold
- [ ] Force money delta above allowed threshold
- [ ] Confirm runtime is routed to blocked or manual review
- [ ] Confirm no destructive source write occurs

## 1.6 Equipment sync

- [ ] Change clone equipped items only
- [ ] Dismiss clone
- [ ] Confirm equipment sync plan is selected
- [ ] Confirm source equipment matches expected result
- [ ] Confirm duplicated item ownership and slot mapping are valid
- [ ] Confirm no unexpected inventory corruption occurs

## 1.7 Inventory sync

### Safe same-bag scenarios

- [ ] Keep root inventory bag item types unchanged
- [ ] Change bag contents only
- [ ] Dismiss clone
- [ ] Confirm inventory sync plan is selected
- [ ] Confirm nested container remap succeeds
- [ ] Confirm source inventory matches expected logical layout

### Unsafe bag-container-change scenarios

- [ ] Change root inventory bag type in slots `19-22`
- [ ] Dismiss clone
- [ ] Confirm `bagContainersChanged` is detected
- [ ] Confirm path escalates to manual review
- [ ] Confirm no automatic inventory write occurs

## 1.8 Bank sync

### Safe same-bank-bag scenarios

- [ ] Keep root bank bag item types unchanged
- [ ] Change bank contents only
- [ ] Dismiss clone
- [ ] Confirm bank sync plan is selected
- [ ] Confirm source bank layout matches expected logical layout

### Unsafe bank bag change

- [ ] Change root bank bag type in slots `67-73`
- [ ] Dismiss clone
- [ ] Confirm `bagContainersChanged` is detected
- [ ] Confirm path escalates to manual review
- [ ] Confirm no automatic bank write occurs

## 1.9 Restart / interrupted-state recovery

For each guarded runtime state:

- [ ] `SyncingBack`
- [ ] `SyncingEquipment`
- [ ] `SyncingInventory`
- [ ] `SyncingBank`

Run the same sequence:

- [ ] Force runtime into the target state during active work
- [ ] Stop or crash `worldserver`
- [ ] Restart `worldserver`
- [ ] Log owner back in
- [ ] Confirm owner-login recovery discovers the runtime
- [ ] Confirm retry behavior is correct
- [ ] Confirm operation is idempotent
- [ ] Confirm no duplicate item or progress writes occur
- [ ] Confirm final runtime state is stable

## 1.10 Name lease / reclaim

- [ ] Confirm source name is parked on spawn
- [ ] Confirm clone takes visible source name
- [ ] Confirm source name is restored on clean dismiss
- [ ] Confirm clone does not keep visible name after dismiss
- [ ] Confirm failed reclaim blocks safely instead of guessing

## 1.11 Group state from server side

- [ ] Confirm bot joins owner group on successful login
- [ ] Confirm bot leaves group on dismiss
- [ ] Confirm bot leaves group on owner logout
- [ ] Confirm no stale group membership remains after restart recovery

---

# 2. In-Game Required Validation

These tasks require a real client. They are not trusted if validated only by DB
or server logs.

## 2.1 Command UX

- [ ] `.lwbot roster list` renders expected entries
- [ ] `.lwbot roster request <id>` shows correct approval or rejection
- [ ] `.lwbot roster dismiss <id>` shows correct result
- [ ] duplicate-request messaging is readable
- [ ] pending-login rejection is readable
- [ ] manual-review or blocked messaging is readable

## 2.2 Player log ladder

Validate the same event at all levels:

- [ ] `.lw loglevel 1`
- [ ] `.lw loglevel 2`
- [ ] `.lw loglevel 3`
- [ ] `.lw loglevel 4`

Confirm:

- [ ] level `1` shows minimal outcome only
- [ ] level `2` adds reason
- [ ] level `3` adds debug context
- [ ] level `4` adds full trace details
- [ ] server logs remain fully detailed at every player level

## 2.3 Visible spawn / dismiss behavior

- [ ] bot visibly appears near owner
- [ ] bot follows owner after spawn
- [ ] bot is visible in party UI
- [ ] dismiss visibly removes bot
- [ ] no ghost or duplicate body remains visible

## 2.4 Trade flow

This must be validated with a real client because it uses native trade flow.

- [ ] owner starts trade with controlled clone
- [ ] bot-side trade opens correctly
- [ ] owner accept drives bot auto-confirm only after owner accept
- [ ] invalid trade is still blocked by stock handlers
- [ ] bag-space and validity checks behave correctly
- [ ] post-trade dismiss preserves expected inventory state

## 2.5 Follow and combat feel

- [ ] bot follows owner after spawn
- [ ] bot re-follows after owner leaves combat
- [ ] bot attacks owner target when expected
- [ ] bot stops attacking when combat ends
- [ ] no obvious follow jitter or stuck state occurs

## 2.6 Relog visibility checks

- [ ] owner relog after clean dismiss shows correct source character name
- [ ] owner relog after interrupted recovery shows correct message
- [ ] startup recovery summary is understandable in chat
- [ ] no stale active clone appears in party or world after relog

---

# 3. DB Verification After Each Scenario

Run these checks after every scenario. A scenario is not complete without them.

## 3.1 `living_world_account_alt_runtime`

Verify:

- [ ] source account id is correct
- [ ] source character guid is correct
- [ ] owner guid is correct
- [ ] clone account id is correct
- [ ] clone character guid is correct
- [ ] runtime state is expected
- [ ] recovery timestamps are updated correctly
- [ ] no duplicate active row exists for the same source alt

## 3.2 `living_world_bot_account_pool`

Verify:

- [ ] reserved account matches source alt
- [ ] reservation is retained across clean dismiss
- [ ] availability flags match expected state
- [ ] no second pool account was consumed unexpectedly

## 3.3 `characters`

Verify source row:

- [ ] visible name is correct
- [ ] level is correct
- [ ] XP is correct
- [ ] money is correct
- [ ] online flag is correct after dismiss or relog

Verify clone row when applicable:

- [ ] clone name state is correct
- [ ] clone account placement is correct
- [ ] clone online flag is correct
- [ ] clone row is reused or retired as expected

## 3.4 `character_inventory`

Verify:

- [ ] source equipped slots are correct
- [ ] source inventory bag placement is correct
- [ ] source bank placement is correct
- [ ] nested container positions are valid
- [ ] no impossible duplicate slot occupancy exists

## 3.5 `item_instance`

Verify:

- [ ] expected item rows exist
- [ ] duplicated items have new GUIDs where expected
- [ ] ownership points at the correct source character
- [ ] no orphaned nested-container items remain
- [ ] no unintended duplicate items were created

---

# 4. Suggested Execution Order

Run validation in this order.

## Phase A - server-assisted first

- [ ] spawn lifecycle
- [ ] dismiss lifecycle
- [ ] progress sync
- [ ] equipment sync
- [ ] safe inventory sync
- [ ] safe bank sync
- [ ] manual-review bag change cases
- [ ] restart / interrupted-state recovery
- [ ] name lease / reclaim

## Phase B - in-game smoke tests

- [ ] command UX
- [ ] player log ladder
- [ ] visible spawn / dismiss
- [ ] trade flow
- [ ] follow and combat feel
- [ ] relog visibility checks

## Phase C - signoff

- [ ] no unresolved corruption found
- [ ] no unresolved stale runtime rows found
- [ ] no unresolved name reclaim failures found
- [ ] no unresolved duplicate item creation found
- [ ] early validation pass marked complete

---

# 5. Notes for Future Runs

## DB-only is not enough

Use DB checks to verify persistence and recovery state, but do not use DB-only
results as proof of:

- client UX correctness
- trade UI correctness
- visible spawn / dismiss correctness
- follow / combat feel

## Best use of headless validation

Headless or server-assisted validation is best for:

- lifecycle correctness
- recovery correctness
- restart safety
- idempotence
- race protection
- data integrity

## Best use of in-game validation

Real client validation is best for:

- command readability
- trade flow
- visual correctness
- party UI correctness
- movement and follow feel
