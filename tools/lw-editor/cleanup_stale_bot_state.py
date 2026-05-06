#!/usr/bin/env python3
"""
cleanup_stale_bot_state.py
--------------------------
Fixes stale database state left behind by a worldserver crash while bot
clones were active.

Two categories of stale state are cleaned up:

  1. auth.account.online = 1 for bot pool accounts that are no longer
     connected.  AzerothCore normally resets this in LogoutPlayer(), but a
     crash prevents that cleanup from running.

  2. living_world_account_alt_runtime rows whose state = 'Active' but whose
     clone character is not actually logged in.  These rows are left in
     Active so the coordinator can still find and reuse the clone on the next
     request.  We only reset them to 'Active' (no change needed there), but
     we DO release their bot pool account (is_available = 1) so the pool is
     not permanently exhausted.

Usage:
    python cleanup_stale_bot_state.py [--dry-run]

Options:
    --dry-run   Print what would be changed without executing any writes.
"""

import argparse
import configparser
import sys

import mysql.connector

try:
    from sshtunnel import SSHTunnelForwarder
    SSH_TUNNEL_AVAILABLE = True
except ImportError:
    SSHTunnelForwarder = None
    SSH_TUNNEL_AVAILABLE = False


def load_config(path: str = "config.ini") -> configparser.ConfigParser:
    cfg = configparser.ConfigParser()
    cfg.read(path)
    return cfg


def open_connections(cfg: configparser.ConfigParser):
    """Return (tunnel_or_None, auth_conn, char_conn)."""
    tunnel = None
    ssh_enabled = cfg.getboolean("ssh", "enabled", fallback=False)

    if ssh_enabled:
        if not SSH_TUNNEL_AVAILABLE:
            print("ERROR: sshtunnel not installed.  pip install sshtunnel")
            sys.exit(1)

        print("[SSH]  Connecting via SSH tunnel ...")
        ssh_kwargs = {
            "ssh_address_or_host": (
                cfg.get("ssh", "host"),
                cfg.getint("ssh", "port", fallback=22),
            ),
            "ssh_username": cfg.get("ssh", "user"),
            "remote_bind_address": (
                cfg.get("ssh", "db_host"),
                cfg.getint("ssh", "db_port", fallback=3306),
            ),
        }
        key_file = cfg.get("ssh", "key_file", fallback="")
        password = cfg.get("ssh", "password", fallback="")
        if key_file:
            ssh_kwargs["ssh_pkey"] = key_file
        elif password:
            ssh_kwargs["ssh_password"] = password
        else:
            print("ERROR: SSH enabled but no password or key_file provided.")
            sys.exit(1)

        tunnel = SSHTunnelForwarder(**ssh_kwargs)
        tunnel.start()
        db_host = "127.0.0.1"
        db_port = tunnel.local_bind_port
        db_user = cfg.get("ssh", "db_user")
        db_pass = cfg.get("ssh", "db_password")
    else:
        print("[LOCAL] Connecting directly to database ...")
        db_host = cfg.get("database", "host", fallback="127.0.0.1")
        db_port = cfg.getint("database", "port", fallback=3306)
        db_user = cfg.get("database", "user")
        db_pass = cfg.get("database", "password")

    common = dict(host=db_host, port=db_port, user=db_user,
                  password=db_pass, charset="utf8mb4")

    auth_conn = mysql.connector.connect(database="acore_auth", **common)
    char_conn = mysql.connector.connect(database="acore_characters", **common)
    return tunnel, auth_conn, char_conn


def fetch_bot_pool_accounts(auth_conn) -> list[dict]:
    """Return all accounts in the bot pool with their online / available flags."""
    cur = auth_conn.cursor(dictionary=True)
    cur.execute("""
        SELECT p.account_id, p.is_available, p.reserved_for,
               a.username, a.online
        FROM living_world_bot_account_pool p
        JOIN account a ON a.id = p.account_id
        ORDER BY p.account_id
    """)
    rows = cur.fetchall()
    cur.close()
    return rows


def fetch_active_runtimes(char_conn) -> list[dict]:
    """Return runtime rows in Active state (tinyint value 1)."""
    # AccountAltRuntimeState enum: PreparingClone=0, Active=1, ...
    ACTIVE_STATE = 1
    cur = char_conn.cursor(dictionary=True)
    cur.execute("""
        SELECT runtime_id, source_account_id, source_character_guid,
               clone_character_guid, clone_account_id,
               source_character_name, clone_character_name, state
        FROM living_world_account_alt_runtime
        WHERE state = %s
    """, (ACTIVE_STATE,))
    rows = cur.fetchall()
    cur.close()
    return rows


def cleanup(dry_run: bool) -> None:
    cfg = load_config()
    tunnel, auth_conn, char_conn = open_connections(cfg)
    tag = "[DRY RUN] " if dry_run else ""

    try:
        print()
        print("=" * 70)
        print("STEP 1 — Reset stale online=1 flags for bot pool accounts")
        print("=" * 70)

        pool_accounts = fetch_bot_pool_accounts(auth_conn)
        stale_online = [r for r in pool_accounts if r["online"] == 1]

        if not stale_online:
            print("  ✅  No bot pool accounts have online=1.  Nothing to do.")
        else:
            for row in stale_online:
                print(f"  {tag}Resetting account {row['account_id']} "
                      f"({row['username']}) online -> 0")
                if not dry_run:
                    cur = auth_conn.cursor()
                    cur.execute(
                        "UPDATE account SET online = 0 WHERE id = %s",
                        (row["account_id"],),
                    )
                    auth_conn.commit()
                    cur.close()

        print()
        print("=" * 70)
        print("STEP 2 — Release bot pool accounts locked by crashed runtimes")
        print("=" * 70)

        active_runtimes = fetch_active_runtimes(char_conn)

        if not active_runtimes:
            print("  ✅  No Active runtimes found.  Nothing to do.")
        else:
            for rt in active_runtimes:
                clone_guid = rt["clone_character_guid"]
                bot_account_id = rt["clone_account_id"]

                # Check if the clone is actually online right now (it won't be
                # if the server is down or the bot was never successfully spawned
                # after a crash).
                cur = char_conn.cursor(dictionary=True)
                cur.execute(
                    "SELECT guid FROM characters WHERE guid = %s LIMIT 1",
                    (clone_guid,),
                )
                clone_exists = cur.fetchone() is not None
                cur.close()

                print(f"\n  Runtime {rt['runtime_id']}: "
                      f"source='{rt['source_character_name']}' "
                      f"clone='{rt['clone_character_name']}' "
                      f"clone_guid={clone_guid} "
                      f"bot_account={bot_account_id}")
                print(f"    Clone character exists in DB: {clone_exists}")

                # Release the bot pool account so it can be reused.
                # (The runtime row itself stays Active so the coordinator
                # finds the existing clone and reuses it on the next request.)
                cur_auth = auth_conn.cursor(dictionary=True)
                cur_auth.execute(
                    "SELECT is_available, reserved_for "
                    "FROM living_world_bot_account_pool "
                    "WHERE account_id = %s LIMIT 1",
                    (bot_account_id,),
                )
                pool_row = cur_auth.fetchone()
                cur_auth.close()

                if pool_row and pool_row["is_available"] == 0:
                    print(f"    {tag}Releasing bot account {bot_account_id} "
                          f"back to pool  (reserved_for={pool_row['reserved_for']} -> NULL)")
                    if not dry_run:
                        cur_auth = auth_conn.cursor()
                        cur_auth.execute(
                            "UPDATE living_world_bot_account_pool "
                            "SET is_available = 1, reserved_for = NULL "
                            "WHERE account_id = %s",
                            (bot_account_id,),
                        )
                        auth_conn.commit()
                        cur_auth.close()
                else:
                    print(f"    Bot account {bot_account_id} is already "
                          "marked available — no pool change needed.")

        print()
        print("=" * 70)
        print("Done." if not dry_run else "Done (dry run — no changes written).")
        print("=" * 70)
    finally:
        auth_conn.close()
        char_conn.close()
        if tunnel:
            tunnel.stop()


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Clean up stale bot account / runtime state after a crash."
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Show what would change without writing anything.",
    )
    args = parser.parse_args()
    cleanup(dry_run=args.dry_run)


if __name__ == "__main__":
    main()
