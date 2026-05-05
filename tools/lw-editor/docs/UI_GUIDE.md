# UI Guide

## Top Bar — Connection

```
┌─ Direct Connection ──────────────────────────────────────────┐
│ MySQL Host: [192.168.0.93]  Port: [3306]  User: [acore]     │
│ Pass: [****]  [Connect]  ● Connected                         │
└──────────────────────────────────────────────────────────────┘

┌─ SSH Tunnel (for remote/private networks) ───────────────────┐
│ ☑ Enable SSH Tunnel                                          │
│ SSH:   Host: [192.168.0.50]  Port: [22]                      │
│        User: [john]  Pass: [****]                            │
│        Key File: [C:\Users\...\id_rsa]  [Browse...]          │
│ MySQL: Host: [192.168.0.93]  Port: [3306]                    │
│        User: [acore]  Pass: [****]                           │
└──────────────────────────────────────────────────────────────┘
```

When **SSH Tunnel is disabled**: Direct Connection fields are active.  
When **SSH Tunnel is enabled**: SSH + MySQL fields are active; Direct fields are grayed out.

Status messages:

| Message | Meaning |
|---|---|
| `Not connected` | No connection |
| `● Connected` | Direct TCP connection active |
| `● Connected (via SSH tunnel)` | SSH tunnel active |
| `✗ <error>` | Connection failed |

For full SSH configuration see [`SSH_SETUP.md`](SSH_SETUP.md).

---

## Main Tabs

```
[ Bot Profiles | Default Profiles | Character Editor | Accounts ]
```

---

## Character Editor Layout

```
┌─ Left panel ──────────────────┐  ┌─ Right panel ──────────────────────────────┐
│ Account: [dropdown]           │  │ [🔄 Refresh Character]                     │
│                               │  │ ┌──────────────────────────────────────────┐│
│ Characters:                   │  │ │ Summary │ Reputation │ Equipped Gear │   ││
│  Name      Lvl  Class         │  │ │ Inventory │ Achievements │ Talents │     ││
│  BotWarrior 80  Warrior       │  │ │ Professions                              ││
│  BotPaladin 80  Paladin       │  │ └──────────────────────────────────────────┘│
│  ...                          │  │   [tab content]                             │
└───────────────────────────────┘  └────────────────────────────────────────────┘
```

### Summary tab

Displays character identity (read-only) and editable core stats:

```
Name        [BotWarrior     ]  GUID  [1234 ]  Account [5   ]
Class       [Warrior        ]  Race  [Human ]  Online  [No  ]
Level       [80  ]  XP   [0          ]  Bank Slots  [6  ]
Gold (copper) [1234567  ]  Gold  [123g 45s 67c]  Guild Bank  [0g]
Guild       [MyGuild        ]  Location  [Map 0 / Zone 0]  Created [...]
─────────────────────────────────────────────────────────────────
Health:  12450 / 12450    Power:   8200 / 8200    Armor:  14320
Strength: 412   Agility: 198   Stamina: 580   Intellect: 95   Spirit: 88
Attack Power: 2840   Ranged AP: 0   Spell Power: 210   Resilience: 380
Arena Points: 0   Honor Points: 14820   Total Kills: 2341
─────────────────────────────────────────────────────────────────
[💾 Save Basic Stats]
```

### Reputation tab

Four sub-tabs matching the in-game Reputation panel:

```
[ Classic | TBC | WotLK | Other ]
```

Columns: **Faction · Group · Raw Standing · Rank · Flags**

- **Rank** is color-coded:
  - Exalted (green) → Revered (blue) → Honored (teal) → Friendly (olive) → Neutral (gray) → Unfriendly (orange) → Hostile (red) → Hated (dark red)
- Profession-spec pseudo-factions are excluded (filtered at extraction time)
- Click any column header to sort ▲ / ▼

### Equipped Gear tab

19 equipment slots each with:
- **Item picker** — dropdown filtered by class + slot + quality level
- **Quality filter** — changes available items without affecting what's equipped
- **Enchant picker** — slot-appropriate enchants only
- **Gem pickers** — up to 3 gem sockets (hidden when slot has no sockets)
- **Tooltip on hover** — WoW-style tooltip showing:
  - Weapon: damage range, secondary damage (e.g. +16–30 Nature), Speed, DPS
  - Armor value
  - Stats (+Strength, +Stamina, etc.)
  - Built-in effects: `Equip:`, `Use:`, `Chance on Hit:` with resolved spell names
  - Sockets and socket bonus
  - Requirements (level, class)

Click **Apply Gear Changes** to write all modified slots to the DB.

### Inventory tab

Full bag + bank listing.  
Columns: Bag · Slot · Item ID · Item Name · Count · Item GUID  
Click any column header to sort.

### Achievements tab

All earned achievements.  
Columns: Achievement ID · Name · Date Earned  
Click any column header to sort.

### Talents tab

Visual per-tree talent display (requires `talent_data.json`).  
Shows current spend per tree and total available points.  
Buttons: **Add Point**, **Remove Point**, **Reset Talents**.

### Professions tab

```
[ First Aid | Cooking | Fishing | Engineering (Gnomish) | Blacksmithing ]
                                  ↑ dynamic: only shown if character has it
```

Header line shows gathering professions: `Herbalism 450  •  Mining 375`

Each profession tab:
```
Skill: 375 / 450           23 / 74 known
─────────────────────────────────────────────────────
✓   Recipe: Riding Crop                    225
✓   Recipe: Gnomish Battle Chicken         230
—   Recipe: Gnomish Net-o-Matic Projector  235
...
── Gnomish Engineering ──
✓   Recipe: Gnomish Rocket Boots           200
...
```

- ✓ = character knows this recipe (green)
- — = not known (gray)
- Specialisation recipes appear in a labeled section below shared recipes
- Click column header to sort by Recipe Name or Min Skill

---

## Sorting Data Grids

Every Treeview / data grid in the editor is sortable:

| Action | Result |
|---|---|
| Click column header | Sort ascending ▲ |
| Click same header again | Sort descending ▼ |
| Click a different header | New sort column, ▲ starts |

Numeric columns (level, standing, skill rank, count, etc.) sort as numbers.
Text columns sort case-insensitively.

---

## Session Cache

The editor caches character data in memory to eliminate repeated DB round-trips:

| Data | Cached per | Reset on |
|---|---|---|
| Summary, inventory, reps, achievements, talents, skills, known spells | Character GUID | Account switch |
| Item dropdown options | (class, slot, quality) | Reconnect / new connection |
| Enchant dropdown options | Slot ID | Reconnect |
| Gem dropdown options | Socket color | Reconnect |

**🔄 Refresh Character** bypasses the cache and re-fetches everything from DB.

After a successful save:
- **Save Basic Stats** re-fetches only `summary`
- **Apply Gear Changes** re-fetches only `inventory_rows`
- **Reset Talents** re-fetches only `talents`

---

## Keyboard Shortcuts

| Shortcut | Action |
|---|---|
| Click column header | Sort column |
| Click again | Reverse sort |
| `Tab` / `Shift+Tab` | Move between fields |


## New Improved UI Layout

The connection bar is now divided into two clear sections:

```
┌─────────────────────────────────────────────────────────────────────────┐
│ ┌─ Direct Connection ─────────────────────────────────────────────────┐ │
│ │ MySQL Host: [192.168.0.93] Port: [3306] User: [acore] Pass: [****] │ │
│ │ [Connect] ● Connected                                               │ │
│ └─────────────────────────────────────────────────────────────────────┘ │
│                                                                           │
│ ┌─ SSH Tunnel (for remote/private networks) ──────────────────────────┐ │
│ │ ☑ Enable SSH Tunnel  → When enabled, connects through a jump host   │ │
│ │ SSH:  Host: [192.168.0.50]  Port: [22]  User: [john]  Pass: [****]  │ │
│ │ SSH:  Key File: [C:\Users\...\id_rsa           ] [Browse...]         │ │
│ │       (leave blank to use password)                                  │ │
│ │ MySQL: Host: [192.168.0.93] Port: [3306] User: [acore] Pass: [****] │ │
│ │        (MySQL server as seen from SSH host)                          │ │
│ └─────────────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────────────┘
```

## UI Behavior

### When SSH Tunnel is DISABLED (unchecked)
- **Direct Connection section**: ENABLED (you can edit all fields)
- **SSH Tunnel section**: GRAYED OUT (cannot edit)
- **Connect button**: Uses Direct Connection settings
- **What happens**: Editor connects directly to MySQL server

### When SSH Tunnel is ENABLED (checked)
- **Direct Connection section**: GRAYED OUT (auto-set to 127.0.0.1:3306)
- **SSH Tunnel section**: ENABLED (you can edit all fields)
- **Connect button**: Uses SSH Tunnel settings
- **What happens**: 
  1. Editor creates SSH tunnel to jump host
  2. Editor connects to MySQL through the tunnel

## Field Meanings

### Direct Connection Section
| Field | Description | Example |
|-------|-------------|---------|
| MySQL Host | IP/hostname of MySQL server | `192.168.0.93` |
| Port | MySQL port | `3306` |
| User | MySQL username | `acore` |
| Pass | MySQL password | `acore` |

**Use this when**: Database is directly accessible from your PC

### SSH Tunnel Section

#### SSH Credentials (Blue label)
| Field | Description | Example |
|-------|-------------|---------|
| Host | Jump host IP/hostname | `192.168.0.50` |
| Port | SSH port | `22` |
| User | SSH username | `john` |
| Pass | SSH password | `mypassword` |
| Key File | Path to SSH private key | `C:\Users\John\.ssh\id_rsa` |

**Use**: Password OR Key File (key is more secure)

#### MySQL Credentials (Green label)  
| Field | Description | Example |
|-------|-------------|---------|
| Host | DB server address *from jump host* | `192.168.0.93` or `127.0.0.1` |
| Port | MySQL port on remote | `3306` |
| User | MySQL username | `acore` |
| Pass | MySQL password | `acore` |

**Important**: "Host" here is the address the jump host uses to reach the database

## Common Configurations

### Config 1: Direct Connection (No SSH)
```
┌─ Direct Connection ────────────────────────┐
│ Host: 192.168.0.93  Port: 3306            │
│ User: acore         Pass: ****            │
└────────────────────────────────────────────┘

┌─ SSH Tunnel ───────────────────────────────┐
│ ☐ Enable SSH Tunnel (UNCHECKED)           │
│ (all fields grayed out)                    │
└────────────────────────────────────────────┘
```

### Config 2: SSH to Jump, MySQL on Different Server
```
┌─ Direct Connection ────────────────────────┐
│ (grayed out - auto-set to 127.0.0.1:3306) │
└────────────────────────────────────────────┘

┌─ SSH Tunnel ───────────────────────────────┐
│ ☑ Enable SSH Tunnel (CHECKED)             │
│ SSH:   Host: 192.168.0.50  Port: 22       │
│        User: john          Pass: ****      │
│ MySQL: Host: 192.168.0.93  Port: 3306     │
│        User: acore         Pass: ****      │
└────────────────────────────────────────────┘
```

### Config 3: SSH and MySQL on Same Server
```
┌─ Direct Connection ────────────────────────┐
│ (grayed out - auto-set to 127.0.0.1:3306) │
└────────────────────────────────────────────┘

┌─ SSH Tunnel ───────────────────────────────┐
│ ☑ Enable SSH Tunnel (CHECKED)             │
│ SSH:   Host: myserver.com  Port: 22       │
│        User: admin         Pass: ****      │
│        Key:  C:\...\id_rsa                 │
│ MySQL: Host: 127.0.0.1     Port: 3306     │
│        User: acore         Pass: ****      │
└────────────────────────────────────────────┘
```

## Color Coding

The labels use colors to help distinguish sections:
- **Blue "SSH:"** = SSH connection credentials (to jump host)
- **Green "MySQL:"** = MySQL database credentials (on remote server)
- **Gray text** = Helpful hints and explanations
- **Orange text** = Warnings (like missing package)

## Automatic Behavior

When you check "Enable SSH Tunnel":
1. Direct Connection fields become grayed out
2. SSH Tunnel fields become editable
3. Direct Connection Host auto-sets to `127.0.0.1`
4. Direct Connection Port auto-sets to `3306`
5. Direct Connection User/Pass sync from MySQL section

When you uncheck "Enable SSH Tunnel":
1. SSH Tunnel fields become grayed out
2. Direct Connection fields become editable
3. You can configure direct connection normally

## Connection Flow

### Direct Mode (SSH unchecked)
```
Your PC → MySQL Server
(Direct TCP connection on port 3306)
```

### SSH Tunnel Mode (SSH checked)
```
Your PC → SSH Tunnel → Jump Host → MySQL Server
        (encrypted)              (private network)
```

## Status Messages

| Message | Meaning |
|---------|---------|
| `Not connected` | No active connection |
| `● Connected` | Direct connection successful |
| `● Connected (via SSH tunnel)` | SSH tunnel connection successful |
| `✗ [error message]` | Connection failed (see error) |

## Tips

1. **Always fill in MySQL credentials** in the SSH section when using SSH tunnel
2. **Use SSH keys** instead of passwords for better security
3. **Test SSH first** before configuring the editor (use `ssh user@host`)
4. **Check "MySQL Host"** - it's the address from jump host's perspective, not yours
5. **Leave Key File blank** if using SSH password authentication

## Troubleshooting

| Problem | Solution |
|---------|----------|
| SSH fields are grayed out | Check the "Enable SSH Tunnel" checkbox |
| "SSH tunnel not available" | Install: `pip install sshtunnel paramiko<3.0` |
| Can't edit Direct Connection | SSH Tunnel is enabled - uncheck it |
| Connection works without SSH but not with SSH | Check MySQL Host is from jump host's view |
| "Access denied" with SSH enabled | Check MySQL User/Pass in SSH section (green labels) |
