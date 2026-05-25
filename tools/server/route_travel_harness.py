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
import re
import signal
import subprocess
import tempfile
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
    parser.add_argument("--target-point-key", default="", help="Optional living_world_task_point key to attach to the debug travel task/step")
    parser.add_argument("--waypoint-keys", default="", help="Optional comma-separated living_world_task_point keys for a sequential in-zone gauntlet")
    parser.add_argument("--waypoint-count", type=int, default=0, help="Optional per-bot cap for shuffled waypoint keys; 0 uses the full list")
    parser.add_argument("--shuffle-seed", type=int, default=0, help="Batch seed for per-bot waypoint shuffling")
    parser.add_argument("--arrival-threshold-yards", type=float, default=3.0, help="Harness-only arrival threshold used for proving local links")
    parser.add_argument("--persist-links", action="store_true", help="Persist local link outcomes into living_world_task_point_link")
    parser.add_argument("--mode", choices=("route", "path_scout"), default="route")
    parser.add_argument("--spacing-yards", type=float, default=3.0)
    parser.add_argument("--transit-route-key", default="", help="Optional authored transit route key for a Travel -> Transit -> Hold harness run")
    parser.add_argument("--interest-zone-id", type=int, default=0, help="Optional synthetic-interest zone override; defaults to destination zone")
    parser.add_argument("--interest-switch-map-id", type=int, default=0, help="Optional synthetic-interest switch map id")
    parser.add_argument("--interest-switch-zone-id", type=int, default=0, help="Optional synthetic-interest switch zone id")
    parser.add_argument("--interest-switch-ms", type=int, default=0, help="Optional synthetic-interest switch delay in milliseconds")
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
        "world_db": parser.get("database", "database", fallback="acore_world"),
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


def file_contains_any(path: pathlib.Path, patterns: list[str]) -> bool:
    if not path.exists():
        return False

    try:
        text = path.read_text(encoding="utf-8", errors="ignore")
    except OSError:
        return False

    return any(pattern in text for pattern in patterns)


def read_tail(path: pathlib.Path, max_chars: int = 4000) -> str:
    if not path.exists():
        return ""

    try:
        text = path.read_text(encoding="utf-8", errors="ignore")
    except OSError:
        return ""

    if len(text) <= max_chars:
        return text
    return text[-max_chars:]


def wait_for_server_ready(
    process: subprocess.Popen[str],
    stdout_path: pathlib.Path,
    stderr_path: pathlib.Path,
    settings: dict[str, str | int],
    names: list[str],
    timeout_seconds: int,
) -> None:
    deadline = time.time() + timeout_seconds
    ready_markers = [
        "WORLD: World Initialized",
        "AzerothCore 3.3.5a is ready.",
    ]
    name_list = ",".join(f"'{name}'" for name in names)

    while time.time() < deadline:
        if file_contains_any(stdout_path, ready_markers):
            return

        rows = run_mysql_query(
            settings,
            str(settings["characters_db"]),
            "SELECT bot_name, COUNT(*) "
            "FROM living_world_bot_activity_log "
            f"WHERE bot_name IN ({name_list}) "
            "GROUP BY bot_name",
        )
        found = {row[0] for row in rows if int(row[1]) > 0}
        if found:
            return

        if process.poll() is not None:
            raise RuntimeError(
                f"worldserver exited before readiness markers appeared (exit={process.returncode}).\n"
                f"stdout tail:\n{read_tail(stdout_path)}\n"
                f"stderr tail:\n{read_tail(stderr_path)}"
            )

        time.sleep(1.0)

    raise RuntimeError(
        "worldserver did not reach ready state before timeout.\n"
        f"stdout tail:\n{read_tail(stdout_path)}\n"
        f"stderr tail:\n{read_tail(stderr_path)}"
    )


def wait_for_any_activity_rows(
    settings: dict[str, str | int],
    names: list[str],
    timeout_seconds: int,
) -> bool:
    deadline = time.time() + timeout_seconds
    name_list = ",".join(f"'{name}'" for name in names)
    while time.time() < deadline:
        rows = run_mysql_query(
            settings,
            str(settings["characters_db"]),
            "SELECT bot_name, COUNT(*) "
            "FROM living_world_bot_activity_log "
            f"WHERE bot_name IN ({name_list}) "
            "GROUP BY bot_name",
        )
        found = {row[0] for row in rows if int(row[1]) > 0}
        if all(name in found for name in names):
            return True
        time.sleep(1.0)
    return False


def wait_for_progress_rows(
    settings: dict[str, str | int],
    names: list[str],
    timeout_seconds: int,
) -> bool:
    deadline = time.time() + timeout_seconds
    name_list = ",".join(f"'{name}'" for name in names)
    interesting_events = [
        "travel_option",
        "travel_plan",
        "travel_start",
        "travel_waypoint",
        "travel_taxi_start",
        "travel_taxi_board",
        "travel_taxi_arrive",
        "travel_transit_wait",
        "travel_transit_board",
        "travel_transit_arrive",
        "travel_arrive",
        "travel_scout_rejected",
        "activity_complete",
        "session_complete",
        "travel_stuck",
        "travel_timeout",
        "session_abort",
    ]
    event_list = ",".join(f"'{event_name}'" for event_name in interesting_events)
    while time.time() < deadline:
        rows = run_mysql_query(
            settings,
            str(settings["characters_db"]),
            "SELECT bot_name, COUNT(*) "
            "FROM living_world_bot_activity_log "
            f"WHERE bot_name IN ({name_list}) "
            f"AND event_type IN ({event_list}) "
            "GROUP BY bot_name",
        )
        found = {row[0] for row in rows if int(row[1]) > 0}
        if all(name in found for name in names):
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


def fetch_identity_rows(
    settings: dict[str, str | int],
    names: list[str],
) -> list[tuple[str, ...]]:
    name_list = ",".join(f"'{name}'" for name in names)
    return run_mysql_query(
        settings,
        str(settings["characters_db"]),
        "SELECT name, level, is_available, session_count, active_world_session_ms, "
        "last_seen_zone, "
        "COALESCE(NULLIF(runtime_state, ''), '-') AS runtime_state, "
        "COALESCE(NULLIF(runtime_detail, ''), '-') AS runtime_detail "
        "FROM living_world_bot_identity "
        f"WHERE name IN ({name_list}) "
        "ORDER BY name ASC",
    )


def clear_prior_rows(settings: dict[str, str | int], names: list[str]) -> None:
    name_list = ",".join(f"'{name}'" for name in names)
    run_mysql_query(
        settings,
        str(settings["characters_db"]),
        "DELETE FROM living_world_bot_activity_log "
        f"WHERE bot_name IN ({name_list})",
    )


def sql_quote(value: str) -> str:
    return "'" + value.replace("\\", "\\\\").replace("'", "''") + "'"


def parse_waypoint_orders(stdout_path: pathlib.Path) -> dict[str, list[str]]:
    if not stdout_path.exists():
        return {}

    pattern = re.compile(r"RouteHarness spawned '([^']+)'.*waypoint_keys='([^']*)'")
    orders: dict[str, list[str]] = {}
    for line in stdout_path.read_text(encoding="utf-8", errors="ignore").splitlines():
        match = pattern.search(line)
        if not match:
            continue
        bot_name = match.group(1)
        waypoint_keys = [token.strip() for token in match.group(2).split(",") if token.strip()]
        orders[bot_name] = waypoint_keys
    return orders


def persist_local_link_rows(
    settings: dict[str, str | int],
    names: list[str],
    stdout_path: pathlib.Path,
    rows: list[tuple[str, ...]],
) -> int:
    orders = parse_waypoint_orders(stdout_path)
    outcomes_by_bot: dict[str, list[str]] = {name: [] for name in names}
    for row in rows:
        bot_name, event_type = row[0], row[1]
        if bot_name not in outcomes_by_bot:
            continue
        if event_type == "travel_arrive":
            outcomes_by_bot[bot_name].append("ok")
        elif event_type == "travel_no_path":
            outcomes_by_bot[bot_name].append("fail")

    statements: list[str] = []
    persisted = 0
    for bot_name in names:
        waypoint_keys = orders.get(bot_name, [])
        outcomes = outcomes_by_bot.get(bot_name, [])
        current_anchor: str | None = None
        for waypoint_key, outcome in zip(waypoint_keys, outcomes):
            if current_anchor:
                success_delta = 1 if outcome == "ok" else 0
                failure_delta = 1 if outcome == "fail" else 0
                statements.append(
                    "INSERT INTO living_world_task_point_link "
                    "(from_point_key, to_point_key, link_kind, manual_verified, success_count, failure_count, "
                    "first_seen_at, last_seen_at, last_success_at, last_failure_at, source, notes) "
                    f"VALUES ({sql_quote(current_anchor)}, {sql_quote(waypoint_key)}, 'local_nav', 0, "
                    f"{success_delta}, {failure_delta}, NOW(), NOW(), "
                    f"{'NOW()' if success_delta else 'NULL'}, {'NOW()' if failure_delta else 'NULL'}, "
                    "'debug_route_harness', '') "
                    "ON DUPLICATE KEY UPDATE "
                    f"success_count = success_count + {success_delta}, "
                    f"failure_count = failure_count + {failure_delta}, "
                    "last_seen_at = NOW(), "
                    f"last_success_at = {'NOW()' if success_delta else 'last_success_at'}, "
                    f"last_failure_at = {'NOW()' if failure_delta else 'last_failure_at'}, "
                    "source = 'debug_route_harness'"
                )
                persisted += 1

            if outcome == "ok":
                current_anchor = waypoint_key

    if not statements:
        return 0

    sql_path = pathlib.Path(tempfile.gettempdir()) / "lw_task_point_link_persist.sql"
    sql_path.write_text(";\n".join(statements) + ";\n", encoding="utf-8")
    try:
        command = [
            "mysql",
            "-h", str(settings["host"]),
            "-P", str(settings["port"]),
            "-u", str(settings["user"]),
            f"-p{settings['password']}",
            str(settings["world_db"]),
            "--execute",
            f"source {sql_path.as_posix()}",
        ]
        result = subprocess.run(command, capture_output=True, text=True)
        if result.returncode != 0:
            raise RuntimeError(
                f"mysql query failed for {settings['world_db']}:\n{result.stdout}{result.stderr}"
            )
    finally:
        try:
            sql_path.unlink()
        except OSError:
            pass
    return persisted


def build_report(
    identity_rows: list[tuple[str, ...]],
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

    if identity_rows:
        lines.append("[identities]")
        for row in identity_rows:
            runtime_state = row[6] if len(row) > 6 else "-"
            runtime_detail = row[7] if len(row) > 7 else "-"
            lines.append(
                "  "
                f"name={row[0]} level={row[1]} is_available={row[2]} "
                f"session_count={row[3]} active_world_session_ms={row[4]} last_seen_zone={row[5]}"
                f" runtime_state={runtime_state} runtime_detail={runtime_detail}"
            )
        lines.append("")

    for bot_name, bot_rows in grouped.items():
        lines.append(f"[{bot_name}]")
        for event_type in (
            "session_start",
            "session_blueprint",
            "build_prepared",
            "travel_option",
            "travel_start",
            "travel_plan",
            "travel_waypoint",
            "travel_taxi_start",
            "travel_taxi_board",
            "travel_taxi_arrive",
            "travel_taxi_resume_ground",
            "travel_transit_wait",
            "travel_transit_board",
            "travel_transit_timeout",
            "travel_transit_arrive",
            "travel_arrive",
            "travel_scout_rejected",
            "travel_timeout",
            "travel_stuck",
            "travel_skip",
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
        "LivingWorld.AmbientForceSpawnCount": "0",
        "LivingWorld.DebugSyntheticInterestEnabled": "1",
        "LivingWorld.DebugSyntheticInterestMapId": str(args.map_id),
        "LivingWorld.DebugSyntheticInterestZoneId": str(args.interest_zone_id or args.dest_zone_id),
        "LivingWorld.DebugSyntheticInterestSwitchMapId": str(args.interest_switch_map_id),
        "LivingWorld.DebugSyntheticInterestSwitchZoneId": str(args.interest_switch_zone_id),
        "LivingWorld.DebugSyntheticInterestSwitchMs": str(args.interest_switch_ms),
        "LivingWorld.DebugSyntheticInterestClearMs": "0",
        "LivingWorld.DebugHotZoneCooldownMs": "0",
        "LivingWorld.DebugForceIdentityIds": "\"\"",
        "LivingWorld.DebugForceSessionZoneId": "0",
        "LivingWorld.DebugForceSessionComposeAttempts": "24",
        "LivingWorld.DebugCombatManaDrainIdentityId": "0",
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
        "LivingWorld.DebugRouteHarnessTargetPointKey": f"\"{args.target_point_key}\"",
        "LivingWorld.DebugRouteHarnessWaypointKeys": f"\"{args.waypoint_keys}\"",
        "LivingWorld.DebugRouteHarnessWaypointCount": str(args.waypoint_count),
        "LivingWorld.DebugRouteHarnessShuffleSeed": str(args.shuffle_seed),
        "LivingWorld.DebugRouteHarnessArrivalThresholdYards": str(args.arrival_threshold_yards),
        "LivingWorld.DebugRouteHarnessMode": f"\"{args.mode}\"",
        "LivingWorld.DebugRouteHarnessSpacingYards": str(args.spacing_yards),
        "LivingWorld.DebugRouteHarnessTransitRouteKey": f"\"{args.transit_route_key}\"",
        "LivingWorld.DebugRouteHarnessExploredZones": f"\"{args.explored_zone_ids}\"",
        "LivingWorld.DebugRouteHarnessBakeRouteZ": "1" if args.bake_zone_ids else "0",
        "LivingWorld.DebugRouteHarnessBakeZoneIds": f"\"{args.bake_zone_ids}\"",
        "LivingWorld.DebugRouteHarnessIdleDurationSec": str(args.idle_seconds),
        "LivingWorld.RouteExportDir": f"\"{(REPO_ROOT / 'tools' / 'lw-zone-editor' / 'data' / 'exported_routes').as_posix()}\"",
        "LivingWorld.RouteExportBakeZOnStartup": "0",
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

        wait_for_server_ready(
            process,
            stdout_path,
            stderr_path,
            settings,
            names,
            args.startup_timeout,
        )

        any_activity = wait_for_any_activity_rows(settings, names, max(30, args.startup_timeout // 2))
        if not any_activity:
            identity_rows = fetch_identity_rows(settings, names)
            rows = fetch_activity_rows(settings, names)
            build_report(identity_rows, rows, stdout_path, stderr_path, report_path)
            raise RuntimeError(
                "Harness bots did not emit any activity rows before timeout. "
                f"Report: {report_path}"
            )

        progressed = wait_for_progress_rows(settings, names, max(45, args.startup_timeout // 2))
        if not progressed:
            identity_rows = fetch_identity_rows(settings, names)
            rows = fetch_activity_rows(settings, names)
            build_report(identity_rows, rows, stdout_path, stderr_path, report_path)
            raise RuntimeError(
                "Harness bots spawned but did not reach travel/taxi/session progress rows before timeout. "
                f"Report: {report_path}"
            )

        time.sleep(args.seconds)
        identity_rows = fetch_identity_rows(settings, names)
        rows = fetch_activity_rows(settings, names)
        persisted_links = 0
        if args.persist_links:
            persisted_links = persist_local_link_rows(settings, names, stdout_path, rows)
        build_report(identity_rows, rows, stdout_path, stderr_path, report_path)
        if args.persist_links:
            print(f"Persisted local link rows: {persisted_links}")
        print(report_path)
        return 0
    finally:
        cleanup()


if __name__ == "__main__":
    raise SystemExit(main())
