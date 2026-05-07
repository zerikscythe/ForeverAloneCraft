"""
Phase 1 corrected migration:
  - Drops living_world_prebuilt_bot_roster (created by the incorrect Phase 1 attempt)
  - Creates living_world_pool_character (the correct Phase 1 table)
"""
import configparser, os, warnings
import mysql.connector
from sshtunnel import SSHTunnelForwarder

warnings.filterwarnings("ignore")

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
cfg = configparser.ConfigParser()
cfg.read(os.path.join(SCRIPT_DIR, "config.ini"))

DDL_DROP = "DROP TABLE IF EXISTS living_world_prebuilt_bot_roster"

DDL_CREATE = """
CREATE TABLE IF NOT EXISTS living_world_pool_character (
    id             INT UNSIGNED      NOT NULL AUTO_INCREMENT PRIMARY KEY,
    bot_account_id INT UNSIGNED      NOT NULL,
    character_guid BIGINT UNSIGNED   NOT NULL,
    class_id       TINYINT UNSIGNED  NOT NULL,
    spec_role      VARCHAR(16)       NOT NULL,
    spec_key       VARCHAR(32)       NOT NULL,
    level          TINYINT UNSIGNED  NOT NULL,
    avg_ilvl       SMALLINT UNSIGNED NOT NULL DEFAULT 0,
    faction        TINYINT UNSIGNED  NOT NULL,
    is_available   TINYINT UNSIGNED  NOT NULL DEFAULT 1,
    UNIQUE KEY uq_char (character_guid),
    KEY idx_role   (class_id, spec_role, level, avg_ilvl, faction, is_available)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
  COMMENT='Server-pool raid bots spawnable via .lwbot raid request'
"""

with SSHTunnelForwarder(
    (cfg["ssh"]["host"], int(cfg["ssh"]["port"])),
    ssh_username=cfg["ssh"]["user"],
    ssh_password=cfg["ssh"]["password"],
    remote_bind_address=(cfg["ssh"]["db_host"], int(cfg["ssh"]["db_port"])),
) as tunnel:
    conn = mysql.connector.connect(
        host="127.0.0.1", port=tunnel.local_bind_port,
        user=cfg["ssh"]["db_user"], password=cfg["ssh"]["db_password"],
        database="acore_characters",
    )
    cur = conn.cursor()

    cur.execute(DDL_DROP)
    conn.commit()
    print("[migrate] Dropped living_world_prebuilt_bot_roster (if existed).")

    cur.execute(DDL_CREATE)
    conn.commit()
    print("[migrate] Created living_world_pool_character.")

    # Confirm final state of living_world tables
    cur.execute("SHOW TABLES LIKE 'living_world%'")
    tables = [r[0] for r in cur.fetchall()]
    print(f"[migrate] living_world tables now: {tables}")

    cur.execute("DESCRIBE living_world_pool_character")
    print("[migrate] Schema:")
    for col in cur.fetchall():
        print(f"  {col[0]:20s}  {col[1]:25s}  null={col[2]}  default={col[4]}")

    cur.close()
    conn.close()

print("[migrate] Done.")
