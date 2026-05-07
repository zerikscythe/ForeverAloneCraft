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
        database="acore_world",
    )
    cur = conn.cursor()
    cur.execute("DESCRIBE living_world_bot_combat_default_profile")
    print("living_world_bot_combat_default_profile columns:")
    for col in cur.fetchall():
        print(f"  {col[0]:40s} {col[1]}")
    cur.close()
    conn.close()
