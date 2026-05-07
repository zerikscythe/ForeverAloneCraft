"""
Inspect living_world tables across all three ACore databases.
"""
import configparser, os, warnings
import mysql.connector
from sshtunnel import SSHTunnelForwarder

warnings.filterwarnings("ignore")

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
cfg = configparser.ConfigParser()
cfg.read(os.path.join(SCRIPT_DIR, "config.ini"))

ssh_host = cfg["ssh"]["host"]
ssh_port = int(cfg["ssh"]["port"])
ssh_user = cfg["ssh"]["user"]
ssh_pass = cfg["ssh"]["password"]
db_host  = cfg["ssh"]["db_host"]
db_port  = int(cfg["ssh"]["db_port"])
db_user  = cfg["ssh"]["db_user"]
db_pass  = cfg["ssh"]["db_password"]

def inspect_db(cur, db_name):
    cur.execute(f"USE `{db_name}`")
    cur.execute("SHOW TABLES LIKE 'living_world%'")
    tables = [r[0] for r in cur.fetchall()]
    if not tables:
        print(f"  (none)")
        return
    for t in tables:
        cur.execute(f"DESCRIBE `{t}`")
        cols = cur.fetchall()
        cur.execute(f"SELECT COUNT(*) FROM `{t}`")
        count = cur.fetchone()[0]
        print(f"\n  [{t}]  ({count} row{'s' if count != 1 else ''})")
        for col in cols:
            print(f"    {col[0]:35s}  {col[1]:30s}  {col[2]}  {col[4]}")

with SSHTunnelForwarder(
    (ssh_host, ssh_port),
    ssh_username=ssh_user,
    ssh_password=ssh_pass,
    remote_bind_address=(db_host, db_port),
) as tunnel:
    conn = mysql.connector.connect(
        host="127.0.0.1",
        port=tunnel.local_bind_port,
        user=db_user,
        password=db_pass,
    )
    cur = conn.cursor()

    for db in ("acore_auth", "acore_characters", "acore_world"):
        print(f"\n{'='*60}")
        print(f"  {db}")
        print(f"{'='*60}")
        inspect_db(cur, db)

    # Also show pool account rows if they exist
    for db in ("acore_auth", "acore_characters"):
        try:
            cur.execute(f"USE `{db}`")
            cur.execute("SELECT * FROM living_world_bot_account_pool LIMIT 20")
            rows = cur.fetchall()
            cur.execute("DESCRIBE living_world_bot_account_pool")
            col_names = [c[0] for c in cur.fetchall()]
            print(f"\n[{db}] living_world_bot_account_pool rows:")
            for row in rows:
                print(" ", dict(zip(col_names, row)))
        except mysql.connector.Error:
            pass

    cur.close()
    conn.close()

print("\n[inspect] Done.")
