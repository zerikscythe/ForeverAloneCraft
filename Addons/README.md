# Addons workspace notes

This workspace contains a custom WoW addon UI for the LivingWorld bot system.

## Primary addon to inspect

- `Addons/LivingWorld/LivingWorld.lua`
- `Addons/LivingWorld/LivingWorld.xml`
- `Addons/LivingWorld/LivingWorld.toc`

If an issue involves the addon UI, button wiring, page refresh behavior, addon messages, or `.lwbot` command usage, start with those files.

## Server-side references for `.lwbot`

The addon is coupled to the server command and addon-message implementation here:

- `modules/mod-living-world/src/script/LivingWorldCommandGrammar.h`
- `modules/mod-living-world/src/script/LivingWorldCommandGrammar.cpp`
- `modules/mod-living-world/src/script/LivingWorldCommandScript.cpp`

Use those files to verify:

- supported `.lwbot` verbs
- command argument grammar
- server-to-client addon message payloads
- inventory, trainer, and quest message formats

## Current addon architecture

### UI definition

`LivingWorld.xml` defines:

- shared button templates
- minimap button
- main frame `LWCPFrame`
- pages:
  - `LWCPPageBots`
  - `LWCPPageCombat`
  - `LWCPPageNPC`
  - `LWCPPageGear`
  - `LWCPPageBags`
  - `LWCPPageSettings`

### Lua behavior

`LivingWorld.lua` owns:

- tab switching
- roster state
- `.lwbot` command dispatch
- addon message parsing for `LWBOT`
- inventory rendering
- NPC quest/trainer rendering
- global refresh behavior

## Refresh behavior

Use the top refresh icon button:

- XML control: `LWCPRefreshBtn`
- Lua handler: `LWCP_RefreshActivePage()`

Avoid adding page-local refresh buttons unless a page has a proven special-case need.
Current intent is that the top refresh button is the single refresh control for all pages.

## Selection rules

The addon uses roster slot `0` as `Party`.

Important Lua helpers:

- `GetBotRef()`
- `IsPartySelected()`

When adding new actions:

- use `GetBotRef()` when the command supports both a single bot and `party`
- block with `IsPartySelected()` only when the command requires an individual bot

## Examples

Commands that support `party` should follow patterns like:

- `LWCP_Follow()`
- `LWCP_Refreshments()`
- `LWCP_Buff()`
- `LWCP_Yoink()`

Commands that require a single bot should follow patterns like:

- `LWCP_Train()`
- `LWCP_CastOnTarget()`
- `LWCP_RequestBotBags()`
- `LWCP_EquipItem()`

## Addon message protocol

Prefix:

- `LWBOT`

Lua currently handles message families such as:

- `INV`
- `QCLR`, `QMODE`, `QST`, `QEND`
- `QACLR`, `QA`, `QAEND`
- `TACLR`, `TABOT`, `TA`, `TAEND`

If UI behavior looks broken, verify both:

1. the Lua parser in `LivingWorld.lua`
2. the payload builder in `LivingWorldCommandScript.cpp`

## Known guidance for future agents

- Keep changes minimal.
- Prefer updating existing patterns over inventing new ones.
- Check Lua/XML naming alignment carefully.
- If a new UI control is added in XML, confirm its Lua references exist.
- If a new `.lwbot` action is added in Lua, confirm the grammar and command script support it server-side.
