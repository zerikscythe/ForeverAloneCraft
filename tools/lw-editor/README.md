# LivingWorld Bot Editor

A standalone Python/Tkinter GUI for managing LivingWorld bot combat profiles, character data,
accounts, reputations, gear, talents, and professions against an AzerothCore WotLK (3.3.5a) database.

---

## Requirements

```bash
pip install -r requirements.txt
```

| Package | Required | Purpose |
|---|---|---|
| `mysql-connector-python` | ✅ Yes | All database access |
| `sshtunnel` | Optional | SSH tunnel to remote servers |
| `paramiko` | Optional | SSH key authentication |

---

## Setup — First Run

### 1. Extract DBC data (one time)

Copy these files from your WoW 3.3.5a client's `DBFilesClient/` into `var/extractors/dbc/`:

```
Spell.dbc
SkillLineAbility.dbc
SkillLine.dbc
Faction.dbc
Talent.dbc
TalentTab.dbc
SpellItemEnchantment.dbc
GemProperties.dbc
```

Then run:

```bash
cd tools/lw-editor
python extract_dbc_data.py
```

This generates the following data files used by the editor at runtime:

| File | Contents |
|---|---|
| `spell_names.json` | 48 k spell ID → name mappings |
| `class_spells.json` | Per-class spell lists |
| `faction_names.json` | 87 player-facing factions with expansion grouping |
| `talent_data.json` | Talent trees for all 10 classes |
| `enchantment_data.json` | 2,655 item enchantments |
| `gem_properties.json` | 626 gem socket properties |
| `skill_line_abilities.json` | 3,556 profession recipes across all professions |

> Re-run `extract_dbc_data.py` any time you update DBC files.

### 2. Launch the editor

```bash
python lw_bot_editor.py
```

Connection settings are saved automatically to `config.ini`.

### 3. Validate your setup

```bash
python startup_validate.py
```

Runs a full pre-flight check: data files, module imports, tab instantiation, DB method
surface, and internal widget structure. Exits non-zero if any check fails.

---

## Connection Modes

### Direct Connection
Connect directly to a MySQL server on your network.  
Fill in **MySQL Host / Port / User / Pass** and click **Connect**.

### SSH Tunnel
Check **Enable SSH Tunnel** to connect through a jump host to a MySQL server on a private network.

- **SSH Host/Port/User/Pass** — credentials for the jump host
- **SSH Key File** — optional; path to an `id_rsa` / `.pem` file (leave blank to use password)
- **MySQL Host/Port/User/Pass** — database credentials *as seen from the jump host*

See [`docs/SSH_SETUP.md`](docs/SSH_SETUP.md) for full configuration examples.

---

## Tab Overview

### Bot Profiles / Default Profiles
Create and edit per-bot or server-wide default combat rotation profiles.

- Priority-based rotation entries (lower number = executes first)
- Interrupt entries
- Spell / Item actions with rank modes (`Best Known`, `Exact ID`, `Specific Rank`)
- Condition logic with `All (AND)` / `Any (OR)` grouping

See [Conditions Reference](#conditions-reference) below.

---

### Character Editor

The main character management screen. Select an **Account** then a **Character** from the left panel.

#### Summary tab
| Field | Editable | Notes |
|---|---|---|
| Name, GUID, Account, Class, Race | Read-only | |
| Online, Guild, Location, Created | Read-only | |
| Level, XP, Gold, Bank Slots | ✅ Editable | Click **💾 Save Basic Stats** |
| Stored Stats grid | Read-only | Health, Power, Attributes, Offensive, PvP stats |

#### Reputation tab
Four sub-tabs — one per expansion — matching the in-game reputation panel:

| Sub-tab | Contents |
|---|---|
| **Classic** | Vanilla factions (Stormwind, Ironforge, Argent Dawn, Steamwheedle…) |
| **TBC** | Honor Hold, Aldor, Scryers, Cenarion Expedition, etc. |
| **WotLK** | Argent Crusade, Kirin Tor, Wyrmrest Accord, Ebon Blade, etc. |
| **Other** | Factions not under the three expansion roots |

Columns: **Faction · Group · Raw Standing · Rank · Flags**  
Rank is color-coded: Exalted (green) → Hated (dark red).  
Profession-spec pseudo-factions (Blacksmithing-Armorsmithing, Engineering-Gnome, etc.) are filtered out.

#### Equipped Gear tab
Per-slot gear editor with item, enchant, and gem pickers.  
Hover over any item to see a WoW-style tooltip including:
- Weapon damage range, speed, and DPS
- Armor value
- Stats (+Strength, +Stamina, etc.)
- Built-in spell effects (Equip: / Use: / Chance on Hit:) with spell names resolved
- Socket bonus, requirements, and class restrictions

#### Inventory tab
Full bag + bank inventory listing.

#### Achievements tab
All earned achievements with dates.

#### Talents tab
Visual talent-tree view per specialisation tree (requires `talent_data.json`).  
Use **Add Point**, **Remove Point**, and **Reset Talents** buttons.

#### Professions tab
Recipe browser with sub-tabs per profession:

| Sub-tab | Always shown |
|---|---|
| First Aid | ✅ |
| Cooking | ✅ |
| Fishing | ✅ |
| *Crafting Prof 1* | Only if character has it |
| *Crafting Prof 2* | Only if character has it |

Each tab shows a recipe list: `✓ / —  |  Recipe Name  |  Min Skill`  
- Known recipes are shown in **green**
- Gathering professions (Herbalism, Mining, Skinning) show current level in the header bar
- Engineering auto-detects Gnomish vs. Goblin specialisation and groups spec recipes separately
- Counter shows `42 / 74 known` and turns green when complete

#### All data grids — click-to-sort
Every Treeview in the editor supports **click-to-sort** on any column:
- Click header once → sort **ascending** ▲
- Click again → sort **descending** ▼
- Numeric columns (standing values, levels, skill ranks, counts) sort as numbers automatically

---

### Accounts tab
Create, rename, set passwords for, and delete AzerothCore accounts.

---

## Session Cache

Character data is cached in memory so switching between characters you've already viewed
is instant (zero DB queries):

| Cache | Keyed by | Cleared when |
|---|---|---|
| Character data (summary, reps, inventory, achievements, talents, skills, known spells) | `guid` | Account switch |
| Item option lists (gear slot dropdowns) | `(class_id, slot_id, quality_id)` | Reconnect |
| Enchant option lists | `slot_id` | Reconnect |
| Gem option lists | `socket_color` | Reconnect |

Use **🔄 Refresh Character** to force a re-fetch from DB for the currently selected character.

---

## Conditions Reference

### Subject Keys
| Key | Refers to |
|---|---|
| `owner` | The player who owns the bot |
| `bot` | The bot itself |
| `target` | The bot's current target |
| `owner.target` | The owner's current target |

### Stat Keys

**Resources**
| Key | Type | Description |
|---|---|---|
| `hp_pct` | 0–100 | Health percentage |
| `mana_pct` | 0–100 | Mana percentage |
| `rage` | 0–100 | Rage |
| `energy` | 0–100 | Energy |
| `runic_power` | 0–100 | Runic Power |
| `combo_points` | 0–5 | Combo points |

**Position / State**
| Key | Type | Description |
|---|---|---|
| `distance` | yards | Distance to subject |
| `in_melee` | bool | In melee range |
| `is_moving` | bool | Subject is moving |
| `is_casting` | bool | Subject is casting |

**Auras**
| Key | Type | Description |
|---|---|---|
| `aura` | bool | Aura/buff/debuff present (by spell ID) |
| `aura_stacks` | 0–255 | Stack count of a specific aura |

**Threat**
| Key | Type | Description |
|---|---|---|
| `threat_pct` | 0–200+ | Threat as % of top threat holder |
| `is_aggro_holder` | bool | Subject currently holds aggro |

### Target Keys
| Key | Description |
|---|---|
| `enemy` / `enemy_primary` | Primary enemy target |
| `enemy_trash` | Trash mob attacking the owner |
| `enemy_primary_victim` | Current victim of the primary target |
| `self` | The bot itself |
| `owner` | The bot's owner |
| `lowest_hp_party` | Party member with lowest HP% |
| `focus` | Focus target (future use) |

### Operators
`==` `!=` `<` `<=` `>` `>=` `Has` `NotHas` `Exists`

---

## Rotation Examples

### Tank — Auto-Taunt when losing aggro
```
Priority 10 | bot.is_aggro_holder == False
Action: Taunt → enemy_primary
```

### DPS — Threat dump at 90%
```
Priority 5 | bot.threat_pct >= 90
Action: Feint → self
```

### DPS — Hold fire when near top
```
Priority 3 | bot.threat_pct >= 85  AND  owner.is_aggro_holder == False
Action: (wait)
```

### Healer — Smart heal
```
Priority 5 | lowest_hp_party.hp_pct < 60
Action: Flash Heal → lowest_hp_party
```

### Healer — Fade when aggro'd
```
Priority 8 | bot.is_aggro_holder == True
Action: Fade → self
```

---

## Database Tables

### acore_world
| Table | Purpose |
|---|---|
| `living_world_bot_combat_default_profile` | Default profile settings |
| `living_world_bot_combat_default_entry` | Default rotation entries |
| `living_world_bot_combat_default_action` | Actions per entry |
| `living_world_bot_combat_default_condition` | Conditions per entry |

### acore_characters
| Table | Purpose |
|---|---|
| `living_world_bot_combat_profile` | Per-bot profile settings |
| `living_world_bot_combat_entry` | Per-bot rotation entries |
| `living_world_bot_combat_action` | Per-bot actions |
| `living_world_bot_combat_condition` | Per-bot conditions |
| `character_reputation` | Faction standings (read + display) |
| `character_skills` | Profession skill levels |
| `character_spell` | Known spells / recipes |
| `character_talent` | Talent points |
| `character_inventory` | Equipped gear and inventory |
| `item_instance` | Per-item enchantment and gem data |

---

## Validation

Always run before reporting issues:

```bash
python startup_validate.py
```

Checks (all must pass before the editor is considered healthy):
1. All 7 required data JSON files present and parseable
2. All 11 `lw_editor.*` modules importable
3. All 5 tab classes instantiate under a real `Tk` root
4. `CharacterEditorTab` internal structure (13 attributes checked)
5. `ProfessionsPanel` recipe data loaded and all profession skill lines present
6. 20 `DBCtx` public methods present
7. 17 `helpers` module exports present
8. 19 `constants` module exports present
9. `SSH_TUNNEL_AVAILABLE` and `PARAMIKO_AVAILABLE` in `db` module

---

## Tips

1. **Re-run extraction** any time you add custom factions, spells, or enchants to the server DBC files
2. **Quality filter** on the gear picker adjusts which items appear in the dropdown — changing it rebuilds the item list for that slot only
3. **Reputation tab** profession entries (Blacksmithing-Armorsmithing etc.) are filtered out by the extractor — they'll never appear in the reputation panel
4. **Profession tab** only shows crafting professions the character actually has; gathering professions appear in the header summary line instead
5. **Sort any grid** by clicking its column header; click again to reverse
6. **SSH keys** are more reliable than passwords for long-running sessions — use a `.pem` or `id_rsa` file
