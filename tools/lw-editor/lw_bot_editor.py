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
import struct

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
        end = string_block.find(b"\x00", offset)
        if end == -1:
            end = len(string_block)
        return string_block[offset:end].decode("utf-8", errors="ignore")

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
                "down_rank_floor=%s WHERE profile_id=%s",
                (p["slot"], p["profile_name"], p["guessed_spec_key"], p["guessed_role_key"],
                 p.get("spec_override_key"), p.get("role_override_key"),
                 p["conservation_mode"], p["mana_low_water"], p["mana_high_water"],
                 p["enable_down_rank"], p["down_rank_floor"], p["profile_id"]))
            return p["profile_id"]
        return self.run(self.chars,
            "INSERT INTO living_world_bot_combat_profile "
            "(source_character_guid, owner_account_id, slot, profile_name, "
            "guessed_spec_key, guessed_role_key, conservation_mode, mana_low_water, "
            "mana_high_water, enable_down_rank, down_rank_floor) VALUES "
            "(%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s)",
            (p["source_character_guid"], p["owner_account_id"], p["slot"],
             p["profile_name"], p["guessed_spec_key"], p["guessed_role_key"],
             p["conservation_mode"], p["mana_low_water"], p["mana_high_water"],
             p["enable_down_rank"], p["down_rank_floor"]))

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
                "rank_value=%s, target_key=%s WHERE action_id=%s",
                (a["action_type"], a["spell_base_id"], a["item_id"], a["rank_mode"],
                 a["rank_value"], a["target_key"], a["action_id"]))
        else:
            self.run(self.chars,
                "INSERT INTO living_world_bot_combat_profile_action "
                "(entry_id, slot, action_type, spell_base_id, item_id, rank_mode, "
                "rank_value, target_key) VALUES (%s,%s,%s,%s,%s,%s,%s,%s)",
                (entry_id, a["slot"], a["action_type"], a["spell_base_id"],
                 a["item_id"], a["rank_mode"], a["rank_value"], a["target_key"]))

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

    def create_bot_account(self, username: str, password: str) -> int:
        """Create a new acore_auth.account and add it to the bot pool."""
        u = username.upper()
        sha = hashlib.sha1(f"{u}:{password.upper()}".encode()).hexdigest().upper()
        acct_id = self.run(self.auth,
            "INSERT INTO account (username, sha_pass_hash, expansion) VALUES (%s,%s,2)",
            (u, sha))
        self.run(self.auth,
            "INSERT INTO living_world_bot_account_pool "
            "(account_id, account_name, is_enabled) VALUES (%s,%s,1)",
            (acct_id, u))
        return acct_id

    def set_account_enabled(self, account_id: int, enabled: bool):
        self.run(self.auth,
            "UPDATE living_world_bot_account_pool SET is_enabled=%s "
            "WHERE account_id=%s",
            (1 if enabled else 0, account_id))


db = DBCtx()


# ═══════════════════════════════════════════════════════════════════════════
#  SHARED HELPER: labeled row builder
# ═══════════════════════════════════════════════════════════════════════════

def lbl(parent, text, row, col, **kw):
    ttk.Label(parent, text=text).grid(row=row, column=col, sticky="w",
                                      padx=4, pady=2, **kw)

def entry_w(parent, var, row, col, width=10, **kw):
    e = ttk.Entry(parent, textvariable=var, width=width)
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
            a.update(slot=slot,
                     action_type=ACTION_INV.get(w["type"].get(), 0),
                     spell_base_id=int(w["spell_id"].get() or 0),
                     item_id=int(w["item_id"].get() or 0),
                     rank_mode=RANK_INV.get(w["rank_mode"].get(), 0),
                     rank_value=int(w["rank_val"].get() or 0),
                     target_key=w["target"].get() or "enemy")
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
        self._chars    = []
        self._profiles = []
        self._sel_char = None
        self._sel_prof = None
        self._build()

    def _build(self):
        pane = ttk.PanedWindow(self, orient=tk.HORIZONTAL)
        pane.pack(fill=tk.BOTH, expand=True)

        # ── Left: character + profile slot list ─────────────────────────────
        left = ttk.Frame(pane, width=240)
        pane.add(left, weight=0)

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

    def refresh(self):
        if not db.ok():
            return
        try:
            self._chars = db.load_source_characters()
            self._char_tv.delete(*self._char_tv.get_children())
            for c in self._chars:
                cls = WOW_CLASSES.get(c.get("class", 0), str(c.get("class", "")))
                self._char_tv.insert("", "end", iid=str(c["guid"]),
                                     values=(c["name"], c["level"], cls))
            self._sel_char = None
            self._sel_prof = None
            self._profiles = []
            self._prof_lb.delete(0, tk.END)
            self._hdr.clear()
            self._rot.clear()
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
                     enable_down_rank=1, down_rank_floor=2)
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
        ttk.Button(top, text="✓ Enable",         command=self._enable).pack(side=tk.LEFT, padx=4)
        ttk.Button(top, text="✗ Disable",        command=self._disable).pack(side=tk.LEFT, padx=4)

        cols = ("id", "username", "enabled", "src_acct", "src_char")
        self._tv = ttk.Treeview(self, columns=cols, show="headings",
                                selectmode="browse")
        self._tv.heading("id",       text="Account ID")
        self._tv.heading("username", text="Username")
        self._tv.heading("enabled",  text="Enabled")
        self._tv.heading("src_acct", text="Src Account")
        self._tv.heading("src_char", text="Src Char GUID")
        self._tv.column("id",       width=80,  anchor="center")
        self._tv.column("username", width=150)
        self._tv.column("enabled",  width=60,  anchor="center")
        self._tv.column("src_acct", width=100, anchor="center")
        self._tv.column("src_char", width=120, anchor="center")
        self._tv.pack(fill=tk.BOTH, expand=True, padx=8, pady=4)

        # Info bar
        self._info = ttk.Label(self, text="", foreground="#555")
        self._info.pack(anchor="w", padx=8, pady=2)

    def refresh(self):
        if not db.ok():
            return
        try:
            self._rows = db.load_pool_accounts()
            self._tv.delete(*self._tv.get_children())
            for r in self._rows:
                en = "✓" if r.get("is_enabled") else "✗"
                self._tv.insert("", "end",
                                iid=str(r["account_id"]),
                                tags=("enabled",) if r.get("is_enabled") else ("disabled",),
                                values=(r["account_id"],
                                        r.get("account_name") or r.get("username", ""),
                                        en,
                                        r.get("assigned_source_account_id") or "—",
                                        r.get("assigned_source_character_guid") or "—"))
            self._tv.tag_configure("enabled",  foreground="#1a7f1a")
            self._tv.tag_configure("disabled", foreground="#999")
            total   = len(self._rows)
            enabled = sum(1 for r in self._rows if r.get("is_enabled"))
            assigned = sum(1 for r in self._rows if r.get("assigned_source_account_id"))
            self._info.configure(
                text=f"{total} pool accounts  |  {enabled} enabled  |  {assigned} assigned")
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
        username = simpledialog.askstring("New bot account", "Username (e.g. BOTHOUSE011):")
        if not username:
            return
        password = simpledialog.askstring("New bot account",
                                          f"Password for {username.upper()}:", show="*")
        if not password:
            return
        try:
            aid = db.create_bot_account(username, password)
            messagebox.showinfo("Created",
                f"Bot account '{username.upper()}' created with ID {aid}.\n"
                "Remember to create a character on this account before using it.")
            self.refresh()
        except MySQLError as e:
            messagebox.showerror("DB error", str(e))


# ═══════════════════════════════════════════════════════════════════════════
#  MAIN APPLICATION WINDOW
# ═══════════════════════════════════════════════════════════════════════════

class App(tk.Tk):

    def __init__(self):
        super().__init__()
        self.title(f"LivingWorld Bot Editor  v{VERSION}")
        self.geometry("1280x800")
        self.minsize(1000, 640)
        self._build_connection_bar()
        self._build_notebook()
        self._load_saved_config()

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
        with open(CONFIG_FILE, "w") as f:
            cfg.write(f)

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

        # Checkbox row
        check_row = ttk.Frame(ssh_frame)
        check_row.pack(fill=tk.X, pady=(0, 2))

        self.v_ssh_enabled = tk.BooleanVar(value=False)
        ssh_check = ttk.Checkbutton(check_row, text="Enable SSH Tunnel", variable=self.v_ssh_enabled,
                                     command=self._toggle_ssh_fields)
        ssh_check.pack(side=tk.LEFT, padx=(4, 8))

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
        self.tab_accounts = AccountsTab(self.nb)
        self.nb.add(self.tab_defaults, text="  Class Defaults  ")
        self.nb.add(self.tab_profiles, text="  Bot Profiles  ")
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
            self._save_config()
            tunnel_info = " (via SSH tunnel)" if self.v_ssh_enabled.get() else ""
            self.v_status.set(f"● Connected{tunnel_info}")
            self._status_lbl.configure(foreground="#1a7f1a")
            self.tab_defaults.refresh()
            self.tab_profiles.refresh()
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
