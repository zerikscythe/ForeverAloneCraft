"""
Show: characters per pool account, new prebuilt roster, and prebuilt table schema.
"""
import configparser, os, warnings
import mysql.connector
from sshtunnel import SSHTunnelForwarder

warnings.filterwarnings("ignore")

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
cfg = configparser.ConfigParser()
cfg.read(os.path.join(SCRIPT_DIR, "config.ini"))

with SSHTunnelForwarder(
    (cfg["ssh"]["host"], int(cfg["ssh"]["port"])),
    ssh_username=cfg["ssh"]["user"],
    ssh_password=cfg["ssh"]["password"],
    remote_bind_address=(cfg["ssh"]["db_host"], int(cfg["ssh"]["db_port"])),
) as tunnel:
    conn = mysql.connector.connect(
        host="127.0.0.1", port=tunnel.local_bind_port,
        user=cfg["ssh"]["db_user"], password=cfg["ssh"]["db_password"],
    )
    cur = conn.cursor()

    # ── characters on every pool account ──────────────────────────────────
    print("=== Characters on pool accounts ===")
    cur.execute("""
        SELECT p.account_id, p.account_name,
               c.guid, c.name, c.race, c.class, c.level
        FROM acore_auth.living_world_bot_account_pool p
        LEFT JOIN acore_characters.characters c ON c.account = p.account_id
        ORDER BY p.account_id, c.guid
    """)
    for row in cur.fetchall():
        acc_id, acc_name, guid, name, race, cls, lvl = row
        if guid:
            print(f"  account {acc_id:3d} ({acc_name})  "
                  f"guid={guid}  name={name}  race={race}  class={cls}  level={lvl}")
        else:
            print(f"  account {acc_id:3d} ({acc_name})  (no characters)")

    # ── new prebuilt roster table ──────────────────────────────────────────
    print("\n=== living_world_prebuilt_bot_roster schema ===")
    cur.execute("USE acore_characters")
    cur.execute("DESCRIBE living_world_prebuilt_bot_roster")
    for col in cur.fetchall():
        print(f"  {col[0]:35s}  {col[1]:25s}  null={col[2]}  default={col[4]}")

    cur.execute("SELECT * FROM living_world_prebuilt_bot_roster")
    rows = cur.fetchall()
    print(f"\n  Rows: {len(rows)}")
    for r in rows:
        print(" ", r)

    # ── player account 1 characters (the owner) ───────────────────────────
    print("\n=== Characters on player account 1 (owner) ===")
    cur.execute("""
        SELECT guid, name, race, class, level
        FROM acore_characters.characters
        WHERE account = 1 ORDER BY guid
    """)
    for row in cur.fetchall():
        print(f"  guid={row[0]}  name={row[1]}  race={row[2]}  class={row[3]}  level={row[4]}")

    cur.close()
    conn.close()

print("\n[done]")
