"""
Phase 1 migration: applies living_world_prebuilt_bot_roster to the character DB.
Reads connection details from config.ini in the same directory.
"""
import configparser
import os
import warnings
import mysql.connector
from sshtunnel import SSHTunnelForwarder

warnings.filterwarnings("ignore")

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
cfg = configparser.ConfigParser()
cfg.read(os.path.join(SCRIPT_DIR, "config.ini"))

ssh_host    = cfg["ssh"]["host"]
ssh_port    = int(cfg["ssh"]["port"])
ssh_user    = cfg["ssh"]["user"]
ssh_pass    = cfg["ssh"]["password"]
db_host     = cfg["ssh"]["db_host"]
db_port     = int(cfg["ssh"]["db_port"])
db_user     = cfg["ssh"]["db_user"]
db_pass     = cfg["ssh"]["db_password"]

DDL = """
CREATE TABLE IF NOT EXISTS living_world_prebuilt_bot_roster (
    id               INT UNSIGNED     NOT NULL AUTO_INCREMENT PRIMARY KEY,
    owner_account_id INT UNSIGNED     NOT NULL COMMENT 'Player account allowed to summon this bot',
    bot_account_id   INT UNSIGNED     NOT NULL COMMENT 'Pool account the bot character lives on',
    character_guid   BIGINT UNSIGNED  NOT NULL COMMENT 'GUID from the characters table',
    UNIQUE KEY uq_char (character_guid),
    KEY idx_owner (owner_account_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Pre-built companion bots (Phase 1)';
"""

print(f"[migrate] SSH {ssh_user}@{ssh_host}:{ssh_port} -> {db_host}:{db_port}")

with SSHTunnelForwarder(
    (ssh_host, ssh_port),
    ssh_username=ssh_user,
    ssh_password=ssh_pass,
    remote_bind_address=(db_host, db_port),
) as tunnel:
    print(f"[migrate] Tunnel open on localhost:{tunnel.local_bind_port}")

    # Find the character database
    conn = mysql.connector.connect(
        host="127.0.0.1",
        port=tunnel.local_bind_port,
        user=db_user,
        password=db_pass,
    )
    cur = conn.cursor()
    cur.execute("SHOW DATABASES")
    all_dbs = [r[0] for r in cur.fetchall()]
    print("[migrate] Databases:", all_dbs)

    # Heuristic: prefer a DB named *characters* or *acore_characters*
    char_db = None
    for candidate in ("acore_characters", "characters"):
        if candidate in all_dbs:
            char_db = candidate
            break
    if char_db is None:
        # Fall back to any db containing 'char'
        for db in all_dbs:
            if "char" in db.lower():
                char_db = db
                break

    if char_db is None:
        print("[migrate] ERROR: could not identify character database. Aborting.")
        cur.close()
        conn.close()
        raise SystemExit(1)

    print(f"[migrate] Using character database: {char_db}")
    cur.execute(f"USE `{char_db}`")

    # Check existing tables of interest
    cur.execute("SHOW TABLES LIKE 'living_world%'")
    existing = [r[0] for r in cur.fetchall()]
    print(f"[migrate] Existing living_world tables: {existing}")

    # Apply migration
    if "living_world_prebuilt_bot_roster" in existing:
        print("[migrate] Table already exists — checking columns.")
        cur.execute("DESCRIBE living_world_prebuilt_bot_roster")
        cols = cur.fetchall()
        print("[migrate] Current schema:")
        for col in cols:
            print("  ", col)
    else:
        print("[migrate] Creating living_world_prebuilt_bot_roster ...")
        cur.execute(DDL)
        conn.commit()
        print("[migrate] Table created OK.")

    # Also show living_world_bot_account_pool if it exists
    if "living_world_bot_account_pool" in existing:
        cur.execute("SELECT * FROM living_world_bot_account_pool LIMIT 10")
        rows = cur.fetchall()
        cur.execute("DESCRIBE living_world_bot_account_pool")
        cols = [c[0] for c in cur.fetchall()]
        print(f"\n[migrate] living_world_bot_account_pool columns: {cols}")
        print(f"[migrate] Rows (up to 10):")
        for row in rows:
            print("  ", dict(zip(cols, row)))
    else:
        print("\n[migrate] living_world_bot_account_pool does not exist yet.")

    cur.close()
    conn.close()

print("[migrate] Done.")
