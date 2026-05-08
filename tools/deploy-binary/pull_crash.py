r"""
pull_crash.py -- Download the server Crashes folder and examine its contents.

Reads connection details from the same config files as deploy_binary.py.
Downloads every file in <remote_path>/Crashes/ to a local temp folder, then
prints a summary of each .dmp or .log found.

Usage:
    python pull_crash.py               # pulls and summarises all crash files
    python pull_crash.py --open        # also opens the temp folder in Explorer
    python pull_crash.py --clean       # delete remote Crashes folder contents after pull
"""

import argparse
import configparser
import os
import re
import subprocess
import sys
import tempfile
from datetime import datetime
from pathlib import Path

try:
    import paramiko
except ImportError:
    print("ERROR: paramiko is not installed. Run: pip install paramiko")
    sys.exit(1)

import warnings
warnings.filterwarnings("ignore")

# ── Paths ─────────────────────────────────────────────────────────────────────

SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT  = SCRIPT_DIR.parent.parent
LW_CONFIG  = REPO_ROOT / "tools" / "lw-editor" / "config.ini"
DEPLOY_INI = SCRIPT_DIR / "deploy.ini"


# ── Config ────────────────────────────────────────────────────────────────────

def load_config():
    for path, label in [(LW_CONFIG, "lw-editor/config.ini"), (DEPLOY_INI, "deploy-binary/deploy.ini")]:
        if not path.exists():
            print(f"ERROR: {label} not found at {path}")
            sys.exit(1)

    lw  = configparser.ConfigParser(); lw.read(LW_CONFIG)
    dep = configparser.ConfigParser(); dep.read(DEPLOY_INI)

    jump = {
        "host":     lw.get("ssh", "host"),
        "port":     lw.getint("ssh", "port", fallback=22),
        "user":     lw.get("ssh", "user"),
        "password": lw.get("ssh", "password", fallback=""),
        "key_file": lw.get("ssh", "key_file", fallback=""),
    }
    server = {
        "host":     dep.get("deploy", "server_host"),
        "port":     dep.getint("deploy", "server_port", fallback=22),
        "user":     dep.get("deploy", "server_user"),
        "password": dep.get("deploy", "server_password", fallback=""),
    }
    remote_root = dep.get("deploy", "remote_path").rstrip("/")
    return jump, server, remote_root


# ── SSH helpers (identical pattern to deploy_binary.py) ──────────────────────

def connect_jump(jump: dict) -> paramiko.SSHClient:
    client = paramiko.SSHClient()
    client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    kwargs = {"hostname": jump["host"], "port": jump["port"],
              "username": jump["user"], "timeout": 15}
    key = jump["key_file"].strip()
    if key and Path(key).exists():
        kwargs["key_filename"] = key
        if jump["password"]: kwargs["passphrase"] = jump["password"]
    else:
        kwargs["password"] = jump["password"]
    client.connect(**kwargs)
    return client


def connect_server(jump_client: paramiko.SSHClient, server: dict) -> paramiko.SSHClient:
    channel = jump_client.get_transport().open_channel(
        "direct-tcpip", (server["host"], server["port"]), ("127.0.0.1", 0))
    client = paramiko.SSHClient()
    client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    client.connect(server["host"], username=server["user"],
                   password=server["password"], sock=channel, timeout=15)
    return client


def run_remote(ssh: paramiko.SSHClient, cmd: str) -> str:
    _, out, _ = ssh.exec_command(cmd)
    return out.read().decode(errors="replace").strip()


# ── Crash analysis ────────────────────────────────────────────────────────────

def summarise_log(path: Path) -> None:
    """Print the tail of a .log file, highlighting ERROR/FATAL lines."""
    try:
        text = path.read_text(errors="replace")
    except OSError:
        print(f"  [cannot read {path.name}]")
        return

    lines = text.splitlines()
    # Always show last 80 lines
    tail = lines[-80:]

    error_lines = [l for l in lines if re.search(r"\b(ERROR|FATAL|ASSERT|exception|crash)\b", l, re.I)]

    print(f"\n{'─'*70}")
    print(f"  FILE : {path.name}  ({len(lines)} lines, {path.stat().st_size // 1024} KB)")
    print(f"{'─'*70}")

    if error_lines:
        print(f"\n  ── {len(error_lines)} ERROR/FATAL line(s) ──")
        for l in error_lines[-30:]:   # cap at 30
            print(f"  {l}")

    print(f"\n  ── tail (last {len(tail)} lines) ──")
    for l in tail:
        print(f"  {l}")


def summarise_dmp(path: Path) -> None:
    """Print basic info about a .dmp file (size, timestamp)."""
    stat = path.stat()
    ts   = datetime.fromtimestamp(stat.st_mtime).strftime("%Y-%m-%d %H:%M:%S")
    print(f"\n{'─'*70}")
    print(f"  FILE : {path.name}")
    print(f"  Size : {stat.st_size // 1024} KB    Modified: {ts}")
    print(f"  Note : .dmp files require WinDbg or Visual Studio to open.")
    print(f"         Full path: {path}")


# ── Main ──────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(description="Download and examine server crash files.")
    parser.add_argument("--open",  action="store_true", help="Open temp folder in Explorer after pull")
    parser.add_argument("--clean", action="store_true", help="Delete remote crash files after successful pull")
    args = parser.parse_args()

    jump_cfg, server_cfg, remote_root = load_config()
    remote_crashes = remote_root + "/Crashes"

    # Local temp dir — reuse across runs so Explorer stays on same folder
    local_dir = Path(tempfile.gettempdir()) / "lw_server_crashes"
    local_dir.mkdir(exist_ok=True)

    print(f"\n{'='*60}")
    print(f"  LivingWorld Server Crash Pull")
    print(f"{'='*60}")
    print(f"  Jump   : {jump_cfg['user']}@{jump_cfg['host']}:{jump_cfg['port']}")
    print(f"  Server : {server_cfg['user']}@{server_cfg['host']}")
    print(f"  Remote : {remote_crashes}")
    print(f"  Local  : {local_dir}")

    print(f"\nConnecting...")
    jump   = connect_jump(jump_cfg)
    server = connect_server(jump, server_cfg)
    sftp   = server.open_sftp()

    # Also try to grab the server log (it lives next to the exe, not in Crashes/)
    # Common log paths on Windows AzerothCore
    log_candidates = [
        remote_root + "/Server.log",
        remote_root + "/logs/Server.log",
        remote_root + "/logs/worldserver.log",
        remote_root + "/worldserver.log",
    ]

    downloaded = []

    # Pull Crashes/ folder
    try:
        entries = sftp.listdir(remote_crashes)
    except FileNotFoundError:
        print(f"\n  Crashes folder not found at {remote_crashes}")
        print("  Either the server hasn't crashed yet, or the path is different.")
        entries = []

    if not entries:
        print("  Crashes folder is empty — no minidumps present.")
    else:
        print(f"\n  Found {len(entries)} file(s) in Crashes/:")
        for name in sorted(entries):
            remote_file = remote_crashes + "/" + name
            local_file  = local_dir / name
            size_info = ""
            try:
                stat = sftp.stat(remote_file)
                size_info = f"  ({stat.st_size // 1024} KB)"
            except Exception:
                pass
            print(f"    {name}{size_info}")
            try:
                sftp.get(remote_file, str(local_file))
                downloaded.append(local_file)
            except Exception as e:
                print(f"    WARNING: could not download {name}: {e}")

    # Pull server log
    print(f"\n  Checking for server log...")
    for log_path in log_candidates:
        try:
            sftp.stat(log_path)
            local_log = local_dir / Path(log_path).name
            print(f"  Pulling {log_path}...")
            sftp.get(log_path, str(local_log))
            downloaded.append(local_log)
            break
        except FileNotFoundError:
            continue
    else:
        # Try reading via exec_command as fallback
        for log_path in log_candidates:
            result = run_remote(server, f'type "{log_path.replace("/", chr(92))}" 2>nul')
            if result:
                local_log = local_dir / "Server.log"
                local_log.write_text(result, encoding="utf-8", errors="replace")
                downloaded.append(local_log)
                print(f"  Pulled via exec_command.")
                break
        else:
            print("  No server log found. The server may not have written one yet.")

    # Clean remote if requested
    if args.clean and entries:
        print(f"\n  Cleaning remote Crashes/ folder...")
        for name in entries:
            try:
                sftp.remove(remote_crashes + "/" + name)
                print(f"    Removed {name}")
            except Exception as e:
                print(f"    WARNING: could not remove {name}: {e}")

    sftp.close()
    server.close()
    jump.close()

    # ── Analyse locally ───────────────────────────────────────────────────────
    if not downloaded:
        print("\n  Nothing downloaded. Server may have exited cleanly or has no logs yet.")
        return

    print(f"\n\n{'='*60}")
    print(f"  Analysis  ({len(downloaded)} file(s))")
    print(f"{'='*60}")

    for path in sorted(downloaded):
        if path.suffix.lower() in (".log", ".txt"):
            summarise_log(path)
        elif path.suffix.lower() == ".dmp":
            summarise_dmp(path)
        else:
            print(f"\n  {path.name}  ({path.stat().st_size} bytes) — unknown type, skipping")

    print(f"\n\n  All files saved to: {local_dir}")

    if args.open:
        subprocess.Popen(f'explorer "{local_dir}"')


if __name__ == "__main__":
    main()
