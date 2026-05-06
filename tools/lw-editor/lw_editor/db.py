"""
db.py -- DBCtx database context class and module-level singleton.
"""
import hashlib, json, pathlib, secrets, struct
import mysql.connector
from mysql.connector import Error as MySQLError

try:
    import paramiko
    PARAMIKO_AVAILABLE = True
except ImportError:
    paramiko = None
    PARAMIKO_AVAILABLE = False

try:
    from sshtunnel import SSHTunnelForwarder
    SSH_TUNNEL_AVAILABLE = True
except ImportError:
    SSHTunnelForwarder = None
    SSH_TUNNEL_AVAILABLE = False

from .constants import (
    WOW_CLASSES, CLASS_SPELL_FAMILY, EQUIPMENT_SLOT_TO_INVENTORY_TYPES,
    CLASS_ARMOR_SUBCLASSES, ARMOR_PROFICIENCY_SLOT_IDS,
    SLOT_SPELL_NAME_KEYWORDS, ENCHANT_CATEGORY_KEYWORDS, SLOT_ENCHANT_CATEGORY,
    ITEM_MOD_NAMES,
    ITEM_ENCHANTMENT_SLOT_COUNT, ITEM_ENCHANTMENT_OFFSET_COUNT,
)
from lw_editor import DATA_DIR, DBC_DIR


# ── DB-internal helpers (duplicated here to avoid a circular helpers→db import)

def _is_noise_item_name(name: str) -> bool:
    lowered = (name or "").strip().lower()
    return (
        not lowered or
        lowered.startswith("zzold") or
        lowered.startswith("deprecated") or
        lowered.startswith("test") or
        lowered.startswith("old")
    )

def _slot_enchant_category(slot_id: int) -> str | None:
    return SLOT_ENCHANT_CATEGORY.get(int(slot_id))

def _filter_enchant_rows_for_slot(slot_id: int, rows: list) -> list:
    category = _slot_enchant_category(slot_id)
    cleaned = []
    for row in rows:
        name = (row.get("Name_Lang_enUS") or "").strip()
        if not name:
            continue
        if any(w in name.lower() for w in ("test", "deprecated", "qa ", "zzold")):
            continue
        cleaned.append(row)
    if not category:
        return cleaned
    spell_kw = SLOT_SPELL_NAME_KEYWORDS.get(category, [])
    stat_kw  = ENCHANT_CATEGORY_KEYWORDS.get(category, [])
    if spell_kw:
        by_spell = [r for r in cleaned
                    if (r.get("SpellName") or "").strip().lower() and
                    any(kw in (r.get("SpellName") or "").lower() for kw in spell_kw)]
        if by_spell:
            return by_spell
    if stat_kw:
        by_stat = [r for r in cleaned
                   if any(kw in (r.get("Name_Lang_enUS") or "").lower() for kw in stat_kw)]
        if by_stat:
            return by_stat
    return cleaned


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
        self._dbc_root = DBC_DIR
        self._ssh_tunnel = None
        self._spell_data_warning_shown = False
        # JSON cache paths
        self._json_cache_dir      = DATA_DIR
        self._spell_names_json    = self._json_cache_dir / "spell_names.json"
        self._class_spells_json   = self._json_cache_dir / "class_spells.json"
        self._enchantment_json    = self._json_cache_dir / "enchantment_data.json"
        self._gem_properties_json = self._json_cache_dir / "gem_properties.json"
        self._faction_names_json  = self._json_cache_dir / "faction_names.json"
        self._json_spell_cache        = None
        self._json_class_spells_cache = None
        self._dbc_enchant_cache       = None
        self._dbc_gemproperties_cache = None
        self._json_faction_cache      = None

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
        self._migrate_schema()

    def _migrate_schema(self):
        """Run named migrations exactly once each.

        Each entry in _MIGRATIONS is a (migration_id, db_attr, sql) tuple.
        The migration_id is recorded in living_world_editor_migrations after
        the SQL runs successfully.  Re-connecting never re-runs a migration
        that already completed — so edits made through the editor are safe.

        To add a future schema change: append a new tuple with a unique id.
        Never edit or reorder existing entries.
        """
        if not self.ok():
            return

        # Ensure the migration tracking table exists first.
        try:
            self.run(self.world,
                "CREATE TABLE IF NOT EXISTS living_world_editor_migrations ("
                "  migration_id VARCHAR(100) NOT NULL,"
                "  applied_at   TIMESTAMP    NOT NULL DEFAULT CURRENT_TIMESTAMP,"
                "  PRIMARY KEY (migration_id)"
                ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci")
        except Exception as e:
            print(f"[lw-editor] Could not create migration table: {e}", flush=True)
            return

        applied = {
            r.get("migration_id")
            for r in (self.q(self.world,
                "SELECT migration_id FROM living_world_editor_migrations") or [])
        }

        for migration_id, db_attr, sql in self._MIGRATIONS:
            if migration_id in applied:
                continue
            conn = getattr(self, db_attr, None)
            if conn is None:
                continue
            try:
                self.run(conn, sql)
            except Exception as e:
                import mysql.connector
                # Error 1060 = Duplicate column name: column already exists,
                # desired state is already reached — mark applied and move on.
                # Error 1050 = Table already exists: same logic.
                if hasattr(e, 'errno') and e.errno in (1050, 1060):
                    pass
                else:
                    print(f"[lw-editor] Migration '{migration_id}' failed: {e}", flush=True)
                    continue
            try:
                self.run(self.world,
                    "INSERT IGNORE INTO living_world_editor_migrations (migration_id) VALUES (%s)",
                    (migration_id,))
                applied.add(migration_id)
            except Exception as e:
                print(f"[lw-editor] Could not record migration '{migration_id}': {e}", flush=True)

    # -----------------------------------------------------------------------
    # Migration registry — append only, never edit existing entries.
    # -----------------------------------------------------------------------
    _MIGRATIONS = [

        # ── Pre-existing column addition ────────────────────────────────────
        ("2024_01_combat_profile_class_key",
         "world",
         "ALTER TABLE living_world_bot_combat_default_profile "
         "ADD COLUMN class_key VARCHAR(20) DEFAULT NULL AFTER display_name"),

        # ── Hazard sensor tables ─────────────────────────────────────────────
        ("2025_01_hazard_auras_table",
         "world",
         "CREATE TABLE IF NOT EXISTS living_world_hazard_auras ("
         "  id       INT UNSIGNED NOT NULL AUTO_INCREMENT,"
         "  spell_id INT UNSIGNED NOT NULL,"
         "  severity FLOAT        NOT NULL DEFAULT 1.0,"
         "  notes    VARCHAR(255)          DEFAULT NULL,"
         "  enabled  TINYINT(1)   NOT NULL DEFAULT 1,"
         "  PRIMARY KEY (id),"
         "  UNIQUE KEY uq_spell (spell_id)"
         ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci"),

        ("2025_02_hazard_auras_seed",
         "world",
         "INSERT IGNORE INTO living_world_hazard_auras (spell_id, severity, notes) VALUES"
         "  (28524, 1.0, 'Naxxramas - Slime Pool (Grobbulus)'),"
         "  (26575, 1.0, 'Generic - Void Zone'),"
         "  (37591, 1.0, 'Serpentshrine Cavern - Toxic Spores'),"
         "  (40923, 1.0, 'Black Temple - Fel Eruption'),"
         "  (46228, 1.0, 'Sunwell - Dark Decay (Eredar Twins)'),"
         "  (63018, 1.5, 'Ulduar - Searing Flames (Ignis)'),"
         "  (64290, 1.5, 'Ulduar - Saronite Vapors (General Vezax)'),"
         "  (67480, 1.0, 'Trial of the Crusader - Firebomb (Jaraxxus)'),"
         "  (70952, 2.0, 'Icecrown Citadel - Defile (Lich King)'),"
         "  (72754, 1.5, 'Icecrown Citadel - Frozen Orb ground effect'),"
         "  (74527, 1.5, 'Ruby Sanctum - Combustion ground fire')"),

        ("2025_03_hazard_class_rules_table",
         "world",
         "CREATE TABLE IF NOT EXISTS living_world_hazard_class_rules ("
         "  id                     INT UNSIGNED     NOT NULL AUTO_INCREMENT,"
         "  class_id               TINYINT UNSIGNED NOT NULL,"
         "  skip_escape            TINYINT(1)       NOT NULL DEFAULT 0,"
         "  owner_hp_gate_pct      FLOAT            NOT NULL DEFAULT 0.0,"
         "  requires_aggro_to_skip TINYINT(1)       NOT NULL DEFAULT 0,"
         "  notes                  VARCHAR(255)              DEFAULT NULL,"
         "  PRIMARY KEY (id),"
         "  UNIQUE KEY uq_class (class_id)"
         ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci"),

        ("2025_04_hazard_class_rules_seed",
         "world",
         "INSERT IGNORE INTO living_world_hazard_class_rules"
         "  (class_id, skip_escape, owner_hp_gate_pct, requires_aggro_to_skip, notes) VALUES"
         "  (1,  1, 0.0, 1, 'Warrior - skip escape only while tanking'),"
         "  (6,  1, 0.0, 1, 'Death Knight - skip escape only while tanking'),"
         "  (2,  0, 50.0, 0, 'Paladin - suppress escape when healing emergency'),"
         "  (7,  0, 50.0, 0, 'Shaman - suppress escape when healing emergency'),"
         "  (11, 0, 50.0, 0, 'Druid - suppress escape when healing emergency')"),

        ("2025_05_hazard_config_table",
         "world",
         "CREATE TABLE IF NOT EXISTS living_world_hazard_config ("
         "  config_key   VARCHAR(64) NOT NULL,"
         "  config_value FLOAT       NOT NULL,"
         "  notes        VARCHAR(255)         DEFAULT NULL,"
         "  PRIMARY KEY (config_key)"
         ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci"),

        ("2025_06_hazard_config_seed",
         "world",
         "INSERT IGNORE INTO living_world_hazard_config (config_key, config_value, notes) VALUES"
         "  ('damage_threshold_pct',      2.0,    'HP drop per 500ms tick to count as damage'),"
         "  ('consecutive_damage_ticks',  2.0,    'Ticks needed for layer-2 danger declaration'),"
         "  ('max_movement_yards',        2.0,    'Max movement between ticks before ignoring HP drop'),"
         "  ('anchor_search_radius',     40.0,    'Yards to search for a clean party anchor'),"
         "  ('escape_step_yards',         5.0,    'Yards to step toward anchor per escape tick'),"
         "  ('commit_window_ms',       2000.0,    'Ms to keep the same anchor before re-evaluating')"),

        # ── Global bot behaviour ─────────────────────────────────────────────
        ("2025_07_bot_global_config_table",
         "world",
         "CREATE TABLE IF NOT EXISTS living_world_bot_global_config ("
         "  config_key   VARCHAR(64) NOT NULL,"
         "  config_value FLOAT       NOT NULL,"
         "  notes        VARCHAR(255)         DEFAULT NULL,"
         "  PRIMARY KEY (config_key)"
         ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci"),

        ("2025_08_bot_global_config_seed",
         "world",
         "INSERT IGNORE INTO living_world_bot_global_config (config_key, config_value, notes) VALUES"
         "  ('follow_distance',   2.0, 'Yards bots keep from owner while following'),"
         "  ('follow_formation',  0.0, '0=Ring  1=V-shape  2=Line  3=Cluster'),"
         "  ('follow_slot_count', 7.0, 'Number of positions in Ring formation (3-9)'),"
         "  ('mount_with_owner',  1.0, '1=bots mount when owner mounts (implementation pending)'),"
         "  ('auto_loot',         0.0, '1=bots auto-loot nearby corpses (implementation pending)')"),

        # ── Role-based hazard escape rules (replaces class-based table) ──────
        ("2025_09_hazard_role_rules_table",
         "world",
         "CREATE TABLE IF NOT EXISTS living_world_hazard_role_rules ("
         "  role_key               VARCHAR(20)  NOT NULL,"
         "  skip_escape            TINYINT(1)   NOT NULL DEFAULT 0,"
         "  owner_hp_gate_pct      FLOAT        NOT NULL DEFAULT 0.0,"
         "  requires_aggro_to_skip TINYINT(1)   NOT NULL DEFAULT 0,"
         "  notes                  VARCHAR(255)          DEFAULT NULL,"
         "  PRIMARY KEY (role_key)"
         ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci"),

        ("2025_10_hazard_role_rules_seed",
         "world",
         "INSERT IGNORE INTO living_world_hazard_role_rules"
         "  (role_key, skip_escape, owner_hp_gate_pct, requires_aggro_to_skip, notes) VALUES"
         "  ('TANK',          1, 0.0,  1, 'Skip escape only while actively holding aggro'),"
         "  ('HEALER',        0, 50.0, 0, 'Suppress escape when owner HP is critical'),"
         "  ('HYBRID_HEALER', 0, 50.0, 0, 'Suppress escape when owner HP is critical'),"
         "  ('MELEE_DPS',     0, 0.0,  0, 'Always escape - no special handling'),"
         "  ('RANGED_DPS',    0, 0.0,  0, 'Always escape - no special handling')"),

        # ── Role-based follow distances ──────────────────────────────────────
        ("2025_11_bot_follow_distance_by_role",
         "world",
         "INSERT IGNORE INTO living_world_bot_global_config (config_key, config_value, notes) VALUES"
         "  ('follow_distance',         2.0, 'Fallback follow yards when role is unknown (Passive mode)'),"
         "  ('follow_distance_melee',   1.0, 'Follow yards: Tank and Melee DPS'),"
         "  ('follow_distance_healer',  1.5, 'Follow yards: Healer and Hybrid Healer'),"
         "  ('follow_distance_ranged',  2.5, 'Follow yards: Ranged and caster DPS')"),

        # ── OOC behavior columns on bot combat profile ───────────────────────
        ("2025_12_bot_profile_ooc_buff_scope",
         "chars",
         "ALTER TABLE living_world_bot_combat_profile "
         "ADD COLUMN buff_scope TINYINT NOT NULL DEFAULT 2 "
         "COMMENT '0=off 1=self 2=party'"),

        ("2025_13_bot_profile_ooc_buff_reapply",
         "chars",
         "ALTER TABLE living_world_bot_combat_profile "
         "ADD COLUMN buff_reapply_secs SMALLINT NOT NULL DEFAULT 30 "
         "COMMENT 'Re-cast when aura has fewer than N seconds remaining'"),

        ("2025_14_bot_profile_ooc_buff_on_spawn",
         "chars",
         "ALTER TABLE living_world_bot_combat_profile "
         "ADD COLUMN buff_on_spawn TINYINT NOT NULL DEFAULT 1"),

        ("2025_15_bot_profile_ooc_follow_override",
         "chars",
         "ALTER TABLE living_world_bot_combat_profile "
         "ADD COLUMN follow_dist_override FLOAT DEFAULT NULL "
         "COMMENT 'NULL = use global role-based distance'"),

        ("2025_16_bot_profile_ooc_auto_loot",
         "chars",
         "ALTER TABLE living_world_bot_combat_profile "
         "ADD COLUMN auto_loot_override TINYINT DEFAULT NULL "
         "COMMENT 'NULL=global  0=off  1=on'"),

        ("2025_17_bot_profile_ooc_loot_quality",
         "chars",
         "ALTER TABLE living_world_bot_combat_profile "
         "ADD COLUMN loot_quality_min TINYINT NOT NULL DEFAULT 0 "
         "COMMENT '0=all  1=white+  2=green+  3=blue+  4=purple+'"),

        ("2025_18_bot_profile_ooc_gather_nodes",
         "chars",
         "ALTER TABLE living_world_bot_combat_profile "
         "ADD COLUMN gather_nodes TINYINT NOT NULL DEFAULT 0 "
         "COMMENT '0=off  1=mining  2=herbing  3=both'"),

        ("2025_19_bot_profile_ooc_gather_skin",
         "chars",
         "ALTER TABLE living_world_bot_combat_profile "
         "ADD COLUMN gather_skin TINYINT NOT NULL DEFAULT 0 "
         "COMMENT '0=off  1=if loot below threshold  2=always'"),

        ("2025_20_bot_profile_ooc_skin_quality",
         "chars",
         "ALTER TABLE living_world_bot_combat_profile "
         "ADD COLUMN skin_loot_quality_max TINYINT NOT NULL DEFAULT 0 "
         "COMMENT '0=gray only 1=white and below 2=green and below etc.'"),

        ("2025_21_bot_profile_loot_category_flags",
         "chars",
         "ALTER TABLE living_world_bot_combat_profile "
         "ADD COLUMN loot_category_flags INT NOT NULL DEFAULT 0 "
         "COMMENT 'Bitmask: 0x01=cloth/leather 0x02=ore 0x04=herbs 0x08=enchanting 0x10=recipes 0x20=gems'"),

        # ── Per-character OOC config table (replaces per-profile OOC columns) ─
        ("2025_22_bot_ooc_config_table",
         "chars",
         "CREATE TABLE IF NOT EXISTS living_world_bot_ooc_config ("
         "  source_character_guid BIGINT UNSIGNED NOT NULL,"
         "  buff_scope             TINYINT  NOT NULL DEFAULT 2,"
         "  buff_reapply_secs      SMALLINT NOT NULL DEFAULT 30,"
         "  buff_on_spawn          TINYINT  NOT NULL DEFAULT 1,"
         "  follow_dist_override   FLOAT    DEFAULT NULL,"
         "  auto_loot_override     TINYINT  DEFAULT NULL,"
         "  loot_quality_min       TINYINT  NOT NULL DEFAULT 0,"
         "  loot_category_flags    INT      NOT NULL DEFAULT 63,"
         "  gather_nodes           TINYINT  NOT NULL DEFAULT 0,"
         "  gather_skin            TINYINT  NOT NULL DEFAULT 0,"
         "  skin_loot_quality_max  TINYINT  NOT NULL DEFAULT 0,"
         "  PRIMARY KEY (source_character_guid)"
         ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci"),

        # ── context_key column + unique key on default profiles ──────────────
        ("2025_24_default_profile_context_key",
         "world",
         "ALTER TABLE living_world_bot_combat_default_profile "
         "ADD COLUMN context_key VARCHAR(10) NOT NULL DEFAULT 'PvE' "
         "COMMENT 'PvE | PvP (future: Arena, BG)'"),

        ("2025_25_default_profile_context_unique",
         "world",
         "ALTER TABLE living_world_bot_combat_default_profile "
         "ADD UNIQUE KEY uq_class_spec_context (class_key, spec_key, context_key)"),

        # ── PvP seed — mirrors every PvE entry with context_key='PvP' ────────
        ("2025_26_pvp_profiles_seed",
         "world",
         "INSERT IGNORE INTO living_world_bot_combat_default_profile "
         "(class_key, spec_key, role_key, context_key, display_name, "
         " conservation_mode, mana_low_water, mana_high_water, "
         " enable_down_rank, down_rank_floor, "
         " default_aoe_mode, default_aoe_min_targets, default_aoe_scan_radius) VALUES "
         "('Warrior','Arms','DPS','PvP','Warrior \u2014 Arms (PvP)',1,55,75,1,2,0,2,10.0),"
         "('Warrior','Fury','DPS','PvP','Warrior \u2014 Fury (PvP)',1,55,75,1,2,0,2,10.0),"
         "('Warrior','Protection','TANK','PvP','Warrior \u2014 Protection (PvP)',1,55,75,1,2,0,2,10.0),"
         "('Paladin','Holy','HEAL','PvP','Paladin \u2014 Holy (PvP)',1,55,75,1,2,0,2,10.0),"
         "('Paladin','Protection','TANK','PvP','Paladin \u2014 Protection (PvP)',1,55,75,1,2,0,2,10.0),"
         "('Paladin','Retribution','DPS','PvP','Paladin \u2014 Retribution (PvP)',1,55,75,1,2,0,2,10.0),"
         "('Hunter','BeastMastery','DPS','PvP','Hunter \u2014 Beast Mastery (PvP)',1,55,75,1,2,0,2,10.0),"
         "('Hunter','Marksmanship','DPS','PvP','Hunter \u2014 Marksmanship (PvP)',1,55,75,1,2,0,2,10.0),"
         "('Hunter','Survival','DPS','PvP','Hunter \u2014 Survival (PvP)',1,55,75,1,2,0,2,10.0),"
         "('Rogue','Assassination','DPS','PvP','Rogue \u2014 Assassination (PvP)',1,55,75,1,2,0,2,10.0),"
         "('Rogue','Combat','DPS','PvP','Rogue \u2014 Combat (PvP)',1,55,75,1,2,0,2,10.0),"
         "('Rogue','Subtlety','DPS','PvP','Rogue \u2014 Subtlety (PvP)',1,55,75,1,2,0,2,10.0),"
         "('Priest','Discipline','HEAL','PvP','Priest \u2014 Discipline (PvP)',1,55,75,1,2,0,2,10.0),"
         "('Priest','Holy','HEAL','PvP','Priest \u2014 Holy (PvP)',1,55,75,1,2,0,2,10.0),"
         "('Priest','Shadow','DPS','PvP','Priest \u2014 Shadow (PvP)',1,55,75,1,2,0,2,10.0),"
         "('Death Knight','Blood','TANK','PvP','Death Knight \u2014 Blood (PvP)',1,55,75,1,2,0,2,10.0),"
         "('Death Knight','Frost','DPS','PvP','Death Knight \u2014 Frost (PvP)',1,55,75,1,2,0,2,10.0),"
         "('Death Knight','Unholy','DPS','PvP','Death Knight \u2014 Unholy (PvP)',1,55,75,1,2,0,2,10.0),"
         "('Shaman','Elemental','DPS','PvP','Shaman \u2014 Elemental (PvP)',1,55,75,1,2,0,2,10.0),"
         "('Shaman','Enhancement','DPS','PvP','Shaman \u2014 Enhancement (PvP)',1,55,75,1,2,0,2,10.0),"
         "('Shaman','Restoration','HEAL','PvP','Shaman \u2014 Restoration (PvP)',1,55,75,1,2,0,2,10.0),"
         "('Mage','Arcane','DPS','PvP','Mage \u2014 Arcane (PvP)',1,55,75,1,2,0,2,10.0),"
         "('Mage','Fire','DPS','PvP','Mage \u2014 Fire (PvP)',1,55,75,1,2,0,2,10.0),"
         "('Mage','Frost','DPS','PvP','Mage \u2014 Frost (PvP)',1,55,75,1,2,0,2,10.0),"
         "('Warlock','Affliction','DPS','PvP','Warlock \u2014 Affliction (PvP)',1,55,75,1,2,0,2,10.0),"
         "('Warlock','Demonology','DPS','PvP','Warlock \u2014 Demonology (PvP)',1,55,75,1,2,0,2,10.0),"
         "('Warlock','Destruction','DPS','PvP','Warlock \u2014 Destruction (PvP)',1,55,75,1,2,0,2,10.0),"
         "('Druid','Balance','DPS','PvP','Druid \u2014 Balance (PvP)',1,55,75,1,2,0,2,10.0),"
         "('Druid','Feral','DPS','PvP','Druid \u2014 Feral (PvP)',1,55,75,1,2,0,2,10.0),"
         "('Druid','Restoration','HEAL','PvP','Druid \u2014 Restoration (PvP)',1,55,75,1,2,0,2,10.0)"),

    ]

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

        # Prefer pre-extracted JSON (no DBC file required at runtime).
        if self._enchantment_json.exists():
            try:
                with self._enchantment_json.open(encoding="utf-8") as f:
                    self._dbc_enchant_cache = json.load(f)
                return self._dbc_enchant_cache
            except Exception:
                pass

        # Fallback: read directly from DBC.
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

        # Prefer pre-extracted JSON.
        if self._gem_properties_json.exists():
            try:
                with self._gem_properties_json.open(encoding="utf-8") as f:
                    raw = json.load(f)
                    self._dbc_gemproperties_cache = {int(k): v for k, v in raw.items()}
                return self._dbc_gemproperties_cache
            except Exception:
                pass

        # Fallback: read directly from DBC.
        cache = {}
        parsed = self._read_dbc_rows("GemProperties.dbc", expected_fields=5)
        if parsed:
            record_count, field_count, record_size, records, _strings = parsed
            for i in range(record_count):
                row = struct.unpack_from(f"<{field_count}I", records, i * record_size)
                gemprop_id  = int(row[0] or 0)
                enchant_id  = int(row[1] or 0)
                socket_mask = int(row[4] or 0)
                if gemprop_id:
                    cache[gemprop_id] = {
                        "ID":         gemprop_id,
                        "Enchant_Id": enchant_id,
                        "Type":       socket_mask,
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
            with self._spell_names_json.open("r", encoding="utf-8") as f:
                data = json.load(f)
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
                    with self._class_spells_json.open("r", encoding="utf-8") as f:
                        self._json_class_spells_cache = json.load(f)
                except Exception:
                    self._json_class_spells_cache = {}

        # Map class_id to class name
        class_name = WOW_CLASSES.get(class_id, f"Class{class_id}")
        return self._json_class_spells_cache.get(class_name, [])

    def _load_faction_name_cache(self) -> dict:
        """Return {faction_id: {name, parent_id, parent_name, expansion}} from faction_names.json."""
        if self._json_faction_cache is not None:
            return self._json_faction_cache
        if not self._faction_names_json.exists():
            self._json_faction_cache = {}
            return self._json_faction_cache
        try:
            with self._faction_names_json.open(encoding="utf-8") as f:
                raw = json.load(f)
            # Support both the old {id: "name"} format and new {id: {name,expansion,...}} format
            result = {}
            for k, v in raw.items():
                if isinstance(v, dict):
                    result[int(k)] = v
                else:
                    result[int(k)] = {"name": v, "parent_name": "", "expansion": "Other"}
            self._json_faction_cache = result
        except Exception:
            self._json_faction_cache = {}
        return self._json_faction_cache

    def faction_name(self, faction_id: int) -> str:
        """Resolve a faction ID to its display name."""
        if not faction_id:
            return ""
        cache = self._load_faction_name_cache()
        entry = cache.get(int(faction_id))
        return entry["name"] if isinstance(entry, dict) else (entry or "")

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

    def spell_name_cache(self) -> dict[int, str]:
        """Public access to the full {spell_id: name} mapping."""
        return self._load_json_spell_cache()

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

    def item_tooltip_data(self, entry_id: int) -> dict | None:
        """Fetch all fields needed to render a WoW-style item tooltip."""
        if not entry_id or not self.ok():
            return None
        try:
            rows = self.q(self.world,
                "SELECT entry, name, Quality, ItemLevel, RequiredLevel, AllowableClass, "
                "`class` AS item_class, subclass AS item_subclass, "
                "InventoryType, armor, block, "
                "stat_type1,  stat_value1,  stat_type2,  stat_value2, "
                "stat_type3,  stat_value3,  stat_type4,  stat_value4, "
                "stat_type5,  stat_value5,  stat_type6,  stat_value6, "
                "stat_type7,  stat_value7,  stat_type8,  stat_value8, "
                "stat_type9,  stat_value9,  stat_type10, stat_value10, "
                "socketColor_1, socketColor_2, socketColor_3, socketBonus, "
                "RequiredSkill, RequiredSkillRank, "
                "dmg_min1, dmg_max1, dmg_type1, dmg_min2, dmg_max2, dmg_type2, delay, "
                "spellid_1, spelltrigger_1, spellid_2, spelltrigger_2, "
                "spellid_3, spelltrigger_3, spellid_4, spelltrigger_4, "
                "spellid_5, spelltrigger_5 "
                "FROM item_template WHERE entry=%s LIMIT 1",
                (int(entry_id),))
            return rows[0] if rows else None
        except Exception:
            return None

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
            # When using the JSON cache, fetch the full set first, apply the
            # slot filter, then truncate — so the filter sees all candidates
            # rather than just the first `limit` alphabetical entries.
            pool = self._search_item_enchantments_dbc(raw, limit=9999)
            if slot_id is not None:
                pool = _filter_enchant_rows_for_slot(slot_id, pool)
            return pool[:limit]

        if slot_id is None:
            return rows
        return _filter_enchant_rows_for_slot(slot_id, rows)[:limit]

    def enchant_name(self, enchant_id: int) -> str:
        """Return the stat tooltip for an enchantment (Name_Lang_enUS)."""
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

    def enchant_spell_name(self, enchant_id: int) -> str:
        """Return the enchanting spell name for an enchantment ID (from JSON cache)."""
        if not enchant_id:
            return ""
        cache = self._load_dbc_enchant_cache()
        for entry in cache:
            if int(entry.get("ID", 0)) == enchant_id:
                return entry.get("SpellName", "")
        return ""

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
                "spec_key=%s, role_key=%s, display_name=%s, class_key=%s, context_key=%s, "
                "conservation_mode=%s, mana_low_water=%s, mana_high_water=%s, "
                "enable_down_rank=%s, down_rank_floor=%s, default_aoe_mode=%s, "
                "default_aoe_min_targets=%s, default_aoe_scan_radius=%s "
                "WHERE default_profile_id=%s",
                (p["spec_key"], p["role_key"], p.get("display_name"),
                 p.get("class_key"), p.get("context_key", "PvE"),
                 p["conservation_mode"], p["mana_low_water"], p["mana_high_water"],
                 p["enable_down_rank"], p["down_rank_floor"], p["default_aoe_mode"],
                 p["default_aoe_min_targets"], p["default_aoe_scan_radius"],
                 p["default_profile_id"]))
            return p["default_profile_id"]
        return self.run(self.world,
            "INSERT INTO living_world_bot_combat_default_profile "
            "(spec_key, role_key, display_name, class_key, context_key, conservation_mode, "
            "mana_low_water, mana_high_water, enable_down_rank, down_rank_floor, "
            "default_aoe_mode, default_aoe_min_targets, default_aoe_scan_radius) VALUES "
            "(%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s)",
            (p["spec_key"], p["role_key"], p.get("display_name"), p.get("class_key"),
             p.get("context_key", "PvE"),
             p["conservation_mode"], p["mana_low_water"], p["mana_high_water"],
             p["enable_down_rank"], p["down_rank_floor"], p["default_aoe_mode"],
             p["default_aoe_min_targets"], p["default_aoe_scan_radius"]))

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

    def load_bot_pool_accounts(self):
        """Accounts that are registered in the bot pool — the only accounts
        whose clone characters should ever have combat profiles."""
        return self.q(self.auth,
            "SELECT a.id AS account_id, a.username, COUNT(c.guid) AS char_count "
            "FROM account a "
            "INNER JOIN living_world_bot_account_pool p ON p.account_id = a.id "
            "LEFT JOIN acore_characters.characters c ON c.account = a.id "
            "GROUP BY a.id, a.username "
            "ORDER BY a.username")

    def load_bot_ooc_config(self, source_char_guid: int) -> dict:
        """Load OOC config for a bot character. Returns defaults if no row exists."""
        rows = self.q(self.chars,
            "SELECT buff_scope, buff_reapply_secs, buff_on_spawn, "
            "follow_dist_override, auto_loot_override, loot_quality_min, "
            "loot_category_flags, gather_nodes, gather_skin, skin_loot_quality_max "
            "FROM living_world_bot_ooc_config WHERE source_character_guid=%s",
            (source_char_guid,))
        if rows:
            return rows[0]
        # Return defaults — server will INSERT on first C++ load; editor mirrors defaults here.
        return {
            "buff_scope": 2, "buff_reapply_secs": 30, "buff_on_spawn": 1,
            "follow_dist_override": None, "auto_loot_override": None,
            "loot_quality_min": 0, "loot_category_flags": 0x3F,
            "gather_nodes": 0, "gather_skin": 0, "skin_loot_quality_max": 0,
        }

    def save_bot_ooc_config(self, source_char_guid: int, p: dict):
        self.run(self.chars,
            "INSERT INTO living_world_bot_ooc_config "
            "(source_character_guid, buff_scope, buff_reapply_secs, buff_on_spawn, "
            "follow_dist_override, auto_loot_override, loot_quality_min, "
            "loot_category_flags, gather_nodes, gather_skin, skin_loot_quality_max) "
            "VALUES (%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s) "
            "ON DUPLICATE KEY UPDATE "
            "buff_scope=%s, buff_reapply_secs=%s, buff_on_spawn=%s, "
            "follow_dist_override=%s, auto_loot_override=%s, loot_quality_min=%s, "
            "loot_category_flags=%s, gather_nodes=%s, gather_skin=%s, skin_loot_quality_max=%s",
            (source_char_guid,
             p.get("buff_scope", 2), p.get("buff_reapply_secs", 30),
             p.get("buff_on_spawn", 1), p.get("follow_dist_override"),
             p.get("auto_loot_override"), p.get("loot_quality_min", 0),
             p.get("loot_category_flags", 0x3F),
             p.get("gather_nodes", 0), p.get("gather_skin", 0),
             p.get("skin_loot_quality_max", 0),
             # repeat for ON DUPLICATE UPDATE
             p.get("buff_scope", 2), p.get("buff_reapply_secs", 30),
             p.get("buff_on_spawn", 1), p.get("follow_dist_override"),
             p.get("auto_loot_override"), p.get("loot_quality_min", 0),
             p.get("loot_category_flags", 0x3F),
             p.get("gather_nodes", 0), p.get("gather_skin", 0),
             p.get("skin_loot_quality_max", 0)))

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
                "default_aoe_scan_radius=%s, "
                "buff_scope=%s, buff_reapply_secs=%s, buff_on_spawn=%s, "
                "follow_dist_override=%s, auto_loot_override=%s, loot_quality_min=%s, "
                "gather_nodes=%s, gather_skin=%s, skin_loot_quality_max=%s, "
                "loot_category_flags=%s "
                "WHERE profile_id=%s",
                (p["slot"], p["profile_name"], p["guessed_spec_key"], p["guessed_role_key"],
                 p.get("spec_override_key"), p.get("role_override_key"),
                 p["conservation_mode"], p["mana_low_water"], p["mana_high_water"],
                 p["enable_down_rank"], p["down_rank_floor"], p["default_aoe_mode"],
                 p["default_aoe_min_targets"], p["default_aoe_scan_radius"],
                 p.get("buff_scope", 2), p.get("buff_reapply_secs", 30),
                 p.get("buff_on_spawn", 1), p.get("follow_dist_override"),
                 p.get("auto_loot_override"), p.get("loot_quality_min", 0),
                 p.get("gather_nodes", 0), p.get("gather_skin", 0),
                 p.get("skin_loot_quality_max", 0),
                 p.get("loot_category_flags", 0), p["profile_id"]))
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
        rows = self.q(self.chars,
            "SELECT cr.faction, cr.standing, cr.flags "
            "FROM character_reputation cr "
            "WHERE cr.guid=%s "
            "ORDER BY cr.faction ASC",
            (guid,))
        faction_cache = self._load_faction_name_cache()
        result = []
        for row in rows:
            fid   = int(row.get("faction", 0))
            entry = faction_cache.get(fid)
            if entry is None:
                # Unknown faction — include as "Other" so nothing is silently dropped
                row["faction_name"]  = f"Faction {fid}"
                row["parent_name"]   = ""
                row["expansion"]     = "Other"
            else:
                row["faction_name"]  = entry.get("name", f"Faction {fid}")
                row["parent_name"]   = entry.get("parent_name", "")
                row["expansion"]     = entry.get("expansion", "Other")
            result.append(row)
        return result

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

    def load_character_skills(self, guid: int) -> list:
        """Return [{skill, value, max}] for all skills the character has."""
        if not self.ok():
            return []
        return self.q(self.chars,
            "SELECT skill, value, max FROM character_skills WHERE guid=%s",
            (guid,))

    def load_character_known_spells(self, guid: int) -> set:
        """Return a set of spell IDs the character currently knows."""
        if not self.ok():
            return set()
        rows = self.q(self.chars,
            "SELECT spell FROM character_spell WHERE guid=%s",
            (guid,))
        return {int(r["spell"]) for r in rows}

    def load_character_talents(self, guid: int) -> dict:
        """Return {spell_id: specMask} for all talents on this character."""
        rows = self.q(self.chars,
            "SELECT spell, specMask FROM character_talent WHERE guid=%s",
            (guid,))
        return {int(r["spell"]): int(r["specMask"]) for r in rows}

    def add_character_talent(self, guid: int, spell_id: int, spec: int = 0):
        """Insert a talent rank; silently ignore if already present."""
        self.run(self.chars,
            "INSERT IGNORE INTO character_talent (guid, spell, specMask) "
            "VALUES (%s, %s, %s)",
            (guid, spell_id, spec))

    def remove_character_talent(self, guid: int, spell_id: int, spec: int = 0):
        """Remove a single talent rank row."""
        self.run(self.chars,
            "DELETE FROM character_talent WHERE guid=%s AND spell=%s AND specMask=%s",
            (guid, spell_id, spec))

    def reset_character_talents(self, guid: int, spec: int = 0):
        """Remove all talent rows for this character and spec."""
        self.run(self.chars,
            "DELETE FROM character_talent WHERE guid=%s AND spec=%s",
            (guid, spec))

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




# Module-level singleton -- import this from all UI modules.
db = DBCtx()
