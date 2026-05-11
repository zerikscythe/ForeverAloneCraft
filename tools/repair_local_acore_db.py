import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

LIVE_HOST = "192.168.0.93"
LIVE_PORT = "3306"
LIVE_USER = "acore"
LIVE_PASSWORD = "acore"

LOCAL_HOST = "127.0.0.1"
LOCAL_PORT = "3306"
LOCAL_USER = "acore"
LOCAL_PASSWORD = "acore"

ROOT = Path(__file__).resolve().parents[1]

BASE_SQL = {
    "acore_auth": {
        "updates": ROOT / "data/sql/base/db_auth/updates.sql",
        "updates_include": ROOT / "data/sql/base/db_auth/updates_include.sql",
    },
    "acore_characters": {
        "updates": ROOT / "data/sql/base/db_characters/updates.sql",
        "updates_include": ROOT / "data/sql/base/db_characters/updates_include.sql",
    },
    "acore_world": {
        "updates": ROOT / "data/sql/base/db_world/updates.sql",
        "updates_include": ROOT / "data/sql/base/db_world/updates_include.sql",
    },
}


def which_or_die(name: str) -> str:
    exe = shutil.which(name)
    if not exe:
        print(f"ERROR: required executable not found: {name}")
        sys.exit(2)
    return exe


def run(cmd, env=None, dry_run=False, stdin_bytes=None):
    printable = " ".join(f'"{c}"' if " " in c else c for c in cmd)
    print(f"> {printable}")
    if dry_run:
        return 0

    proc = subprocess.run(
        cmd,
        input=stdin_bytes,
        env=env,
        capture_output=True,
    )
    if proc.stdout:
        print(proc.stdout.decode(errors="ignore").strip())
    if proc.returncode != 0:
        if proc.stderr:
            print(proc.stderr.decode(errors="ignore").strip())
    return proc.returncode


def mysql_env(password: str):
    env = os.environ.copy()
    env["MYSQL_PWD"] = password
    return env


def mysql_cmd(host, port, user, database=None):
    mysql = which_or_die("mysql")
    cmd = [mysql, "-h", host, "-P", port, "-u", user]
    if database:
        cmd += ["-D", database]
    return cmd


def mysqldump_cmd(host, port, user, database):
    mysqldump = which_or_die("mysqldump")
    return [
        mysqldump,
        "-h", host,
        "-P", port,
        "-u", user,
        "--single-transaction",
        "--skip-lock-tables",
        "--routines=0",
        "--events=0",
        database,
    ]


def clone_live_database(database: str, dry_run: bool):
    cmd = mysqldump_cmd(LIVE_HOST, LIVE_PORT, LIVE_USER, database)
    env = mysql_env(LIVE_PASSWORD)
    printable = " ".join(f'"{c}"' if " " in c else c for c in cmd)
    print(f"> {printable} | mysql ...")
    if dry_run:
        return 0

    dump_proc = subprocess.run(cmd, env=env, capture_output=True)
    if dump_proc.returncode != 0:
        if dump_proc.stderr:
            print(dump_proc.stderr.decode(errors="ignore").strip())
        return dump_proc.returncode

    import_cmd = mysql_cmd(LOCAL_HOST, LOCAL_PORT, LOCAL_USER, database)
    import_proc = subprocess.run(import_cmd, input=dump_proc.stdout, env=mysql_env(LOCAL_PASSWORD), capture_output=True)
    if import_proc.stdout:
        print(import_proc.stdout.decode(errors="ignore").strip())
    if import_proc.returncode != 0 and import_proc.stderr:
        print(import_proc.stderr.decode(errors="ignore").strip())
    return import_proc.returncode


def recreate_local_update_tables(database: str, dry_run: bool):
    for table_name in ("updates", "updates_include"):
        drop_sql = f"DROP TABLE IF EXISTS `{table_name}`;"
        rc = run(mysql_cmd(LOCAL_HOST, LOCAL_PORT, LOCAL_USER, database) + ["-e", drop_sql], env=mysql_env(LOCAL_PASSWORD), dry_run=dry_run)
        if rc != 0:
            return rc

        sql_file = BASE_SQL[database][table_name]
        cmd = mysql_cmd(LOCAL_HOST, LOCAL_PORT, LOCAL_USER, database)
        print(f"> {cmd[0]} ... < {sql_file}")
        if dry_run:
            continue
        with open(sql_file, "rb") as f:
            rc = run(cmd, env=mysql_env(LOCAL_PASSWORD), dry_run=False, stdin_bytes=f.read())
        if rc != 0:
            return rc
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="Repair local AzerothCore DB tables from healthy sources.")
    parser.add_argument("--apply", action="store_true", help="Execute changes. Without this flag, only print the plan.")
    parser.add_argument("--auth-from-live", action="store_true", help="Rebuild local acore_auth from the live server on 192.168.0.93.")
    parser.add_argument("--recreate-local-update-tables", action="store_true", help="Recreate local updates/updates_include tables for acore_characters and acore_world from base SQL.")
    args = parser.parse_args()

    dry_run = not args.apply

    if not args.auth_from_live and not args.recreate_local_update_tables:
        print("No action selected. Use --auth-from-live and/or --recreate-local-update-tables. Add --apply to execute.")
        return 1

    if args.auth_from_live:
        print("=== Rebuild local acore_auth from healthy live server ===")
        rc = clone_live_database("acore_auth", dry_run)
        if rc != 0:
            return rc

    if args.recreate_local_update_tables:
        print("=== Recreate local update tables for acore_characters ===")
        rc = recreate_local_update_tables("acore_characters", dry_run)
        if rc != 0:
            return rc

        print("=== Recreate local update tables for acore_world ===")
        rc = recreate_local_update_tables("acore_world", dry_run)
        if rc != 0:
            return rc

    print("DONE")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
