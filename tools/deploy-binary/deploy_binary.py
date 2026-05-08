#!/usr/bin/env python3
r"""
deploy_binary.py -- Upload a built binary to the remote server over SSH/SFTP.

Reads connection details from the same config.ini used by the lw-editor so
there is only one place to maintain SSH credentials.

Usage:
    python deploy_binary.py                          # deploys default binary
    python deploy_binary.py path/to/worldserver.exe  # deploy a specific file
    python deploy_binary.py --help

Drag-and-drop: drag worldserver.exe onto this script in Explorer and it will
deploy that file automatically.

Config is read from:
    tools/lw-editor/config.ini   (relative to repo root, same as lw-editor)

The remote destination path is read from:
    tools/deploy-binary/deploy.ini   (not tracked by git)
Copy deploy.ini.example to deploy.ini and fill in your remote server path.
"""

import argparse
import configparser
import os
import sys
import time
from pathlib import Path

try:
    import paramiko
except ImportError:
    print("ERROR: paramiko is not installed. Run: pip install paramiko")
    sys.exit(1)

# ── Paths ─────────────────────────────────────────────────────────────────────

SCRIPT_DIR  = Path(__file__).resolve().parent
REPO_ROOT   = SCRIPT_DIR.parent.parent
LW_CONFIG   = REPO_ROOT / "tools" / "lw-editor" / "config.ini"
DEPLOY_INI  = SCRIPT_DIR / "deploy.ini"

DEFAULT_BINARY = REPO_ROOT / "out" / "build-vs2022" / "bin" / "Debug" / "worldserver.exe"


# ── Config loading ─────────────────────────────────────────────────────────────

def load_config():
    if not LW_CONFIG.exists():
        print(f"ERROR: lw-editor config not found at {LW_CONFIG}")
        print("Copy tools/lw-editor/config.ini.example to config.ini and fill it in.")
        sys.exit(1)

    if not DEPLOY_INI.exists():
        print(f"ERROR: deploy config not found at {DEPLOY_INI}")
        print("Copy tools/deploy-binary/deploy.ini.example to deploy.ini and fill it in.")
        sys.exit(1)

    lw_cfg = configparser.ConfigParser()
    lw_cfg.read(LW_CONFIG)

    deploy_cfg = configparser.ConfigParser()
    deploy_cfg.read(DEPLOY_INI)

    # Jump host — from lw-editor config.ini
    jump = {
        "enabled":  lw_cfg.getboolean("ssh", "enabled", fallback=False),
        "host":     lw_cfg.get("ssh", "host", fallback=""),
        "port":     lw_cfg.getint("ssh", "port", fallback=22),
        "user":     lw_cfg.get("ssh", "user", fallback=""),
        "password": lw_cfg.get("ssh", "password", fallback=""),
        "key_file": lw_cfg.get("ssh", "key_file", fallback=""),
    }

    # Final server hop — from deploy.ini
    server = {
        "host":     deploy_cfg.get("deploy", "server_host", fallback=""),
        "port":     deploy_cfg.getint("deploy", "server_port", fallback=22),
        "user":     deploy_cfg.get("deploy", "server_user", fallback=""),
        "password": deploy_cfg.get("deploy", "server_password", fallback=""),
    }

    remote_path = deploy_cfg.get("deploy", "remote_path", fallback="")

    if not remote_path:
        print("ERROR: deploy.ini [deploy] remote_path is not set.")
        sys.exit(1)

    if not jump["enabled"]:
        print("ERROR: SSH is not enabled in lw-editor config.ini.")
        print("This script requires SSH to reach the server machine.")
        sys.exit(1)

    if not jump["host"] or not jump["user"]:
        print("ERROR: SSH host and user must be set in lw-editor config.ini [ssh].")
        sys.exit(1)

    if not server["host"] or not server["user"]:
        print("ERROR: server_host and server_user must be set in deploy.ini [deploy].")
        sys.exit(1)

    return jump, server, remote_path


# ── SSH/SFTP helpers ───────────────────────────────────────────────────────────

def connect_jump(jump: dict) -> paramiko.SSHClient:
    """Open SSH connection to the jump host."""
    client = paramiko.SSHClient()
    client.set_missing_host_key_policy(paramiko.AutoAddPolicy())

    connect_kwargs = {
        "hostname": jump["host"],
        "port":     jump["port"],
        "username": jump["user"],
        "timeout":  15,
    }

    key_file = jump["key_file"].strip()
    if key_file and Path(key_file).exists():
        connect_kwargs["key_filename"] = key_file
        if jump["password"]:
            connect_kwargs["passphrase"] = jump["password"]
        print(f"  Auth: SSH key ({key_file})")
    else:
        connect_kwargs["password"] = jump["password"]
        print(f"  Auth: password")

    client.connect(**connect_kwargs)
    return client


def connect_server(jump_client: paramiko.SSHClient, server: dict) -> paramiko.SSHClient:
    """Open SSH connection to the game server through the jump host."""
    transport = jump_client.get_transport()
    channel   = transport.open_channel(
        "direct-tcpip",
        (server["host"], server["port"]),
        ("127.0.0.1", 0),
    )

    client = paramiko.SSHClient()
    client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    client.connect(
        server["host"],
        username=server["user"],
        password=server["password"],
        sock=channel,
        timeout=15,
    )
    return client


def ensure_remote_dir(sftp: paramiko.SFTPClient, remote_dir: str):
    """Create remote directory if it does not exist (non-recursive, single level)."""
    try:
        sftp.stat(remote_dir)
    except FileNotFoundError:
        print(f"  Remote dir not found, creating: {remote_dir}")
        sftp.mkdir(remote_dir)


def progress_callback(transferred: int, total: int):
    pct = (transferred / total * 100) if total else 0
    mb_done = transferred / 1_048_576
    mb_total = total / 1_048_576
    print(f"\r  Uploading ... {mb_done:.1f} / {mb_total:.1f} MB  ({pct:.0f}%)", end="", flush=True)


# ── Main ───────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(description="Deploy a built binary to the remote server over SSH.")
    parser.add_argument(
        "binary",
        nargs="?",
        default=str(DEFAULT_BINARY),
        help=f"Path to the binary to deploy (default: {DEFAULT_BINARY})",
    )
    args = parser.parse_args()

    binary_path = Path(args.binary).resolve()

    if not binary_path.exists():
        print(f"ERROR: Binary not found: {binary_path}")
        sys.exit(1)

    file_size   = binary_path.stat().st_size
    jump_cfg, server_cfg, remote_dir = load_config()
    remote_dir  = remote_dir.rstrip("/")
    remote_file = remote_dir + "/" + binary_path.name

    print(f"\n{'='*60}")
    print(f"  AzerothCore Binary Deploy")
    print(f"{'='*60}")
    print(f"  Source  : {binary_path}")
    print(f"  Size    : {file_size / 1_048_576:.1f} MB")
    print(f"  Jump    : {jump_cfg['user']}@{jump_cfg['host']}:{jump_cfg['port']}")
    print(f"  Server  : {server_cfg['user']}@{server_cfg['host']}")
    print(f"  Dest    : {remote_file}")
    print(f"{'='*60}\n")

    print(f"[1/4] Connecting to jump host {jump_cfg['host']}:{jump_cfg['port']} ...")
    try:
        jump_client = connect_jump(jump_cfg)
    except Exception as e:
        print(f"\nERROR: Jump host connection failed: {e}")
        sys.exit(1)
    print("  Connected.")

    print(f"\n[2/4] Hopping to {server_cfg['user']}@{server_cfg['host']} ...")
    try:
        server_client = connect_server(jump_client, server_cfg)
    except Exception as e:
        print(f"\nERROR: Server connection failed: {e}")
        jump_client.close()
        sys.exit(1)
    print("  Connected.")

    try:
        sftp = server_client.open_sftp()

        print(f"\n[3/4] Checking remote path ...")
        try:
            sftp.stat(remote_dir)
            print(f"  Remote path exists: {remote_dir}")
        except FileNotFoundError:
            print(f"ERROR: Remote path does not exist: {remote_dir}")
            print("Check [deploy] remote_path in tools/deploy-binary/deploy.ini")
            sys.exit(1)

        print(f"\n[4/4] Uploading {binary_path.name} ...")
        start = time.time()
        sftp.put(str(binary_path), remote_file, callback=progress_callback)
        elapsed = time.time() - start
        print(f"\n  Done in {elapsed:.1f}s")

        sftp.close()

    finally:
        server_client.close()
        jump_client.close()

    print(f"\n{'='*60}")
    print(f"  Deploy complete.")
    print(f"  {binary_path.name} -> {server_cfg['host']}:{remote_file}")
    print(f"{'='*60}\n")


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\nCancelled.")
        sys.exit(1)
