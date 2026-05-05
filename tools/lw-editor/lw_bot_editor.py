#!/usr/bin/env python3
"""
LivingWorld Bot Editor  v0.1
─────────────────────────────────────────────────────────────────────────────
Standalone Python/Tkinter GUI for editing LivingWorld bot combat profiles
and managing bot accounts — no server recompile required.

Install deps once:
    pip install mysql-connector-python

Run:
    python lw_editor.py

Connection settings are saved to config.ini in the same folder.
─────────────────────────────────────────────────────────────────────────────
"""

import warnings
# Suppress cryptography deprecation warnings from paramiko (TripleDES)
warnings.filterwarnings('ignore', category=DeprecationWarning, module='paramiko')
warnings.filterwarnings('ignore', message='.*TripleDES.*')
warnings.filterwarnings('ignore', message='.*Blowfish.*')

import tkinter as tk
from tkinter import ttk, messagebox, simpledialog
import mysql.connector
from mysql.connector import Error as MySQLError
import configparser
import hashlib
import os
import pathlib
import secrets
import struct
from datetime import datetime

try:
    from sshtunnel import SSHTunnelForwarder
    SSH_TUNNEL_AVAILABLE = True
except ImportError:
    SSHTunnelForwarder = None
    SSH_TUNNEL_AVAILABLE = False

# ═══════════════════════════════════════════════════════════════════════════
#  CONSTANTS / ENUM MAPS
# ═══════════════════════════════════════════════════════════════════════════

VERSION     = "0.1"
CONFIG_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "config.ini")

# DB enum int → display string
CONSERVATION_MODES = {0: "Full Force", 1: "Conservative", 2: "JIT Casting"}
ACTION_TYPES       = {0: "Spell",      1: "Item"}
RANK_MODES         = {0: "Best Known", 1: "Exact Spell ID", 2: "Specific Rank"}
COND_LOGIC         = {0: "All (AND)",  1: "Any (OR)"}
COND_OPS           = {0: "==", 1: "!=", 2: "<", 3: "<=", 4: ">", 5: ">=",
                      6: "Has", 7: "NotHas", 8: "Exists"}
AOE_MODES          = {0: "Centroid",   1: "Feet"}

# Display string → DB enum int (inverse maps)
def _inv(d): return {v: k for k, v in d.items()}

CONSERVATION_INV = _inv(CONSERVATION_MODES)
ACTION_INV       = _inv(ACTION_TYPES)
RANK_INV         = _inv(RANK_MODES)
COND_LOGIC_INV   = _inv(COND_LOGIC)
COND_OPS_INV     = _inv(COND_OPS)
AOE_INV          = _inv(AOE_MODES)

# Dropdown option lists
CONSERVATION_OPTS = list(CONSERVATION_MODES.values())
ACTION_OPTS       = list(ACTION_TYPES.values())
RANK_OPTS         = list(RANK_MODES.values())
COND_LOGIC_OPTS   = list(COND_LOGIC.values())
COND_OPS_OPTS     = list(COND_OPS.values())
AOE_OPTS          = list(AOE_MODES.values())

TARGET_KEYS   = ["enemy", "enemy_primary", "enemy_trash", "enemy_primary_victim",
                 "self", "owner", "ally_lowest_hp", "lowest_hp_party", "focus"]
SUBJECT_KEYS  = ["owner", "bot", "target", "owner.target"]
STAT_KEYS     = ["hp_pct", "mana_pct", "rage", "energy", "runic_power",
                 "combo_points", "aura", "aura_stacks", "distance",
                 "threat_pct", "is_aggro_holder",
                 "in_melee", "is_casting", "is_moving", "target_hp_pct"]
BOOL_STAT_KEYS = {"in_melee", "is_casting", "is_moving", "is_aggro_holder"}

WOW_CLASSES = {
    1: "Warrior", 2: "Paladin", 3: "Hunter", 4: "Rogue",
    5: "Priest", 6: "Death Knight", 7: "Shaman", 8: "Mage",
    9: "Warlock", 11: "Druid",
}

CLASS_OPTS = [name for _, name in sorted(WOW_CLASSES.items())]
CLASS_NAME_TO_ID = {name: cid for cid, name in WOW_CLASSES.items()}
CLASS_SPEC_OPTS = {
    1: ["Arms", "Fury", "Protection"],
    2: ["Holy", "Protection", "Retribution"],
    3: ["BeastMastery", "Marksmanship", "Survival"],
    4: ["Assassination", "Combat", "Subtlety"],
    5: ["Discipline", "Holy", "Shadow"],
    6: ["Blood", "Frost", "Unholy"],
    7: ["Elemental", "Enhancement", "Restoration"],
    8: ["Arcane", "Fire", "Frost"],
    9: ["Affliction", "Demonology", "Destruction"],
    11: ["Balance", "Feral", "Restoration"],
}
ROLE_OPTS = ["DPS", "HEAL", "TANK", "OFF_TANK"]

WOW_RACES = {
    1: "Human", 2: "Orc", 3: "Dwarf", 4: "Night Elf", 5: "Undead",
    6: "Tauren", 7: "Gnome", 8: "Troll", 10: "Blood Elf", 11: "Draenei",
}

EQUIPMENT_SLOT_NAMES = {
    0: "Head",
    1: "Neck",
    2: "Shoulder",
    3: "Shirt",
    4: "Chest",
    5: "Waist",
    6: "Legs",
    7: "Feet",
    8: "Wrist",
    9: "Hands",
    10: "Finger 1",
    11: "Finger 2",
    12: "Trinket 1",
    13: "Trinket 2",
    14: "Back",
    15: "Main Hand",
    16: "Off Hand",
    17: "Ranged",
    18: "Tabard",
}

ITEM_ENCHANTMENT_SLOT_COUNT = 12
ITEM_ENCHANTMENT_OFFSET_COUNT = 3
PERM_ENCHANTMENT_SLOT = 0
SOCK_ENCHANTMENT_SLOT = 2
SOCK_ENCHANTMENT_SLOT_2 = 3
SOCK_ENCHANTMENT_SLOT_3 = 4

EQUIPMENT_SLOT_TO_INVENTORY_TYPES = {
    0: [1],            # Head
    1: [2],            # Neck
    2: [3],            # Shoulder
    3: [4],            # Shirt
    4: [5, 20],        # Chest / Robe
    5: [6],            # Waist
    6: [7],            # Legs
    7: [8],            # Feet
    8: [9],            # Wrist
    9: [10],           # Hands
    10: [11],          # Finger
    11: [11],          # Finger
    12: [12],          # Trinket
    13: [12],          # Trinket
    14: [16],          # Back
    15: [13, 17, 21],  # Main hand / weapon / two hand
    16: [14, 22, 23],  # Off hand / shield / holdable
    17: [15, 25, 26, 28],  # Ranged / thrown / ranged right / relic
    18: [19],          # Tabard
}

SOCKET_COLOR_NAMES = {
    0: "None",
    1: "Meta",
    2: "Red",
    4: "Yellow",
    8: "Blue",
    16: "Prismatic",
}

ARMOR_PROFICIENCY_SLOT_IDS = {0, 2, 4, 5, 6, 7, 8, 9}
CLASS_ARMOR_SUBCLASSES = {
    1: {1, 2, 3, 4},   # Warrior
    2: {1, 2, 3, 4},   # Paladin
    3: {1, 2, 3},      # Hunter
    4: {1, 2},         # Rogue
    5: {1},            # Priest
    6: {1, 2, 3, 4},   # Death Knight
    7: {1, 2, 3},      # Shaman
    8: {1},            # Mage
    9: {1},            # Warlock
    11: {1, 2},        # Druid
}

ITEM_QUALITY_OPTIONS = [
    ("Any", None),
    ("Poor / Gray", 0),
    ("Common / White", 1),
    ("Uncommon / Green", 2),
    ("Rare / Blue", 3),
    ("Epic / Purple", 4),
    ("Legendary / Orange", 5),
    ("Artifact / Light Yellow", 6),
    ("Heirloom / Gold", 7),
]
ITEM_QUALITY_LABEL_TO_ID = {label: quality_id for label, quality_id in ITEM_QUALITY_OPTIONS}
ITEM_QUALITY_LABELS = [label for label, _quality_id in ITEM_QUALITY_OPTIONS]

SLOT_ENCHANT_CATEGORY = {
    0: "armor",
    2: "armor",
    4: "armor",
    5: "armor",
    6: "armor",
    7: "armor",
    8: "armor",
    9: "armor",
    14: "cloak",
    15: "weapon",
    16: "offhand",
    17: "ranged",
}
ENCHANT_CATEGORY_KEYWORDS = {
    "armor": ["arcanum", "inscription", "thread", "reinforced", "stamina", "stats", "spirit", "resilience", "assault", "greater"],
    "cloak": ["cloak", "shadow armor", "speed", "wisdom", "mighty armor", "subtlety", "spell piercing"],
    "weapon": ["berserking", "mongoose", "executioner", "accuracy", "black magic", "spellpower", "icebreaker", "lifeward", "titanium weapon chain", "weapon chain", "slayer", "potency", "agility", "intellect", "spellsurge"],
    "offhand": ["shield", "defense", "intellect", "resilience", "greater stamina", "major stamina", "titanium plating"],
    "ranged": ["scope", "heartseeker", "sun scope", "diamond-cut", "sniper", "accuracy"],
}

# spell_template.SpellFamilyName value for each WoW class_id
CLASS_SPELL_FAMILY = {
    1: 2,   # Warrior
    2: 8,   # Paladin
    3: 7,   # Hunter
    4: 6,   # Rogue
    5: 4,   # Priest
    6: 10,  # Death Knight
    7: 9,   # Shaman
    8: 1,   # Mage
    9: 3,   # Warlock
    11: 5,  # Druid
}

# Lowercase class name → class_id (used to infer class from spec_key string)
SPEC_TO_CLASS = {name.lower(): cid for cid, name in WOW_CLASSES.items()}
SPEC_ALIAS_TO_CLASS = {
    "arms": 1,
    "fury": 1,
    "protection warrior": 1,
    "holy paladin": 2,
    "protection paladin": 2,
    "retribution": 2,
    "beastmastery": 3,
    "beast mastery": 3,
    "marksmanship": 3,
    "survival": 3,
    "assassination": 4,
    "combat": 4,
    "subtlety": 4,
    "discipline": 5,
    "holy priest": 5,
    "shadow": 5,
    "blood": 6,
    "frost death knight": 6,
    "unholy": 6,
    "elemental": 7,
    "enhancement": 7,
    "restoration shaman": 7,
    "arcane": 8,
    "fire": 8,
    "frost mage": 8,
    "affliction": 9,
    "demonology": 9,
    "destruction": 9,
    "balance": 11,
    "feral": 11,
    "restoration druid": 11,
}

ROLE_ALIASES = {
    "heal": "HEAL",
    "healer": "HEAL",
    "heals": "HEAL",
    "tank": "TANK",
    "off-tank": "OFF_TANK",
    "off_tank": "OFF_TANK",
    "offtank": "OFF_TANK",
    "dps": "DPS",
}


def _normalize_role(role_key: str) -> str:
    if not role_key:
        return ""
    text = role_key.strip()
    upper = text.upper()
    if upper in ROLE_OPTS:
        return upper
    return ROLE_ALIASES.get(text.lower(), upper)


SRP6_G = 7
SRP6_N = int("894B645E89E1535BBDAD5B8B290650530801B18EBFBF5E8FAB3C82872A3E9BB7", 16)


def _upper_only_latin(text: str) -> str:
    return (text or "").upper()


def _srp6_registration_data(username: str, password: str) -> tuple[bytes, bytes]:
    username_up = _upper_only_latin(username)
    password_up = _upper_only_latin(password)
    salt = secrets.token_bytes(32)
    inner = hashlib.sha1(f"{username_up}:{password_up}".encode("utf-8")).digest()
    x = int.from_bytes(hashlib.sha1(salt + inner).digest(), "big")
    verifier = pow(SRP6_G, x, SRP6_N).to_bytes(32, "big")
    return salt, verifier

# ═══════════════════════════════════════════════════════════════════════════
#  DATABASE LAYER
# ═══════════════════════════════════════════════════════════════════════════

class DBCtx:
    """Holds connections to all three AzerothCore databases."""

    def __init__(self):
        self.auth  = None
        self.world = None
        self.chars = None
        self._world_tables = None
        self._spell_name_cache = None
        self._spell_rank_cache = None
        self._class_spell_rows = None
        self._class_skillline_ids = None
        self._dbc_root = (pathlib.Path(__file__).resolve().parents[2] /
                          "var" / "extractors" / "dbc")
        self._ssh_tunnel = None
        self._dbc_warning_shown = False
        self._spell_data_warning_shown = False
        # JSON cache paths
        self._json_cache_dir = pathlib.Path(__file__).resolve().parent
        self._spell_names_json = self._json_cache_dir / "spell_names.json"
        self._class_spells_json = self._json_cache_dir / "class_spells.json"
        self._json_spell_cache = None
        self._json_class_spells_cache = None
        self._dbc_enchant_cache = None
        self._dbc_gemproperties_cache = None

    def connect(self, host: str, port: int, user: str, password: str,
                 ssh_enabled: bool = False, ssh_host: str = "", ssh_port: int = 22,
                 ssh_user: str = "", ssh_password: str = "", ssh_key_file: str = "",
                 db_host: str = "127.0.0.1", db_port: int = 3306):
        """Connect to MySQL, optionally through an SSH tunnel.

        Args:
            host: Direct MySQL host (or localhost if using SSH tunnel)
            port: Direct MySQL port (or local tunnel port if using SSH tunnel)
            user: MySQL username
            password: MySQL password
            ssh_enabled: Whether to use SSH tunnel
            ssh_host: SSH jump host address
            ssh_port: SSH port (usually 22)
            ssh_user: SSH username
            ssh_password: SSH password (if not using key)
            ssh_key_file: Path to SSH private key file
            db_host: Database server address as seen from SSH host
            db_port: Database port on the remote server
        """
        # Close any existing tunnel
        self._close_ssh_tunnel()

        actual_host = host
        actual_port = port

        # Set up SSH tunnel if enabled
        if ssh_enabled and SSH_TUNNEL_AVAILABLE:
            if not ssh_host or not ssh_user:
                raise ValueError("SSH host and user are required when SSH is enabled")

            ssh_auth = {}
            if ssh_key_file and os.path.exists(ssh_key_file):
                ssh_auth["ssh_private_key"] = ssh_key_file
                if ssh_password:  # Key passphrase
                    ssh_auth["ssh_private_key_password"] = ssh_password
            elif ssh_password:
                ssh_auth["ssh_password"] = ssh_password
            else:
                raise ValueError("SSH requires either password or key file")

            self._ssh_tunnel = SSHTunnelForwarder(
                (ssh_host, ssh_port),
                ssh_username=ssh_user,
                remote_bind_address=(db_host, db_port),
                **ssh_auth
            )
            self._ssh_tunnel.start()

            # Connect to localhost through the tunnel
            actual_host = "127.0.0.1"
            actual_port = self._ssh_tunnel.local_bind_port
        elif ssh_enabled and not SSH_TUNNEL_AVAILABLE:
            raise ImportError(
                "SSH tunnel requested but sshtunnel package not installed.\n"
                "Install it with: pip install sshtunnel")

        base = dict(host=actual_host, port=actual_port, user=user, password=password,
                    autocommit=True, charset="utf8mb4")
        self.auth  = mysql.connector.connect(**base, database="acore_auth")
        self.world = mysql.connector.connect(**base, database="acore_world")
        self.chars = mysql.connector.connect(**base, database="acore_characters")
        self._world_tables = None
        self._spell_rank_cache = None

    def _close_ssh_tunnel(self):
        """Close SSH tunnel if active."""
        if self._ssh_tunnel:
            try:
                self._ssh_tunnel.stop()
            except Exception:
                pass
            self._ssh_tunnel = None

    def disconnect(self):
        for c in (self.auth, self.world, self.chars):
            try:
                if c and c.is_connected():
                    c.close()
            except Exception:
                pass
        self.auth = self.world = self.chars = None
        self._world_tables = None
        self._close_ssh_tunnel()

    def ok(self) -> bool:
        return bool(self.auth and self.auth.is_connected())

    def q(self, conn, sql: str, params=()):
        """Run SELECT, return list-of-dicts."""
        cur = conn.cursor(dictionary=True)
        cur.execute(sql, params)
        rows = cur.fetchall()
        cur.close()
        return rows

    def run(self, conn, sql: str, params=()):
        """Run INSERT/UPDATE/DELETE, return lastrowid."""
        cur = conn.cursor()
        cur.execute(sql, params)
        lid = cur.lastrowid
        cur.close()
        return lid

    def _load_world_tables(self):
        if self._world_tables is not None or not self.world:
            return
        try:
            rows = self.q(self.world, "SHOW TABLES")
            self._world_tables = {next(iter(r.values())) for r in rows}
        except Exception:
            self._world_tables = set()

    def _has_world_table(self, table_name: str) -> bool:
        self._load_world_tables()
        return table_name in (self._world_tables or set())

    def _read_dbc_rows(self, file_name: str, expected_fields: int | None = None):
        path = self._dbc_root / file_name
        if not path.exists():
            if not self._dbc_warning_shown:
                import sys
                print(f"[lw-editor] DBC files not found in {self._dbc_root}", file=sys.stderr)
                print(f"[lw-editor] Spell names will be limited. You can enter spell IDs manually.", file=sys.stderr)
                print(f"[lw-editor] To extract DBC files, run the WoW client data extractors.", file=sys.stderr)
                self._dbc_warning_shown = True
            return None
        try:
            with path.open("rb") as f:
                magic, record_count, field_count, record_size, string_size = struct.unpack(
                    "<4s4I", f.read(20))
                if magic != b"WDBC":
                    return None
                if expected_fields and field_count != expected_fields:
                    return None
                records = f.read(record_count * record_size)
                string_block = f.read(string_size)
            return record_count, field_count, record_size, records, string_block
        except Exception:
            return None

    def _dbc_string(self, string_block: bytes, offset: int) -> str:
        if not offset:
            return ""
        end = string_block.find(b"\0", offset)
        if end == -1:
            end = len(string_block)
        return string_block[offset:end].decode("utf-8", errors="ignore")

    def _load_dbc_enchant_cache(self):
        if self._dbc_enchant_cache is not None:
            return self._dbc_enchant_cache

        cache = []
        parsed = self._read_dbc_rows("SpellItemEnchantment.dbc", expected_fields=38)
        if parsed:
            record_count, field_count, record_size, records, strings = parsed
            for i in range(record_count):
                row = struct.unpack_from(f"<{field_count}I", records, i * record_size)
                enchant_id = int(row[0] or 0)
                name = self._dbc_string(strings, row[14]).strip()
                min_level = int(row[37] or 0)
                if enchant_id and name:
                    cache.append({
                        "ID": enchant_id,
                        "Name_Lang_enUS": name,
                        "MinLevel": min_level,
                    })

        self._dbc_enchant_cache = cache
        return self._dbc_enchant_cache

    def _load_dbc_gemproperties_cache(self):
        if self._dbc_gemproperties_cache is not None:
            return self._dbc_gemproperties_cache

        cache = {}
        parsed = self._read_dbc_rows("GemProperties.dbc", expected_fields=5)
        if parsed:
            record_count, field_count, record_size, records, _strings = parsed
            for i in range(record_count):
                row = struct.unpack_from(f"<{field_count}I", records, i * record_size)
                gemprop_id = int(row[0] or 0)
                enchant_id = int(row[1] or 0)
                socket_mask = int(row[4] or 0)
                if gemprop_id:
                    cache[gemprop_id] = {
                        "ID": gemprop_id,
                        "Enchant_Id": enchant_id,
                        "Type": socket_mask,
                    }

        self._dbc_gemproperties_cache = cache
        return self._dbc_gemproperties_cache

    def _search_item_enchantments_dbc(self, text: str, limit: int = 50) -> list:
        raw = (text or "").strip()
        rows = self._load_dbc_enchant_cache()
        if not rows:
            return []

        if raw and raw.isdigit():
            wanted = int(raw)
            return [row for row in rows if int(row.get("ID", 0)) == wanted][:limit]

        if raw:
            lowered = raw.lower()
            rows = [row for row in rows if lowered in (row.get("Name_Lang_enUS") or "").lower()]

        return sorted(rows, key=lambda row: (row.get("Name_Lang_enUS") or "").lower())[:limit]

    def _search_gem_items_dbc(self, text: str, socket_color: int = 0, limit: int = 50) -> list:
        if not self.ok():
            return []

        gemprops = self._load_dbc_gemproperties_cache()
        if not gemprops:
            return []

        raw = (text or "").strip()
        sql = (
            "SELECT entry, name, GemProperties "
            "FROM item_template "
            "WHERE GemProperties > 0 "
        )
        params = []
        if raw:
            if raw.isdigit():
                sql += "AND entry=%s "
                params.append(int(raw))
            else:
                sql += "AND name LIKE %s "
                params.append(f"%{raw}%")
        sql += "ORDER BY name LIMIT %s"
        params.append(max(int(limit) * 5, 250))

        base_rows = self.q(self.world, sql, tuple(params))
        results = []
        for row in base_rows:
            gemprop_id = int(row.get("GemProperties", 0) or 0)
            gemprop = gemprops.get(gemprop_id)
            if not gemprop:
                continue
            gem_color = int(gemprop.get("Type", 0) or 0)
            if socket_color and (gem_color & int(socket_color)) == 0:
                continue
            results.append({
                "entry": row.get("entry"),
                "name": row.get("name"),
                "gem_color": gem_color,
                "Enchant_Id": int(gemprop.get("Enchant_Id", 0) or 0),
            })
            if len(results) >= int(limit):
                break
        return results

    def _lookup_gem_item_by_enchant_dbc(self, enchant_id: int):
        if not enchant_id or not self.ok():
            return None

        gemprops = self._load_dbc_gemproperties_cache()
        if not gemprops:
            return None

        gemprop_ids = [gid for gid, row in gemprops.items()
                       if int(row.get("Enchant_Id", 0) or 0) == int(enchant_id)]
        if not gemprop_ids:
            return None

        placeholders = ",".join(["%s"] * len(gemprop_ids))
        rows = self.q(self.world,
            f"SELECT entry, name, GemProperties FROM item_template "
            f"WHERE GemProperties IN ({placeholders}) "
            "ORDER BY entry LIMIT 1",
            tuple(gemprop_ids))
        if not rows:
            return None

        row = rows[0]
        gemprop = gemprops.get(int(row.get("GemProperties", 0) or 0), {})
        return {
            "entry": row.get("entry"),
            "name": row.get("name"),
            "gem_color": int(gemprop.get("Type", 0) or 0),
        }

    def _get_gem_enchant_id_dbc(self, gem_item_entry: int) -> int:
        if not gem_item_entry or not self.ok():
            return 0

        gemprops = self._load_dbc_gemproperties_cache()
        if not gemprops:
            return 0

        rows = self.q(self.world,
            "SELECT GemProperties FROM item_template WHERE entry=%s LIMIT 1",
            (int(gem_item_entry),))
        if not rows:
            return 0

        gemprop_id = int(rows[0].get("GemProperties", 0) or 0)
        gemprop = gemprops.get(gemprop_id)
        return int(gemprop.get("Enchant_Id", 0) or 0) if gemprop else 0

    def _load_json_spell_cache(self):
        """Load spell names from JSON cache file if available."""
        if self._json_spell_cache is not None:
            return self._json_spell_cache

        if not self._spell_names_json.exists():
            self._json_spell_cache = {}
            return self._json_spell_cache

        try:
            import json
            with self._spell_names_json.open("r", encoding="utf-8") as f:
                data = json.load(f)
                # Convert string keys to int keys
                self._json_spell_cache = {int(k): v for k, v in data.items()}
            import sys
            print(f"[lw-editor] Loaded {len(self._json_spell_cache):,} spells from {self._spell_names_json.name}", file=sys.stderr)
            return self._json_spell_cache
        except Exception as exc:
            import sys
            print(f"[lw-editor] Failed to load JSON cache: {exc}", file=sys.stderr)
            self._json_spell_cache = {}
            return self._json_spell_cache

    def _load_json_class_spells(self, class_id: int):
        """Load class spell list from JSON cache file if available."""
        if self._json_class_spells_cache is None:
            if not self._class_spells_json.exists():
                self._json_class_spells_cache = {}
            else:
                try:
                    import json
                    with self._class_spells_json.open("r", encoding="utf-8") as f:
                        self._json_class_spells_cache = json.load(f)
                except Exception:
                    self._json_class_spells_cache = {}

        # Map class_id to class name
        class_name = WOW_CLASSES.get(class_id, f"Class{class_id}")
        return self._json_class_spells_cache.get(class_name, [])

    def _load_spell_name_cache(self):
        if self._spell_name_cache is not None:
            return self._spell_name_cache
        cache = {}
        try:
            parsed = self._read_dbc_rows("Spell.dbc", expected_fields=234)
            if parsed:
                record_count, field_count, record_size, records, strings = parsed
                name_idx = 136
                for i in range(record_count):
                    row = struct.unpack_from(f"<{field_count}I", records, i * record_size)
                    spell_id = row[0]
                    name = self._dbc_string(strings, row[name_idx]).strip()
                    if name:
                        cache[spell_id] = name
        except Exception:
            cache = {}
        self._spell_name_cache = cache
        return cache

    def _load_spell_rank_cache(self):
        if self._spell_rank_cache is not None:
            return self._spell_rank_cache
        rank_map = {}
        if self.ok():
            try:
                rows = self.q(self.world,
                              "SELECT first_spell_id, spell_id FROM spell_ranks")
                rank_map = {int(r["spell_id"]): int(r["first_spell_id"]) for r in rows}
            except Exception:
                rank_map = {}
        self._spell_rank_cache = rank_map
        return rank_map

    def _load_class_spell_rows(self):
        if self._class_spell_rows is not None:
            return self._class_spell_rows
        rows = []
        try:
            parsed = self._read_dbc_rows("SkillLineAbility.dbc", expected_fields=14)
            if parsed:
                record_count, field_count, record_size, records, _strings = parsed
                for i in range(record_count):
                    row = struct.unpack_from(f"<{field_count}I", records, i * record_size)
                    rows.append({
                        "skillline": row[1],
                        "spell": row[2],
                        "classmask": row[4],
                        "excludeclass": row[6],
                    })
        except Exception:
            rows = []
        self._class_spell_rows = rows
        return rows

    def _load_class_skillline_ids(self):
        if self._class_skillline_ids is not None:
            return self._class_skillline_ids
        ids = set()
        try:
            parsed = self._read_dbc_rows("SkillLine.dbc")
            if parsed:
                record_count, field_count, record_size, records, _strings = parsed
                for i in range(record_count):
                    row = struct.unpack_from(f"<{field_count}I", records, i * record_size)
                    if row[1] == 7:  # SKILL_CATEGORY_CLASS
                        ids.add(int(row[0]))
        except Exception:
            ids = set()
        self._class_skillline_ids = ids
        return ids

    def _is_noise_spell_name(self, name: str) -> bool:
        if not name:
            return True
        lowered = name.lower()
        return (lowered.startswith("zzold") or
                lowered.endswith("(dnd)") or
                "test" in lowered)

    # ── Spell name lookup ────────────────────────────────────────────────────

    def spell_name(self, spell_id: int) -> str:
        """Try to resolve a human-readable spell name."""
        if not spell_id:
            return ""
        try:
            sid = int(spell_id)
        except (TypeError, ValueError):
            return ""

        # First check JSON cache
        json_cache = self._load_json_spell_cache()
        if sid in json_cache:
            return json_cache[sid]

        # Then check DBC cache
        spell_names = self._load_spell_name_cache()
        if sid in spell_names:
            return spell_names[sid]

        # Try database lookup - spell_dbc is standard AzerothCore table
        if not self.ok():
            return ""

        try:
            if self._has_world_table("spell_dbc"):
                rows = self.q(self.world,
                    "SELECT Name_Lang_enUS FROM spell_dbc WHERE ID=%s LIMIT 1",
                    (sid,))
                if rows:
                    name = rows[0].get("Name_Lang_enUS", "") or ""
                    if name:
                        return name
        except Exception:
            pass

        return ""

    def load_class_spells(self, class_id: int) -> list:
        """Return [{"id": int, "display": str}] for rank-1 spells of the class."""
        if not class_id:
            return []

        # Try JSON cache first (fastest and most convenient)
        json_spells = self._load_json_class_spells(class_id)
        if json_spells:
            result = [
                {"id": spell["id"], "display": f"{spell['name']}  [{spell['id']}]"}
                for spell in json_spells
            ]
            if not self._spell_data_warning_shown and result:
                import sys
                class_name = WOW_CLASSES.get(class_id, f"Class{class_id}")
                print(f"[lw-editor] Loaded {len(result):,} {class_name} spells from JSON cache", file=sys.stderr)
                self._spell_data_warning_shown = True
            return result

        class_rows = self._load_class_spell_rows()
        class_skilllines = self._load_class_skillline_ids()
        spell_names = self._load_spell_name_cache()
        rank_map = self._load_spell_rank_cache()
        mask = 1 << (int(class_id) - 1)

        # Try DBC-based loading (most reliable and properly filtered)
        if class_rows and spell_names:
            choices = {}
            for row in class_rows:
                if class_skilllines and row["skillline"] not in class_skilllines:
                    continue
                if not (row["classmask"] & mask):
                    continue
                if row["excludeclass"] & mask:
                    continue
                spell_id = int(row["spell"])
                base_id = int(rank_map.get(spell_id, spell_id))
                name = spell_names.get(base_id) or spell_names.get(spell_id) or ""
                if self._is_noise_spell_name(name):
                    continue
                prev = choices.get(name)
                if prev is None or base_id < prev:
                    choices[name] = base_id
            if choices:
                return [{"id": sid, "display": f"{name}  [{sid}]"}
                        for name, sid in sorted(choices.items(),
                                                key=lambda x: x[0].lower())]

        # Fallback: Use spell_dbc with DBC-based class filtering
        if self.ok() and self._has_world_table("spell_dbc") and class_rows:
            try:
                # Get list of spell IDs that are valid for this class from DBC
                valid_spell_ids = set()
                for row in class_rows:
                    if class_skilllines and row["skillline"] not in class_skilllines:
                        continue
                    if not (row["classmask"] & mask):
                        continue
                    if row["excludeclass"] & mask:
                        continue
                    spell_id = int(row["spell"])
                    base_id = int(rank_map.get(spell_id, spell_id))
                    valid_spell_ids.add(base_id)

                if valid_spell_ids:
                    # Query spell_dbc but only for spell IDs we know are valid for this class
                    # Build IN clause with spell IDs (limit to prevent too large query)
                    id_list = list(valid_spell_ids)[:3000]
                    placeholders = ','.join(['%s'] * len(id_list))

                    rows = self.q(self.world,
                        f"SELECT ID, Name_Lang_enUS FROM spell_dbc "
                        f"WHERE ID IN ({placeholders}) "
                        f"AND Name_Lang_enUS IS NOT NULL AND Name_Lang_enUS != '' "
                        f"ORDER BY Name_Lang_enUS",
                        tuple(id_list))

                    if rows:
                        filtered = [
                            {"id": r["ID"], "display": f"{r['Name_Lang_enUS']}  [{r['ID']}]"}
                            for r in rows
                            if not self._is_noise_spell_name(r["Name_Lang_enUS"])
                        ]

                        if filtered:
                            if not self._spell_data_warning_shown:
                                import sys
                                print(f"[lw-editor] Loaded {len(filtered)} spells for class {class_id} from spell_dbc", file=sys.stderr)
                                self._spell_data_warning_shown = True
                            return filtered
            except Exception as exc:
                # If spell_dbc query fails, fall through
                import sys
                print(f"[lw-editor] spell_dbc query failed: {exc}", file=sys.stderr)

        # Show helpful message once if no spell data sources are available
        if not self._spell_data_warning_shown:
            import sys
            print(f"[lw-editor] No spell data available for class {class_id}", file=sys.stderr)
            print(f"[lw-editor] Quick setup: Run 'python extract_dbc_data.py' to create JSON cache", file=sys.stderr)
            print(f"[lw-editor] Or extract DBC files to var/extractors/dbc/", file=sys.stderr)
            print(f"[lw-editor] You can still enter spell IDs manually (find IDs on wowhead.com)", file=sys.stderr)
            self._spell_data_warning_shown = True

        # Return empty list - user can still type spell IDs manually
        return []

    def search_items(self, name: str) -> list:
        """Search item_template by name (case-insensitive LIKE). Returns up to 20 hits."""
        if not self.ok() or not name.strip():
            return []
        try:
            rows = self.q(self.world,
                "SELECT entry, name FROM item_template WHERE name LIKE %s LIMIT 20",
                (f"%{name.strip()}%",))
            return [{"id": r["entry"],
                     "display": f"{r['name']}  [{r['entry']}]"}
                    for r in rows]
        except Exception as exc:
            import sys
            print(f"[lw-editor] search_items failed: {exc}", file=sys.stderr)
            return []

    def item_name(self, item_id: int) -> str:
        """Reverse-lookup an item name by entry ID."""
        if not item_id or not self.ok():
            return ""
        try:
            rows = self.q(self.world,
                "SELECT name FROM item_template WHERE entry=%s LIMIT 1", (item_id,))
            if rows:
                return rows[0].get("name") or ""
        except Exception:
            pass
        return ""

    def get_item_max_durability(self, item_id: int) -> int:
        if not item_id or not self.ok():
            return 0
        try:
            rows = self.q(self.world,
                "SELECT MaxDurability FROM item_template WHERE entry=%s LIMIT 1",
                (item_id,))
            if rows:
                return int(rows[0].get("MaxDurability") or 0)
        except Exception:
            pass
        return 0

    def search_equippable_items_for_slot(self, slot_id: int, text: str,
                                         class_id: int | None = None,
                                         quality: int | None = None,
                                         limit: int = 50) -> list:
        if not self.ok():
            return []
        raw = (text or "").strip()

        inv_types = EQUIPMENT_SLOT_TO_INVENTORY_TYPES.get(slot_id, [])
        params = []
        sql = (
            "SELECT entry, name, Quality, InventoryType, MaxDurability, "
            "`class` AS item_class, subclass AS item_subclass, AllowableClass AS allowable_class "
            "FROM item_template WHERE InventoryType > 0 "
        )

        if inv_types:
            placeholders = ",".join(["%s"] * len(inv_types))
            sql += f"AND InventoryType IN ({placeholders}) "
            params.extend(inv_types)

        if class_id:
            class_mask = 1 << (int(class_id) - 1)
            sql += "AND (AllowableClass = -1 OR (AllowableClass & %s) <> 0) "
            params.append(class_mask)

        if quality is not None:
            sql += "AND Quality=%s "
            params.append(int(quality))

        if raw:
            if raw.isdigit():
                sql += "AND entry=%s "
                params.append(int(raw))
            else:
                sql += "AND name LIKE %s "
                params.append(f"%{raw}%")

        sql += "ORDER BY name LIMIT %s"
        params.append(int(limit))

        rows = self.q(self.world, sql, tuple(params))
        if not class_id:
            return [r for r in rows if not _is_noise_item_name(r.get("name", ""))]

        allowed_armor = CLASS_ARMOR_SUBCLASSES.get(int(class_id), {1, 2, 3, 4})
        filtered = []
        for row in rows:
            if _is_noise_item_name(row.get("name", "")):
                continue
            if slot_id in ARMOR_PROFICIENCY_SLOT_IDS and int(row.get("item_class", 0) or 0) == 4:
                subclass = int(row.get("item_subclass", 0) or 0)
                if subclass in {1, 2, 3, 4} and subclass not in allowed_armor:
                    continue
            filtered.append(row)
        return filtered

    def load_valid_equippable_items_for_slot(self, class_id: int | None, slot_id: int,
                                             quality: int | None = None, limit: int = 250) -> list:
        return self.search_equippable_items_for_slot(slot_id, "", class_id=class_id, quality=quality, limit=limit)

    def search_item_enchantments(self, text: str, slot_id: int | None = None, limit: int = 50) -> list:
        if not self.ok():
            return []
        raw = (text or "").strip()

        rows = []
        try:
            if raw and raw.isdigit():
                rows = self.q(self.world,
                    "SELECT ID, Name_Lang_enUS, MinLevel "
                    "FROM spellitemenchantment_dbc "
                    "WHERE ID=%s LIMIT %s",
                    (int(raw), int(limit)))
            else:
                params = []
                sql = (
                    "SELECT ID, Name_Lang_enUS, MinLevel "
                    "FROM spellitemenchantment_dbc "
                    "WHERE Name_Lang_enUS IS NOT NULL AND Name_Lang_enUS != '' "
                )
                if raw:
                    sql += "AND Name_Lang_enUS LIKE %s "
                    params.append(f"%{raw}%")
                sql += "ORDER BY Name_Lang_enUS LIMIT %s"
                params.append(int(limit))
                rows = self.q(self.world, sql, tuple(params))
        except Exception:
            rows = []

        if not rows:
            rows = self._search_item_enchantments_dbc(raw, limit=limit)

        if slot_id is None:
            return rows
        return filter_enchant_rows_for_slot(slot_id, rows)

    def enchant_name(self, enchant_id: int) -> str:
        if not enchant_id:
            return ""
        if self.ok():
            try:
                rows = self.q(self.world,
                    "SELECT Name_Lang_enUS "
                    "FROM spellitemenchantment_dbc "
                    "WHERE ID=%s LIMIT 1",
                    (enchant_id,))
                if rows:
                    return rows[0].get("Name_Lang_enUS") or ""
            except Exception:
                pass

        rows = self._search_item_enchantments_dbc(str(int(enchant_id)), limit=1)
        return rows[0].get("Name_Lang_enUS") or "" if rows else ""

    def search_gem_items(self, text: str, socket_color: int = 0) -> list:
        if not self.ok():
            return []
        raw = (text or "").strip()

        rows = []
        try:
            sql = (
                "SELECT it.entry, it.name, gp.Type AS gem_color, gp.Enchant_Id "
                "FROM item_template it "
                "JOIN gemproperties_dbc gp ON gp.ID = it.GemProperties "
                "WHERE it.GemProperties > 0 "
            )
            params = []

            if socket_color:
                sql += "AND (gp.Type & %s) <> 0 "
                params.append(int(socket_color))

            if raw:
                if raw.isdigit():
                    sql += "AND it.entry=%s "
                    params.append(int(raw))
                else:
                    sql += "AND it.name LIKE %s "
                    params.append(f"%{raw}%")

            sql += "ORDER BY it.name LIMIT 50"
            rows = self.q(self.world, sql, tuple(params))
        except Exception:
            rows = []

        if not rows:
            rows = self._search_gem_items_dbc(raw, socket_color=socket_color, limit=50)
        return rows

    def lookup_gem_item_by_enchant(self, enchant_id: int):
        if not enchant_id or not self.ok():
            return None
        rows = []
        try:
            rows = self.q(self.world,
                "SELECT it.entry, it.name, gp.Type AS gem_color "
                "FROM item_template it "
                "JOIN gemproperties_dbc gp ON gp.ID = it.GemProperties "
                "WHERE gp.Enchant_Id=%s "
                "LIMIT 1",
                (enchant_id,))
        except Exception:
            rows = []
        return rows[0] if rows else self._lookup_gem_item_by_enchant_dbc(enchant_id)

    def get_gem_enchant_id(self, gem_item_entry: int) -> int:
        if not gem_item_entry or not self.ok():
            return 0
        rows = []
        try:
            rows = self.q(self.world,
                "SELECT gp.Enchant_Id "
                "FROM item_template it "
                "JOIN gemproperties_dbc gp ON gp.ID = it.GemProperties "
                "WHERE it.entry=%s "
                "LIMIT 1",
                (gem_item_entry,))
        except Exception:
            rows = []
        if rows:
            return int(rows[0].get("Enchant_Id") or 0)
        return self._get_gem_enchant_id_dbc(gem_item_entry)

    def update_item_instance_equipment(self, item_guid: int, item_entry: int, enchantments: str, durability: int):
        self.run(self.chars,
            "UPDATE item_instance "
            "SET itemEntry=%s, enchantments=%s, durability=%s, randomPropertyId=0 "
            "WHERE guid=%s",
            (item_entry, enchantments, durability, item_guid))

    def next_item_instance_guid(self) -> int:
        rows = self.q(self.chars, "SELECT COALESCE(MAX(guid), 0) + 1 AS next_guid FROM item_instance")
        return int(rows[0].get("next_guid", 1) or 1) if rows else 1

    def create_equipped_item_for_character(self, character_guid: int, slot_id: int, item_entry: int) -> int:
        item_guid = self.next_item_instance_guid()
        enchantments = build_item_enchantments([0] * (ITEM_ENCHANTMENT_SLOT_COUNT * ITEM_ENCHANTMENT_OFFSET_COUNT))
        durability = self.get_item_max_durability(item_entry)

        self.run(self.chars,
            "INSERT INTO item_instance "
            "(guid, itemEntry, owner_guid, creatorGuid, giftCreatorGuid, count, duration, charges, "
            "flags, enchantments, randomPropertyId, durability, playedTime, text) "
            "VALUES (%s,%s,%s,0,0,1,0,'',0,%s,0,%s,0,'')",
            (item_guid, item_entry, character_guid, enchantments, durability))
        self.run(self.chars,
            "REPLACE INTO character_inventory (guid, bag, slot, item) VALUES (%s,0,%s,%s)",
            (character_guid, slot_id, item_guid))
        return item_guid

    # ── Default profiles (acore_world) ──────────────────────────────────────

    def load_default_profiles(self):
        return self.q(self.world,
            "SELECT * FROM living_world_bot_combat_default_profile "
            "ORDER BY spec_key, role_key")

    def upsert_default_profile(self, p: dict) -> int:
        if p.get("default_profile_id"):
            self.run(self.world,
                "UPDATE living_world_bot_combat_default_profile SET "
                "spec_key=%s, role_key=%s, display_name=%s, conservation_mode=%s, "
                "mana_low_water=%s, mana_high_water=%s, enable_down_rank=%s, "
                "down_rank_floor=%s, default_aoe_mode=%s, default_aoe_min_targets=%s, "
                "default_aoe_scan_radius=%s WHERE default_profile_id=%s",
                (p["spec_key"], p["role_key"], p["display_name"], p["conservation_mode"],
                 p["mana_low_water"], p["mana_high_water"], p["enable_down_rank"],
                 p["down_rank_floor"], p["default_aoe_mode"], p["default_aoe_min_targets"],
                 p["default_aoe_scan_radius"], p["default_profile_id"]))
            return p["default_profile_id"]
        return self.run(self.world,
            "INSERT INTO living_world_bot_combat_default_profile "
            "(spec_key, role_key, display_name, conservation_mode, mana_low_water, "
            "mana_high_water, enable_down_rank, down_rank_floor, default_aoe_mode, "
            "default_aoe_min_targets, default_aoe_scan_radius) VALUES "
            "(%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s)",
            (p["spec_key"], p["role_key"], p["display_name"], p["conservation_mode"],
             p["mana_low_water"], p["mana_high_water"], p["enable_down_rank"],
             p["down_rank_floor"], p["default_aoe_mode"], p["default_aoe_min_targets"],
             p["default_aoe_scan_radius"]))

    def delete_default_profile(self, pid: int):
        for e in self.q(self.world,
                "SELECT entry_id FROM living_world_bot_combat_default_entry "
                "WHERE default_profile_id=%s", (pid,)):
            self.delete_default_entry(e["entry_id"])
        self.run(self.world,
            "DELETE FROM living_world_bot_combat_default_profile "
            "WHERE default_profile_id=%s", (pid,))

    def load_default_entries(self, pid: int):
        return self.q(self.world,
            "SELECT * FROM living_world_bot_combat_default_entry "
            "WHERE default_profile_id=%s ORDER BY is_interrupt ASC, priority ASC",
            (pid,))

    def upsert_default_entry(self, e: dict, profile_id: int) -> int:
        if e.get("entry_id"):
            self.run(self.world,
                "UPDATE living_world_bot_combat_default_entry SET "
                "priority=%s, label=%s, is_interrupt=%s, breaks_current_cast=%s, "
                "enabled=%s, condition_logic=%s WHERE entry_id=%s",
                (e["priority"], e["label"], e["is_interrupt"], e["breaks_current_cast"],
                 e["enabled"], e["condition_logic"], e["entry_id"]))
            return e["entry_id"]
        return self.run(self.world,
            "INSERT INTO living_world_bot_combat_default_entry "
            "(default_profile_id, priority, label, is_interrupt, "
            "breaks_current_cast, enabled, condition_logic) VALUES (%s,%s,%s,%s,%s,%s,%s)",
            (profile_id, e["priority"], e["label"], e["is_interrupt"],
             e["breaks_current_cast"], e["enabled"], e["condition_logic"]))

    def delete_default_entry(self, eid: int):
        self.run(self.world,
            "DELETE FROM living_world_bot_combat_default_action WHERE entry_id=%s", (eid,))
        self.run(self.world,
            "DELETE FROM living_world_bot_combat_default_condition WHERE entry_id=%s", (eid,))
        self.run(self.world,
            "DELETE FROM living_world_bot_combat_default_entry WHERE entry_id=%s", (eid,))

    def load_default_actions(self, eid: int):
        return self.q(self.world,
            "SELECT * FROM living_world_bot_combat_default_action "
            "WHERE entry_id=%s ORDER BY slot", (eid,))

    def upsert_default_action(self, a: dict, entry_id: int):
        if a.get("action_id"):
            self.run(self.world,
                "UPDATE living_world_bot_combat_default_action SET "
                "action_type=%s, spell_base_id=%s, item_id=%s, rank_mode=%s, "
                "rank_value=%s, target_key=%s, aoe_mode=%s, aoe_min_targets=%s, "
                "aoe_radius=%s WHERE action_id=%s",
                (a["action_type"], a["spell_base_id"], a["item_id"], a["rank_mode"],
                 a["rank_value"], a["target_key"], a.get("aoe_mode"),
                 a.get("aoe_min_targets"), a.get("aoe_radius"), a["action_id"]))
        else:
            self.run(self.world,
                "INSERT INTO living_world_bot_combat_default_action "
                "(entry_id, slot, action_type, spell_base_id, item_id, rank_mode, "
                "rank_value, target_key) VALUES (%s,%s,%s,%s,%s,%s,%s,%s)",
                (entry_id, a["slot"], a["action_type"], a["spell_base_id"],
                 a["item_id"], a["rank_mode"], a["rank_value"], a["target_key"]))

    def delete_default_action(self, aid: int):
        self.run(self.world,
            "DELETE FROM living_world_bot_combat_default_action WHERE action_id=%s", (aid,))

    def load_default_conditions(self, eid: int):
        return self.q(self.world,
            "SELECT * FROM living_world_bot_combat_default_condition "
            "WHERE entry_id=%s ORDER BY sequence", (eid,))

    def upsert_default_condition(self, c: dict, entry_id: int):
        if c.get("condition_id"):
            self.run(self.world,
                "UPDATE living_world_bot_combat_default_condition SET "
                "sequence=%s, subject_key=%s, stat_key=%s, comparison=%s, "
                "numeric_value=%s, string_value=%s WHERE condition_id=%s",
                (c["sequence"], c["subject_key"], c["stat_key"], c["comparison"],
                 c["numeric_value"], c["string_value"], c["condition_id"]))
        else:
            self.run(self.world,
                "INSERT INTO living_world_bot_combat_default_condition "
                "(entry_id, sequence, subject_key, stat_key, comparison, "
                "numeric_value, string_value) VALUES (%s,%s,%s,%s,%s,%s,%s)",
                (entry_id, c["sequence"], c["subject_key"], c["stat_key"],
                 c["comparison"], c["numeric_value"], c["string_value"]))

    def delete_default_condition(self, cid: int):
        self.run(self.world,
            "DELETE FROM living_world_bot_combat_default_condition "
            "WHERE condition_id=%s", (cid,))

    # ── Per-bot profiles (acore_characters) ─────────────────────────────────

    def load_source_characters(self):
        """Player characters (excluding bot pool accounts)."""
        pool_ids = [r["account_id"] for r in
                    self.q(self.auth,
                           "SELECT account_id FROM living_world_bot_account_pool")]
        if pool_ids:
            ph = ",".join(["%s"] * len(pool_ids))
            return self.q(self.chars,
                f"SELECT guid, account, name, level, class FROM characters "
                f"WHERE account NOT IN ({ph}) ORDER BY account, name",
                tuple(pool_ids))
        return self.q(self.chars,
            "SELECT guid, account, name, level, class "
            "FROM characters ORDER BY account, name")

    def load_source_characters_for_account(self, account_id: int):
        return self.q(self.chars,
            "SELECT guid, account, name, level, class "
            "FROM characters WHERE account=%s ORDER BY name, guid",
            (account_id,))

    def load_player_accounts(self):
        return self.q(self.auth,
            "SELECT a.id AS account_id, a.username, COUNT(c.guid) AS char_count "
            "FROM account a "
            "LEFT JOIN acore_characters.characters c ON c.account = a.id "
            "GROUP BY a.id, a.username "
            "HAVING COUNT(c.guid) > 0 "
            "ORDER BY a.username")

    def load_bot_profiles(self, source_char_guid: int):
        return self.q(self.chars,
            "SELECT * FROM living_world_bot_combat_profile "
            "WHERE source_character_guid=%s ORDER BY slot",
            (source_char_guid,))

    def upsert_bot_profile(self, p: dict) -> int:
        if p.get("profile_id"):
            self.run(self.chars,
                "UPDATE living_world_bot_combat_profile SET "
                "slot=%s, profile_name=%s, guessed_spec_key=%s, guessed_role_key=%s, "
                "spec_override_key=%s, role_override_key=%s, conservation_mode=%s, "
                "mana_low_water=%s, mana_high_water=%s, enable_down_rank=%s, "
                "down_rank_floor=%s, default_aoe_mode=%s, default_aoe_min_targets=%s, "
                "default_aoe_scan_radius=%s WHERE profile_id=%s",
                (p["slot"], p["profile_name"], p["guessed_spec_key"], p["guessed_role_key"],
                 p.get("spec_override_key"), p.get("role_override_key"),
                 p["conservation_mode"], p["mana_low_water"], p["mana_high_water"],
                 p["enable_down_rank"], p["down_rank_floor"], p["default_aoe_mode"],
                 p["default_aoe_min_targets"], p["default_aoe_scan_radius"], p["profile_id"]))
            return p["profile_id"]
        return self.run(self.chars,
            "INSERT INTO living_world_bot_combat_profile "
            "(source_character_guid, owner_account_id, slot, profile_name, "
            "guessed_spec_key, guessed_role_key, conservation_mode, mana_low_water, "
            "mana_high_water, enable_down_rank, down_rank_floor, default_aoe_mode, "
            "default_aoe_min_targets, default_aoe_scan_radius) VALUES "
            "(%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s)",
            (p["source_character_guid"], p["owner_account_id"], p["slot"],
             p["profile_name"], p["guessed_spec_key"], p["guessed_role_key"],
             p["conservation_mode"], p["mana_low_water"], p["mana_high_water"],
             p["enable_down_rank"], p["down_rank_floor"], p["default_aoe_mode"],
             p["default_aoe_min_targets"], p["default_aoe_scan_radius"]))

    def delete_bot_profile(self, pid: int):
        for e in self.q(self.chars,
                "SELECT entry_id FROM living_world_bot_combat_profile_entry "
                "WHERE profile_id=%s", (pid,)):
            self.delete_bot_entry(e["entry_id"])
        self.run(self.chars,
            "DELETE FROM living_world_bot_combat_profile WHERE profile_id=%s", (pid,))

    def load_bot_entries(self, profile_id: int):
        return self.q(self.chars,
            "SELECT * FROM living_world_bot_combat_profile_entry "
            "WHERE profile_id=%s ORDER BY is_interrupt ASC, priority ASC",
            (profile_id,))

    def upsert_bot_entry(self, e: dict, profile_id: int) -> int:
        if e.get("entry_id"):
            self.run(self.chars,
                "UPDATE living_world_bot_combat_profile_entry SET "
                "priority=%s, label=%s, is_interrupt=%s, breaks_current_cast=%s, "
                "enabled=%s, condition_logic=%s WHERE entry_id=%s",
                (e["priority"], e["label"], e["is_interrupt"], e["breaks_current_cast"],
                 e["enabled"], e["condition_logic"], e["entry_id"]))
            return e["entry_id"]
        return self.run(self.chars,
            "INSERT INTO living_world_bot_combat_profile_entry "
            "(profile_id, priority, label, is_interrupt, breaks_current_cast, "
            "enabled, condition_logic) VALUES (%s,%s,%s,%s,%s,%s,%s)",
            (profile_id, e["priority"], e["label"], e["is_interrupt"],
             e["breaks_current_cast"], e["enabled"], e["condition_logic"]))

    def delete_bot_entry(self, eid: int):
        self.run(self.chars,
            "DELETE FROM living_world_bot_combat_profile_action WHERE entry_id=%s", (eid,))
        self.run(self.chars,
            "DELETE FROM living_world_bot_combat_profile_condition WHERE entry_id=%s", (eid,))
        self.run(self.chars,
            "DELETE FROM living_world_bot_combat_profile_entry WHERE entry_id=%s", (eid,))

    def load_bot_actions(self, eid: int):
        return self.q(self.chars,
            "SELECT * FROM living_world_bot_combat_profile_action "
            "WHERE entry_id=%s ORDER BY slot", (eid,))

    def upsert_bot_action(self, a: dict, entry_id: int):
        if a.get("action_id"):
            self.run(self.chars,
                "UPDATE living_world_bot_combat_profile_action SET "
                "action_type=%s, spell_base_id=%s, item_id=%s, rank_mode=%s, "
                "rank_value=%s, target_key=%s, aoe_mode=%s, aoe_min_targets=%s, "
                "aoe_radius=%s WHERE action_id=%s",
                (a["action_type"], a["spell_base_id"], a["item_id"], a["rank_mode"],
                 a["rank_value"], a["target_key"], a.get("aoe_mode"),
                 a.get("aoe_min_targets"), a.get("aoe_radius"), a["action_id"]))
        else:
            self.run(self.chars,
                "INSERT INTO living_world_bot_combat_profile_action "
                "(entry_id, slot, action_type, spell_base_id, item_id, rank_mode, "
                "rank_value, target_key, aoe_mode, aoe_min_targets, aoe_radius) "
                "VALUES (%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s)",
                (entry_id, a["slot"], a["action_type"], a["spell_base_id"],
                 a["item_id"], a["rank_mode"], a["rank_value"], a["target_key"],
                 a.get("aoe_mode"), a.get("aoe_min_targets"), a.get("aoe_radius")))

    def load_bot_conditions(self, eid: int):
        return self.q(self.chars,
            "SELECT * FROM living_world_bot_combat_profile_condition "
            "WHERE entry_id=%s ORDER BY sequence", (eid,))

    def upsert_bot_condition(self, c: dict, entry_id: int):
        if c.get("condition_id"):
            self.run(self.chars,
                "UPDATE living_world_bot_combat_profile_condition SET "
                "sequence=%s, subject_key=%s, stat_key=%s, comparison=%s, "
                "numeric_value=%s, string_value=%s WHERE condition_id=%s",
                (c["sequence"], c["subject_key"], c["stat_key"], c["comparison"],
                 c["numeric_value"], c["string_value"], c["condition_id"]))
        else:
            self.run(self.chars,
                "INSERT INTO living_world_bot_combat_profile_condition "
                "(entry_id, sequence, subject_key, stat_key, comparison, "
                "numeric_value, string_value) VALUES (%s,%s,%s,%s,%s,%s,%s)",
                (entry_id, c["sequence"], c["subject_key"], c["stat_key"],
                 c["comparison"], c["numeric_value"], c["string_value"]))

    def delete_bot_condition(self, cid: int):
        self.run(self.chars,
            "DELETE FROM living_world_bot_combat_profile_condition "
            "WHERE condition_id=%s", (cid,))

    # ── Bot account pool (acore_auth) ────────────────────────────────────────

    def load_pool_accounts(self):
        return self.q(self.auth,
            "SELECT p.account_id, p.account_name, p.is_enabled, "
            "p.assigned_source_account_id, p.assigned_source_character_guid, "
            "a.username "
            "FROM living_world_bot_account_pool p "
            "LEFT JOIN account a ON a.id = p.account_id "
            "ORDER BY p.account_id")

    def load_all_accounts(self):
        return self.q(self.auth,
            "SELECT a.id AS account_id, a.username, a.email, a.reg_mail, a.locked, "
            "a.online, a.expansion, COUNT(c.guid) AS char_count, "
            "p.account_id IS NOT NULL AS in_pool, p.is_enabled, p.account_name, "
            "p.assigned_source_account_id, p.assigned_source_character_guid "
            "FROM account a "
            "LEFT JOIN acore_characters.characters c ON c.account = a.id "
            "LEFT JOIN living_world_bot_account_pool p ON p.account_id = a.id "
            "GROUP BY a.id, a.username, a.email, a.reg_mail, a.locked, a.online, a.expansion, "
            "p.account_id, p.is_enabled, p.account_name, p.assigned_source_account_id, p.assigned_source_character_guid "
            "ORDER BY a.id")

    def load_character_summary(self, guid: int):
        rows = self.q(self.chars,
            "SELECT c.guid, c.account, c.name, c.race, c.class, c.gender, c.level, c.xp, "
            "c.money, c.bankSlots, c.online, c.zone, c.map, c.health, c.power1, c.power2, "
            "c.power3, c.power4, c.power5, c.power6, c.power7, c.arenaPoints, "
            "c.totalHonorPoints, c.totalKills, c.creation_date, "
            "s.maxhealth, s.maxpower1, s.maxpower2, s.maxpower3, s.maxpower4, s.maxpower5, "
            "s.maxpower6, s.maxpower7, s.strength, s.agility, s.stamina, s.intellect, "
            "s.spirit, s.armor, s.attackPower, s.rangedAttackPower, s.spellPower, "
            "s.resilience, gm.guildid, g.name AS guild_name, g.BankMoney AS guild_bank_money "
            "FROM characters c "
            "LEFT JOIN character_stats s ON s.guid = c.guid "
            "LEFT JOIN guild_member gm ON gm.guid = c.guid "
            "LEFT JOIN guild g ON g.guildid = gm.guildid "
            "WHERE c.guid=%s LIMIT 1",
            (guid,))
        return rows[0] if rows else None

    def load_character_reputations(self, guid: int):
        return self.q(self.chars,
            "SELECT cr.faction, cr.standing, cr.flags, "
            "fd.Name_Lang_enUS AS faction_name "
            "FROM character_reputation cr "
            "LEFT JOIN acore_world.faction_dbc fd ON fd.ID = cr.faction "
            "WHERE cr.guid=%s "
            "ORDER BY cr.standing DESC, cr.faction ASC",
            (guid,))

    def load_character_inventory_rows(self, guid: int):
        return self.q(self.chars,
            "SELECT ci.bag, ci.slot, ci.item AS item_guid, "
            "ii.itemEntry, ii.count, ii.enchantments, ii.durability, "
            "it.name AS item_name, it.InventoryType, it.MaxDurability, "
            "it.socketColor_1, it.socketColor_2, it.socketColor_3 "
            "FROM character_inventory ci "
            "LEFT JOIN item_instance ii ON ii.guid = ci.item "
            "LEFT JOIN acore_world.item_template it ON it.entry = ii.itemEntry "
            "WHERE ci.guid=%s "
            "ORDER BY ci.bag ASC, ci.slot ASC, ci.item ASC",
            (guid,))

    def load_character_achievements(self, guid: int):
        if self.ok() and self._has_world_table("achievement_dbc"):
            return self.q(self.chars,
                "SELECT ca.achievement, ca.date, ad.Title_Lang_enUS AS achievement_name "
                "FROM character_achievement ca "
                "LEFT JOIN acore_world.achievement_dbc ad ON ad.ID = ca.achievement "
                "WHERE ca.guid=%s "
                "ORDER BY ca.date DESC, ca.achievement ASC",
                (guid,))
        return self.q(self.chars,
            "SELECT achievement, date, NULL AS achievement_name "
            "FROM character_achievement "
            "WHERE guid=%s "
            "ORDER BY date DESC, achievement ASC",
            (guid,))

    def update_character_core_stats(self, guid: int, level: int, xp: int, money: int, bank_slots: int):
        self.run(self.chars,
            "UPDATE characters SET level=%s, xp=%s, money=%s, bankSlots=%s "
            "WHERE guid=%s",
            (level, xp, money, bank_slots, guid))

    def create_account(self, username: str, password: str, email: str = "", add_to_pool: bool = False) -> int:
        username = _upper_only_latin(username)
        email = _upper_only_latin(email)
        salt, verifier = _srp6_registration_data(username, password)
        acct_id = self.run(self.auth,
            "INSERT INTO account (username, salt, verifier, expansion, reg_mail, email, joindate) "
            "VALUES (%s,%s,%s,%s,%s,%s,NOW())",
            (username, salt, verifier, 2, email, email))
        self.run(self.auth,
            "INSERT INTO realmcharacters (realmid, acctid, numchars) "
            "SELECT id, %s, 0 FROM realmlist",
            (acct_id,))
        if add_to_pool:
            self.run(self.auth,
                "INSERT INTO living_world_bot_account_pool "
                "(account_id, account_name, is_enabled) VALUES (%s,%s,1)",
                (acct_id, username))
        return acct_id

    def set_pool_account_enabled(self, account_id: int, enabled: bool):
        rows = self.q(self.auth,
            "SELECT account_id FROM living_world_bot_account_pool WHERE account_id=%s",
            (account_id,))
        if rows:
            self.run(self.auth,
                "UPDATE living_world_bot_account_pool SET is_enabled=%s WHERE account_id=%s",
                (1 if enabled else 0, account_id))
            return
        if enabled:
            name_rows = self.q(self.auth,
                "SELECT username FROM account WHERE id=%s LIMIT 1",
                (account_id,))
            account_name = name_rows[0]["username"] if name_rows else str(account_id)
            self.run(self.auth,
                "INSERT INTO living_world_bot_account_pool "
                "(account_id, account_name, is_enabled) VALUES (%s,%s,1)",
                (account_id, account_name))

    def set_account_enabled(self, account_id: int, enabled: bool):
        self.set_pool_account_enabled(account_id, enabled)

    def rename_account(self, account_id: int, new_username: str, new_password: str):
        new_username = _upper_only_latin(new_username)
        salt, verifier = _srp6_registration_data(new_username, new_password)
        self.run(self.auth,
            "UPDATE account SET username=%s, salt=%s, verifier=%s WHERE id=%s",
            (new_username, salt, verifier, account_id))
        self.run(self.auth,
            "UPDATE living_world_bot_account_pool SET account_name=%s WHERE account_id=%s",
            (new_username, account_id))

    def change_account_password(self, account_id: int, new_password: str):
        rows = self.q(self.auth,
            "SELECT username FROM account WHERE id=%s LIMIT 1",
            (account_id,))
        if not rows:
            raise ValueError(f"Account {account_id} not found")
        username = rows[0]["username"]
        salt, verifier = _srp6_registration_data(username, new_password)
        self.run(self.auth,
            "UPDATE account SET salt=%s, verifier=%s WHERE id=%s",
            (salt, verifier, account_id))

    def delete_account(self, account_id: int):
        char_rows = self.q(self.chars,
            "SELECT COUNT(*) AS n FROM characters WHERE account=%s",
            (account_id,))
        char_count = int(char_rows[0]["n"] or 0) if char_rows else 0
        if char_count > 0:
            raise ValueError(f"Account {account_id} still has {char_count} character(s); delete or move them first")

        self.run(self.auth, "DELETE FROM realmcharacters WHERE acctid=%s", (account_id,))
        self.run(self.auth, "DELETE FROM account_access WHERE id=%s", (account_id,))
        self.run(self.auth, "DELETE FROM account_banned WHERE id=%s", (account_id,))
        self.run(self.auth, "DELETE FROM account_muted WHERE guid=%s", (account_id,))
        self.run(self.auth, "DELETE FROM living_world_bot_account_pool WHERE account_id=%s", (account_id,))
        self.run(self.auth, "DELETE FROM account WHERE id=%s", (account_id,))


db = DBCtx()


# ═══════════════════════════════════════════════════════════════════════════
#  SHARED HELPER: labeled row builder
# ═══════════════════════════════════════════════════════════════════════════

def lbl(parent, text, row, col, **kw):
    ttk.Label(parent, text=text).grid(row=row, column=col, sticky="w",
                                      padx=4, pady=2, **kw)

def entry_w(parent, var, row, col, width=10, **kw):
    widget_kwargs = {}
    for key in ("state",):
        if key in kw:
            widget_kwargs[key] = kw.pop(key)
    e = ttk.Entry(parent, textvariable=var, width=width, **widget_kwargs)
    e.grid(row=row, column=col, sticky="w", padx=4, pady=2, **kw)
    return e

def combo_w(parent, var, values, row, col, width=14, **kw):
    c = ttk.Combobox(parent, textvariable=var, values=values,
                     state="readonly", width=width)
    c.grid(row=row, column=col, sticky="w", padx=4, pady=2, **kw)
    return c

def check_w(parent, var, text, row, col, **kw):
    cb = ttk.Checkbutton(parent, text=text, variable=var)
    cb.grid(row=row, column=col, sticky="w", padx=4, pady=2, **kw)
    return cb

def money_text(copper) -> str:
    try:
        total = int(copper or 0)
    except (TypeError, ValueError):
        total = 0
    gold = total // 10000
    silver = (total % 10000) // 100
    copper_only = total % 100
    return f"{gold}g {silver}s {copper_only}c"

def unix_text(value) -> str:
    try:
        ts = int(value or 0)
    except (TypeError, ValueError):
        return ""
    if ts <= 0:
        return ""
    try:
        return datetime.fromtimestamp(ts).strftime("%Y-%m-%d %H:%M:%S")
    except Exception:
        return str(ts)

def _is_noise_item_name(name: str) -> bool:
    lowered = (name or "").strip().lower()
    return (
        not lowered or
        lowered.startswith("zzold") or
        lowered.startswith("deprecated") or
        lowered.startswith("test") or
        lowered.startswith("old")
    )

def item_quality_filter_id(label: str):
    return ITEM_QUALITY_LABEL_TO_ID.get((label or "").strip(), None)

def slot_enchant_category(slot_id: int) -> str | None:
    return SLOT_ENCHANT_CATEGORY.get(int(slot_id))

def filter_enchant_rows_for_slot(slot_id: int, rows: list) -> list:
    category = slot_enchant_category(slot_id)
    cleaned = []
    for row in rows:
        name = (row.get("Name_Lang_enUS") or "").strip()
        lowered = name.lower()
        if not lowered:
            continue
        if any(word in lowered for word in ("test", "deprecated", "qa ", "zzold")):
            continue
        cleaned.append(row)

    if not category:
        return cleaned

    keywords = ENCHANT_CATEGORY_KEYWORDS.get(category, [])
    filtered = []
    for row in cleaned:
        lowered = (row.get("Name_Lang_enUS") or "").strip().lower()
        if any(keyword in lowered for keyword in keywords):
            filtered.append(row)

    return filtered or cleaned

def parse_display_id(text: str):
    s = (text or "").strip()
    if not s:
        return None
    if s.endswith("]") and "[" in s:
        try:
            return int(s.rsplit("[", 1)[-1].rstrip("]").strip())
        except ValueError:
            pass
    if s.startswith("#"):
        try:
            return int(s[1:].strip())
        except ValueError:
            pass
    if s.isdigit():
        return int(s)
    return None

def item_display_text(item_id, name: str = "") -> str:
    if not item_id:
        return ""
    try:
        iid = int(item_id)
    except (TypeError, ValueError):
        return str(item_id)
    return f"{name or f'Item {iid}'} [{iid}]"

def enchant_display_text(enchant_id, name: str = "") -> str:
    if not enchant_id:
        return ""
    try:
        eid = int(enchant_id)
    except (TypeError, ValueError):
        return str(enchant_id)
    return f"{name or f'Enchant {eid}'} [{eid}]"

def socket_color_text(mask) -> str:
    try:
        value = int(mask or 0)
    except (TypeError, ValueError):
        value = 0
    if value in SOCKET_COLOR_NAMES:
        return SOCKET_COLOR_NAMES[value]
    parts = [name for bit, name in SOCKET_COLOR_NAMES.items() if bit and (value & bit)]
    return "/".join(parts) if parts else str(value)

def parse_item_enchantments(text: str) -> list[int]:
    values = []
    for part in (text or "").split():
        try:
            values.append(int(part))
        except ValueError:
            values.append(0)
    target_len = ITEM_ENCHANTMENT_SLOT_COUNT * ITEM_ENCHANTMENT_OFFSET_COUNT
    if len(values) < target_len:
        values.extend([0] * (target_len - len(values)))
    return values[:target_len]

def build_item_enchantments(values: list[int]) -> str:
    target_len = ITEM_ENCHANTMENT_SLOT_COUNT * ITEM_ENCHANTMENT_OFFSET_COUNT
    padded = [int(v or 0) for v in list(values[:target_len])]
    if len(padded) < target_len:
        padded.extend([0] * (target_len - len(padded)))
    return " ".join(str(v) for v in padded)

def enchant_value_index(slot: int, offset: int = 0) -> int:
    return slot * ITEM_ENCHANTMENT_OFFSET_COUNT + offset

def get_item_enchant_id(values: list[int], slot: int) -> int:
    idx = enchant_value_index(slot, 0)
    return int(values[idx] or 0) if idx < len(values) else 0

def set_item_enchant_id(values: list[int], slot: int, enchant_id: int):
    base = enchant_value_index(slot, 0)
    if base + 2 >= len(values):
        return
    values[base] = int(enchant_id or 0)
    values[base + 1] = 0
    values[base + 2] = 0


# ═══════════════════════════════════════════════════════════════════════════
#  PROFILE HEADER FRAME  (reused for both default and bot profiles)
# ═══════════════════════════════════════════════════════════════════════════

class ProfileHeaderFrame(ttk.LabelFrame):
    """Displays and edits the scalar fields of a combat profile."""

    def __init__(self, parent, is_default=True, on_class_change=None, **kw):
        super().__init__(parent, text="Profile Settings", padding=6, **kw)
        self.is_default = is_default
        self._on_class_change_cb = on_class_change
        self._build()
        self.clear()

    def _build(self):
        f = self
        # Row 0 – display name / class / spec / role
        if not self.is_default:
            lbl(f, "Name:",         0, 0)
            self.v_name     = tk.StringVar()
            entry_w(f, self.v_name, 0, 1, width=20)
            lbl(f, "Slot:",         0, 2)
            self.v_slot     = tk.StringVar()
            entry_w(f, self.v_slot, 0, 3, width=4)
        _hr = 0 if self.is_default else 1
        lbl(f, "Class:", _hr, 0)
        self.v_class = tk.StringVar()
        class_cb = ttk.Combobox(f, textvariable=self.v_class, values=CLASS_OPTS,
                                state="readonly", width=14)
        class_cb.grid(row=_hr, column=1, sticky="w", padx=4, pady=2)
        class_cb.bind("<<ComboboxSelected>>", self._on_class_change)
        lbl(f, "Spec:", _hr, 2)
        self.v_spec = tk.StringVar()
        self._spec_cb = ttk.Combobox(f, textvariable=self.v_spec, values=[],
                                     state="readonly", width=16)
        self._spec_cb.grid(row=_hr, column=3, sticky="w", padx=4, pady=2)
        lbl(f, "Role:", _hr, 4)
        self.v_role = tk.StringVar()
        combo_w(f, self.v_role, ROLE_OPTS, _hr, 5, width=10)

        if self.is_default:
            lbl(f, "Display name:", 1, 0)
            self.v_display = tk.StringVar()
            entry_w(f, self.v_display, 1, 1, width=28, columnspan=5)

        # Row 2 – conservation / mana
        r = 2
        lbl(f, "Conservation:", r, 0)
        self.v_conservation = tk.StringVar()
        combo_w(f, self.v_conservation, CONSERVATION_OPTS, r, 1)

        lbl(f, "Mana low %:", r, 2)
        self.v_mana_low = tk.StringVar()
        entry_w(f, self.v_mana_low, r, 3, width=5)

        lbl(f, "Mana high %:", r, 4)
        self.v_mana_high = tk.StringVar()
        entry_w(f, self.v_mana_high, r, 5, width=5)

        # Row 3 – down-rank / AoE
        r = 3
        self.v_downrank = tk.BooleanVar()
        check_w(f, self.v_downrank, "Down-rank", r, 0)

        lbl(f, "DR floor:", r, 1)
        self.v_dr_floor = tk.StringVar()
        entry_w(f, self.v_dr_floor, r, 2, width=4)

        lbl(f, "AoE mode:", r, 3)
        self.v_aoe_mode = tk.StringVar()
        combo_w(f, self.v_aoe_mode, AOE_OPTS, r, 4, width=10)

        lbl(f, "AoE min targets:", r, 5)
        self.v_aoe_min = tk.StringVar()
        entry_w(f, self.v_aoe_min, r, 6, width=4)

        lbl(f, "AoE radius:", r, 7)
        self.v_aoe_radius = tk.StringVar()
        entry_w(f, self.v_aoe_radius, r, 8, width=6)

    def _spec_values_for_class(self, class_name: str) -> list[str]:
        class_id = CLASS_NAME_TO_ID.get(class_name)
        return CLASS_SPEC_OPTS.get(class_id, [])

    def _apply_spec_values(self, class_name: str, preferred_spec: str = ""):
        values = self._spec_values_for_class(class_name)
        self._spec_cb.configure(values=values)
        if preferred_spec in values:
            self.v_spec.set(preferred_spec)
        elif values:
            self.v_spec.set(values[0] if self.v_spec.get() not in values else self.v_spec.get())
        else:
            self.v_spec.set(preferred_spec)

    def _emit_class_change(self):
        if self._on_class_change_cb:
            self._on_class_change_cb(CLASS_NAME_TO_ID.get(self.v_class.get()))

    def _on_class_change(self, _=None):
        self._apply_spec_values(self.v_class.get())
        self._emit_class_change()

    def clear(self):
        self.v_class.set("")
        self.v_spec.set("")
        self.v_role.set("")
        self.v_conservation.set(CONSERVATION_MODES[1])
        self.v_mana_low.set("55")
        self.v_mana_high.set("75")
        self.v_downrank.set(True)
        self.v_dr_floor.set("2")
        self.v_aoe_mode.set(AOE_MODES[0])
        self.v_aoe_min.set("2")
        self.v_aoe_radius.set("10.0")
        if self.is_default:
            self.v_display.set("")
        else:
            self.v_name.set("")
            self.v_slot.set("")

    def load(self, p: dict, forced_class_id: int | None = None):
        if self.is_default:
            raw_spec = p.get("spec_key", "") or ""
            raw_role = p.get("role_key", "") or ""
            class_id = forced_class_id or _class_from_spec(raw_spec, p.get("display_name", "") or "")
        else:
            raw_spec = (p.get("spec_override_key") or
                        p.get("guessed_spec_key") or "")
            raw_role = (p.get("role_override_key") or
                        p.get("guessed_role_key") or "")
            class_id = forced_class_id or _class_from_spec(raw_spec, p.get("profile_name", "") or "")

        class_name = WOW_CLASSES.get(class_id, "")
        self.v_class.set(class_name)
        self._apply_spec_values(class_name, raw_spec)
        self.v_role.set(_normalize_role(raw_role))
        self.v_conservation.set(CONSERVATION_MODES.get(p.get("conservation_mode", 1), "Conservative"))
        self.v_mana_low.set(str(p.get("mana_low_water", 55)))
        self.v_mana_high.set(str(p.get("mana_high_water", 75)))
        self.v_downrank.set(bool(p.get("enable_down_rank", 1)))
        self.v_dr_floor.set(str(p.get("down_rank_floor", 2)))
        self.v_aoe_mode.set(AOE_MODES.get(p.get("default_aoe_mode", 0), "Centroid"))
        self.v_aoe_min.set(str(p.get("default_aoe_min_targets", 2)))
        self.v_aoe_radius.set(str(p.get("default_aoe_scan_radius", 10.0)))
        if self.is_default:
            self.v_display.set(p.get("display_name", "") or "")
        else:
            self.v_name.set(p.get("profile_name", "") or "")
            self.v_slot.set(str(p.get("slot", "")))
        self._emit_class_change()

    def collect(self, base: dict) -> dict:
        base["conservation_mode"]    = CONSERVATION_INV.get(self.v_conservation.get(), 1)
        base["mana_low_water"]       = int(self.v_mana_low.get() or 55)
        base["mana_high_water"]      = int(self.v_mana_high.get() or 75)
        base["enable_down_rank"]     = int(self.v_downrank.get())
        base["down_rank_floor"]      = int(self.v_dr_floor.get() or 2)
        base["default_aoe_mode"]     = AOE_INV.get(self.v_aoe_mode.get(), 0)
        base["default_aoe_min_targets"] = int(self.v_aoe_min.get() or 2)
        base["default_aoe_scan_radius"] = float(self.v_aoe_radius.get() or 10.0)
        if self.is_default:
            base["spec_key"] = self.v_spec.get().strip()
            base["role_key"] = _normalize_role(self.v_role.get().strip())
            base["display_name"] = self.v_display.get().strip()
        else:
            base["spec_override_key"] = self.v_spec.get().strip() or None
            base["role_override_key"] = _normalize_role(self.v_role.get().strip()) or None
            base["profile_name"] = self.v_name.get().strip()
            base["slot"]         = int(self.v_slot.get() or 0)
        return base


class DefaultProfilePicker:
    """Modal picker for choosing a default profile template or blank slate."""

    def __init__(self, parent, options: list[tuple[str, int | None]], title="New Bot Profile"):
        self._options = options or [("Blank slate", None)]
        self.selection = None
        self.cancelled = True

        self._choice = tk.StringVar(value=self._options[0][0])
        self._result = None
        self._win = tk.Toplevel(parent)
        self._win.title(title)
        self._win.transient(parent)
        self._win.resizable(False, False)
        self._win.protocol("WM_DELETE_WINDOW", self._cancel)

        body = ttk.Frame(self._win, padding=10)
        body.pack(fill=tk.BOTH, expand=True)
        ttk.Label(body, text="Create the new profile from:").grid(
            row=0, column=0, sticky="w", pady=(0, 4))
        combo = ttk.Combobox(
            body,
            textvariable=self._choice,
            values=[label for label, _value in self._options],
            state="readonly",
            width=40,
        )
        combo.grid(row=1, column=0, sticky="ew")
        combo.focus_set()

        btns = ttk.Frame(body)
        btns.grid(row=2, column=0, sticky="e", pady=(10, 0))
        ttk.Button(btns, text="OK", command=self._ok).pack(side=tk.LEFT, padx=(0, 6))
        ttk.Button(btns, text="Cancel", command=self._cancel).pack(side=tk.LEFT)

        body.columnconfigure(0, weight=1)
        self._win.bind("<Return>", lambda _e: self._ok())
        self._win.bind("<Escape>", lambda _e: self._cancel())
        self._win.update_idletasks()

        parent_x = parent.winfo_rootx()
        parent_y = parent.winfo_rooty()
        parent_w = parent.winfo_width()
        parent_h = parent.winfo_height()
        win_w = self._win.winfo_width()
        win_h = self._win.winfo_height()
        pos_x = parent_x + max((parent_w - win_w) // 2, 0)
        pos_y = parent_y + max((parent_h - win_h) // 2, 0)
        self._win.geometry(f"+{pos_x}+{pos_y}")

        self._win.grab_set()
        parent.wait_window(self._win)

    def _ok(self):
        selected = self._choice.get()
        self.cancelled = False
        for label, value in self._options:
            if label == selected:
                self.selection = value
                break
        self._close()

    def _cancel(self):
        self.cancelled = True
        self.selection = None
        self._close()

    def _close(self):
        if self._win and self._win.winfo_exists():
            self._win.grab_release()
            self._win.destroy()


# ═══════════════════════════════════════════════════════════════════════════
#  ROTATION EDITOR  (entry list + action/condition detail)
# ═══════════════════════════════════════════════════════════════════════════

class RotationEditor(ttk.Frame):
    """
    Full rotation editor.  Caller provides load/save callback bundles
    so the same widget works for both default and per-bot profiles.
    """

    def __init__(self, parent, cbs: dict, **kw):
        """
        cbs keys:
          load_entries(profile_id) -> [dict]
          upsert_entry(e, profile_id) -> entry_id
          delete_entry(entry_id)
          load_actions(entry_id) -> [dict]
          upsert_action(a, entry_id)
          load_conditions(entry_id) -> [dict]
          upsert_condition(c, entry_id)
          delete_condition(condition_id)
        """
        super().__init__(parent, **kw)
        self._cbs          = cbs
        self._profile_id   = None
        self._entries      = []          # list of entry dicts (in display order)
        self._sel_entry    = None        # currently selected entry dict
        self._actions      = {}          # slot -> action dict (may be empty)
        self._conditions   = []          # list of condition dicts
        self._class_id     = None        # current WoW class_id for spell list
        self._class_spells = []          # [{"id": int, "display": str}]
        self._build()

    def _build(self):
        # ── left: entry list ────────────────────────────────────────────────
        left = ttk.Frame(self)
        left.pack(side=tk.LEFT, fill=tk.Y, padx=(0, 4))

        ttk.Label(left, text="Rotation entries (priority order)").pack(anchor="w")

        cols = ("pri", "label", "flags")
        self._tv = ttk.Treeview(left, columns=cols, show="headings",
                                height=14, selectmode="browse")
        self._tv.heading("pri",   text="Pri")
        self._tv.heading("label", text="Label")
        self._tv.heading("flags", text="Flags")
        self._tv.column("pri",   width=30, anchor="center")
        self._tv.column("label", width=160)
        self._tv.column("flags", width=80, anchor="center")
        self._tv.pack(fill=tk.Y, expand=True)
        self._tv.bind("<<TreeviewSelect>>", self._on_entry_select)

        btn_row = ttk.Frame(left)
        btn_row.pack(fill=tk.X, pady=2)
        ttk.Button(btn_row, text="+ Add",    command=self._add_entry).pack(side=tk.LEFT, padx=2)
        ttk.Button(btn_row, text="↑",        command=self._move_up).pack(side=tk.LEFT, padx=2)
        ttk.Button(btn_row, text="↓",        command=self._move_down).pack(side=tk.LEFT, padx=2)
        ttk.Button(btn_row, text="✕ Remove", command=self._del_entry).pack(side=tk.LEFT, padx=2)

        # ── right: detail panel ──────────────────────────────────────────────
        right = ttk.Frame(self)
        right.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

        # Entry header
        hdr = ttk.LabelFrame(right, text="Entry settings", padding=4)
        hdr.pack(fill=tk.X, pady=(0, 4))

        lbl(hdr, "Label:",    0, 0)
        self.v_label    = tk.StringVar()
        entry_w(hdr, self.v_label, 0, 1, width=20)
        lbl(hdr, "Priority:", 0, 2)
        self.v_priority = tk.StringVar()
        entry_w(hdr, self.v_priority, 0, 3, width=4)

        self.v_enabled   = tk.BooleanVar(value=True)
        self.v_interrupt = tk.BooleanVar()
        self.v_break     = tk.BooleanVar()
        check_w(hdr, self.v_enabled,   "Enabled",      1, 0)
        check_w(hdr, self.v_interrupt, "Interrupt",     1, 1)
        check_w(hdr, self.v_break,     "Break cast",   1, 2)
        lbl(hdr, "Cond logic:", 1, 3)
        self.v_cond_logic = tk.StringVar(value=COND_LOGIC[0])
        combo_w(hdr, self.v_cond_logic, COND_LOGIC_OPTS, 1, 4, width=10)

        ttk.Button(hdr, text="Save entry", command=self._save_entry).grid(
            row=0, column=5, rowspan=2, padx=8, sticky="ns")

        # Actions (primary + secondary in sub-frames)
        act_frame = ttk.LabelFrame(right, text="Actions", padding=4)
        act_frame.pack(fill=tk.X, pady=(0, 4))
        self._action_widgets = {}
        for slot, label in ((0, "Primary"), (1, "Secondary")):
            self._action_widgets[slot] = self._build_action_row(act_frame, label, slot)

        # Conditions
        cond_frame = ttk.LabelFrame(right, text="Conditions", padding=4)
        cond_frame.pack(fill=tk.BOTH, expand=True)
        self._build_condition_panel(cond_frame)

        self._set_detail_enabled(False)

    # ── Action row builder ───────────────────────────────────────────────────

    def _build_action_row(self, parent, label: str, slot: int) -> dict:
        f = ttk.Frame(parent)
        f.pack(fill=tk.X, pady=2)
        ttk.Label(f, text=f"{label}:", width=10).pack(side=tk.LEFT)

        v = {}
        v["type"]         = tk.StringVar(value=ACTION_TYPES[0])
        v["spell_id"]     = tk.StringVar()  # spell_base_id for save
        v["item_id"]      = tk.StringVar()  # item_id for save
        v["combo_text"]   = tk.StringVar()  # what the combo box shows
        v["id_display"]   = tk.StringVar()  # resolved ID shown in badge (or "INF")
        v["rank_mode"]    = tk.StringVar(value=RANK_MODES[0])
        v["rank_val"]     = tk.StringVar(value="0")
        v["target"]       = tk.StringVar(value="enemy")
        v["aoe_mode"]     = tk.StringVar(value="")
        v["aoe_min"]      = tk.StringVar(value="")
        v["aoe_radius"]   = tk.StringVar(value="")

        # ── Type picker (Spell / Item) ──────────────────────────────────────
        type_cb = ttk.Combobox(f, textvariable=v["type"], values=ACTION_OPTS,
                               state="readonly", width=7)
        type_cb.pack(side=tk.LEFT, padx=2)
        v["_type_widget"] = type_cb

        def _on_type_changed(event, _v=v):
            self._on_type_change(_v)
        type_cb.bind("<<ComboboxSelected>>", _on_type_changed)

        # ── Spell / Item picker combo ───────────────────────────────────────
        # Spell mode: pre-filled with class spell list.
        # Item  mode: blank — user types a name and presses Enter to search.
        combo = ttk.Combobox(f, textvariable=v["combo_text"], values=[], width=32)
        combo.pack(side=tk.LEFT, padx=2)
        v["_combo_widget"] = combo

        def _on_combo_select(event, _v=v):
            if _v["type"].get() == "Spell":
                self._on_spell_combo_select(_v)
            else:
                self._on_item_combo_select(_v)
        combo.bind("<<ComboboxSelected>>", _on_combo_select)

        def _on_combo_action(event, _v=v):
            if _v["type"].get() == "Spell":
                self._on_spell_combo_typed(_v)
            else:
                self._on_item_search(_v)
        combo.bind("<Return>",   _on_combo_action)
        combo.bind("<FocusOut>", _on_combo_action)

        # ── Read-only ID badge ──────────────────────────────────────────────
        ttk.Label(f, text="Rank:").pack(side=tk.LEFT, padx=(4, 0))
        ttk.Combobox(f, textvariable=v["rank_mode"], values=RANK_OPTS,
                     state="readonly", width=12).pack(side=tk.LEFT, padx=2)
        ttk.Entry(f, textvariable=v["rank_val"], width=3).pack(side=tk.LEFT, padx=2)
        ttk.Label(f, text="Target:").pack(side=tk.LEFT, padx=(4, 0))
        ttk.Combobox(f, textvariable=v["target"], values=TARGET_KEYS,
                     state="readonly", width=14).pack(side=tk.LEFT, padx=2)

        ttk.Label(f, text="AoE:").pack(side=tk.LEFT, padx=(4, 0))
        ttk.Combobox(f, textvariable=v["aoe_mode"], values=[""] + AOE_OPTS,
                     state="readonly", width=10).pack(side=tk.LEFT, padx=2)
        ttk.Label(f, text="Min:").pack(side=tk.LEFT)
        ttk.Entry(f, textvariable=v["aoe_min"], width=3).pack(side=tk.LEFT, padx=2)
        ttk.Label(f, text="R:").pack(side=tk.LEFT)
        ttk.Entry(f, textvariable=v["aoe_radius"], width=4).pack(side=tk.LEFT, padx=2)
        return v

    # ── Spell / Item combo helpers ───────────────────────────────────────────

    def set_class(self, class_id: int | None):
        """Load spells for class_id; populate combos currently in Spell mode."""
        self._class_id     = class_id
        self._class_spells = db.load_class_spells(class_id) if class_id else []
        values = [s["display"] for s in self._class_spells]
        for w in self._action_widgets.values():
            if w["type"].get() == "Spell":
                w["_combo_widget"].configure(values=values)
        if hasattr(self, "_cond_spell_combo"):
            self._cond_spell_combo.configure(values=values)

    def _spell_display_for_id(self, spell_id) -> str:
        """Return 'Name  [ID]' for a spell_id, class list first then DB fallback."""
        if not spell_id:
            return ""
        try:
            sid = int(spell_id)
        except (ValueError, TypeError):
            return ""
        for s in self._class_spells:
            if s["id"] == sid:
                return s["display"]
        name = db.spell_name(sid)
        return f"{name}  [{sid}]" if name else f"#{sid}"

    def _parse_id_from_display(self, display: str):
        """Extract numeric ID from 'Name  [12345]', '#12345', or bare '12345'. Returns int|None."""
        s = display.strip()
        if s.endswith("]") and "[" in s:
            try:
                return int(s.rsplit("[", 1)[-1].rstrip("]").strip())
            except ValueError:
                pass
        if s.startswith("#"):
            try:
                return int(s[1:].strip())
            except ValueError:
                pass
        if s.isdigit():
            return int(s)
        return None

    def _condition_spell_display(self, raw_value) -> str:
        """Render aura spell IDs as 'Name [ID]' where possible."""
        if raw_value in (None, ""):
            return ""
        try:
            sid = int(float(raw_value))
        except (TypeError, ValueError):
            text = str(raw_value).strip()
            sid = self._parse_id_from_display(text)
            if sid is None:
                return text
        name = db.spell_name(sid)
        return f"{name} [{sid}]" if name else str(sid)

    def _condition_desc(self, c: dict) -> str:
        op = COND_OPS.get(c.get("comparison", 4), ">=")
        stat_key = c.get("stat_key", "")
        subject = c.get("subject_key", "")
        string_value = c.get("string_value", "")
        numeric_value = c.get("numeric_value", "")

        if stat_key == "aura":
            raw_value = string_value if string_value not in (None, "") else numeric_value
            value_text = self._condition_spell_display(raw_value)
        elif stat_key == "aura_stacks":
            raw_value = string_value if string_value not in (None, "") else numeric_value
            spell_display = self._condition_spell_display(raw_value)
            value_text = f"{spell_display} stacks={numeric_value}"
        elif stat_key in BOOL_STAT_KEYS:
            try:
                value_text = "True" if int(float(numeric_value or 0)) else "False"
            except (TypeError, ValueError):
                value_text = str(numeric_value)
        else:
            value_text = str(numeric_value)
            if string_value:
                value_text = f"{value_text} '{string_value}'"

        return f"{subject}.{stat_key} {op} {value_text}".strip()

    def _sync_condition_value_editor(self):
        aura_mode = self.v_stat.get() == "aura"
        aura_stacks_mode = self.v_stat.get() == "aura_stacks"
        bool_mode = self.v_stat.get() in BOOL_STAT_KEYS
        if aura_mode:
            self._cond_value_label.pack_forget()
            self._cond_nval_entry.pack_forget()
            self._cond_string_label.pack_forget()
            self._cond_sval_entry.pack_forget()
            self._cond_bool_label.pack_forget()
            self._cond_bool_combo.pack_forget()
            self._cond_aura_label.pack(side=tk.LEFT)
            self._cond_spell_combo.pack(side=tk.LEFT, padx=2)
        elif aura_stacks_mode:
            self._cond_bool_label.pack_forget()
            self._cond_bool_combo.pack_forget()
            self._cond_aura_label.pack(side=tk.LEFT)
            self._cond_spell_combo.pack(side=tk.LEFT, padx=2)
            self._cond_value_label.pack(side=tk.LEFT)
            self._cond_nval_entry.pack(side=tk.LEFT, padx=2)
            self._cond_string_label.pack_forget()
            self._cond_sval_entry.pack_forget()
        elif bool_mode:
            self._cond_aura_label.pack_forget()
            self._cond_spell_combo.pack_forget()
            self._cond_value_label.pack_forget()
            self._cond_nval_entry.pack_forget()
            self._cond_string_label.pack_forget()
            self._cond_sval_entry.pack_forget()
            self._cond_bool_label.pack(side=tk.LEFT)
            self._cond_bool_combo.pack(side=tk.LEFT, padx=2)
        else:
            self._cond_aura_label.pack_forget()
            self._cond_spell_combo.pack_forget()
            self._cond_bool_label.pack_forget()
            self._cond_bool_combo.pack_forget()
            self._cond_value_label.pack(side=tk.LEFT)
            self._cond_nval_entry.pack(side=tk.LEFT, padx=2)
            self._cond_string_label.pack(side=tk.LEFT)
            self._cond_sval_entry.pack(side=tk.LEFT, padx=2)

    def _on_cond_stat_changed(self, _=None):
        if self.v_stat.get() == "aura":
            raw_value = self.v_sval.get().strip() or self.v_nval.get().strip()
            self.v_cond_spell.set(self._condition_spell_display(raw_value))
        elif self.v_stat.get() == "aura_stacks":
            raw_value = self.v_sval.get().strip()
            self.v_cond_spell.set(self._condition_spell_display(raw_value))
        elif self.v_stat.get() in BOOL_STAT_KEYS:
            try:
                self.v_cond_bool.set("True" if int(float(self.v_nval.get() or 0)) else "False")
            except (TypeError, ValueError):
                self.v_cond_bool.set("False")
        self._sync_condition_value_editor()

    def _on_cond_bool_changed(self, _=None):
        self.v_nval.set("1" if self.v_cond_bool.get() == "True" else "0")
        self.v_sval.set("")

    def _on_cond_spell_pick(self, _=None):
        sid = self._parse_id_from_display(self.v_cond_spell.get())
        if sid is not None:
            if self.v_stat.get() == "aura_stacks":
                self.v_sval.set(str(sid))
            else:
                self.v_nval.set(str(sid))
                self.v_sval.set("")
            self.v_cond_spell.set(self._spell_display_for_id(sid))

    def _on_cond_spell_typed(self, _=None):
        raw = self.v_cond_spell.get().strip()
        if not raw:
            if self.v_stat.get() == "aura_stacks":
                self.v_sval.set("")
            else:
                self.v_nval.set("0")
                self.v_sval.set("")
            return
        sid = self._parse_id_from_display(raw)
        if sid is not None:
            if self.v_stat.get() == "aura_stacks":
                self.v_sval.set(str(sid))
            else:
                self.v_nval.set(str(sid))
                self.v_sval.set("")
            self.v_cond_spell.set(self._spell_display_for_id(sid))

    # ── Type toggle ─────────────────────────────────────────────────────────

    def _on_type_change(self, v: dict):
        """User switched Spell ↔ Item — reset combo and badge."""
        v["spell_id"].set("")
        v["item_id"].set("")
        v["combo_text"].set("")
        v["id_display"].set("")
        if v["type"].get() == "Spell":
            values = [s["display"] for s in self._class_spells]
            v["_combo_widget"].configure(values=values)
        else:
            v["_combo_widget"].configure(values=[])

    # ── Spell-mode handlers ──────────────────────────────────────────────────

    def _on_spell_combo_select(self, v: dict):
        """User picked a spell from the dropdown."""
        sid = self._parse_id_from_display(v["combo_text"].get())
        if sid is not None:
            v["spell_id"].set(str(sid))
            v["id_display"].set(str(sid))
        else:
            v["spell_id"].set("")
            v["id_display"].set("")

    def _on_spell_combo_typed(self, v: dict):
        """User typed a raw ID or name in spell combo; resolve to display form."""
        raw = v["combo_text"].get().strip()
        sid = self._parse_id_from_display(raw)
        if sid is not None:
            v["spell_id"].set(str(sid))
            v["id_display"].set(str(sid))
            v["combo_text"].set(self._spell_display_for_id(sid))

    # ── Item-mode handlers ───────────────────────────────────────────────────

    def _on_item_search(self, v: dict):
        """User pressed Enter in Item mode — search item_template by name."""
        raw = v["combo_text"].get().strip()
        if not raw:
            return
        # Bare numeric ID → reverse lookup
        if raw.isdigit():
            iid = int(raw)
            name = db.item_name(iid)
            if name:
                display = f"{name}  [{iid}]"
                v["item_id"].set(str(iid))
                v["id_display"].set(str(iid))
                v["combo_text"].set(display)
                v["_combo_widget"].configure(values=[display])
            else:
                v["item_id"].set("")
                v["id_display"].set("INF")
            return
        # Already resolved "Name [ID]" format
        sid = self._parse_id_from_display(raw)
        if sid is not None:
            v["item_id"].set(str(sid))
            v["id_display"].set(str(sid))
            return
        # Name search
        results = db.search_items(raw)
        if not results:
            v["item_id"].set("")
            v["id_display"].set("INF")
            v["_combo_widget"].configure(values=[])
            return
        displays = [r["display"] for r in results]
        v["_combo_widget"].configure(values=displays)
        first = results[0]
        v["combo_text"].set(first["display"])
        v["item_id"].set(str(first["id"]))
        v["id_display"].set(str(first["id"]))

    def _on_item_combo_select(self, v: dict):
        """User picked one of the item search results."""
        sid = self._parse_id_from_display(v["combo_text"].get())
        if sid is not None:
            v["item_id"].set(str(sid))
            v["id_display"].set(str(sid))
        else:
            v["item_id"].set("")
            v["id_display"].set("INF")

    # ── Condition panel ──────────────────────────────────────────────────────

    def _build_condition_panel(self, parent):
        edit = ttk.Frame(parent)
        edit.pack(fill=tk.X, pady=(0, 4))

        self.v_subj  = tk.StringVar(value=SUBJECT_KEYS[0])
        self.v_stat  = tk.StringVar(value=STAT_KEYS[0])
        self.v_op    = tk.StringVar(value=COND_OPS[4])    # ">=" default
        self.v_nval  = tk.StringVar(value="0")
        self.v_sval  = tk.StringVar()
        self.v_cond_spell = tk.StringVar()
        self.v_cond_bool = tk.StringVar(value="False")

        ttk.Label(edit, text="Subject:").pack(side=tk.LEFT)
        ttk.Combobox(edit, textvariable=self.v_subj, values=SUBJECT_KEYS,
                     state="readonly", width=12).pack(side=tk.LEFT, padx=2)
        ttk.Label(edit, text="Stat:").pack(side=tk.LEFT)
        stat_cb = ttk.Combobox(edit, textvariable=self.v_stat, values=STAT_KEYS,
                               state="readonly", width=14)
        stat_cb.pack(side=tk.LEFT, padx=2)
        stat_cb.bind("<<ComboboxSelected>>", self._on_cond_stat_changed)
        ttk.Label(edit, text="Op:").pack(side=tk.LEFT)
        ttk.Combobox(edit, textvariable=self.v_op, values=COND_OPS_OPTS,
                     state="readonly", width=6).pack(side=tk.LEFT, padx=2)
        self._cond_value_label = ttk.Label(edit, text="Value:")
        self._cond_value_label.pack(side=tk.LEFT)
        self._cond_nval_entry = ttk.Entry(edit, textvariable=self.v_nval, width=6)
        self._cond_nval_entry.pack(side=tk.LEFT, padx=2)
        self._cond_string_label = ttk.Label(edit, text="String:")
        self._cond_string_label.pack(side=tk.LEFT)
        self._cond_sval_entry = ttk.Entry(edit, textvariable=self.v_sval, width=10)
        self._cond_sval_entry.pack(side=tk.LEFT, padx=2)
        self._cond_aura_label = ttk.Label(edit, text="Aura:")
        self._cond_spell_combo = ttk.Combobox(
            edit, textvariable=self.v_cond_spell,
            values=[s["display"] for s in self._class_spells], width=28)
        self._cond_spell_combo.bind("<<ComboboxSelected>>", self._on_cond_spell_pick)
        self._cond_spell_combo.bind("<Return>", self._on_cond_spell_typed)
        self._cond_spell_combo.bind("<FocusOut>", self._on_cond_spell_typed)
        self._cond_bool_label = ttk.Label(edit, text="Value:")
        self._cond_bool_combo = ttk.Combobox(
            edit, textvariable=self.v_cond_bool, values=["True", "False"],
            state="readonly", width=7)
        self._cond_bool_combo.bind("<<ComboboxSelected>>", self._on_cond_bool_changed)
        self._sync_condition_value_editor()

        list_f = ttk.Frame(parent)
        list_f.pack(fill=tk.BOTH, expand=True, pady=(0, 4))

        cols = ("seq", "desc")
        self._cond_tv = ttk.Treeview(list_f, columns=cols, show="headings",
                                     height=8, selectmode="browse")
        self._cond_tv.heading("seq",  text="#")
        self._cond_tv.heading("desc", text="Condition")
        self._cond_tv.column("seq",  width=25, anchor="center")
        self._cond_tv.column("desc", width=300)
        self._cond_tv.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        self._cond_tv.bind("<<TreeviewSelect>>", self._on_cond_select)

        sb = ttk.Scrollbar(list_f, orient="vertical", command=self._cond_tv.yview)
        self._cond_tv.configure(yscrollcommand=sb.set)
        sb.pack(side=tk.LEFT, fill=tk.Y)

        btn_row = ttk.Frame(parent)
        btn_row.pack(fill=tk.X)
        ttk.Button(btn_row, text="+ Save cond",
                   command=self._save_condition).pack(side=tk.LEFT, padx=(0, 4))
        ttk.Button(btn_row, text="✕ Remove",
                   command=self._del_condition).pack(side=tk.LEFT)

    # ── Public API ───────────────────────────────────────────────────────────

    def load_profile(self, profile_id: int):
        self._profile_id = profile_id
        self._sel_entry  = None
        self._entries    = self._cbs["load_entries"](profile_id)
        self._refresh_entry_list()
        self._set_detail_enabled(False)

    def clear(self):
        self._profile_id = None
        self._sel_entry  = None
        self._entries    = []
        self._refresh_entry_list()
        self._set_detail_enabled(False)

    # ── Internal ─────────────────────────────────────────────────────────────

    def _refresh_entry_list(self):
        self._tv.delete(*self._tv.get_children())
        for e in self._entries:
            flags = []
            if e.get("is_interrupt"):  flags.append("INT")
            if not e.get("enabled"):   flags.append("off")
            iid = str(e.get("entry_id", id(e)))
            self._tv.insert("", "end", iid=iid,
                            values=(e.get("priority", 0),
                                    e.get("label", ""),
                                    " ".join(flags)))

    def _on_entry_select(self, _=None):
        sel = self._tv.selection()
        if not sel:
            return
        iid = sel[0]
        self._sel_entry = next(
            (e for e in self._entries if str(e.get("entry_id", id(e))) == iid), None)
        if self._sel_entry:
            self._load_entry_detail(self._sel_entry)
            self._set_detail_enabled(True)

    def _load_entry_detail(self, e: dict):
        self.v_label.set(e.get("label", ""))
        self.v_priority.set(str(e.get("priority", 0)))
        self.v_enabled.set(bool(e.get("enabled", 1)))
        self.v_interrupt.set(bool(e.get("is_interrupt", 0)))
        self.v_break.set(bool(e.get("breaks_current_cast", 0)))
        self.v_cond_logic.set(COND_LOGIC.get(e.get("condition_logic", 0), "All (AND)"))

        eid = e.get("entry_id")
        actions = {a["slot"]: a for a in (self._cbs["load_actions"](eid) if eid else [])}
        for slot, w in self._action_widgets.items():
            a = actions.get(slot, {})
            action_type = a.get("action_type", 0)
            w["type"].set(ACTION_TYPES.get(action_type, "Spell"))
            w["rank_mode"].set(RANK_MODES.get(a.get("rank_mode", 0), "Best Known"))
            w["rank_val"].set(str(a.get("rank_value", 0) or 0))
            w["target"].set(a.get("target_key", "enemy") or "enemy")
            w["aoe_mode"].set(AOE_MODES.get(a.get("aoe_mode"), "") if a.get("aoe_mode") is not None else "")
            w["aoe_min"].set("" if a.get("aoe_min_targets") in (None, "") else str(a.get("aoe_min_targets")))
            w["aoe_radius"].set("" if a.get("aoe_radius") in (None, "") else str(a.get("aoe_radius")))

            if action_type == 0:  # Spell
                sid = a.get("spell_base_id") or 0
                w["spell_id"].set(str(sid) if sid else "")
                w["item_id"].set("")
                display = self._spell_display_for_id(sid) if sid else ""
                w["combo_text"].set(display)
                w["id_display"].set(str(sid) if sid else "")
                w["_combo_widget"].configure(
                    values=[s["display"] for s in self._class_spells])
            else:  # Item
                iid = a.get("item_id") or 0
                w["spell_id"].set("")
                w["item_id"].set(str(iid) if iid else "")
                if iid:
                    name = db.item_name(int(iid))
                    display = f"{name}  [{iid}]" if name else f"#{iid}"
                    w["combo_text"].set(display)
                    w["id_display"].set(str(iid))
                    w["_combo_widget"].configure(values=[display])
                else:
                    w["combo_text"].set("")
                    w["id_display"].set("")
                    w["_combo_widget"].configure(values=[])

        self._conditions = self._cbs["load_conditions"](eid) if eid else []
        self._refresh_cond_list()

    def _refresh_cond_list(self):
        self._cond_tv.delete(*self._cond_tv.get_children())
        for c in self._conditions:
            self._cond_tv.insert("", "end",
                                 iid=str(c.get("condition_id", id(c))),
                                 values=(c.get("sequence", 0), self._condition_desc(c)))

    def _on_cond_select(self, _=None):
        sel = self._cond_tv.selection()
        if not sel:
            return
        iid = sel[0]
        c = next((x for x in self._conditions
                  if str(x.get("condition_id", id(x))) == iid), None)
        if c:
            self.v_subj.set(c.get("subject_key", SUBJECT_KEYS[0]))
            self.v_stat.set(c.get("stat_key", STAT_KEYS[0]))
            self.v_op.set(COND_OPS.get(c.get("comparison", 4), ">="))
            self.v_nval.set(str(c.get("numeric_value", 0) or 0))
            self.v_sval.set(c.get("string_value", "") or "")
            if c.get("stat_key") == "aura":
                raw_value = c.get("string_value", "") or c.get("numeric_value", "")
                self.v_cond_spell.set(self._condition_spell_display(raw_value))
                self.v_cond_bool.set("False")
            elif c.get("stat_key") in BOOL_STAT_KEYS:
                try:
                    self.v_cond_bool.set(
                        "True" if int(float(c.get("numeric_value", 0) or 0)) else "False")
                except (TypeError, ValueError):
                    self.v_cond_bool.set("False")
                self.v_cond_spell.set("")
            else:
                self.v_cond_spell.set("")
                self.v_cond_bool.set("False")
            self._sync_condition_value_editor()

    def _set_detail_enabled(self, on: bool):
        state = "normal" if on else "disabled"

        def _apply(widget):
            try:
                widget.configure(state=state)
            except Exception:
                pass
            for child in widget.winfo_children():
                _apply(child)

        for w in self.winfo_children():
            _apply(w)

    # ── Entry CRUD ───────────────────────────────────────────────────────────

    def _add_entry(self):
        if not self._profile_id:
            return
        e = dict(priority=len(self._entries), label="New entry",
                 is_interrupt=0, breaks_current_cast=0, enabled=1, condition_logic=0)
        eid = self._cbs["upsert_entry"](e, self._profile_id)
        e["entry_id"] = eid
        self._entries.append(e)
        self._refresh_entry_list()
        # Select the new row
        self._tv.selection_set(str(eid))
        self._on_entry_select()

    def _del_entry(self):
        if not self._sel_entry:
            return
        if not messagebox.askyesno("Confirm", "Delete this entry and all its actions/conditions?"):
            return
        eid = self._sel_entry.get("entry_id")
        if eid:
            self._cbs["delete_entry"](eid)
        self._entries.remove(self._sel_entry)
        self._sel_entry = None
        self._refresh_entry_list()
        self._set_detail_enabled(False)

    def _move_up(self):
        self._swap_priority(-1)

    def _move_down(self):
        self._swap_priority(1)

    def _swap_priority(self, direction: int):
        if not self._sel_entry:
            return
        idx = self._entries.index(self._sel_entry)
        new_idx = idx + direction
        if new_idx < 0 or new_idx >= len(self._entries):
            return
        # Swap priorities
        a, b = self._entries[idx], self._entries[new_idx]
        a["priority"], b["priority"] = b["priority"], a["priority"]
        self._cbs["upsert_entry"](a, self._profile_id)
        self._cbs["upsert_entry"](b, self._profile_id)
        self._entries[idx], self._entries[new_idx] = b, a
        self._refresh_entry_list()
        self._tv.selection_set(str(self._sel_entry.get("entry_id", id(self._sel_entry))))

    def _save_entry(self):
        if not self._sel_entry or not self._profile_id:
            return
        self._sel_entry["label"]              = self.v_label.get().strip()
        self._sel_entry["priority"]           = int(self.v_priority.get() or 0)
        self._sel_entry["enabled"]            = int(self.v_enabled.get())
        self._sel_entry["is_interrupt"]       = int(self.v_interrupt.get())
        self._sel_entry["breaks_current_cast"]= int(self.v_break.get())
        self._sel_entry["condition_logic"]    = COND_LOGIC_INV.get(self.v_cond_logic.get(), 0)

        eid = self._cbs["upsert_entry"](self._sel_entry, self._profile_id)
        if not self._sel_entry.get("entry_id"):
            self._sel_entry["entry_id"] = eid

        # Save actions
        existing = {a["slot"]: a for a in self._cbs["load_actions"](eid)}
        for slot, w in self._action_widgets.items():
            a = existing.get(slot, {})
            aoe_mode_text = w["aoe_mode"].get().strip()
            aoe_min_text = w["aoe_min"].get().strip()
            aoe_radius_text = w["aoe_radius"].get().strip()
            a.update(slot=slot,
                     action_type=ACTION_INV.get(w["type"].get(), 0),
                     spell_base_id=int(w["spell_id"].get() or 0),
                     item_id=int(w["item_id"].get() or 0),
                     rank_mode=RANK_INV.get(w["rank_mode"].get(), 0),
                     rank_value=int(w["rank_val"].get() or 0),
                     target_key=w["target"].get() or "enemy",
                     aoe_mode=AOE_INV.get(aoe_mode_text) if aoe_mode_text else None,
                     aoe_min_targets=int(aoe_min_text) if aoe_min_text else None,
                     aoe_radius=float(aoe_radius_text) if aoe_radius_text else None)
            self._cbs["upsert_action"](a, eid)

        self._refresh_entry_list()
        self._tv.selection_set(str(eid))

    # ── Condition CRUD ────────────────────────────────────────────────────────

    def _save_condition(self):
        if not self._sel_entry:
            return
        eid = self._sel_entry.get("entry_id")
        if not eid:
            messagebox.showwarning("Save entry first", "Save the entry before adding conditions.")
            return
        # Find if editing existing
        sel = self._cond_tv.selection()
        existing_id = None
        if sel:
            iid = sel[0]
            matched = next((c for c in self._conditions
                            if str(c.get("condition_id", id(c))) == iid), None)
            if matched:
                existing_id = matched.get("condition_id")

        seq = (max((c.get("sequence", 0) for c in self._conditions), default=-1) + 1
               if not existing_id else
               next(c.get("sequence", 0) for c in self._conditions
                    if c.get("condition_id") == existing_id))

        numeric_value = float(self.v_nval.get() or 0)
        string_value = self.v_sval.get()
        if self.v_stat.get() == "aura":
            self._on_cond_spell_typed()
            numeric_value = float(self.v_nval.get() or 0)
            string_value = ""
        elif self.v_stat.get() in BOOL_STAT_KEYS:
            self._on_cond_bool_changed()
            numeric_value = float(self.v_nval.get() or 0)
            string_value = ""

        c = dict(condition_id=existing_id, sequence=seq,
                 subject_key=self.v_subj.get(), stat_key=self.v_stat.get(),
                 comparison=COND_OPS_INV.get(self.v_op.get(), 4),
                 numeric_value=numeric_value,
                 string_value=string_value)
        self._cbs["upsert_condition"](c, eid)
        self._conditions = self._cbs["load_conditions"](eid)
        self._refresh_cond_list()

    def _del_condition(self):
        sel = self._cond_tv.selection()
        if not sel or not self._sel_entry:
            return
        iid = sel[0]
        c = next((x for x in self._conditions
                  if str(x.get("condition_id", id(x))) == iid), None)
        if c and c.get("condition_id"):
            self._cbs["delete_condition"](c["condition_id"])
            self._conditions.remove(c)
            self._refresh_cond_list()


def _class_from_spec(*texts: str) -> int | None:
    """Guess class_id from profile text such as spec_key or display_name."""
    blob = " ".join(t for t in texts if t).strip()
    if not blob:
        return None
    sk = blob.lower()
    for name, cid in SPEC_TO_CLASS.items():
        if name in sk:
            return cid
    normalized = sk.replace("_", " ").replace("/", " ")
    for alias, cid in SPEC_ALIAS_TO_CLASS.items():
        if alias in normalized:
            return cid
    return None


# ═══════════════════════════════════════════════════════════════════════════
#  TAB: CLASS DEFAULTS
# ═══════════════════════════════════════════════════════════════════════════

class DefaultProfilesTab(ttk.Frame):

    def __init__(self, parent, **kw):
        super().__init__(parent, **kw)
        self._profiles  = []
        self._sel       = None
        self._build()

    def _build(self):
        pane = ttk.PanedWindow(self, orient=tk.HORIZONTAL)
        pane.pack(fill=tk.BOTH, expand=True)

        # ── Left: profile list ───────────────────────────────────────────────
        left = ttk.Frame(pane, width=220)
        pane.add(left, weight=0)

        ttk.Label(left, text="Default profiles").pack(anchor="w", padx=4, pady=2)
        self._lb = tk.Listbox(left, selectmode=tk.SINGLE, exportselection=False)
        self._lb.pack(fill=tk.BOTH, expand=True, padx=4)
        self._lb.bind("<<ListboxSelect>>", self._on_select)

        btn = ttk.Frame(left)
        btn.pack(fill=tk.X, padx=4, pady=4)
        ttk.Button(btn, text="+ New",   command=self._new_profile).pack(side=tk.LEFT, padx=2)
        ttk.Button(btn, text="✕ Delete",command=self._del_profile).pack(side=tk.LEFT, padx=2)

        # ── Right: editor ────────────────────────────────────────────────────
        right = ttk.Frame(pane)
        pane.add(right, weight=1)

        self._hdr = ProfileHeaderFrame(right, is_default=True)
        self._hdr.pack(fill=tk.X, padx=4, pady=4)

        save_row = ttk.Frame(right)
        save_row.pack(fill=tk.X, padx=4)
        ttk.Button(save_row, text="💾 Save profile header",
                   command=self._save_header).pack(side=tk.LEFT, padx=2)

        ttk.Separator(right, orient="horizontal").pack(fill=tk.X, pady=4)

        cbs = dict(
            load_entries    = db.load_default_entries,
            upsert_entry    = db.upsert_default_entry,
            delete_entry    = db.delete_default_entry,
            load_actions    = db.load_default_actions,
            upsert_action   = db.upsert_default_action,
            load_conditions = db.load_default_conditions,
            upsert_condition= db.upsert_default_condition,
            delete_condition= db.delete_default_condition,
        )
        self._rot = RotationEditor(right, cbs)
        self._hdr._on_class_change_cb = self._rot.set_class
        self._rot.pack(fill=tk.BOTH, expand=True, padx=4, pady=4)

    def refresh(self):
        if not db.ok():
            return
        try:
            self._profiles = db.load_default_profiles()
            self._lb.delete(0, tk.END)
            for p in self._profiles:
                self._lb.insert(tk.END, p.get("display_name") or
                                f"{p['spec_key']} {p['role_key']}")
            self._sel = None
            self._hdr.clear()
            self._rot.clear()
        except MySQLError as e:
            messagebox.showerror("DB error", str(e))

    def _on_select(self, _=None):
        sel = self._lb.curselection()
        if not sel:
            return
        self._sel = self._profiles[sel[0]]
        self._hdr.load(self._sel)
        self._rot.load_profile(self._sel["default_profile_id"])

    def _save_header(self):
        if not self._sel:
            return
        self._hdr.collect(self._sel)
        try:
            db.upsert_default_profile(self._sel)
            self.refresh()
        except MySQLError as e:
            messagebox.showerror("DB error", str(e))

    def _new_profile(self):
        if not db.ok():
            return
        existing = {(p.get("spec_key", ""), _normalize_role(p.get("role_key", "")))
                    for p in self._profiles}
        next_idx = 1
        while (f"NewSpec{next_idx}", "DPS") in existing:
            next_idx += 1
        p = dict(spec_key=f"NewSpec{next_idx}", role_key="DPS",
                 display_name=f"New Profile {next_idx}",
                 conservation_mode=1, mana_low_water=55, mana_high_water=75,
                 enable_down_rank=1, down_rank_floor=2,
                 default_aoe_mode=0, default_aoe_min_targets=2, default_aoe_scan_radius=10.0)
        try:
            pid = db.upsert_default_profile(p)
            p["default_profile_id"] = pid
            self.refresh()
            # Select the new one
            idx = next((i for i, x in enumerate(self._profiles)
                        if x["default_profile_id"] == pid), None)
            if idx is not None:
                self._lb.selection_set(idx)
                self._on_select()
        except MySQLError as e:
            messagebox.showerror("DB error", str(e))

    def _del_profile(self):
        if not self._sel:
            return
        if not messagebox.askyesno("Confirm",
                f"Delete '{self._sel.get('display_name')}' and all its entries?"):
            return
        try:
            db.delete_default_profile(self._sel["default_profile_id"])
            self._sel = None
            self.refresh()
        except MySQLError as e:
            messagebox.showerror("DB error", str(e))


# ═══════════════════════════════════════════════════════════════════════════
#  TAB: BOT PROFILES  (per source character)
# ═══════════════════════════════════════════════════════════════════════════

class BotProfilesTab(ttk.Frame):

    def __init__(self, parent, **kw):
        super().__init__(parent, **kw)
        self._accounts = []
        self._chars    = []
        self._profiles = []
        self._account_by_label = {}
        self._selected_account_id = None
        self._sel_char = None
        self._sel_prof = None
        self._build()

    def _build(self):
        pane = ttk.PanedWindow(self, orient=tk.HORIZONTAL)
        pane.pack(fill=tk.BOTH, expand=True)

        # ── Left: character + profile slot list ─────────────────────────────
        left = ttk.Frame(pane, width=240)
        pane.add(left, weight=0)

        ttk.Label(left, text="Account").pack(anchor="w", padx=4, pady=(4, 0))
        self.v_account = tk.StringVar()
        self._acct_cb = ttk.Combobox(left, textvariable=self.v_account, state="readonly", width=34)
        self._acct_cb.pack(fill=tk.X, padx=4)
        self._acct_cb.bind("<<ComboboxSelected>>", self._on_account_select)

        ttk.Label(left, text="Characters").pack(anchor="w", padx=4, pady=(4, 0))
        char_cols = ("name", "lvl", "class")
        self._char_tv = ttk.Treeview(left, columns=char_cols, show="headings",
                                     height=8, selectmode="browse")
        self._char_tv.heading("name",  text="Name")
        self._char_tv.heading("lvl",   text="Lvl")
        self._char_tv.heading("class", text="Class")
        self._char_tv.column("name",  width=100)
        self._char_tv.column("lvl",   width=30, anchor="center")
        self._char_tv.column("class", width=80)
        self._char_tv.pack(fill=tk.X, padx=4)
        self._char_tv.bind("<<TreeviewSelect>>", self._on_char_select)

        ttk.Separator(left, orient="horizontal").pack(fill=tk.X, pady=4)

        ttk.Label(left, text="Profile slots (1-10)").pack(anchor="w", padx=4)
        self._prof_lb = tk.Listbox(left, selectmode=tk.SINGLE, exportselection=False, height=10)
        self._prof_lb.pack(fill=tk.BOTH, expand=True, padx=4)
        self._prof_lb.bind("<<ListboxSelect>>", self._on_prof_select)

        btn = ttk.Frame(left)
        btn.pack(fill=tk.X, padx=4, pady=4)
        ttk.Button(btn, text="+ New slot",  command=self._new_profile).pack(side=tk.LEFT, padx=2)
        ttk.Button(btn, text="✕ Delete",    command=self._del_profile).pack(side=tk.LEFT, padx=2)

        # ── Right: profile editor ────────────────────────────────────────────
        right = ttk.Frame(pane)
        pane.add(right, weight=1)

        self._hdr = ProfileHeaderFrame(right, is_default=False)
        self._hdr.pack(fill=tk.X, padx=4, pady=4)

        save_row = ttk.Frame(right)
        save_row.pack(fill=tk.X, padx=4)
        ttk.Button(save_row, text="💾 Save profile header",
                   command=self._save_header).pack(side=tk.LEFT, padx=2)

        ttk.Separator(right, orient="horizontal").pack(fill=tk.X, pady=4)

        cbs = dict(
            load_entries    = db.load_bot_entries,
            upsert_entry    = db.upsert_bot_entry,
            delete_entry    = db.delete_bot_entry,
            load_actions    = db.load_bot_actions,
            upsert_action   = db.upsert_bot_action,
            load_conditions = db.load_bot_conditions,
            upsert_condition= db.upsert_bot_condition,
            delete_condition= db.delete_bot_condition,
        )
        self._rot = RotationEditor(right, cbs)
        self._hdr._on_class_change_cb = self._rot.set_class
        self._rot.pack(fill=tk.BOTH, expand=True, padx=4, pady=4)

    def _account_label(self, row: dict) -> str:
        return f"{row.get('username', '')} [{row.get('account_id')}] ({row.get('char_count', 0)} chars)"

    def _select_account_id(self, account_id: int | None):
        if not self._accounts:
            self.v_account.set("")
            self._selected_account_id = None
            return

        if account_id is None:
            account_id = self._accounts[0].get("account_id")

        for row in self._accounts:
            if row.get("account_id") == account_id:
                label = self._account_label(row)
                self.v_account.set(label)
                self._selected_account_id = account_id
                self._load_characters_for_selected_account()
                return

        first = self._accounts[0]
        self.v_account.set(self._account_label(first))
        self._selected_account_id = first.get("account_id")
        self._load_characters_for_selected_account()

    def _load_characters_for_selected_account(self):
        self._char_tv.delete(*self._char_tv.get_children())
        self._sel_char = None
        self._sel_prof = None
        self._profiles = []
        self._prof_lb.delete(0, tk.END)
        self._hdr.clear()
        self._rot.clear()

        if not self._selected_account_id:
            return

        self._chars = db.load_source_characters_for_account(self._selected_account_id)
        for c in self._chars:
            cls = WOW_CLASSES.get(c.get("class", 0), str(c.get("class", "")))
            self._char_tv.insert("", "end", iid=str(c["guid"]),
                                 values=(c["name"], c["level"], cls))

    def _on_account_select(self, _=None):
        label = self.v_account.get()
        account_id = self._account_by_label.get(label)
        self._selected_account_id = account_id
        app = self.winfo_toplevel()
        if hasattr(app, "set_preferred_game_account") and account_id:
            app.set_preferred_game_account(account_id)
        self._load_characters_for_selected_account()

    def refresh(self):
        if not db.ok():
            return
        try:
            self._accounts = db.load_player_accounts()
            labels = [self._account_label(row) for row in self._accounts]
            self._account_by_label = {self._account_label(row): row.get("account_id") for row in self._accounts}
            self._acct_cb.configure(values=labels)

            preferred_id = None
            app = self.winfo_toplevel()
            if hasattr(app, "get_preferred_game_account"):
                preferred_id = app.get_preferred_game_account()

            self._select_account_id(preferred_id)
        except MySQLError as e:
            messagebox.showerror("DB error", str(e))

    def _on_char_select(self, _=None):
        sel = self._char_tv.selection()
        if not sel:
            return
        guid = int(sel[0])
        self._sel_char = next((c for c in self._chars if c["guid"] == guid), None)
        if not self._sel_char:
            return
        # Pre-load class spells so the rotation editor is ready when a profile loads
        self._rot.set_class(self._sel_char.get("class"))
        try:
            self._profiles = db.load_bot_profiles(guid)
            self._prof_lb.delete(0, tk.END)
            for p in self._profiles:
                self._prof_lb.insert(tk.END,
                    f"Slot {p['slot']}: {p.get('profile_name') or '(unnamed)'}")
            self._sel_prof = None
            self._hdr.clear()
            self._rot.clear()
        except MySQLError as e:
            messagebox.showerror("DB error", str(e))

    def _on_prof_select(self, _=None):
        sel = self._prof_lb.curselection()
        if not sel:
            return
        self._sel_prof = self._profiles[sel[0]]
        self._hdr.load(self._sel_prof, forced_class_id=self._sel_char.get("class") if self._sel_char else None)
        self._rot.load_profile(self._sel_prof["profile_id"])

    def _save_header(self):
        if not self._sel_prof or not self._sel_char:
            return
        self._hdr.collect(self._sel_prof)
        try:
            db.upsert_bot_profile(self._sel_prof)
            self._on_char_select()   # refresh slot list
        except MySQLError as e:
            messagebox.showerror("DB error", str(e))

    def _default_profile_options_for_class(self, class_id: int) -> list[tuple[str, int | None]]:
        options = [("Blank slate", None)]
        try:
            defaults = db.load_default_profiles()
        except MySQLError:
            return options

        for p in defaults:
            pid = p.get("default_profile_id")
            profile_class = _class_from_spec(p.get("spec_key", ""), p.get("display_name", ""))
            if profile_class != class_id:
                continue
            label = p.get("display_name") or f"{p.get('spec_key', '')} / {p.get('role_key', '')}"
            options.append((label, pid))
        return options

    def _copy_default_profile_to_bot(self, default_profile_id: int, bot_profile_id: int):
        defaults = {p["default_profile_id"]: p for p in db.load_default_profiles()}
        src = defaults.get(default_profile_id)
        if not src:
            return

        target = next((p for p in self._profiles if p.get("profile_id") == bot_profile_id), None)
        if not target:
            rows = db.load_bot_profiles(self._sel_char["guid"])
            target = next((p for p in rows if p.get("profile_id") == bot_profile_id), None)
        if not target:
            return

        target["spec_override_key"] = src.get("spec_key") or None
        target["role_override_key"] = _normalize_role(src.get("role_key", "")) or None
        target["conservation_mode"] = src.get("conservation_mode", 1)
        target["mana_low_water"] = src.get("mana_low_water", 55)
        target["mana_high_water"] = src.get("mana_high_water", 75)
        target["enable_down_rank"] = src.get("enable_down_rank", 1)
        target["down_rank_floor"] = src.get("down_rank_floor", 2)
        if "default_aoe_mode" in src:
            target["default_aoe_mode"] = src.get("default_aoe_mode", 0)
        if "default_aoe_min_targets" in src:
            target["default_aoe_min_targets"] = src.get("default_aoe_min_targets", 2)
        if "default_aoe_scan_radius" in src:
            target["default_aoe_scan_radius"] = src.get("default_aoe_scan_radius", 10.0)
        db.upsert_bot_profile(target)

        for src_entry in db.load_default_entries(default_profile_id):
            new_entry = dict(
                priority=src_entry.get("priority", 0),
                label=src_entry.get("label", ""),
                is_interrupt=src_entry.get("is_interrupt", 0),
                breaks_current_cast=src_entry.get("breaks_current_cast", 0),
                enabled=src_entry.get("enabled", 1),
                condition_logic=src_entry.get("condition_logic", 0),
            )
            new_entry_id = db.upsert_bot_entry(new_entry, bot_profile_id)

            for src_action in db.load_default_actions(src_entry["entry_id"]):
                new_action = dict(
                    slot=src_action.get("slot", 0),
                    action_type=src_action.get("action_type", 0),
                    spell_base_id=src_action.get("spell_base_id", 0),
                    item_id=src_action.get("item_id", 0),
                    rank_mode=src_action.get("rank_mode", 0),
                    rank_value=src_action.get("rank_value", 0),
                    target_key=src_action.get("target_key", "enemy"),
                    aoe_mode=src_action.get("aoe_mode"),
                    aoe_min_targets=src_action.get("aoe_min_targets"),
                    aoe_radius=src_action.get("aoe_radius"),
                )
                db.upsert_bot_action(new_action, new_entry_id)

            for src_cond in db.load_default_conditions(src_entry["entry_id"]):
                new_cond = dict(
                    sequence=src_cond.get("sequence", 0),
                    subject_key=src_cond.get("subject_key", SUBJECT_KEYS[0]),
                    stat_key=src_cond.get("stat_key", STAT_KEYS[0]),
                    comparison=src_cond.get("comparison", 4),
                    numeric_value=src_cond.get("numeric_value", 0),
                    string_value=src_cond.get("string_value", ""),
                )
                db.upsert_bot_condition(new_cond, new_entry_id)

    def _new_profile(self):
        try:
            if not db.ok():
                return
            if not self._sel_char:
                messagebox.showinfo("Select character",
                                    "Select a character on the left before creating a bot profile slot.")
                return
            used_slots = {p["slot"] for p in self._profiles}
            slot = next((s for s in range(1, 11) if s not in used_slots), None)
            if slot is None:
                messagebox.showinfo("Full", "All 10 profile slots are already used.")
                return
            picker = DefaultProfilePicker(
                self.winfo_toplevel(),
                self._default_profile_options_for_class(self._sel_char.get("class")),
                title="New Bot Profile")
            if picker.cancelled:
                return
            p = dict(source_character_guid=self._sel_char["guid"],
                     owner_account_id=self._sel_char["account"],
                     slot=slot, profile_name=f"Profile {slot}",
                     guessed_spec_key="", guessed_role_key="DPS",
                     spec_override_key=None, role_override_key=None,
                     conservation_mode=1, mana_low_water=55, mana_high_water=75,
                     enable_down_rank=1, down_rank_floor=2,
                     default_aoe_mode=0, default_aoe_min_targets=2,
                     default_aoe_scan_radius=10.0)
            new_profile_id = db.upsert_bot_profile(p)
            if picker.selection is not None:
                self._copy_default_profile_to_bot(picker.selection, new_profile_id)
            self._on_char_select()
            idx = next((i for i, prof in enumerate(self._profiles)
                        if prof.get("profile_id") == new_profile_id), None)
            if idx is not None:
                self._prof_lb.selection_clear(0, tk.END)
                self._prof_lb.selection_set(idx)
                self._on_prof_select()
        except MySQLError as e:
            messagebox.showerror("DB error", str(e))
        except Exception as e:
            messagebox.showerror("New profile error", str(e))

    def _del_profile(self):
        if not self._sel_prof:
            return
        if not messagebox.askyesno("Confirm",
                f"Delete slot {self._sel_prof['slot']} and all its entries?"):
            return
        try:
            db.delete_bot_profile(self._sel_prof["profile_id"])
            self._sel_prof = None
            self._on_char_select()
            self._rot.clear()
        except MySQLError as e:
            messagebox.showerror("DB error", str(e))


# ═══════════════════════════════════════════════════════════════════════════
#  TAB: CHARACTER EDITOR
# ═══════════════════════════════════════════════════════════════════════════

class CharacterEditorTab(ttk.Frame):

    def __init__(self, parent, **kw):
        super().__init__(parent, **kw)
        self._accounts = []
        self._chars = []
        self._selected_account_id = None
        self._account_by_label = {}
        self._sel_char = None
        self._gear_rows = {}
        self._gear_slot_current_rows = {}
        self._selected_gear_row = None
        self._gear_slot_controls = {}
        self._build()

    def _build(self):
        pane = ttk.PanedWindow(self, orient=tk.HORIZONTAL)
        pane.pack(fill=tk.BOTH, expand=True)

        left = ttk.Frame(pane, width=260)
        pane.add(left, weight=0)

        ttk.Label(left, text="Account").pack(anchor="w", padx=4, pady=(4, 0))
        self.v_account = tk.StringVar()
        self._acct_cb = ttk.Combobox(left, textvariable=self.v_account, state="readonly", width=36)
        self._acct_cb.pack(fill=tk.X, padx=4)
        self._acct_cb.bind("<<ComboboxSelected>>", self._on_account_select)

        ttk.Label(left, text="Characters").pack(anchor="w", padx=4, pady=(6, 0))
        cols = ("name", "lvl", "class")
        char_wrap = ttk.Frame(left)
        char_wrap.pack(fill=tk.BOTH, expand=True, padx=4, pady=(0, 4))

        self._char_tv = ttk.Treeview(char_wrap, columns=cols, show="headings", height=18, selectmode="browse")
        self._char_tv.heading("name", text="Name")
        self._char_tv.heading("lvl", text="Lvl")
        self._char_tv.heading("class", text="Class")
        self._char_tv.column("name", width=115)
        self._char_tv.column("lvl", width=40, anchor="center")
        self._char_tv.column("class", width=85)
        char_scroll = ttk.Scrollbar(char_wrap, orient="vertical", command=self._char_tv.yview)
        self._char_tv.configure(yscrollcommand=char_scroll.set)
        self._char_tv.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        char_scroll.pack(side=tk.LEFT, fill=tk.Y)
        self._char_tv.bind("<<TreeviewSelect>>", self._on_char_select)

        right = ttk.Frame(pane)
        pane.add(right, weight=1)

        top_btns = ttk.Frame(right)
        top_btns.pack(fill=tk.X, padx=4, pady=4)
        ttk.Button(top_btns, text="🔄 Refresh Character", command=self._reload_selected_character).pack(side=tk.LEFT, padx=2)
        ttk.Button(top_btns, text="💾 Save Basic Stats", command=self._save_basic_stats).pack(side=tk.LEFT, padx=2)

        summary = ttk.LabelFrame(right, text="Character Summary", padding=6)
        summary.pack(fill=tk.X, padx=4, pady=(0, 4))

        self.v_char_name = tk.StringVar()
        self.v_char_guid = tk.StringVar()
        self.v_char_account = tk.StringVar()
        self.v_char_class = tk.StringVar()
        self.v_char_race = tk.StringVar()
        self.v_char_guild = tk.StringVar()
        self.v_char_online = tk.StringVar()
        self.v_char_created = tk.StringVar()
        self.v_level = tk.StringVar()
        self.v_xp = tk.StringVar()
        self.v_money = tk.StringVar()
        self.v_money_text = tk.StringVar()
        self.v_bank_slots = tk.StringVar()
        self.v_guild_bank_money = tk.StringVar()
        self.v_location = tk.StringVar()
        self.v_stat_summary = tk.StringVar()

        lbl(summary, "Name:", 0, 0)
        entry_w(summary, self.v_char_name, 0, 1, width=18, state="readonly")
        lbl(summary, "GUID:", 0, 2)
        entry_w(summary, self.v_char_guid, 0, 3, width=8, state="readonly")
        lbl(summary, "Account:", 0, 4)
        entry_w(summary, self.v_char_account, 0, 5, width=8, state="readonly")

        lbl(summary, "Class:", 1, 0)
        entry_w(summary, self.v_char_class, 1, 1, width=18, state="readonly")
        lbl(summary, "Race:", 1, 2)
        entry_w(summary, self.v_char_race, 1, 3, width=14, state="readonly")
        lbl(summary, "Online:", 1, 4)
        entry_w(summary, self.v_char_online, 1, 5, width=10, state="readonly")

        lbl(summary, "Level:", 2, 0)
        entry_w(summary, self.v_level, 2, 1, width=8)
        lbl(summary, "XP:", 2, 2)
        entry_w(summary, self.v_xp, 2, 3, width=12)
        lbl(summary, "Bank Slots:", 2, 4)
        entry_w(summary, self.v_bank_slots, 2, 5, width=8)

        lbl(summary, "Gold (copper):", 3, 0)
        entry_w(summary, self.v_money, 3, 1, width=12)
        lbl(summary, "Gold:", 3, 2)
        entry_w(summary, self.v_money_text, 3, 3, width=16, state="readonly")
        lbl(summary, "Guild Bank Gold:", 3, 4)
        entry_w(summary, self.v_guild_bank_money, 3, 5, width=16, state="readonly")

        lbl(summary, "Guild:", 4, 0)
        entry_w(summary, self.v_char_guild, 4, 1, width=18, state="readonly")
        lbl(summary, "Location:", 4, 2)
        entry_w(summary, self.v_location, 4, 3, width=18, state="readonly")
        lbl(summary, "Created:", 4, 4)
        entry_w(summary, self.v_char_created, 4, 5, width=18, state="readonly")

        ttk.Label(summary, text="Stored Stats:").grid(row=5, column=0, sticky="nw", padx=4, pady=2)
        ttk.Label(summary, textvariable=self.v_stat_summary, justify="left", wraplength=700).grid(
            row=5, column=1, columnspan=5, sticky="w", padx=4, pady=2)

        detail_nb = ttk.Notebook(right)
        detail_nb.pack(fill=tk.BOTH, expand=True, padx=4, pady=4)

        rep_tab = ttk.Frame(detail_nb)
        gear_tab = ttk.Frame(detail_nb)
        inv_tab = ttk.Frame(detail_nb)
        ach_tab = ttk.Frame(detail_nb)
        detail_nb.add(rep_tab, text="  Reputation  ")
        detail_nb.add(gear_tab, text="  Equipped Gear  ")
        detail_nb.add(inv_tab, text="  Inventory  ")
        detail_nb.add(ach_tab, text="  Achievements  ")

        rep_wrap = ttk.Frame(rep_tab)
        rep_wrap.pack(fill=tk.BOTH, expand=True)
        self._rep_tv = ttk.Treeview(rep_wrap, columns=("faction_id", "faction_name", "standing", "flags"), show="headings")
        self._rep_tv.heading("faction_id", text="Faction ID")
        self._rep_tv.heading("faction_name", text="Faction")
        self._rep_tv.heading("standing", text="Standing")
        self._rep_tv.heading("flags", text="Flags")
        self._rep_tv.column("faction_id", width=90, anchor="center")
        self._rep_tv.column("faction_name", width=260)
        self._rep_tv.column("standing", width=110, anchor="center")
        self._rep_tv.column("flags", width=90, anchor="center")
        rep_scroll = ttk.Scrollbar(rep_wrap, orient="vertical", command=self._rep_tv.yview)
        self._rep_tv.configure(yscrollcommand=rep_scroll.set)
        self._rep_tv.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        rep_scroll.pack(side=tk.LEFT, fill=tk.Y)

        quick_gear = ttk.LabelFrame(gear_tab, text="Equipped Gear Editor", padding=6)
        quick_gear.pack(fill=tk.BOTH, expand=True, padx=4, pady=4)

        gear_canvas = tk.Canvas(quick_gear, highlightthickness=0)
        gear_scroll = ttk.Scrollbar(quick_gear, orient="vertical", command=gear_canvas.yview)
        self._gear_rows_frame = ttk.Frame(gear_canvas)

        self._gear_rows_frame.bind(
            "<Configure>",
            lambda _e: gear_canvas.configure(scrollregion=gear_canvas.bbox("all"))
        )

        gear_canvas.create_window((0, 0), window=self._gear_rows_frame, anchor="nw")
        gear_canvas.configure(yscrollcommand=gear_scroll.set)

        gear_canvas.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        gear_scroll.pack(side=tk.LEFT, fill=tk.Y)

        ttk.Label(self._gear_rows_frame, text="Quality Filter").grid(row=0, column=1, sticky="w", padx=4, pady=(0, 4))
        ttk.Label(self._gear_rows_frame, text="Item").grid(row=0, column=2, sticky="w", padx=4, pady=(0, 4))
        ttk.Label(self._gear_rows_frame, text="Enchant").grid(row=0, column=3, sticky="w", padx=4, pady=(0, 4))
        ttk.Label(self._gear_rows_frame, text="Gems").grid(row=0, column=4, sticky="w", padx=4, pady=(0, 4))

        row_idx = 1
        for slot_id, slot_name in EQUIPMENT_SLOT_NAMES.items():
            var = tk.StringVar()
            quality_var = tk.StringVar(value=ITEM_QUALITY_LABELS[0])
            enchant_var = tk.StringVar()
            ttk.Label(self._gear_rows_frame, text=f"{slot_name}:").grid(row=row_idx, column=0, sticky="w", padx=4, pady=2)

            quality_combo = ttk.Combobox(
                self._gear_rows_frame,
                textvariable=quality_var,
                values=ITEM_QUALITY_LABELS,
                width=18,
                state="readonly",
            )
            quality_combo.grid(row=row_idx, column=1, sticky="w", padx=4, pady=2)

            combo = ttk.Combobox(self._gear_rows_frame, textvariable=var, width=34, state="readonly")
            combo.grid(row=row_idx, column=2, sticky="we", padx=4, pady=2)
            combo.bind("<<ComboboxSelected>>", lambda _e, sid=slot_id: self._refresh_gear_slot_row(sid))

            enchant_combo = ttk.Combobox(self._gear_rows_frame, textvariable=enchant_var, width=28, state="readonly")
            enchant_combo.grid(row=row_idx, column=3, sticky="we", padx=4, pady=2)

            gem_frame = ttk.Frame(self._gear_rows_frame)
            gem_frame.grid(row=row_idx, column=4, sticky="we", padx=4, pady=2)
            gem_controls = []
            for gem_index in range(3):
                gem_var = tk.StringVar()
                gem_combo = ttk.Combobox(gem_frame, textvariable=gem_var, width=22, state="readonly")
                gem_combo.grid(row=0, column=gem_index, sticky="we", padx=(0 if gem_index == 0 else 4, 0))
                gem_controls.append({"var": gem_var, "combo": gem_combo})

            quality_combo.bind("<<ComboboxSelected>>", lambda _e, sid=slot_id: self._refresh_gear_slot_row(sid))
            self._gear_slot_controls[slot_id] = {
                "var": var,
                "combo": combo,
                "quality_var": quality_var,
                "quality_combo": quality_combo,
                "enchant_var": enchant_var,
                "enchant_combo": enchant_combo,
                "gem_frame": gem_frame,
                "gems": gem_controls,
            }
            row_idx += 1

        ttk.Button(self._gear_rows_frame, text="Apply Slot Rows", command=self._apply_gear_slot_rows).grid(
            row=row_idx, column=2, sticky="w", padx=4, pady=(8, 2))
        self._gear_rows_frame.columnconfigure(2, weight=1)
        self._gear_rows_frame.columnconfigure(3, weight=1)
        self._gear_rows_frame.columnconfigure(4, weight=1)

        inv_wrap = ttk.Frame(inv_tab)
        inv_wrap.pack(fill=tk.BOTH, expand=True)
        self._inv_tv = ttk.Treeview(inv_wrap, columns=("bag", "slot", "entry", "name", "count", "item_guid"), show="headings")
        self._inv_tv.heading("bag", text="Bag")
        self._inv_tv.heading("slot", text="Slot")
        self._inv_tv.heading("entry", text="Entry")
        self._inv_tv.heading("name", text="Item")
        self._inv_tv.heading("count", text="Count")
        self._inv_tv.heading("item_guid", text="Item GUID")
        self._inv_tv.column("bag", width=60, anchor="center")
        self._inv_tv.column("slot", width=60, anchor="center")
        self._inv_tv.column("entry", width=70, anchor="center")
        self._inv_tv.column("name", width=300)
        self._inv_tv.column("count", width=60, anchor="center")
        self._inv_tv.column("item_guid", width=90, anchor="center")
        inv_scroll = ttk.Scrollbar(inv_wrap, orient="vertical", command=self._inv_tv.yview)
        self._inv_tv.configure(yscrollcommand=inv_scroll.set)
        self._inv_tv.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        inv_scroll.pack(side=tk.LEFT, fill=tk.Y)

        ach_wrap = ttk.Frame(ach_tab)
        ach_wrap.pack(fill=tk.BOTH, expand=True)
        self._ach_tv = ttk.Treeview(ach_wrap, columns=("achievement", "name", "date"), show="headings")
        self._ach_tv.heading("achievement", text="Achievement")
        self._ach_tv.heading("name", text="Name")
        self._ach_tv.heading("date", text="Earned")
        self._ach_tv.column("achievement", width=100, anchor="center")
        self._ach_tv.column("name", width=360)
        self._ach_tv.column("date", width=160)
        ach_scroll = ttk.Scrollbar(ach_wrap, orient="vertical", command=self._ach_tv.yview)
        self._ach_tv.configure(yscrollcommand=ach_scroll.set)
        self._ach_tv.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        ach_scroll.pack(side=tk.LEFT, fill=tk.Y)

        self._clear_detail()

    def _account_label(self, row: dict) -> str:
        return f"{row.get('username', '')} [{row.get('account_id')}] ({row.get('char_count', 0)} chars)"

    def _clear_detail(self):
        self._sel_char = None
        self._gear_rows = {}
        self._gear_slot_current_rows = {}
        for var in (
            self.v_char_name, self.v_char_guid, self.v_char_account, self.v_char_class,
            self.v_char_race, self.v_char_guild, self.v_char_online, self.v_char_created,
            self.v_level, self.v_xp, self.v_money, self.v_money_text, self.v_bank_slots,
            self.v_guild_bank_money, self.v_location, self.v_stat_summary
        ):
            var.set("")
        self._rep_tv.delete(*self._rep_tv.get_children())
        self._inv_tv.delete(*self._inv_tv.get_children())
        self._ach_tv.delete(*self._ach_tv.get_children())
        for controls in self._gear_slot_controls.values():
            controls["var"].set("")
            controls["combo"].configure(values=[""])
            controls["quality_var"].set(ITEM_QUALITY_LABELS[0])
            controls["enchant_var"].set("")
            controls["enchant_combo"].configure(values=[""])
            for gem in controls["gems"]:
                gem["var"].set("")
                gem["combo"].configure(values=[""])
                gem["combo"].grid_remove()

    def _select_account_id(self, account_id: int | None):
        if not self._accounts:
            self.v_account.set("")
            self._selected_account_id = None
            self._char_tv.delete(*self._char_tv.get_children())
            self._clear_detail()
            return

        if account_id is None:
            account_id = self._accounts[0].get("account_id")

        for row in self._accounts:
            if row.get("account_id") == account_id:
                self.v_account.set(self._account_label(row))
                self._selected_account_id = account_id
                self._load_characters_for_selected_account()
                return

        first = self._accounts[0]
        self.v_account.set(self._account_label(first))
        self._selected_account_id = first.get("account_id")
        self._load_characters_for_selected_account()

    def _load_characters_for_selected_account(self):
        self._char_tv.delete(*self._char_tv.get_children())
        self._clear_detail()

        if not self._selected_account_id:
            return

        self._chars = db.load_source_characters_for_account(self._selected_account_id)
        for c in self._chars:
            cls = WOW_CLASSES.get(c.get("class", 0), str(c.get("class", "")))
            self._char_tv.insert("", "end", iid=str(c["guid"]), values=(c["name"], c["level"], cls))

    def _on_account_select(self, _=None):
        label = self.v_account.get()
        account_id = self._account_by_label.get(label)
        self._selected_account_id = account_id
        app = self.winfo_toplevel()
        if hasattr(app, "set_preferred_game_account") and account_id:
            app.set_preferred_game_account(account_id)
        self._load_characters_for_selected_account()

    def refresh(self):
        if not db.ok():
            return
        try:
            self._accounts = db.load_player_accounts()
            labels = [self._account_label(row) for row in self._accounts]
            self._account_by_label = {self._account_label(row): row.get("account_id") for row in self._accounts}
            self._acct_cb.configure(values=labels)

            preferred_id = None
            app = self.winfo_toplevel()
            if hasattr(app, "get_preferred_game_account"):
                preferred_id = app.get_preferred_game_account()

            self._select_account_id(preferred_id)
        except MySQLError as e:
            messagebox.showerror("DB error", str(e))

    def _reload_selected_character(self):
        if self._sel_char:
            self._load_character_data(self._sel_char["guid"])

    def _on_char_select(self, _=None):
        sel = self._char_tv.selection()
        if not sel:
            return
        guid = int(sel[0])
        self._sel_char = next((c for c in self._chars if c["guid"] == guid), None)
        if not self._sel_char:
            return
        self._load_character_data(guid)

    def _load_character_data(self, guid: int):
        summary = db.load_character_summary(guid)
        if not summary:
            self._clear_detail()
            return

        self.v_char_name.set(summary.get("name", ""))
        self.v_char_guid.set(str(summary.get("guid", "")))
        self.v_char_account.set(str(summary.get("account", "")))
        self.v_char_class.set(WOW_CLASSES.get(summary.get("class", 0), str(summary.get("class", ""))))
        self.v_char_race.set(WOW_RACES.get(summary.get("race", 0), str(summary.get("race", ""))))
        self.v_char_guild.set(summary.get("guild_name") or "")
        self.v_char_online.set("Yes" if summary.get("online") else "No")
        self.v_char_created.set(str(summary.get("creation_date", "") or ""))
        self.v_level.set(str(summary.get("level", 0)))
        self.v_xp.set(str(summary.get("xp", 0)))
        self.v_money.set(str(summary.get("money", 0)))
        self.v_money_text.set(money_text(summary.get("money", 0)))
        self.v_bank_slots.set(str(summary.get("bankSlots", 0)))
        guild_bank_money = summary.get("guild_bank_money")
        self.v_guild_bank_money.set(money_text(guild_bank_money) if guild_bank_money is not None else "")
        self.v_location.set(f"Map {summary.get('map', 0)} / Zone {summary.get('zone', 0)}")

        stat_parts = [
            f"Health {summary.get('health', 0)}/{summary.get('maxhealth', 0)}",
            f"Power1 {summary.get('power1', 0)}/{summary.get('maxpower1', 0)}",
            f"Strength {summary.get('strength', 0)}",
            f"Agility {summary.get('agility', 0)}",
            f"Stamina {summary.get('stamina', 0)}",
            f"Intellect {summary.get('intellect', 0)}",
            f"Spirit {summary.get('spirit', 0)}",
            f"Armor {summary.get('armor', 0)}",
            f"AP {summary.get('attackPower', 0)}",
            f"RAP {summary.get('rangedAttackPower', 0)}",
            f"SP {summary.get('spellPower', 0)}",
            f"Resilience {summary.get('resilience', 0)}",
            f"Arena {summary.get('arenaPoints', 0)}",
            f"Honor {summary.get('totalHonorPoints', 0)}",
            f"Kills {summary.get('totalKills', 0)}",
        ]
        self.v_stat_summary.set("  |  ".join(stat_parts))

        self._rep_tv.delete(*self._rep_tv.get_children())
        for row in db.load_character_reputations(guid):
            self._rep_tv.insert("", "end", values=(
                row.get("faction", 0),
                row.get("faction_name") or f"Faction {row.get('faction', 0)}",
                row.get("standing", 0),
                row.get("flags", 0),
            ))

        self._gear_rows = {}
        self._inv_tv.delete(*self._inv_tv.get_children())
        for row in db.load_character_inventory_rows(guid):
            item_name = row.get("item_name") or f"Item {row.get('itemEntry', 0)}"
            values = (
                row.get("itemEntry", 0),
                item_name,
                row.get("count", 1),
                row.get("item_guid", 0),
            )
            if row.get("bag", 0) == 0 and row.get("slot", -1) in EQUIPMENT_SLOT_NAMES:
                item_guid = int(row.get("item_guid", 0) or 0)
                self._gear_rows[item_guid] = dict(row)
            else:
                self._inv_tv.insert("", "end", values=(
                    row.get("bag", 0),
                    row.get("slot", 0),
                    *values,
                ))

        self._ach_tv.delete(*self._ach_tv.get_children())
        for row in db.load_character_achievements(guid):
            self._ach_tv.insert("", "end", values=(
                row.get("achievement", 0),
                row.get("achievement_name") or "",
                unix_text(row.get("date", 0)),
            ))

        self._refresh_gear_slot_rows()

    def _refresh_gear_slot_row(self, slot_id: int):
        controls = self._gear_slot_controls.get(slot_id)
        if not controls:
            return

        class_id = self._sel_char.get("class") if self._sel_char else None
        current_row = next((r for r in self._gear_rows.values() if int(r.get("slot", -1)) == slot_id), None)
        self._gear_slot_current_rows[slot_id] = current_row

        selected_display = (controls["var"].get() or "").strip()
        current_display = ""
        selected_item_id = parse_display_id(selected_display)
        if current_row:
            current_display = item_display_text(current_row.get("itemEntry", 0), current_row.get("item_name") or "")

        quality_id = item_quality_filter_id(controls["quality_var"].get())
        options = db.load_valid_equippable_items_for_slot(class_id, slot_id, quality=quality_id, limit=250)
        displays = [""] + [item_display_text(r.get("entry"), r.get("name") or "") for r in options]

        preferred_display = selected_display or current_display
        if preferred_display and preferred_display not in displays:
            displays.append(preferred_display)

        controls["combo"].configure(values=displays)
        controls["var"].set(preferred_display)

        item_id = selected_item_id or parse_display_id(current_display)
        current_item_id = int(current_row.get("itemEntry", 0) or 0) if current_row else 0
        enchant_values = parse_item_enchantments(current_row.get("enchantments", "") if current_row else "")
        current_enchant_id = get_item_enchant_id(enchant_values, PERM_ENCHANTMENT_SLOT)
        enchant_rows = db.search_item_enchantments("", slot_id=slot_id, limit=250)
        enchant_displays = [""] + [enchant_display_text(r.get("ID"), r.get("Name_Lang_enUS") or "") for r in enchant_rows]
        current_enchant_display = enchant_display_text(current_enchant_id, db.enchant_name(current_enchant_id) or "")
        if current_enchant_display and current_enchant_display not in enchant_displays:
            enchant_displays.append(current_enchant_display)
        controls["enchant_combo"].configure(values=enchant_displays)
        controls["enchant_var"].set(current_enchant_display)

        socket_colors = []
        use_current_item_sockets = current_row and current_item_id and current_item_id == int(item_id or 0)
        if use_current_item_sockets:
            socket_colors = [
                int(current_row.get("socketColor_1", 0) or 0),
                int(current_row.get("socketColor_2", 0) or 0),
                int(current_row.get("socketColor_3", 0) or 0),
            ]

        if item_id and not use_current_item_sockets:
            item_rows = db.q(db.world,
                "SELECT socketColor_1, socketColor_2, socketColor_3 FROM item_template WHERE entry=%s LIMIT 1",
                (int(item_id),))
            if item_rows:
                socket_colors = [
                    int(item_rows[0].get("socketColor_1", 0) or 0),
                    int(item_rows[0].get("socketColor_2", 0) or 0),
                    int(item_rows[0].get("socketColor_3", 0) or 0),
                ]

        socket_slots = [SOCK_ENCHANTMENT_SLOT, SOCK_ENCHANTMENT_SLOT_2, SOCK_ENCHANTMENT_SLOT_3]
        for gem_index, gem_controls in enumerate(controls["gems"]):
            socket_color = socket_colors[gem_index] if gem_index < len(socket_colors) else 0
            gem_slot = socket_slots[gem_index]
            current_gem_enchant = get_item_enchant_id(enchant_values, gem_slot)
            current_gem_item = db.lookup_gem_item_by_enchant(current_gem_enchant) if current_gem_enchant else None
            current_gem_display = item_display_text(
                current_gem_item.get("entry") if current_gem_item else 0,
                current_gem_item.get("name") if current_gem_item else "",
            )

            if socket_color:
                gem_rows = db.search_gem_items("", socket_color=socket_color)
                if not gem_rows:
                    gem_rows = db.search_gem_items("", socket_color=0)
                gem_displays = [""] + [item_display_text(r.get("entry"), r.get("name") or "") for r in gem_rows]
                if current_gem_display and current_gem_display not in gem_displays:
                    gem_displays.append(current_gem_display)
                gem_controls["combo"].configure(values=gem_displays)
                gem_controls["var"].set(current_gem_display)
                gem_controls["combo"].grid()
            else:
                gem_controls["var"].set("")
                gem_controls["combo"].configure(values=[""])
                gem_controls["combo"].grid_remove()

    def _refresh_gear_slot_rows(self):
        self._gear_slot_current_rows = {}
        for slot_id in self._gear_slot_controls:
            self._refresh_gear_slot_row(slot_id)

    def _apply_gear_slot_rows(self):
        if not self._sel_char:
            return
        try:
            char_guid = int(self._sel_char["guid"])
            changed = 0

            for slot_id, controls in self._gear_slot_controls.items():
                selected_item_id = parse_display_id(controls["var"].get())
                current_row = self._gear_slot_current_rows.get(slot_id)

                if not selected_item_id:
                    continue

                enchantments = parse_item_enchantments(
                    current_row.get("enchantments", "") if current_row
                    else build_item_enchantments([0] * (ITEM_ENCHANTMENT_SLOT_COUNT * ITEM_ENCHANTMENT_OFFSET_COUNT))
                )
                selected_enchant_id = parse_display_id(controls["enchant_var"].get()) or 0
                set_item_enchant_id(enchantments, PERM_ENCHANTMENT_SLOT, selected_enchant_id)

                for gem_index, gem_controls in enumerate(controls["gems"]):
                    gem_item_id = parse_display_id(gem_controls["var"].get()) or 0
                    gem_enchant_id = db.get_gem_enchant_id(gem_item_id) if gem_item_id else 0
                    gem_slot = [SOCK_ENCHANTMENT_SLOT, SOCK_ENCHANTMENT_SLOT_2, SOCK_ENCHANTMENT_SLOT_3][gem_index]
                    set_item_enchant_id(enchantments, gem_slot, gem_enchant_id)

                enchantments_text = build_item_enchantments(enchantments)

                if current_row:
                    current_item_id = int(current_row.get("itemEntry", 0) or 0)
                    current_enchantments = current_row.get("enchantments", "") or build_item_enchantments([0] * (ITEM_ENCHANTMENT_SLOT_COUNT * ITEM_ENCHANTMENT_OFFSET_COUNT))
                    if current_item_id == selected_item_id and current_enchantments == enchantments_text:
                        continue

                    db.update_item_instance_equipment(
                        int(current_row.get("item_guid", 0) or 0),
                        selected_item_id,
                        enchantments_text,
                        db.get_item_max_durability(selected_item_id),
                    )
                    changed += 1
                else:
                    item_guid = db.create_equipped_item_for_character(char_guid, slot_id, selected_item_id)
                    db.update_item_instance_equipment(
                        int(item_guid),
                        selected_item_id,
                        enchantments_text,
                        db.get_item_max_durability(selected_item_id),
                    )
                    changed += 1

            if changed:
                self._load_character_data(char_guid)
                messagebox.showinfo("Saved", f"Updated {changed} gear slot(s).")
            else:
                messagebox.showinfo("No changes", "No gear slot changes were applied.")
        except (MySQLError, ValueError) as e:
            messagebox.showerror("Gear update error", str(e))


    def _save_basic_stats(self):
        if not self._sel_char:
            return
        try:
            guid = int(self._sel_char["guid"])
            level = int(self.v_level.get() or 0)
            xp = int(self.v_xp.get() or 0)
            money = int(self.v_money.get() or 0)
            bank_slots = int(self.v_bank_slots.get() or 0)
            db.update_character_core_stats(guid, level, xp, money, bank_slots)
            self.v_money_text.set(money_text(money))
            cls = WOW_CLASSES.get(self._sel_char.get("class", 0), str(self._sel_char.get("class", "")))
            self._char_tv.item(str(guid), values=(self._sel_char.get("name", ""), level, cls))
            self._load_character_data(guid)
            messagebox.showinfo("Saved", f"Updated basic stats for {self._sel_char.get('name', '')}.")
        except (MySQLError, ValueError) as e:
            messagebox.showerror("Save error", str(e))

# ═══════════════════════════════════════════════════════════════════════════
#  TAB: BOT ACCOUNTS
# ═══════════════════════════════════════════════════════════════════════════

class AccountsTab(ttk.Frame):

    def __init__(self, parent, **kw):
        super().__init__(parent, **kw)
        self._rows = []
        self._build()

    def _build(self):
        top = ttk.Frame(self)
        top.pack(fill=tk.X, padx=8, pady=6)
        ttk.Button(top, text="🔄 Refresh",       command=self.refresh).pack(side=tk.LEFT, padx=4)
        ttk.Button(top, text="+ Create account", command=self._create).pack(side=tk.LEFT, padx=4)
        ttk.Button(top, text="Rename",           command=self._rename).pack(side=tk.LEFT, padx=4)
        ttk.Button(top, text="Set password",     command=self._password).pack(side=tk.LEFT, padx=4)
        ttk.Button(top, text="Delete",           command=self._delete).pack(side=tk.LEFT, padx=4)
        ttk.Button(top, text="✓ Pool On",        command=self._enable).pack(side=tk.LEFT, padx=4)
        ttk.Button(top, text="✗ Pool Off",       command=self._disable).pack(side=tk.LEFT, padx=4)

        cols = ("id", "username", "chars", "pool", "enabled", "online")
        self._tv = ttk.Treeview(self, columns=cols, show="headings",
                                selectmode="browse")
        self._tv.heading("id",       text="Account ID")
        self._tv.heading("username", text="Username")
        self._tv.heading("chars",    text="Chars")
        self._tv.heading("pool",     text="Bot Pool")
        self._tv.heading("enabled",  text="Pool Enabled")
        self._tv.heading("online",   text="Online")
        self._tv.column("id",       width=80,  anchor="center")
        self._tv.column("username", width=170)
        self._tv.column("chars",    width=50,  anchor="center")
        self._tv.column("pool",     width=65,  anchor="center")
        self._tv.column("enabled",  width=60,  anchor="center")
        self._tv.column("online",   width=50,  anchor="center")
        self._tv.pack(fill=tk.BOTH, expand=True, padx=8, pady=4)

        # Info bar
        self._info = ttk.Label(self, text="", foreground="#555")
        self._info.pack(anchor="w", padx=8, pady=2)

    def refresh(self):
        if not db.ok():
            return
        try:
            self._rows = db.load_all_accounts()
            self._tv.delete(*self._tv.get_children())
            for r in self._rows:
                en = "✓" if r.get("is_enabled") else "✗"
                in_pool = "✓" if r.get("in_pool") else "✗"
                online = "✓" if r.get("online") else "✗"
                self._tv.insert("", "end",
                                iid=str(r["account_id"]),
                                tags=("enabled",) if r.get("in_pool") and r.get("is_enabled") else ("disabled",),
                                values=(r["account_id"],
                                        r.get("username", ""),
                                        r.get("char_count", 0),
                                        in_pool,
                                        en,
                                        online))
            self._tv.tag_configure("enabled",  foreground="#1a7f1a")
            self._tv.tag_configure("disabled", foreground="#999")
            total   = len(self._rows)
            enabled = sum(1 for r in self._rows if r.get("in_pool") and r.get("is_enabled"))
            in_pool = sum(1 for r in self._rows if r.get("in_pool"))
            self._info.configure(
                text=f"{total} total accounts  |  {in_pool} in bot pool  |  {enabled} pool-enabled")
        except MySQLError as e:
            messagebox.showerror("DB error", str(e))

    def _selected_id(self):
        sel = self._tv.selection()
        return int(sel[0]) if sel else None

    def _enable(self):
        aid = self._selected_id()
        if aid is None:
            return
        db.set_account_enabled(aid, True)
        self.refresh()

    def _disable(self):
        aid = self._selected_id()
        if aid is None:
            return
        db.set_account_enabled(aid, False)
        self.refresh()

    def _create(self):
        if not db.ok():
            return
        username = simpledialog.askstring("New account", "Username:")
        if not username:
            return
        password = simpledialog.askstring("New account",
                                          f"Password for {username.upper()}:", show="*")
        if not password:
            return
        email = simpledialog.askstring("New account", "Email (optional):") or ""
        add_to_pool = messagebox.askyesno("Bot pool", "Add this account to the bot account pool?")
        try:
            aid = db.create_account(username, password, email=email, add_to_pool=add_to_pool)
            messagebox.showinfo("Created",
                f"Account '{username.upper()}' created with ID {aid}.")
            self.refresh()
        except (MySQLError, ValueError) as e:
            messagebox.showerror("DB error", str(e))

    def _rename(self):
        aid = self._selected_id()
        if aid is None:
            return
        row = next((r for r in self._rows if int(r["account_id"]) == aid), None)
        if not row:
            return
        username = simpledialog.askstring("Rename account", "New username:", initialvalue=row.get("username", ""))
        if not username:
            return
        password = simpledialog.askstring("Rename account",
                                          f"New password for {username.upper()}:", show="*")
        if not password:
            return
        try:
            db.rename_account(aid, username, password)
            self.refresh()
        except (MySQLError, ValueError) as e:
            messagebox.showerror("Rename error", str(e))

    def _password(self):
        aid = self._selected_id()
        if aid is None:
            return
        password = simpledialog.askstring("Set password", f"New password for account {aid}:", show="*")
        if not password:
            return
        try:
            db.change_account_password(aid, password)
            messagebox.showinfo("Updated", f"Password updated for account {aid}.")
        except (MySQLError, ValueError) as e:
            messagebox.showerror("Password error", str(e))

    def _delete(self):
        aid = self._selected_id()
        if aid is None:
            return
        if not messagebox.askyesno("Confirm delete", f"Delete account {aid}? This only works if it has no characters."):
            return
        try:
            db.delete_account(aid)
            self.refresh()
        except (MySQLError, ValueError) as e:
            messagebox.showerror("Delete error", str(e))


# ═══════════════════════════════════════════════════════════════════════════
#  MAIN APPLICATION WINDOW
# ═══════════════════════════════════════════════════════════════════════════

class App(tk.Tk):

    def __init__(self):
        super().__init__()
        self.title(f"LivingWorld Bot Editor  v{VERSION}")
        self.geometry("1280x800")
        self.minsize(1000, 640)
        self._preferred_game_account_id = None
        self._auto_connect_enabled = False
        self._build_connection_bar()
        self._build_notebook()
        self._load_saved_config()
        if self._auto_connect_enabled:
            self.after(150, self._connect)

    def _load_saved_config(self):
        cfg = configparser.ConfigParser()
        if os.path.exists(CONFIG_FILE):
            cfg.read(CONFIG_FILE)
        d = cfg["database"] if cfg.has_section("database") else {}
        self.v_host.set(d.get("host", "127.0.0.1"))
        self.v_port.set(d.get("port", "3306"))
        self.v_user.set(d.get("user", "acore"))
        self.v_pass.set(d.get("password", "acore"))

        # SSH tunnel config
        s = cfg["ssh"] if cfg.has_section("ssh") else {}
        self.v_ssh_enabled.set(s.get("enabled", "0") == "1")
        self.v_ssh_host.set(s.get("host", ""))
        self.v_ssh_port.set(s.get("port", "22"))
        self.v_ssh_user.set(s.get("user", ""))
        self.v_ssh_pass.set(s.get("password", ""))
        self.v_ssh_key.set(s.get("key_file", ""))
        self.v_db_host.set(s.get("db_host", "127.0.0.1"))
        self.v_db_port.set(s.get("db_port", "3306"))
        # Load MySQL credentials from SSH section
        self.v_db_user.set(s.get("db_user", d.get("user", "acore")))
        self.v_db_pass.set(s.get("db_password", d.get("password", "acore")))
        pref = cfg["preferences"] if cfg.has_section("preferences") else {}
        pref_id = pref.get("preferred_game_account_id", "").strip()
        self._preferred_game_account_id = int(pref_id) if pref_id.isdigit() else None
        self._auto_connect_enabled = pref.get("auto_connect_on_startup", "0") == "1"
        self.v_ssh_collapsed.set(pref.get("ssh_collapsed", "0") == "1")
        self._toggle_ssh_fields()

    def _save_config(self):
        cfg = configparser.ConfigParser()
        cfg["database"] = {
            "host": self.v_host.get(), "port": self.v_port.get(),
            "user": self.v_user.get(), "password": self.v_pass.get(),
        }
        cfg["ssh"] = {
            "enabled": "1" if self.v_ssh_enabled.get() else "0",
            "host": self.v_ssh_host.get(),
            "port": self.v_ssh_port.get(),
            "user": self.v_ssh_user.get(),
            "password": self.v_ssh_pass.get(),
            "key_file": self.v_ssh_key.get(),
            "db_host": self.v_db_host.get(),
            "db_port": self.v_db_port.get(),
            "db_user": self.v_db_user.get(),
            "db_password": self.v_db_pass.get(),
        }
        cfg["preferences"] = {
            "preferred_game_account_id": str(self._preferred_game_account_id or ""),
            "auto_connect_on_startup": "1" if self._auto_connect_enabled else "0",
            "ssh_collapsed": "1" if self.v_ssh_collapsed.get() else "0",
        }
        with open(CONFIG_FILE, "w") as f:
            cfg.write(f)

    def get_preferred_game_account(self) -> int | None:
        return self._preferred_game_account_id

    def set_preferred_game_account(self, account_id: int | None):
        self._preferred_game_account_id = account_id
        self._save_config()

    def _build_connection_bar(self):
        bar = ttk.Frame(self, padding=(6, 4))
        bar.pack(side=tk.TOP, fill=tk.X)

        # ── Direct Connection Section ──────────────────────────────────────
        direct_frame = ttk.LabelFrame(bar, text="Direct Connection", padding=(4, 2))
        direct_frame.pack(fill=tk.X, pady=(0, 4))

        direct_row = ttk.Frame(direct_frame)
        direct_row.pack(fill=tk.X)

        self.v_host   = tk.StringVar()
        self.v_port   = tk.StringVar()
        self.v_user   = tk.StringVar()
        self.v_pass   = tk.StringVar()
        self.v_status = tk.StringVar(value="Not connected")

        self._direct_widgets = []
        for label, var, w, show in [
            ("MySQL Host:", self.v_host, 16, ""),
            ("Port:", self.v_port, 6,  ""),
            ("User:", self.v_user, 10, ""),
            ("Pass:", self.v_pass, 10, "*"),
        ]:
            lbl = ttk.Label(direct_row, text=label)
            lbl.pack(side=tk.LEFT, padx=(4, 1))
            ent = ttk.Entry(direct_row, textvariable=var, width=w, show=show)
            ent.pack(side=tk.LEFT, padx=(0, 6))
            self._direct_widgets.extend([lbl, ent])

        ttk.Button(direct_row, text="Connect", command=self._connect).pack(side=tk.LEFT, padx=4)
        self._status_lbl = ttk.Label(direct_row, textvariable=self.v_status, foreground="red")
        self._status_lbl.pack(side=tk.LEFT, padx=8)

        # ── SSH Tunnel Section ──────────────────────────────────────────────
        ssh_frame = ttk.LabelFrame(bar, text="SSH Tunnel (for remote/private networks)", padding=(4, 2))
        ssh_frame.pack(fill=tk.X)
        self._ssh_frame = ssh_frame
        self.v_ssh_collapsed = tk.BooleanVar(value=False)

        # Checkbox row
        check_row = ttk.Frame(ssh_frame)
        check_row.pack(fill=tk.X, pady=(0, 2))
        self._ssh_check_row = check_row

        self.v_ssh_enabled = tk.BooleanVar(value=False)
        ssh_check = ttk.Checkbutton(check_row, text="Enable SSH Tunnel", variable=self.v_ssh_enabled,
                                     command=self._toggle_ssh_fields)
        ssh_check.pack(side=tk.LEFT, padx=(4, 8))

        self._ssh_collapse_btn = ttk.Button(
            check_row,
            text="[V]",
            width=4,
            command=self._toggle_ssh_collapsed)
        self._ssh_collapse_btn.pack(side=tk.RIGHT, padx=(4, 4))

        if not SSH_TUNNEL_AVAILABLE:
            ssh_check.configure(state="disabled")
            ttk.Label(check_row, text="⚠ Package not installed: pip install sshtunnel",
                     foreground="orange").pack(side=tk.LEFT)
        else:
            ttk.Label(check_row, text="→ When enabled, connects through a jump host to reach the database",
                     foreground="gray").pack(side=tk.LEFT)

        # SSH credentials row
        ssh_row1 = ttk.Frame(ssh_frame)
        ssh_row1.pack(fill=tk.X, pady=1)
        self._ssh_row1 = ssh_row1

        self.v_ssh_host = tk.StringVar()
        self.v_ssh_port = tk.StringVar()
        self.v_ssh_user = tk.StringVar()
        self.v_ssh_pass = tk.StringVar()
        self.v_ssh_key  = tk.StringVar()

        self._ssh_widgets = []
        ttk.Label(ssh_row1, text="SSH:", foreground="blue").pack(side=tk.LEFT, padx=(4, 4))
        for label, var, w, show in [
            ("Host:", self.v_ssh_host, 18, ""),
            ("Port:", self.v_ssh_port, 5, ""),
            ("User:", self.v_ssh_user, 10, ""),
            ("Pass:", self.v_ssh_pass, 10, "*"),
        ]:
            lbl = ttk.Label(ssh_row1, text=label)
            lbl.pack(side=tk.LEFT, padx=(4, 1))
            ent = ttk.Entry(ssh_row1, textvariable=var, width=w, show=show)
            ent.pack(side=tk.LEFT, padx=(0, 4))
            self._ssh_widgets.extend([lbl, ent])

        # Key file row
        ssh_row2 = ttk.Frame(ssh_frame)
        ssh_row2.pack(fill=tk.X, pady=1)
        self._ssh_row2 = ssh_row2

        ttk.Label(ssh_row2, text="SSH:", foreground="blue").pack(side=tk.LEFT, padx=(4, 4))
        lbl_key = ttk.Label(ssh_row2, text="Key File:")
        lbl_key.pack(side=tk.LEFT, padx=(4, 1))
        ent_key = ttk.Entry(ssh_row2, textvariable=self.v_ssh_key, width=40)
        ent_key.pack(side=tk.LEFT, padx=(0, 2))
        btn_browse = ttk.Button(ssh_row2, text="Browse...", command=self._browse_ssh_key)
        btn_browse.pack(side=tk.LEFT, padx=2)
        self._ssh_widgets.extend([lbl_key, ent_key, btn_browse])

        lbl_hint = ttk.Label(ssh_row2, text="(leave blank to use password)", foreground="gray")
        lbl_hint.pack(side=tk.LEFT, padx=(4, 0))
        self._ssh_widgets.append(lbl_hint)

        # Database credentials row (MySQL on remote server)
        ssh_row3 = ttk.Frame(ssh_frame)
        ssh_row3.pack(fill=tk.X, pady=1)
        self._ssh_row3 = ssh_row3

        self.v_db_host  = tk.StringVar()
        self.v_db_port  = tk.StringVar()
        self.v_db_user  = tk.StringVar()
        self.v_db_pass  = tk.StringVar()

        ttk.Label(ssh_row3, text="MySQL:", foreground="green").pack(side=tk.LEFT, padx=(4, 4))
        for label, var, w, show in [
            ("Host:", self.v_db_host, 18, ""),
            ("Port:", self.v_db_port, 5, ""),
            ("User:", self.v_db_user, 10, ""),
            ("Pass:", self.v_db_pass, 10, "*"),
        ]:
            lbl = ttk.Label(ssh_row3, text=label)
            lbl.pack(side=tk.LEFT, padx=(4, 1))
            ent = ttk.Entry(ssh_row3, textvariable=var, width=w, show=show)
            ent.pack(side=tk.LEFT, padx=(0, 4))
            self._ssh_widgets.extend([lbl, ent])

        lbl_mysql_hint = ttk.Label(ssh_row3, text="(MySQL server as seen from SSH host)", foreground="gray")
        lbl_mysql_hint.pack(side=tk.LEFT, padx=(4, 0))
        self._ssh_widgets.append(lbl_mysql_hint)

    def _toggle_ssh_collapsed(self):
        self.v_ssh_collapsed.set(not self.v_ssh_collapsed.get())
        self._apply_ssh_collapsed_state()
        self._save_config()

    def _apply_ssh_collapsed_state(self):
        collapsed = self.v_ssh_collapsed.get()
        self._ssh_collapse_btn.configure(text="[>]" if collapsed else "[V]")

        if collapsed:
            self._ssh_row1.pack_forget()
            self._ssh_row2.pack_forget()
            self._ssh_row3.pack_forget()
        else:
            if not self._ssh_row1.winfo_manager():
                self._ssh_row1.pack(fill=tk.X, pady=1)
            if not self._ssh_row2.winfo_manager():
                self._ssh_row2.pack(fill=tk.X, pady=1)
            if not self._ssh_row3.winfo_manager():
                self._ssh_row3.pack(fill=tk.X, pady=1)

    def _toggle_ssh_fields(self):
        """Enable/disable SSH fields and direct connection based on mode."""
        ssh_enabled = self.v_ssh_enabled.get()

        # Enable/disable SSH fields
        ssh_state = "normal" if ssh_enabled else "disabled"
        for w in self._ssh_widgets:
            w.configure(state=ssh_state)

        # Show/hide direct connection fields
        direct_state = "disabled" if ssh_enabled else "normal"
        for w in self._direct_widgets:
            w.configure(state=direct_state)

        # Auto-populate direct connection from SSH MySQL fields when enabling SSH
        if ssh_enabled:
            # When using SSH tunnel, direct connection always goes to localhost
            self.v_host.set("127.0.0.1")
            self.v_port.set("3306")
            # Copy MySQL credentials from SSH section to direct section
            if self.v_db_user.get():
                self.v_user.set(self.v_db_user.get())
            if self.v_db_pass.get():
                self.v_pass.set(self.v_db_pass.get())

        self._apply_ssh_collapsed_state()

    def _browse_ssh_key(self):
        """Browse for SSH private key file."""
        from tkinter import filedialog
        filename = filedialog.askopenfilename(
            title="Select SSH Private Key",
            filetypes=[("All files", "*"), ("PEM files", "*.pem"), ("Key files", "id_rsa")]
        )
        if filename:
            self.v_ssh_key.set(filename)

    def _build_notebook(self):
        self.nb = ttk.Notebook(self)
        self.nb.pack(fill=tk.BOTH, expand=True, padx=6, pady=6)
        self.tab_defaults = DefaultProfilesTab(self.nb)
        self.tab_profiles = BotProfilesTab(self.nb)
        self.tab_character_editor = CharacterEditorTab(self.nb)
        self.tab_accounts = AccountsTab(self.nb)
        self.nb.add(self.tab_defaults, text="  Class Defaults  ")
        self.nb.add(self.tab_profiles, text="  Bot Profiles  ")
        self.nb.add(self.tab_character_editor, text="  Character Editor  ")
        self.nb.add(self.tab_accounts, text="  Accounts  ")

    def _connect(self):
        try:
            db.disconnect()

            # When SSH is enabled, use MySQL credentials from SSH section
            mysql_user = self.v_db_user.get() if self.v_ssh_enabled.get() else self.v_user.get()
            mysql_pass = self.v_db_pass.get() if self.v_ssh_enabled.get() else self.v_pass.get()

            db.connect(
                host=self.v_host.get(),
                port=int(self.v_port.get()),
                user=mysql_user,
                password=mysql_pass,
                ssh_enabled=self.v_ssh_enabled.get(),
                ssh_host=self.v_ssh_host.get(),
                ssh_port=int(self.v_ssh_port.get()) if self.v_ssh_port.get() else 22,
                ssh_user=self.v_ssh_user.get(),
                ssh_password=self.v_ssh_pass.get(),
                ssh_key_file=self.v_ssh_key.get(),
                db_host=self.v_db_host.get() if self.v_db_host.get() else "127.0.0.1",
                db_port=int(self.v_db_port.get()) if self.v_db_port.get() else 3306
            )
            self._auto_connect_enabled = True
            self._save_config()
            tunnel_info = " (via SSH tunnel)" if self.v_ssh_enabled.get() else ""
            self.v_status.set(f"● Connected{tunnel_info}")
            self._status_lbl.configure(foreground="#1a7f1a")
            self.tab_defaults.refresh()
            self.tab_profiles.refresh()
            self.tab_character_editor.refresh()
            self.tab_accounts.refresh()
        except (MySQLError, ValueError, ImportError) as e:
            self.v_status.set(f"✗ {e}")
            self._status_lbl.configure(foreground="red")

    def on_close(self):
        db.disconnect()
        self.destroy()


# ═══════════════════════════════════════════════════════════════════════════
#  ENTRY POINT
# ═══════════════════════════════════════════════════════════════════════════

if __name__ == "__main__":
    app = App()
    app.protocol("WM_DELETE_WINDOW", app.on_close)
    app.mainloop()
