#!/usr/bin/env python3
"""Run a deterministic materialized world-bot route travel harness.

This harness:
- rewrites the module config with a temporary debug route-harness setup
- starts worldserver headlessly
- waits for synthetic route-harness bots to spawn and travel
- captures stdout/stderr plus DB-backed activity rows
- writes a compact report showing attach/travel/arrival behavior per bot
"""

from __future__ import annotations

import argparse
import atexit
import configparser
import csv
import datetime as dt
import io
import pathlib
import signal
import subprocess
import time


REPO_ROOT = pathlib.Path(__file__).resolve().parents[2]
BUILD_ROOT = REPO_ROOT / "out" / "build-vs2022" / "bin" / "Debug"
WORLD_EXE = BUILD_ROOT / "worldserver.exe"
MODULE_CONF = BUILD_ROOT / "configs" / "modules" / "mod-living-world.conf"
MODULE_CONF_BACKUP = BUILD_ROOT / "configs" / "modules" / "mod-living-world.conf.route-harness.bak"
DBCONFIG = REPO_ROOT / "tools" / "lw-zone-editor" / "config.ini"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--levels", default="10,60", help="Comma-separated bot levels")
    parser.add_argument("--race-id", type=int, default=1)
    parser.add_argument("--class-id", type=int, default=1)
    parser.add_argument("--gender", type=int, default=0)
    parser.add_argument("--map-id", type=int, default=0)
    parser.add_argument("--start-x", type=float, default=-8833.0)
    parser.add_argument("--start-y", type=float, default=628.0)
    parser.add_argument("--start-z", type=float, default=95.0)
    parser.add_argument("--dest-zone-id", type=int, default=40)
    parser.add_argument("--dest-x", type=float, default=-10053.198)
    parser.add_argument("--dest-y", type=float, default=1455.3373)
    parser.add_argument("--dest-z", type=float, default=44.6324)
    parser.add_argument("--explored-zone-ids", default="", help="Comma-separated explored zones to seed onto each harness bot")
    parser.add_argument("--bake-zone-ids", default="", help="Comma-separated zone ids to terrain-bake before harness spawn")
    parser.add_argument("--idle-seconds", type=int, default=30)
    parser.add_argument("--seconds", type=int, default=360, help="How long to leave worldserver running after travel starts")
    parser.add_argument("--startup-timeout", type=int, default=180, help="How long to wait for spawn rows")
    return parser.parse_args()


def parse_levels(raw: str) -> list[int]:
    levels: list[int] = []
    for token in raw.split(","):
        token = token.strip()
        if not token:
            continue
        value = int(token)
        if value not in levels:
            levels.append(value)
    if not levels:
        raise ValueError("At least one level is required")
    return levels


def ensure_paths() -> None:
    if not WORLD_EXE.exists():
        raise FileNotFoundError(f"worldserver.exe not found: {WORLD_EXE}")
    if not MODULE_CONF.exists():
        raise FileNotFoundError(f"Module config not found: {MODULE_CONF}")


def load_db_settings() -> dict[str, str | int]:
    parser = configparser.ConfigParser()
    if not parser.read(DBCONFIG):
        raise FileNotFoundError(f"Could not read DB config: {DBCONFIG}")

    return {
        "host": parser.get("database", "host"),
        "port": parser.getint("database", "port"),
        "user": parser.get("database", "user"),
        "password": parser.get("database", "password"),
        "characters_db": "acore_characters",
    }


def mysql_base_command(settings: dict[str, str | int], database: str) -> list[str]:
    return [
        "mysql",
        "-h", str(settings["host"]),
        "-P", str(settings["port"]),
        "-u", str(settings["user"]),
        f"-p{settings['password']}",
        "--batch",
        "--raw",
        "--skip-column-names",
        database,
    ]


def run_mysql_query(
    settings: dict[str, str | int],
    database: str,
    sql: str,
) -> list[tuple[str, ...]]:
    command = mysql_base_command(settings, database) + ["-e", sql]
    result = subprocess.run(command, capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError(
            f"mysql query failed for {database}:\n{result.stdout}{result.stderr}"
        )

    output = result.stdout.strip()
    if not output:
        return []

    reader = csv.reader(io.StringIO(output), delimiter="\t")
    return [tuple(row) for row in reader]


def rewrite_module_config(original_text: str, updates: dict[str, str]) -> str:
    lines = original_text.splitlines()
    seen: set[str] = set()
    rewritten: list[str] = []
    for line in lines:
        stripped = line.strip()
        if not stripped or stripped.startswith("#") or "=" not in line:
            rewritten.append(line)
            continue
        key = line.split("=", 1)[0].strip()
        if key in updates:
            rewritten.append(f"{key} = {updates[key]}")
            seen.add(key)
        else:
            rewritten.append(line)

    for key, value in updates.items():
        if key not in seen:
            rewritten.append(f"{key} = {value}")

    return "\n".join(rewritten) + "\n"


def wait_for_spawn_rows(
    settings: dict[str, str | int],
    names: list[str],
    timeout_seconds: int,
) -> bool:
    deadline = time.time() + timeout_seconds
    while time.time() < deadline:
        found = 0
        for name in names:
            rows = run_mysql_query(
                settings,
                str(settings["characters_db"]),
                "SELECT COUNT(*) FROM living_world_bot_activity_log "
                f"WHERE bot_name = '{name}' AND event_type = 'travel_start'",
            )
            if rows and int(rows[0][0]) > 0:
                found += 1
        if found == len(names):
            return True
        time.sleep(1.0)
    return False


def fetch_activity_rows(
    settings: dict[str, str | int],
    names: list[str],
) -> list[tuple[str, ...]]:
    name_list = ",".join(f"'{name}'" for name in names)
    return run_mysql_query(
        settings,
        str(settings["characters_db"]),
        "SELECT bot_name, event_type, detail, map_id, zone_id, pos_x, pos_y, pos_z "
        "FROM living_world_bot_activity_log "
        f"WHERE bot_name IN ({name_list}) "
        "ORDER BY id ASC",
    )


def clear_prior_rows(settings: dict[str, str | int], names: list[str]) -> None:
    name_list = ",".join(f"'{name}'" for name in names)
    run_mysql_query(
        settings,
        str(settings["characters_db"]),
        "DELETE FROM living_world_bot_activity_log "
        f"WHERE bot_name IN ({name_list})",
    )


def build_report(
    rows: list[tuple[str, ...]],
    stdout_path: pathlib.Path,
    stderr_path: pathlib.Path,
    report_path: pathlib.Path,
) -> None:
    grouped: dict[str, list[tuple[str, ...]]] = {}
    for row in rows:
        grouped.setdefault(row[0], []).append(row)

    lines: list[str] = []
    lines.append(f"Route harness report generated {dt.datetime.now().isoformat()}")
    lines.append(f"stdout: {stdout_path}")
    lines.append(f"stderr: {stderr_path}")
    lines.append("")

    for bot_name, bot_rows in grouped.items():
        lines.append(f"[{bot_name}]")
        for event_type in (
            "travel_start",
            "travel_plan",
            "travel_waypoint",
            "travel_arrive",
            "travel_timeout",
            "travel_stuck",
            "session_abort",
            "position_tick",
            "status_change",
            "activity_complete",
            "session_complete",
        ):
            matching = [row for row in bot_rows if row[1] == event_type]
            if not matching:
                continue
            lines.append(f"  {event_type}: {len(matching)}")
            for row in matching[:8]:
                lines.append(
                    f"    zone={row[4]} pos=({row[5]},{row[6]},{row[7]}) detail={row[2]}"
                )
        lines.append("")

    report_path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    args = parse_args()
    levels = parse_levels(args.levels)
    names = [f"RouteHarnessL{level}" for level in levels]
    ensure_paths()
    settings = load_db_settings()

    timestamp = dt.datetime.now().strftime("%Y%m%d-%H%M%S")
    prefix = REPO_ROOT / f"route-harness-{timestamp}"
    stdout_path = prefix.with_suffix(".stdout.log")
    stderr_path = prefix.with_suffix(".stderr.log")
    report_path = prefix.with_suffix(".report.txt")

    if MODULE_CONF_BACKUP.exists():
        MODULE_CONF.write_text(MODULE_CONF_BACKUP.read_text(encoding="utf-8"), encoding="utf-8")
        MODULE_CONF_BACKUP.unlink()

    original_conf = MODULE_CONF.read_text(encoding="utf-8")
    MODULE_CONF_BACKUP.write_text(original_conf, encoding="utf-8")
    updates = {
        "LivingWorld.AmbientPopulation": "0",
        "LivingWorld.DebugSyntheticInterestEnabled": "0",
        "LivingWorld.DebugRouteHarnessEnabled": "1",
        "LivingWorld.DebugRouteHarnessLevels": f"\"{','.join(str(level) for level in levels)}\"",
        "LivingWorld.DebugRouteHarnessRaceId": str(args.race_id),
        "LivingWorld.DebugRouteHarnessClassId": str(args.class_id),
        "LivingWorld.DebugRouteHarnessGender": str(args.gender),
        "LivingWorld.DebugRouteHarnessMapId": str(args.map_id),
        "LivingWorld.DebugRouteHarnessStartX": str(args.start_x),
        "LivingWorld.DebugRouteHarnessStartY": str(args.start_y),
        "LivingWorld.DebugRouteHarnessStartZ": str(args.start_z),
        "LivingWorld.DebugRouteHarnessDestZoneId": str(args.dest_zone_id),
        "LivingWorld.DebugRouteHarnessDestX": str(args.dest_x),
        "LivingWorld.DebugRouteHarnessDestY": str(args.dest_y),
        "LivingWorld.DebugRouteHarnessDestZ": str(args.dest_z),
        "LivingWorld.DebugRouteHarnessExploredZones": f"\"{args.explored_zone_ids}\"",
        "LivingWorld.DebugRouteHarnessBakeRouteZ": "1" if args.bake_zone_ids else "0",
        "LivingWorld.DebugRouteHarnessBakeZoneIds": f"\"{args.bake_zone_ids}\"",
        "LivingWorld.DebugRouteHarnessIdleDurationSec": str(args.idle_seconds),
        "LivingWorld.RouteExportDir": f"\"{(REPO_ROOT / 'tools' / 'lw-zone-editor' / 'data' / 'exported_routes').as_posix()}\"",
        "LivingWorld.RouteTravel.FootYardsPerSecond": "4.5",
        "LivingWorld.RouteTravel.GroundBasicMultiplier": "1.6",
        "LivingWorld.RouteTravel.GroundFastMultiplier": "2.0",
        "LivingWorld.RouteTravel.FlightBasicMultiplier": "2.5",
        "LivingWorld.RouteTravel.FlightFastMultiplier": "4.1",
        "LivingWorld.RouteTravel.TaxiYardsPerSecond": "32.0",
        "LivingWorld.RouteTravel.GroundBasicMinLevel": "20",
        "LivingWorld.RouteTravel.GroundFastMinLevel": "40",
        "LivingWorld.RouteTravel.FlightBasicMinLevel": "60",
        "LivingWorld.RouteTravel.FlightFastMinLevel": "70",
    }

    clear_prior_rows(settings, names)
    MODULE_CONF.write_text(rewrite_module_config(original_conf, updates), encoding="utf-8")

    process: subprocess.Popen[str] | None = None
    restored = False

    def cleanup() -> None:
        nonlocal process, restored
        if not restored:
            MODULE_CONF.write_text(original_conf, encoding="utf-8")
            if MODULE_CONF_BACKUP.exists():
                MODULE_CONF_BACKUP.unlink()
            restored = True
        if process is not None and process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=20)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=10)

    def handle_signal(signum, _frame) -> None:
        cleanup()
        raise SystemExit(128 + signum)

    atexit.register(cleanup)
    signal.signal(signal.SIGINT, handle_signal)
    signal.signal(signal.SIGTERM, handle_signal)
    try:
        stdout_handle = stdout_path.open("w", encoding="utf-8", buffering=1)
        stderr_handle = stderr_path.open("w", encoding="utf-8", buffering=1)
        process = subprocess.Popen(
            [str(WORLD_EXE)],
            cwd=str(BUILD_ROOT),
            stdout=stdout_handle,
            stderr=stderr_handle,
            text=True,
        )

        ready = wait_for_spawn_rows(settings, names, args.startup_timeout)
        if not ready:
            raise RuntimeError("Harness bots did not emit travel_start rows before timeout")

        time.sleep(args.seconds)
        rows = fetch_activity_rows(settings, names)
        build_report(rows, stdout_path, stderr_path, report_path)
        print(report_path)
        return 0
    finally:
        cleanup()


if __name__ == "__main__":
    raise SystemExit(main())
