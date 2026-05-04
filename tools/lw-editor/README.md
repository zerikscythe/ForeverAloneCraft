# LivingWorld Bot Editor

A standalone Python/Tkinter GUI for editing LivingWorld bot combat profiles and managing bot accounts.

## Installation

Install dependencies once:
```bash
pip install -r requirements.txt
```

Or install individually:
```bash
pip install mysql-connector-python
pip install sshtunnel  # Optional: for SSH tunnel support
```

## Running

```bash
python lw_bot_editor.py
```

Connection settings are saved to `config.ini` in the same folder.

## Connection Options

### Direct Connection
Connect directly to a MySQL server on your network.

### SSH Tunnel (for remote servers)
Connect through an SSH "jump host" to reach a database server on a private network.

**See [SSH_SETUP.md](SSH_SETUP.md) for detailed SSH tunnel configuration.**

Common scenarios:
- Database server is not directly accessible from your PC
- Need to hop through a bastion/jump host
- Database is on a private network behind a gateway

## Features

### Combat Profile Management
- Create and edit default profiles by spec/role
- Create per-bot custom profiles
- Priority-based rotation entries
- Interrupt entries
- Complex condition logic (AND/OR)

### Actions
- **Spell actions**: Cast spells with rank modes (Best Known, Exact Spell ID, Specific Rank)
- **Item actions**: Use items by ID
- **Target selection**: Multiple target keys for flexible targeting

### Conditions

#### Subject Keys
- `owner` - The player who owns the bot
- `bot` - The bot itself
- `target` - The bot's current target
- `owner.target` - The owner's current target

#### Stat Keys

**Resource Stats:**
- `hp_pct` - Health percentage (0-100)
- `mana_pct` - Mana percentage (0-100)
- `rage` - Rage (Warriors)
- `energy` - Energy (Rogues, Druids)
- `runic_power` - Runic Power (Death Knights)
- `combo_points` - Combo points (Rogues, Druids)

**Positional Stats:**
- `distance` - Distance to the subject in yards
- `in_melee` - Boolean: whether in melee range
- `is_moving` - Boolean: whether the subject is moving
- `is_casting` - Boolean: whether the subject is casting

**Aura Stats:**
- `aura` - Check for presence of a specific aura/buff/debuff (by spell ID)
- `aura_stacks` - Number of stacks of a specific aura (by spell ID)

**Threat Stats (NEW):**
- `threat_pct` - Subject's threat as a percentage of the top threat holder (0-100+)
  - Example: `bot.threat_pct >= 80` - Bot has 80% or more of top threat
  - Example: `bot.threat_pct < 50` - Bot has less than 50% of top threat
- `is_aggro_holder` - Boolean: whether the subject currently holds aggro
  - Example: `owner.is_aggro_holder == 1` - Owner has aggro
  - Example: `bot.is_aggro_holder == 0` - Bot doesn't have aggro

#### Target Keys
- `enemy` / `enemy_primary` - The primary enemy target
- `enemy_trash` - A trash mob attacking the owner (not the primary target)
- `enemy_primary_victim` - The current victim of the primary target
- `self` - The bot itself
- `owner` - The player who owns the bot
- `ally_lowest_hp` - Ally with the lowest HP (deprecated, use `lowest_hp_party`)
- `lowest_hp_party` - Party member with the lowest HP percentage
- `focus` - Focus target (future use)

#### Comparison Operators
- `==` - Equal to
- `!=` - Not equal to
- `<` - Less than
- `<=` - Less than or equal to
- `>` - Greater than
- `>=` - Greater than or equal to
- `Has` - Has aura (for aura conditions)
- `NotHas` - Does not have aura (for aura conditions)
- `Exists` - Subject exists (non-null)

## Threat-Aware Bot Examples

### Tank Rotation - Taunt When Losing Aggro
**Priority 10 - Taunt**
- Condition: `bot.is_aggro_holder == 0` (Bot doesn't have aggro)
- Action: Cast Taunt on `enemy_primary`

### DPS Rotation - Threat Dump
**Priority 5 - Feint (Rogue)**
- Condition: `bot.threat_pct >= 90` (Bot has 90% or more of top threat)
- Action: Cast Feint on `self`

### DPS Rotation - Stop DPS When Too High Threat
**Priority 3 - Wait for Tank**
- Condition: `bot.threat_pct >= 85` AND `owner.is_aggro_holder == 0`
- Action: No action (wait entry to pause rotation)

### Healer - Fade When Getting Aggro
**Priority 8 - Fade (Priest)**
- Condition: `bot.is_aggro_holder == 1` (Bot has aggro)
- Action: Cast Fade on `self`

### Hybrid - Protective Abilities
**Priority 6 - Hand of Salvation (Paladin)**
- Condition: `lowest_hp_party.threat_pct >= 95` (Party member about to pull aggro)
- Condition Logic: ANY (OR)
- Action: Cast Hand of Salvation on `lowest_hp_party`

## Configuration

The editor supports both:
1. **Default Profiles** - Stored in `acore_world.living_world_bot_combat_default_profile`
   - Shared across all bots of a given spec/role
2. **Custom Profiles** - Stored in `acore_characters.living_world_bot_combat_profile`
   - Per-bot customization for specific characters

## Database Tables

### Default Profiles (acore_world)
- `living_world_bot_combat_default_profile` - Profile settings
- `living_world_bot_combat_default_entry` - Rotation entries
- `living_world_bot_combat_default_action` - Actions per entry
- `living_world_bot_combat_default_condition` - Conditions per entry

### Custom Profiles (acore_characters)
- `living_world_bot_combat_profile` - Per-bot profile settings
- `living_world_bot_combat_entry` - Per-bot rotation entries
- `living_world_bot_combat_action` - Per-bot actions
- `living_world_bot_combat_condition` - Per-bot conditions

## Tips

1. **Priority Order**: Lower priority numbers execute first (e.g., priority 1 before priority 10)
2. **Interrupt Entries**: Use the "Interrupt" flag for spells that should break the current cast
3. **Condition Logic**: Use "All (AND)" when all conditions must be true, "Any (OR)" when at least one must be true
4. **Threat Management**: Use threat conditions to create intelligent threat-aware rotations
5. **Aura Stacks**: Use `aura_stacks` to track stacking debuffs/buffs and react at specific stack counts
6. **Target Selection**: Use `enemy_primary_victim` to target whoever the boss is attacking (useful for adds on the tank)

## Version

Current version: 0.1
