"""
db.py -- DBCtx database context class and module-level singleton.
"""
import hashlib, json, os, pathlib, secrets, struct
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
        self._map_name_cache          = None
        self._area_name_cache         = None

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
        self._map_name_cache = None
        self._area_name_cache = None
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

        ("2026_21_default_profile_targeting_columns",
         "world",
         "ALTER TABLE living_world_bot_combat_default_profile "
         "ADD COLUMN targeting_mode TINYINT NOT NULL DEFAULT 0 COMMENT '0=standard 1=assist 2=skirmish', "
         "ADD COLUMN current_target_bias FLOAT NOT NULL DEFAULT 80, "
         "ADD COLUMN assist_target_bias FLOAT NOT NULL DEFAULT 140, "
         "ADD COLUMN focus_fire_bias FLOAT NOT NULL DEFAULT 55, "
         "ADD COLUMN protect_ally_bias FLOAT NOT NULL DEFAULT 170, "
         "ADD COLUMN prefer_healer_bias FLOAT NOT NULL DEFAULT 220, "
         "ADD COLUMN prefer_dps_bias FLOAT NOT NULL DEFAULT 140, "
         "ADD COLUMN avoid_tank_bias FLOAT NOT NULL DEFAULT 120"),

        ("2026_22_character_profile_targeting_columns",
         "chars",
         "ALTER TABLE living_world_bot_combat_profile "
         "ADD COLUMN targeting_mode TINYINT NOT NULL DEFAULT 0 COMMENT '0=standard 1=assist 2=skirmish', "
         "ADD COLUMN current_target_bias FLOAT NOT NULL DEFAULT 80, "
         "ADD COLUMN assist_target_bias FLOAT NOT NULL DEFAULT 140, "
         "ADD COLUMN focus_fire_bias FLOAT NOT NULL DEFAULT 55, "
         "ADD COLUMN protect_ally_bias FLOAT NOT NULL DEFAULT 170, "
         "ADD COLUMN prefer_healer_bias FLOAT NOT NULL DEFAULT 220, "
         "ADD COLUMN prefer_dps_bias FLOAT NOT NULL DEFAULT 140, "
         "ADD COLUMN avoid_tank_bias FLOAT NOT NULL DEFAULT 120"),

        # ── PvP seed — mirrors every PvE entry with context_key='PvP' ────────
        ("2025_26_pvp_profiles_seed",
         "world",
         "INSERT IGNORE INTO living_world_bot_combat_default_profile "
         "(class_key, spec_key, role_key, context_key, display_name, "
         " conservation_mode, resource_low_water, resource_high_water, "
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

        # Move editor-seeded PvP defaults out of the low reserved ID range used
        # by canonical world SQL patches such as rev_living_world_027.
        ("2026_19_rekey_reserved_pvp_default_profile_entries",
         "world",
         "UPDATE living_world_bot_combat_default_entry e "
         "JOIN living_world_bot_combat_default_profile p ON p.default_profile_id = e.default_profile_id "
         "SET e.default_profile_id = e.default_profile_id + 1000 "
         "WHERE p.context_key = 'PvP' AND p.default_profile_id BETWEEN 19 AND 34"),

        ("2026_20_rekey_reserved_pvp_default_profiles",
         "world",
         "UPDATE living_world_bot_combat_default_profile "
         "SET default_profile_id = default_profile_id + 1000 "
         "WHERE context_key = 'PvP' AND default_profile_id BETWEEN 19 AND 34"),

        # ── Combat positioning thresholds in global bot behaviour ─────────────
        ("2025_27_bot_global_positioning_thresholds",
         "world",
         "INSERT IGNORE INTO living_world_bot_global_config (config_key, config_value, notes) VALUES"
         "  ('combat_follow_override_distance', 20.0, 'If ranged/healer bots drift farther than this from owner, snap back to follow behaviour'),"
         "  ('reposition_distance',             8.0, 'Passive-mode catch-up distance before reissuing follow'),"
         "  ('ranged_min_distance',             8.0, 'Back away when a ranged/healer bot is closer than this to its target'),"
         "  ('ranged_optimal_distance',        25.0, 'Preferred chase stop distance for ranged/healer combat positioning'),"
         "  ('ranged_cast_range',              30.0, 'Approach target when farther than this spell-usage range'),"
         "  ('ranged_retreat_distance',         5.0, 'Short backstep distance when retreating from melee range'),"
         "  ('ranged_retreat_trigger_pct',     80.0, 'Retreat when ranged bot HP drops below this percent in melee range'),"
         "  ('ranged_retreat_reset_pct',       60.0, 'Allow another retreat only after HP drops below this percent again')"),

        ("2025_28_bot_global_targeting_policy",
         "world",
         "INSERT IGNORE INTO living_world_bot_global_config (config_key, config_value, notes) VALUES"
         "  ('assist_use_current_victim', 1.0, 'Normal assist: keep fighting bot current victim if still valid'),"
         "  ('assist_use_owner_victim',   1.0, 'Normal assist: consider owner current victim as follow-up target'),"
         "  ('assist_owner_victim_must_target_owner', 1.0, 'Require owner victim to be actively fighting back against owner before assist picks it'),"
         "  ('attack_lock_use_owner_victim', 1.0, 'During attack-lock, consider owner current victim if current bot victim is unavailable'),"
         "  ('attack_lock_use_owner_selection', 1.0, 'During attack-lock, consider owner selected target if other sources are unavailable'),"
         "  ('guard_use_current_victim', 1.0, 'Guard mode: keep bot current victim if still valid'),"
         "  ('guard_use_owner_attackers', 1.0, 'Guard mode: consider units actively attacking the owner')"),

        ("2025_29_bot_global_target_validity_policy",
         "world",
         "INSERT IGNORE INTO living_world_bot_global_config (config_key, config_value, notes) VALUES"
         "  ('assist_require_targetable_for_attack', 1.0, 'Normal assist and guard: require candidate to currently pass attackable-for-attack checks'),"
         "  ('command_require_targetable_for_attack', 0.0, 'Forced-target and attack-lock assist: require candidate to currently pass attackable-for-attack checks instead of allowing pull/setup flicker')"),

        # ── Living World task/playlist editor schema sync ────────────────────
        ("2026_01_world_task_point_table",
         "world",
         "CREATE TABLE IF NOT EXISTS living_world_task_point ("
         "  point_id   INT UNSIGNED      NOT NULL AUTO_INCREMENT PRIMARY KEY,"
         "  point_key  VARCHAR(64)       NOT NULL,"
         "  zone_id    INT UNSIGNED      NOT NULL,"
         "  map_id     SMALLINT UNSIGNED NOT NULL,"
         "  point_type VARCHAR(32)       NOT NULL,"
         "  point_name VARCHAR(100)      NOT NULL,"
         "  x          FLOAT             NOT NULL,"
         "  y          FLOAT             NOT NULL,"
         "  z          FLOAT             NOT NULL,"
         "  UNIQUE KEY uq_point_key (point_key),"
         "  KEY idx_zone_type (zone_id, point_type)"
         ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci"),

        ("2026_02_world_task_template_table",
         "world",
         "CREATE TABLE IF NOT EXISTS living_world_task_template ("
         "  template_id         INT UNSIGNED      NOT NULL AUTO_INCREMENT PRIMARY KEY,"
         "  template_key        VARCHAR(64)       NOT NULL,"
         "  display_name        VARCHAR(100)      NOT NULL,"
         "  task_family         VARCHAR(32)       NOT NULL DEFAULT 'misc',"
         "  required_faction    TINYINT UNSIGNED  NOT NULL DEFAULT 0,"
         "  min_level           TINYINT UNSIGNED  NOT NULL DEFAULT 1,"
         "  max_level           TINYINT UNSIGNED  NOT NULL DEFAULT 80,"
         "  requires_herbalism  TINYINT(1)        NOT NULL DEFAULT 0,"
         "  requires_mining     TINYINT(1)        NOT NULL DEFAULT 0,"
         "  requires_fishing    TINYINT(1)        NOT NULL DEFAULT 0,"
         "  weight              TINYINT UNSIGNED  NOT NULL DEFAULT 1,"
         "  is_enabled          TINYINT(1)        NOT NULL DEFAULT 1,"
         "  UNIQUE KEY uq_template_key (template_key),"
         "  KEY idx_template_match (is_enabled, required_faction, min_level, max_level)"
         ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci"),

        ("2026_03_world_task_template_step_table",
         "world",
         "CREATE TABLE IF NOT EXISTS living_world_task_template_step ("
         "  template_id         INT UNSIGNED      NOT NULL,"
         "  step_order          SMALLINT UNSIGNED NOT NULL,"
         "  step_type           VARCHAR(32)       NOT NULL,"
         "  target_zone_id      INT UNSIGNED      NOT NULL,"
         "  target_point_key    VARCHAR(64)       NULL,"
         "  resolver_kind       VARCHAR(32)       NOT NULL DEFAULT 'zone',"
         "  subject_kind        VARCHAR(32)       NULL,"
         "  subject_id          INT UNSIGNED      NULL,"
         "  subject_key         VARCHAR(64)       NULL,"
         "  return_anchor_role  VARCHAR(32)       NULL,"
         "  cycle_count         TINYINT UNSIGNED  NOT NULL DEFAULT 1,"
         "  duration_min_sec    INT UNSIGNED      NOT NULL DEFAULT 0,"
         "  duration_max_sec    INT UNSIGNED      NOT NULL DEFAULT 0,"
         "  label               VARCHAR(100)      NOT NULL,"
         "  PRIMARY KEY (template_id, step_order),"
         "  KEY idx_step_zone (target_zone_id)"
         ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci"),

        ("2026_04_world_task_template_step_target_point_key",
         "world",
         "ALTER TABLE living_world_task_template_step "
         "ADD COLUMN target_point_key VARCHAR(64) NULL AFTER target_zone_id"),

        ("2026_05_world_task_template_step_resolver_kind",
         "world",
         "ALTER TABLE living_world_task_template_step "
         "ADD COLUMN resolver_kind VARCHAR(32) NOT NULL DEFAULT 'zone' AFTER target_point_key"),

        ("2026_06_world_task_template_step_subject_kind",
         "world",
         "ALTER TABLE living_world_task_template_step "
         "ADD COLUMN subject_kind VARCHAR(32) NULL AFTER resolver_kind"),

        ("2026_07_world_task_template_step_subject_id",
         "world",
         "ALTER TABLE living_world_task_template_step "
         "ADD COLUMN subject_id INT UNSIGNED NULL AFTER subject_kind"),

        ("2026_08_world_task_template_step_subject_key",
         "world",
         "ALTER TABLE living_world_task_template_step "
         "ADD COLUMN subject_key VARCHAR(64) NULL AFTER subject_id"),

        ("2026_09_world_task_template_step_return_anchor_role",
         "world",
         "ALTER TABLE living_world_task_template_step "
         "ADD COLUMN return_anchor_role VARCHAR(32) NULL AFTER subject_key"),

        ("2026_10_world_task_template_step_cycle_count",
         "world",
         "ALTER TABLE living_world_task_template_step "
         "ADD COLUMN cycle_count TINYINT UNSIGNED NOT NULL DEFAULT 1 AFTER return_anchor_role"),

        ("2026_11_world_transit_route_table",
         "world",
         "CREATE TABLE IF NOT EXISTS living_world_transit_route ("
         "  route_id          INT UNSIGNED      NOT NULL AUTO_INCREMENT PRIMARY KEY,"
         "  route_key         VARCHAR(64)       NOT NULL,"
         "  source_point_key  VARCHAR(64)       NOT NULL,"
         "  dest_point_key    VARCHAR(64)       NOT NULL,"
         "  transit_type      VARCHAR(16)       NOT NULL DEFAULT 'taxi',"
         "  required_faction  TINYINT UNSIGNED  NOT NULL DEFAULT 0,"
         "  min_level         TINYINT UNSIGNED  NOT NULL DEFAULT 1,"
         "  max_level         TINYINT UNSIGNED  NOT NULL DEFAULT 80,"
         "  duration_sec      INT UNSIGNED      NOT NULL DEFAULT 60,"
         "  display_name      VARCHAR(100)      NOT NULL,"
         "  UNIQUE KEY uq_route_key (route_key),"
         "  UNIQUE KEY uq_route_pair (source_point_key, dest_point_key, transit_type)"
         ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci"),

        ("2026_12_world_transit_route_transit_type",
         "world",
         "ALTER TABLE living_world_transit_route "
         "ADD COLUMN transit_type VARCHAR(16) NOT NULL DEFAULT 'taxi' AFTER dest_point_key"),

        ("2026_13_world_transit_route_min_level",
         "world",
         "ALTER TABLE living_world_transit_route "
         "ADD COLUMN min_level TINYINT UNSIGNED NOT NULL DEFAULT 1 AFTER required_faction"),

        ("2026_14_world_transit_route_max_level",
         "world",
         "ALTER TABLE living_world_transit_route "
         "ADD COLUMN max_level TINYINT UNSIGNED NOT NULL DEFAULT 80 AFTER min_level"),

        ("2026_15_world_playlist_table",
         "world",
         "CREATE TABLE IF NOT EXISTS living_world_playlist ("
         "  playlist_id         INT UNSIGNED      NOT NULL AUTO_INCREMENT PRIMARY KEY,"
         "  playlist_key        VARCHAR(64)       NOT NULL,"
         "  display_name        VARCHAR(100)      NOT NULL,"
         "  task_family         VARCHAR(32)       NOT NULL DEFAULT 'routine',"
         "  required_faction    TINYINT UNSIGNED  NOT NULL DEFAULT 0,"
         "  min_level           TINYINT UNSIGNED  NOT NULL DEFAULT 1,"
         "  max_level           TINYINT UNSIGNED  NOT NULL DEFAULT 80,"
         "  requires_herbalism  TINYINT(1)        NOT NULL DEFAULT 0,"
         "  requires_mining     TINYINT(1)        NOT NULL DEFAULT 0,"
         "  requires_fishing    TINYINT(1)        NOT NULL DEFAULT 0,"
         "  weight              TINYINT UNSIGNED  NOT NULL DEFAULT 1,"
         "  is_enabled          TINYINT(1)        NOT NULL DEFAULT 1,"
         "  UNIQUE KEY uq_playlist_key (playlist_key)"
         ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci"),

        ("2026_16_world_playlist_entry_table",
         "world",
         "CREATE TABLE IF NOT EXISTS living_world_playlist_entry ("
         "  playlist_id       INT UNSIGNED      NOT NULL,"
         "  entry_order       INT UNSIGNED      NOT NULL,"
         "  task_template_id  INT UNSIGNED      NOT NULL,"
         "  repeat_count      TINYINT UNSIGNED  NOT NULL DEFAULT 1,"
         "  note              VARCHAR(255)      NULL,"
         "  PRIMARY KEY (playlist_id, entry_order),"
         "  KEY idx_playlist_template (task_template_id)"
         ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci"),

        ("2026_17_world_zone_anchor_table",
         "world",
         "CREATE TABLE IF NOT EXISTS living_world_zone_anchor ("
         "  anchor_id         INT UNSIGNED      NOT NULL AUTO_INCREMENT PRIMARY KEY,"
         "  zone_id           INT UNSIGNED      NOT NULL,"
         "  point_key         VARCHAR(64)       NOT NULL,"
         "  anchor_role       VARCHAR(32)       NOT NULL,"
         "  required_faction  TINYINT UNSIGNED  NOT NULL DEFAULT 0,"
         "  min_level         TINYINT UNSIGNED  NOT NULL DEFAULT 1,"
         "  max_level         TINYINT UNSIGNED  NOT NULL DEFAULT 80,"
         "  weight            TINYINT UNSIGNED  NOT NULL DEFAULT 1,"
         "  notes             VARCHAR(255)      NULL,"
         "  UNIQUE KEY uq_zone_anchor_role_point (zone_id, anchor_role, point_key),"
         "  KEY idx_zone_anchor_lookup (zone_id, anchor_role, required_faction, min_level, max_level),"
         "  KEY idx_zone_anchor_point (point_key)"
         ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci"),

        ("2026_18_world_zone_content_table",
         "world",
         "CREATE TABLE IF NOT EXISTS living_world_zone_content ("
         "  content_id         INT UNSIGNED      NOT NULL AUTO_INCREMENT PRIMARY KEY,"
         "  zone_id            INT UNSIGNED      NOT NULL,"
         "  content_kind       VARCHAR(32)       NOT NULL,"
         "  subject_id         INT UNSIGNED      NULL,"
         "  subject_key        VARCHAR(64)       NULL,"
         "  display_name       VARCHAR(100)      NOT NULL,"
         "  required_faction   TINYINT UNSIGNED  NOT NULL DEFAULT 0,"
         "  min_level          TINYINT UNSIGNED  NOT NULL DEFAULT 1,"
         "  max_level          TINYINT UNSIGNED  NOT NULL DEFAULT 80,"
         "  min_skill          SMALLINT UNSIGNED NOT NULL DEFAULT 0,"
         "  max_skill          SMALLINT UNSIGNED NOT NULL DEFAULT 0,"
         "  weight             TINYINT UNSIGNED  NOT NULL DEFAULT 1,"
         "  anchor_point_key   VARCHAR(64)       NULL,"
         "  return_anchor_role VARCHAR(32)       NULL,"
         "  notes              VARCHAR(255)      NULL,"
         "  UNIQUE KEY uq_zone_content_kind_name (zone_id, content_kind, display_name),"
         "  KEY idx_zone_content_lookup (content_kind, zone_id, required_faction, min_level, max_level),"
         "  KEY idx_zone_content_subject (content_kind, subject_id, subject_key)"
         ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci"),

        ("2026_21_world_bot_identity_personality_key",
         "chars",
         "ALTER TABLE living_world_bot_identity "
         "ADD COLUMN personality_key VARCHAR(32) NOT NULL DEFAULT 'uninterested' AFTER gear_tier"),

        ("2026_22_world_bot_identity_personality_seed",
         "chars",
         "UPDATE living_world_bot_identity "
         "SET personality_key='uninterested' "
         "WHERE personality_key IS NULL OR personality_key = ''"),

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
        self._map_name_cache = None
        self._area_name_cache = None
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

    def load_world_bot_statuses(self, active_only: bool = False) -> list[dict]:
        where = "WHERE i.is_available = 0 AND i.is_retired = 0" if active_only else ""
        personality_select = "'uninterested' AS personality_key"
        runtime_state_select = "'' AS runtime_state"
        runtime_detail_select = "'' AS runtime_detail"
        explored_select = "'' AS explored_zone_ids, 0 AS explored_zone_count"
        explored_join = ""
        try:
            if self.q(self.chars, "SHOW COLUMNS FROM living_world_bot_identity LIKE 'personality_key'"):
                personality_select = "COALESCE(NULLIF(i.personality_key, ''), 'uninterested') AS personality_key"
        except Exception:
            pass
        try:
            if self.q(self.chars, "SHOW COLUMNS FROM living_world_bot_identity LIKE 'runtime_state'"):
                runtime_state_select = "COALESCE(i.runtime_state, '') AS runtime_state"
        except Exception:
            pass
        try:
            if self.q(self.chars, "SHOW COLUMNS FROM living_world_bot_identity LIKE 'runtime_detail'"):
                runtime_detail_select = "COALESCE(i.runtime_detail, '') AS runtime_detail"
        except Exception:
            pass
        try:
            if self.q(self.chars, "SHOW TABLES LIKE 'living_world_bot_explored_zone'"):
                explored_select = (
                    "COALESCE(explored.zone_ids, '') AS explored_zone_ids, "
                    "COALESCE(explored.zone_count, 0) AS explored_zone_count"
                )
                explored_join = """
                LEFT JOIN (
                    SELECT
                        bot_identity_id,
                        GROUP_CONCAT(zone_id ORDER BY first_seen_at ASC, zone_id ASC) AS zone_ids,
                        COUNT(*) AS zone_count
                    FROM living_world_bot_explored_zone
                    GROUP BY bot_identity_id
                ) explored ON explored.bot_identity_id = i.id
                """
        except Exception:
            pass
        return self.q(self.chars,
            f"""
            SELECT
                i.id,
                i.name,
                i.race_id,
                i.class_id,
                i.spec_key,
                i.faction,
                i.level,
                {personality_select},
                i.is_available,
                i.is_retired,
                i.session_count,
                i.total_world_online_ms,
                i.world_online_ms_since_level,
                i.post_max_world_online_ms,
                i.active_world_session_ms,
                i.active_world_session_start,
                {runtime_state_select},
                {runtime_detail_select},
                i.last_seen_zone,
                i.last_seen_at,
                {explored_select},
                latest.event_type AS latest_event_type,
                latest.detail AS latest_detail,
                latest.map_id AS latest_map_id,
                latest.zone_id AS latest_zone_id,
                latest.pos_x AS latest_pos_x,
                latest.pos_y AS latest_pos_y,
                latest.pos_z AS latest_pos_z,
                latest.logged_at AS latest_logged_at,
                sess.detail AS session_start_detail,
                sess.logged_at AS session_start_logged_at
            FROM living_world_bot_identity i
            LEFT JOIN living_world_bot_activity_log latest ON latest.id = (
                SELECT MAX(id)
                FROM living_world_bot_activity_log
                WHERE bot_guid = i.id
            )
            LEFT JOIN living_world_bot_activity_log sess ON sess.id = (
                SELECT MAX(id)
                FROM living_world_bot_activity_log
                WHERE bot_guid = i.id AND event_type = 'session_start'
            )
            {explored_join}
            {where}
            ORDER BY i.is_retired ASC, i.is_available ASC, i.level DESC, i.name ASC
            """)

    def load_world_bot_activity_log(self, bot_guid: int, limit: int = 200) -> list[dict]:
        return self.q(self.chars,
            "SELECT id, event_type, detail, map_id, zone_id, pos_x, pos_y, pos_z, logged_at "
            "FROM living_world_bot_activity_log "
            "WHERE bot_guid = %s "
            "ORDER BY id DESC "
            "LIMIT %s",
            (int(bot_guid), int(limit)))

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

    def _load_map_name_cache(self) -> dict:
        if self._map_name_cache is not None:
            return self._map_name_cache
        cache = {}
        if self.ok() and self._has_world_table("map_dbc"):
            try:
                rows = self.q(self.world,
                    "SELECT ID, MapName_Lang_enUS FROM map_dbc")
                cache = {
                    int(r["ID"]): (r.get("MapName_Lang_enUS") or "").strip()
                    for r in rows
                    if r.get("MapName_Lang_enUS")
                }
            except Exception:
                cache = {}
        self._map_name_cache = cache
        return self._map_name_cache

    def _load_area_name_cache(self) -> dict:
        if self._area_name_cache is not None:
            return self._area_name_cache
        cache = {}
        if self.ok() and self._has_world_table("areatable_dbc"):
            try:
                rows = self.q(self.world,
                    "SELECT ID, AreaName_Lang_enUS FROM areatable_dbc")
                cache = {
                    int(r["ID"]): (r.get("AreaName_Lang_enUS") or "").strip()
                    for r in rows
                    if r.get("AreaName_Lang_enUS")
                }
            except Exception:
                cache = {}
        self._area_name_cache = cache
        return self._area_name_cache

    def map_name(self, map_id: int) -> str:
        if map_id is None:
            return ""
        try:
            mid = int(map_id)
        except (TypeError, ValueError):
            return ""
        return self._load_map_name_cache().get(mid, "")

    def area_name(self, area_id: int) -> str:
        if area_id is None:
            return ""
        try:
            aid = int(area_id)
        except (TypeError, ValueError):
            return ""
        return self._load_area_name_cache().get(aid, "")

    # -----------------------------------------------------------------------
    # World-bot task points / templates
    # -----------------------------------------------------------------------

    def load_task_points(self) -> list[dict]:
        return self.q(self.world,
            "SELECT point_id, point_key, zone_id, map_id, point_type, point_name, x, y, z "
            "FROM living_world_task_point ORDER BY point_type, point_key")

    def upsert_task_point(self, row: dict) -> int:
        point_id = row.get("point_id")
        params = (
            row.get("point_key", "").strip(),
            int(row.get("zone_id") or 0),
            int(row.get("map_id") or 0),
            row.get("point_type", "").strip(),
            row.get("point_name", "").strip(),
            float(row.get("x") or 0),
            float(row.get("y") or 0),
            float(row.get("z") or 0),
        )
        if point_id:
            self.run(self.world,
                "UPDATE living_world_task_point SET point_key=%s, zone_id=%s, map_id=%s, point_type=%s, point_name=%s, x=%s, y=%s, z=%s WHERE point_id=%s",
                params + (int(point_id),))
            return int(point_id)
        return self.run(self.world,
            "INSERT INTO living_world_task_point (point_key, zone_id, map_id, point_type, point_name, x, y, z) VALUES (%s,%s,%s,%s,%s,%s,%s,%s)",
            params)

    def delete_task_point(self, point_id: int):
        self.run(self.world, "DELETE FROM living_world_task_point WHERE point_id=%s", (int(point_id),))

    def load_transit_routes(self) -> list[dict]:
        return self.q(self.world,
            "SELECT route_id, route_key, source_point_key, dest_point_key, transit_type, required_faction, min_level, max_level, duration_sec, display_name "
            "FROM living_world_transit_route ORDER BY route_key")

    def upsert_transit_route(self, row: dict) -> int:
        route_id = row.get("route_id")
        params = (
            row.get("route_key", "").strip(),
            row.get("source_point_key", "").strip(),
            row.get("dest_point_key", "").strip(),
            row.get("transit_type", "taxi").strip(),
            int(row.get("required_faction") or 0),
            int(row.get("min_level") or 1),
            int(row.get("max_level") or 80),
            int(row.get("duration_sec") or 0),
            row.get("display_name", "").strip(),
        )
        if route_id:
            self.run(self.world,
                "UPDATE living_world_transit_route SET route_key=%s, source_point_key=%s, dest_point_key=%s, transit_type=%s, required_faction=%s, min_level=%s, max_level=%s, duration_sec=%s, display_name=%s WHERE route_id=%s",
                params + (int(route_id),))
            return int(route_id)
        return self.run(self.world,
            "INSERT INTO living_world_transit_route (route_key, source_point_key, dest_point_key, transit_type, required_faction, min_level, max_level, duration_sec, display_name) VALUES (%s,%s,%s,%s,%s,%s,%s,%s,%s)",
            params)

    def delete_transit_route(self, route_id: int):
        self.run(self.world, "DELETE FROM living_world_transit_route WHERE route_id=%s", (int(route_id),))

    def load_zone_anchors(self) -> list[dict]:
        return self.q(self.world,
            "SELECT anchor_id, zone_id, point_key, anchor_role, required_faction, min_level, max_level, weight, notes "
            "FROM living_world_zone_anchor ORDER BY zone_id, anchor_role, point_key")

    def upsert_zone_anchor(self, row: dict) -> int:
        anchor_id = row.get("anchor_id")
        params = (
            int(row.get("zone_id") or 0),
            (row.get("point_key") or "").strip(),
            (row.get("anchor_role") or "").strip(),
            int(row.get("required_faction") or 0),
            int(row.get("min_level") or 1),
            int(row.get("max_level") or 80),
            int(row.get("weight") or 1),
            (row.get("notes") or "").strip() or None,
        )
        if anchor_id:
            self.run(self.world,
                "UPDATE living_world_zone_anchor SET zone_id=%s, point_key=%s, anchor_role=%s, required_faction=%s, min_level=%s, max_level=%s, weight=%s, notes=%s WHERE anchor_id=%s",
                params + (int(anchor_id),))
            return int(anchor_id)
        return self.run(self.world,
            "INSERT INTO living_world_zone_anchor (zone_id, point_key, anchor_role, required_faction, min_level, max_level, weight, notes) VALUES (%s,%s,%s,%s,%s,%s,%s,%s)",
            params)

    def delete_zone_anchor(self, anchor_id: int):
        self.run(self.world, "DELETE FROM living_world_zone_anchor WHERE anchor_id=%s", (int(anchor_id),))

    def load_zone_content(self) -> list[dict]:
        return self.q(self.world,
            "SELECT content_id, zone_id, content_kind, subject_id, subject_key, display_name, required_faction, min_level, max_level, min_skill, max_skill, weight, anchor_point_key, return_anchor_role, notes "
            "FROM living_world_zone_content ORDER BY content_kind, zone_id, display_name")

    def upsert_zone_content(self, row: dict) -> int:
        content_id = row.get("content_id")
        params = (
            int(row.get("zone_id") or 0),
            (row.get("content_kind") or "").strip(),
            int(row.get("subject_id") or 0) or None,
            (row.get("subject_key") or "").strip() or None,
            (row.get("display_name") or "").strip(),
            int(row.get("required_faction") or 0),
            int(row.get("min_level") or 1),
            int(row.get("max_level") or 80),
            int(row.get("min_skill") or 0),
            int(row.get("max_skill") or 0),
            int(row.get("weight") or 1),
            (row.get("anchor_point_key") or "").strip() or None,
            (row.get("return_anchor_role") or "").strip() or None,
            (row.get("notes") or "").strip() or None,
        )
        if content_id:
            self.run(self.world,
                "UPDATE living_world_zone_content SET zone_id=%s, content_kind=%s, subject_id=%s, subject_key=%s, display_name=%s, required_faction=%s, min_level=%s, max_level=%s, min_skill=%s, max_skill=%s, weight=%s, anchor_point_key=%s, return_anchor_role=%s, notes=%s WHERE content_id=%s",
                params + (int(content_id),))
            return int(content_id)
        return self.run(self.world,
            "INSERT INTO living_world_zone_content (zone_id, content_kind, subject_id, subject_key, display_name, required_faction, min_level, max_level, min_skill, max_skill, weight, anchor_point_key, return_anchor_role, notes) VALUES (%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s)",
            params)

    def delete_zone_content(self, content_id: int):
        self.run(self.world, "DELETE FROM living_world_zone_content WHERE content_id=%s", (int(content_id),))

    def load_task_templates(self) -> list[dict]:
        return self.q(self.world,
            "SELECT template_id, template_key, display_name, task_family, required_faction, min_level, max_level, requires_herbalism, requires_mining, requires_fishing, weight, is_enabled "
            "FROM living_world_task_template ORDER BY template_key")

    def load_task_template_steps(self, template_id: int) -> list[dict]:
        return self.q(self.world,
            "SELECT template_id, step_order, step_type, target_zone_id, target_point_key, resolver_kind, subject_kind, subject_id, subject_key, return_anchor_role, cycle_count, duration_min_sec, duration_max_sec, label "
            "FROM living_world_task_template_step WHERE template_id=%s ORDER BY step_order",
            (int(template_id),))

    def upsert_task_template(self, row: dict) -> int:
        template_id = row.get("template_id")
        params = (
            row.get("template_key", "").strip(),
            row.get("display_name", "").strip(),
            row.get("task_family", "").strip(),
            int(row.get("required_faction") or 0),
            int(row.get("min_level") or 1),
            int(row.get("max_level") or 80),
            int(bool(row.get("requires_herbalism") or 0)),
            int(bool(row.get("requires_mining") or 0)),
            int(bool(row.get("requires_fishing") or 0)),
            int(row.get("weight") or 1),
            int(bool(row.get("is_enabled") if row.get("is_enabled") is not None else 1)),
        )
        if template_id:
            self.run(self.world,
                "UPDATE living_world_task_template SET template_key=%s, display_name=%s, task_family=%s, required_faction=%s, min_level=%s, max_level=%s, requires_herbalism=%s, requires_mining=%s, requires_fishing=%s, weight=%s, is_enabled=%s WHERE template_id=%s",
                params + (int(template_id),))
            return int(template_id)
        return self.run(self.world,
            "INSERT INTO living_world_task_template (template_key, display_name, task_family, required_faction, min_level, max_level, requires_herbalism, requires_mining, requires_fishing, weight, is_enabled) VALUES (%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s)",
            params)

    def delete_task_template(self, template_id: int):
        self.run(self.world, "DELETE FROM living_world_task_template_step WHERE template_id=%s", (int(template_id),))
        self.run(self.world, "DELETE FROM living_world_task_template WHERE template_id=%s", (int(template_id),))

    def upsert_task_template_step(self, row: dict):
        self.run(self.world,
            "REPLACE INTO living_world_task_template_step (template_id, step_order, step_type, target_zone_id, target_point_key, resolver_kind, subject_kind, subject_id, subject_key, return_anchor_role, cycle_count, duration_min_sec, duration_max_sec, label) VALUES (%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s)",
            (
                int(row.get("template_id") or 0),
                int(row.get("step_order") or 0),
                row.get("step_type", "").strip(),
                int(row.get("target_zone_id") or 0),
                (row.get("target_point_key") or "").strip() or None,
                (row.get("resolver_kind") or "").strip() or None,
                (row.get("subject_kind") or "").strip() or None,
                int(row.get("subject_id") or 0) or None,
                (row.get("subject_key") or "").strip() or None,
                (row.get("return_anchor_role") or "").strip() or None,
                int(row.get("cycle_count") or 1),
                int(row.get("duration_min_sec") or 0),
                int(row.get("duration_max_sec") or 0),
                row.get("label", "").strip(),
            ))

    def delete_task_template_step(self, template_id: int, step_order: int):
        self.run(self.world,
            "DELETE FROM living_world_task_template_step WHERE template_id=%s AND step_order=%s",
            (int(template_id), int(step_order)))

    def load_playlists(self) -> list[dict]:
        return self.q(self.world,
            "SELECT playlist_id, playlist_key, display_name, task_family, required_faction, min_level, max_level, requires_herbalism, requires_mining, requires_fishing, weight, is_enabled "
            "FROM living_world_playlist ORDER BY playlist_key")

    def load_playlist_entries(self, playlist_id: int) -> list[dict]:
        return self.q(self.world,
            "SELECT e.playlist_id, e.entry_order, e.task_template_id, t.template_key, t.display_name AS template_display_name, e.repeat_count, e.note "
            "FROM living_world_playlist_entry e "
            "LEFT JOIN living_world_task_template t ON t.template_id = e.task_template_id "
            "WHERE e.playlist_id=%s ORDER BY e.entry_order",
            (int(playlist_id),))

    def upsert_playlist(self, row: dict) -> int:
        playlist_id = row.get("playlist_id")
        params = (
            row.get("playlist_key", "").strip(),
            row.get("display_name", "").strip(),
            row.get("task_family", "").strip(),
            int(row.get("required_faction") or 0),
            int(row.get("min_level") or 1),
            int(row.get("max_level") or 80),
            int(bool(row.get("requires_herbalism") or 0)),
            int(bool(row.get("requires_mining") or 0)),
            int(bool(row.get("requires_fishing") or 0)),
            int(row.get("weight") or 1),
            int(bool(row.get("is_enabled") if row.get("is_enabled") is not None else 1)),
        )
        if playlist_id:
            self.run(self.world,
                "UPDATE living_world_playlist SET playlist_key=%s, display_name=%s, task_family=%s, required_faction=%s, min_level=%s, max_level=%s, requires_herbalism=%s, requires_mining=%s, requires_fishing=%s, weight=%s, is_enabled=%s WHERE playlist_id=%s",
                params + (int(playlist_id),))
            return int(playlist_id)
        return self.run(self.world,
            "INSERT INTO living_world_playlist (playlist_key, display_name, task_family, required_faction, min_level, max_level, requires_herbalism, requires_mining, requires_fishing, weight, is_enabled) VALUES (%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s)",
            params)

    def delete_playlist(self, playlist_id: int):
        self.run(self.world, "DELETE FROM living_world_playlist_entry WHERE playlist_id=%s", (int(playlist_id),))
        self.run(self.world, "DELETE FROM living_world_playlist WHERE playlist_id=%s", (int(playlist_id),))

    def upsert_playlist_entry(self, row: dict):
        self.run(self.world,
            "REPLACE INTO living_world_playlist_entry (playlist_id, entry_order, task_template_id, repeat_count, note) VALUES (%s,%s,%s,%s,%s)",
            (
                int(row.get("playlist_id") or 0),
                int(row.get("entry_order") or 0),
                int(row.get("task_template_id") or 0),
                int(row.get("repeat_count") or 1),
                (row.get("note") or "").strip() or None,
            ))

    def delete_playlist_entry(self, playlist_id: int, entry_order: int):
        self.run(self.world,
            "DELETE FROM living_world_playlist_entry WHERE playlist_id=%s AND entry_order=%s",
            (int(playlist_id), int(entry_order)))

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

    # ── Default talent templates (acore_world) ───────────────────────────────

    def load_default_talent_template(self, spec_key: str, class_id: int) -> dict | None:
        rows = self.q(self.world,
            "SELECT template_id, spec_key, class_id, display_name "
            "FROM living_world_bot_talent_template "
            "WHERE LOWER(spec_key)=LOWER(%s) AND class_id=%s "
            "LIMIT 1",
            ((spec_key or "").strip(), int(class_id or 0)))
        if not rows:
            return None
        tmpl = dict(rows[0])
        tmpl["entries"] = self.q(self.world,
            "SELECT entry_id, priority, talent_id, talent_name, desired_rank "
            "FROM living_world_bot_talent_template_entry "
            "WHERE template_id=%s ORDER BY priority ASC, talent_id ASC",
            (int(tmpl["template_id"]),))
        return tmpl

    def ensure_default_talent_template(self, spec_key: str, class_id: int, display_name: str) -> int:
        return self.run(self.world,
            "INSERT INTO living_world_bot_talent_template "
            "(spec_key, class_id, display_name) VALUES (%s,%s,%s) "
            "ON DUPLICATE KEY UPDATE "
            "display_name=VALUES(display_name), "
            "template_id=LAST_INSERT_ID(template_id)",
            ((spec_key or "").strip(), int(class_id or 0), (display_name or "").strip()))

    def upsert_default_talent_template_entry(self, spec_key: str, class_id: int,
                                             display_name: str, talent_id: int,
                                             talent_name: str, desired_rank: int,
                                             priority: int):
        tmpl_id = self.ensure_default_talent_template(spec_key, class_id, display_name)
        desired_rank = int(desired_rank or 0)
        if desired_rank <= 0:
            self.run(self.world,
                "DELETE FROM living_world_bot_talent_template_entry "
                "WHERE template_id=%s AND talent_id=%s",
                (int(tmpl_id), int(talent_id or 0)))
            return
        self.run(self.world,
            "INSERT INTO living_world_bot_talent_template_entry "
            "(template_id, priority, talent_id, talent_name, desired_rank) "
            "VALUES (%s,%s,%s,%s,%s) "
            "ON DUPLICATE KEY UPDATE "
            "priority=VALUES(priority), "
            "talent_name=VALUES(talent_name), "
            "desired_rank=VALUES(desired_rank)",
            (int(tmpl_id), int(priority or 0), int(talent_id or 0),
             (talent_name or "").strip(), desired_rank))

    def reset_default_talent_template(self, spec_key: str, class_id: int):
        tmpl = self.load_default_talent_template(spec_key, class_id)
        if not tmpl:
            return
        self.run(self.world,
            "DELETE FROM living_world_bot_talent_template_entry WHERE template_id=%s",
            (int(tmpl["template_id"]),))

    def copy_default_talent_template(self, source_spec_key: str, source_class_id: int,
                                     dest_spec_key: str, dest_class_id: int,
                                     dest_display_name: str):
        source = self.load_default_talent_template(source_spec_key, source_class_id)
        self.reset_default_talent_template(dest_spec_key, dest_class_id)
        if not source:
            self.ensure_default_talent_template(dest_spec_key, dest_class_id, dest_display_name)
            return
        for row in source.get("entries", []):
            self.upsert_default_talent_template_entry(
                dest_spec_key,
                dest_class_id,
                dest_display_name,
                int(row.get("talent_id") or 0),
                row.get("talent_name", ""),
                int(row.get("desired_rank") or 0),
                int(row.get("priority") or 0),
            )

    # ── Default profiles (acore_world) ──────────────────────────────────────

    def load_default_profiles(self):
        return self.q(self.world,
            "SELECT * FROM living_world_bot_combat_default_profile "
            "ORDER BY class_key, spec_key, role_key, context_key, default_profile_id")

    def upsert_default_profile(self, p: dict) -> int:
        if p.get("default_profile_id"):
            self.run(self.world,
                "UPDATE living_world_bot_combat_default_profile SET "
                "spec_key=%s, role_key=%s, display_name=%s, class_key=%s, context_key=%s, "
                "conservation_mode=%s, resource_low_water=%s, resource_high_water=%s, "
                "enable_down_rank=%s, down_rank_floor=%s, default_aoe_mode=%s, "
                "default_aoe_min_targets=%s, default_aoe_scan_radius=%s, "
                "targeting_mode=%s, current_target_bias=%s, assist_target_bias=%s, focus_fire_bias=%s, "
                "protect_ally_bias=%s, prefer_healer_bias=%s, prefer_dps_bias=%s, avoid_tank_bias=%s "
                "WHERE default_profile_id=%s",
                (p["spec_key"], p["role_key"], p.get("display_name"),
                 p.get("class_key"), p.get("context_key", "PvE"),
                 p["conservation_mode"], p["resource_low_water"], p["resource_high_water"],
                 p["enable_down_rank"], p["down_rank_floor"], p["default_aoe_mode"],
                 p["default_aoe_min_targets"], p["default_aoe_scan_radius"],
                 p.get("targeting_mode", 0), p.get("current_target_bias", 80.0),
                 p.get("assist_target_bias", 140.0), p.get("focus_fire_bias", 55.0),
                 p.get("protect_ally_bias", 170.0), p.get("prefer_healer_bias", 220.0),
                 p.get("prefer_dps_bias", 140.0), p.get("avoid_tank_bias", 120.0),
                 p["default_profile_id"]))
            return p["default_profile_id"]
        return self.run(self.world,
            "INSERT INTO living_world_bot_combat_default_profile "
            "(spec_key, role_key, display_name, class_key, context_key, conservation_mode, "
            "resource_low_water, resource_high_water, enable_down_rank, down_rank_floor, "
            "default_aoe_mode, default_aoe_min_targets, default_aoe_scan_radius, targeting_mode, "
            "current_target_bias, assist_target_bias, focus_fire_bias, protect_ally_bias, prefer_healer_bias, "
            "prefer_dps_bias, avoid_tank_bias) VALUES "
            "(%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s)",
            (p["spec_key"], p["role_key"], p.get("display_name"), p.get("class_key"),
             p.get("context_key", "PvE"),
             p["conservation_mode"], p["resource_low_water"], p["resource_high_water"],
             p["enable_down_rank"], p["down_rank_floor"], p["default_aoe_mode"],
             p["default_aoe_min_targets"], p["default_aoe_scan_radius"],
             p.get("targeting_mode", 0), p.get("current_target_bias", 80.0),
             p.get("assist_target_bias", 140.0), p.get("focus_fire_bias", 55.0),
             p.get("protect_ally_bias", 170.0), p.get("prefer_healer_bias", 220.0),
             p.get("prefer_dps_bias", 140.0), p.get("avoid_tank_bias", 120.0)))

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
                "rank_value, target_key, aoe_mode, aoe_min_targets, aoe_radius) "
                "VALUES (%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s)",
                (entry_id, a["slot"], a["action_type"], a["spell_base_id"],
                 a["item_id"], a["rank_mode"], a["rank_value"], a["target_key"],
                 a.get("aoe_mode"), a.get("aoe_min_targets"), a.get("aoe_radius")))

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
                "resource_low_water=%s, resource_high_water=%s, enable_down_rank=%s, "
                "down_rank_floor=%s, default_aoe_mode=%s, default_aoe_min_targets=%s, "
                "default_aoe_scan_radius=%s, targeting_mode=%s, current_target_bias=%s, "
                "assist_target_bias=%s, focus_fire_bias=%s, protect_ally_bias=%s, "
                "prefer_healer_bias=%s, prefer_dps_bias=%s, avoid_tank_bias=%s, "
                "buff_scope=%s, buff_reapply_secs=%s, buff_on_spawn=%s, "
                "follow_dist_override=%s, auto_loot_override=%s, loot_quality_min=%s, "
                "gather_nodes=%s, gather_skin=%s, skin_loot_quality_max=%s, "
                "loot_category_flags=%s "
                "WHERE profile_id=%s",
                (p["slot"], p["profile_name"], p["guessed_spec_key"], p["guessed_role_key"],
                 p.get("spec_override_key"), p.get("role_override_key"),
                 p["conservation_mode"], p["resource_low_water"], p["resource_high_water"],
                 p["enable_down_rank"], p["down_rank_floor"], p["default_aoe_mode"],
                 p["default_aoe_min_targets"], p["default_aoe_scan_radius"],
                 p.get("targeting_mode", 0), p.get("current_target_bias", 80.0),
                 p.get("assist_target_bias", 140.0), p.get("focus_fire_bias", 55.0),
                 p.get("protect_ally_bias", 170.0), p.get("prefer_healer_bias", 220.0),
                 p.get("prefer_dps_bias", 140.0), p.get("avoid_tank_bias", 120.0),
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
            "guessed_spec_key, guessed_role_key, conservation_mode, resource_low_water, "
            "resource_high_water, enable_down_rank, down_rank_floor, default_aoe_mode, "
            "default_aoe_min_targets, default_aoe_scan_radius, targeting_mode, current_target_bias, "
            "assist_target_bias, focus_fire_bias, protect_ally_bias, prefer_healer_bias, prefer_dps_bias, "
            "avoid_tank_bias) VALUES "
            "(%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s)",
            (p["source_character_guid"], p["owner_account_id"], p["slot"],
             p["profile_name"], p["guessed_spec_key"], p["guessed_role_key"],
             p["conservation_mode"], p["resource_low_water"], p["resource_high_water"],
             p["enable_down_rank"], p["down_rank_floor"], p["default_aoe_mode"],
             p["default_aoe_min_targets"], p["default_aoe_scan_radius"],
             p.get("targeting_mode", 0), p.get("current_target_bias", 80.0),
             p.get("assist_target_bias", 140.0), p.get("focus_fire_bias", 55.0),
             p.get("protect_ally_bias", 170.0), p.get("prefer_healer_bias", 220.0),
             p.get("prefer_dps_bias", 140.0), p.get("avoid_tank_bias", 120.0)))

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


# Module-level singleton -- import this from all UI modules.
db = DBCtx()
