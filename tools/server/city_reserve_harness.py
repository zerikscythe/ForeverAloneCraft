#!/usr/bin/env python3
"""Run a focused city-reserve scheduler harness.

This harness:
- ensures a reserve pool exists for a target city
- rewrites mod-living-world.conf with a temporary synthetic-interest city setup
- boots worldserver headlessly
- waits for the ambient scheduler to populate the city
- writes a compact report showing how many reserve bots activated and what they did
"""

from __future__ import annotations

import argparse
import atexit
import configparser
import csv
import datetime as dt
import io
import pathlib
import subprocess
import sys
import time


REPO_ROOT = pathlib.Path(__file__).resolve().parents[2]
BUILD_ROOT = REPO_ROOT / "out" / "build-vs2022" / "bin" / "Debug"
WORLD_EXE = BUILD_ROOT / "worldserver.exe"
MODULE_CONF = BUILD_ROOT / "configs" / "modules" / "mod-living-world.conf"
MODULE_CONF_BACKUP = BUILD_ROOT / "configs" / "modules" / "mod-living-world.conf.city-harness.bak"
DBCONFIG = REPO_ROOT / "tools" / "lw-zone-editor" / "config.ini"
SEEDER = REPO_ROOT / "tools" / "seed_bot_identities.py"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--city-zone-id", type=int, default=1519, help="City zone id to heat (default: Stormwind 1519)")
    parser.add_argument("--city-map-id", type=int, default=0, help="City map id (default: Eastern Kingdoms/Azeroth map 0)")
    parser.add_argument("--faction", type=int, default=1, help="Reserve faction to test (1=Alliance, 2=Horde)")
    parser.add_argument("--reserve-count", type=int, default=100, help="Minimum reserve pool size to ensure")
    parser.add_argument("--active-population", type=int, default=30, help="Ambient population target during the test")
    parser.add_argument("--runtime-seconds", type=int, default=90, help="How long to leave the world running after readiness")
    parser.add_argument("--startup-timeout", type=int, default=180, help="How long to wait for worldserver readiness")
    parser.add_argument("--population-tick-ms", type=int, default=5000, help="Ambient population tick for the harness")
    parser.add_argument("--home-anchor-point-key", default="stormwind_inn", help="Reserve bot home anchor key")
    parser.add_argument("--home-bind-point-key", default="stormwind_inn", help="Reserve bot bind anchor key")
    parser.add_argument("--report-prefix", default="city-reserve-harness", help="Prefix for report/stdout/stderr files")
    return parser.parse_args()


def ensure_paths() -> None:
    if not WORLD_EXE.exists():
        raise FileNotFoundError(f"worldserver.exe not found: {WORLD_EXE}")
    if not MODULE_CONF.exists():
        raise FileNotFoundError(f"Module config not found: {MODULE_CONF}")
    if not SEEDER.exists():
        raise FileNotFoundError(f"Seeder not found: {SEEDER}")


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


def run_mysql_query(settings: dict[str, str | int], database: str, sql: str) -> list[tuple[str, ...]]:
    command = mysql_base_command(settings, database) + ["-e", sql]
    result = subprocess.run(command, capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError(f"mysql query failed for {database}:\n{result.stdout}{result.stderr}")

    output = result.stdout.strip()
    if not output:
        return []

    reader = csv.reader(io.StringIO(output), delimiter="\t")
    return [tuple(row) for row in reader]


def run_mysql_exec(settings: dict[str, str | int], database: str, sql: str) -> None:
    command = mysql_base_command(settings, database) + ["-e", sql]
    result = subprocess.run(command, capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError(f"mysql exec failed for {database}:\n{result.stdout}{result.stderr}")


def query_scalar(settings: dict[str, str | int], sql: str) -> int:
    rows = run_mysql_query(settings, str(settings["characters_db"]), sql)
    return int(rows[0][0]) if rows else 0


def ensure_character_schema(settings: dict[str, str | int]) -> None:
    database = str(settings["characters_db"])

    def has_column(column_name: str) -> bool:
        rows = run_mysql_query(
            settings,
            database,
            f"SHOW COLUMNS FROM living_world_bot_identity LIKE '{column_name}'",
        )
        return bool(rows)

    def has_index(index_name: str) -> bool:
        rows = run_mysql_query(
            settings,
            database,
            f"SHOW INDEX FROM living_world_bot_identity WHERE Key_name = '{index_name}'",
        )
        return bool(rows)

    if not has_column("population_role"):
        run_mysql_exec(
            settings,
            database,
            "ALTER TABLE living_world_bot_identity "
            "ADD COLUMN population_role VARCHAR(32) NOT NULL DEFAULT 'world' AFTER has_fishing",
        )
    if not has_column("reserve_city_zone_id"):
        run_mysql_exec(
            settings,
            database,
            "ALTER TABLE living_world_bot_identity "
            "ADD COLUMN reserve_city_zone_id INT UNSIGNED NULL AFTER population_role",
        )
    if not has_column("runtime_state"):
        run_mysql_exec(
            settings,
            database,
            "ALTER TABLE living_world_bot_identity "
            "ADD COLUMN runtime_state VARCHAR(64) NOT NULL DEFAULT '' AFTER active_world_session_ms",
        )
    if not has_column("runtime_detail"):
        run_mysql_exec(
            settings,
            database,
            "ALTER TABLE living_world_bot_identity "
            "ADD COLUMN runtime_detail VARCHAR(255) NOT NULL DEFAULT '' AFTER runtime_state",
        )
    if not has_index("idx_population_role"):
        run_mysql_exec(
            settings,
            database,
            "ALTER TABLE living_world_bot_identity "
            "ADD INDEX idx_population_role (population_role, reserve_city_zone_id, faction, is_available, is_retired)",
        )


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


def read_tail(path: pathlib.Path, max_chars: int = 5000) -> str:
    if not path.exists():
        return ""
    text = path.read_text(encoding="utf-8", errors="ignore")
    if len(text) <= max_chars:
        return text
    return text[-max_chars:]


def wait_for_server_ready(process: subprocess.Popen[str], stdout_path: pathlib.Path, stderr_path: pathlib.Path, timeout_seconds: int) -> None:
    deadline = time.time() + timeout_seconds
    markers = ["WORLD: World Initialized", "AzerothCore 3.3.5a is ready."]
    while time.time() < deadline:
        if stdout_path.exists():
            text = stdout_path.read_text(encoding="utf-8", errors="ignore")
            if any(marker in text for marker in markers):
                return
        if process.poll() is not None:
            raise RuntimeError(
                f"worldserver exited before ready state (exit={process.returncode}).\n"
                f"stdout tail:\n{read_tail(stdout_path)}\n"
                f"stderr tail:\n{read_tail(stderr_path)}"
            )
        time.sleep(1.0)
    raise RuntimeError(
        "worldserver did not reach ready state before timeout.\n"
        f"stdout tail:\n{read_tail(stdout_path)}\n"
        f"stderr tail:\n{read_tail(stderr_path)}"
    )


def wait_for_reserve_activation(settings: dict[str, str | int], args: argparse.Namespace, timeout_seconds: int) -> int:
    deadline = time.time() + timeout_seconds
    while time.time() < deadline:
        active = query_scalar(
            settings,
            "SELECT COUNT(*) FROM living_world_bot_identity "
            f"WHERE population_role = 'city_reserve' AND reserve_city_zone_id = {args.city_zone_id} "
            f"AND faction = {args.faction} AND is_available = 0"
        )
        if active > 0:
            return active
        time.sleep(1.0)
    return 0


def ensure_reserve_pool(settings: dict[str, str | int], args: argparse.Namespace) -> tuple[int, int]:
    current = query_scalar(
        settings,
        "SELECT COUNT(*) FROM living_world_bot_identity "
        f"WHERE population_role = 'city_reserve' AND reserve_city_zone_id = {args.city_zone_id} "
        f"AND faction = {args.faction}"
    )
    if current >= args.reserve_count:
        return current, 0

    inserted_total = 0
    for attempt in range(5):
        needed = args.reserve_count - current
        if needed <= 0:
            break

        seed_value = int(time.time()) + attempt
        command = [
            sys.executable,
            str(SEEDER),
            "--alliance-count", str(needed if args.faction == 1 else 0),
            "--horde-count", str(needed if args.faction == 2 else 0),
            "--seed", str(seed_value),
            "--population-role", "city_reserve",
            "--reserve-city-zone-id", str(args.city_zone_id),
            "--home-zone-id", str(args.city_zone_id),
            "--home-anchor-point-key", args.home_anchor_point_key,
            "--home-bind-point-key", args.home_bind_point_key,
        ]
        result = subprocess.run(command, cwd=str(REPO_ROOT), capture_output=True, text=True)
        if result.returncode != 0:
            raise RuntimeError(f"Reserve seeding failed:\n{result.stdout}\n{result.stderr}")

        updated = query_scalar(
            settings,
            "SELECT COUNT(*) FROM living_world_bot_identity "
            f"WHERE population_role = 'city_reserve' AND reserve_city_zone_id = {args.city_zone_id} "
            f"AND faction = {args.faction}"
        )
        inserted_total += max(0, updated - current)
        current = updated

    return current, inserted_total


def reset_reserve_pool_availability(settings: dict[str, str | int], args: argparse.Namespace) -> None:
    run_mysql_exec(
        settings,
        str(settings["characters_db"]),
        "UPDATE living_world_bot_identity "
        "SET is_available = 1, "
        "active_world_session_ms = 0, "
        "active_world_session_start = NULL, "
        "runtime_state = '', "
        "runtime_detail = '' "
        f"WHERE population_role = 'city_reserve' AND reserve_city_zone_id = {args.city_zone_id} "
        f"AND faction = {args.faction}",
    )


def fetch_report_rows(settings: dict[str, str | int], args: argparse.Namespace, log_start_id: int) -> dict[str, list[tuple[str, ...]]]:
    database = str(settings["characters_db"])
    reserve_filter = (
        "i.population_role = 'city_reserve' "
        f"AND i.reserve_city_zone_id = {args.city_zone_id} AND i.faction = {args.faction}"
    )

    active_rows = run_mysql_query(
        settings,
        database,
        "SELECT i.name, i.level, i.runtime_state, i.runtime_detail, i.last_seen_zone, "
        "i.active_world_session_ms, i.session_count "
        "FROM living_world_bot_identity i "
        f"WHERE {reserve_filter} AND i.is_available = 0 "
        "ORDER BY i.level DESC, i.name ASC LIMIT 50"
    )
    state_rows = run_mysql_query(
        settings,
        database,
        "SELECT i.runtime_state, COUNT(*) "
        "FROM living_world_bot_identity i "
        f"WHERE {reserve_filter} AND i.is_available = 0 "
        "GROUP BY i.runtime_state ORDER BY COUNT(*) DESC, i.runtime_state ASC"
    )
    event_rows = run_mysql_query(
        settings,
        database,
        "SELECT a.event_type, COUNT(*) "
        "FROM living_world_bot_activity_log a "
        "JOIN living_world_bot_identity i ON i.id = a.bot_guid "
        f"WHERE {reserve_filter} AND a.id > {log_start_id} "
        "GROUP BY a.event_type ORDER BY COUNT(*) DESC, a.event_type ASC"
    )
    recent_rows = run_mysql_query(
        settings,
        database,
        "SELECT a.bot_name, a.event_type, a.zone_id, a.detail "
        "FROM living_world_bot_activity_log a "
        "JOIN living_world_bot_identity i ON i.id = a.bot_guid "
        f"WHERE {reserve_filter} AND a.id > {log_start_id} "
        "ORDER BY a.id DESC LIMIT 80"
    )
    return {
        "active_rows": active_rows,
        "state_rows": state_rows,
        "event_rows": event_rows,
        "recent_rows": recent_rows,
    }


def format_report(
    args: argparse.Namespace,
    reserve_total: int,
    seeded_count: int,
    active_count: int,
    rows: dict[str, list[tuple[str, ...]]],
    stdout_path: pathlib.Path,
    stderr_path: pathlib.Path,
) -> str:
    lines: list[str] = []
    lines.append(f"City reserve harness report @ {dt.datetime.now().isoformat(timespec='seconds')}")
    lines.append(f"city_zone_id={args.city_zone_id} city_map_id={args.city_map_id} faction={args.faction}")
    lines.append(f"reserve_target={args.reserve_count} reserve_total={reserve_total} newly_seeded={seeded_count}")
    lines.append(f"ambient_population_target={args.active_population} active_reserve_count={active_count}")
    lines.append("")
    lines.append("[runtime_state counts]")
    if rows["state_rows"]:
        for state, count in rows["state_rows"]:
            label = state if state else "<empty>"
            lines.append(f"  {label}: {count}")
    else:
        lines.append("  <none>")

    lines.append("")
    lines.append("[event counts]")
    if rows["event_rows"]:
        for event_type, count in rows["event_rows"]:
            lines.append(f"  {event_type}: {count}")
    else:
        lines.append("  <none>")

    lines.append("")
    lines.append("[active reserve bots]")
    if rows["active_rows"]:
        for row in rows["active_rows"]:
            name, level, runtime_state, runtime_detail, last_seen_zone, session_ms, session_count = row
            lines.append(
                f"  {name:<18} lvl={level:<2} state={runtime_state or '<empty>'} "
                f"sessions={session_count} ms={session_ms} zone={last_seen_zone or 'null'}"
            )
            if runtime_detail:
                lines.append(f"    detail={runtime_detail}")
    else:
        lines.append("  <none>")

    lines.append("")
    lines.append("[recent activity]")
    if rows["recent_rows"]:
        for bot_name, event_type, zone_id, detail in rows["recent_rows"]:
            lines.append(f"  {bot_name:<18} {event_type:<24} zone={zone_id or 'null'} {detail}")
    else:
        lines.append("  <none>")

    lines.append("")
    lines.append(f"stdout_log={stdout_path.name}")
    lines.append(f"stderr_log={stderr_path.name}")
    return "\n".join(lines) + "\n"


def main() -> int:
    args = parse_args()
    ensure_paths()
    settings = load_db_settings()
    ensure_character_schema(settings)

    reserve_total, seeded_count = ensure_reserve_pool(settings, args)
    reset_reserve_pool_availability(settings, args)
    log_start_id = query_scalar(settings, "SELECT COALESCE(MAX(id), 0) FROM living_world_bot_activity_log")

    original_conf = MODULE_CONF.read_text(encoding="utf-8")
    MODULE_CONF_BACKUP.write_text(original_conf, encoding="utf-8")
    atexit.register(lambda: MODULE_CONF.write_text(original_conf, encoding="utf-8"))

    config_updates = {
        "LivingWorld.AmbientPopulation": str(args.active_population),
        "LivingWorld.AmbientPopulationTickMs": str(args.population_tick_ms),
        "LivingWorld.AmbientForceSpawnCount": "0",
        "LivingWorld.AmbientForceSpawnMapId": "0",
        "LivingWorld.AmbientForceSpawnX": "0.0",
        "LivingWorld.AmbientForceSpawnY": "0.0",
        "LivingWorld.AmbientForceSpawnZ": "0.0",
        "LivingWorld.DebugSyntheticInterestEnabled": "1",
        "LivingWorld.DebugSyntheticInterestMapId": str(args.city_map_id),
        "LivingWorld.DebugSyntheticInterestZoneId": str(args.city_zone_id),
        "LivingWorld.DebugSyntheticInterestSwitchMapId": "0",
        "LivingWorld.DebugSyntheticInterestSwitchZoneId": "0",
        "LivingWorld.DebugSyntheticInterestSwitchMs": "0",
        "LivingWorld.DebugSyntheticInterestClearMs": "0",
        "LivingWorld.DebugForceIdentityIds": "\"\"",
        "LivingWorld.DebugForceSessionZoneId": "0",
        "LivingWorld.DebugForceSessionComposeAttempts": "16",
        "LivingWorld.DebugCombatManaDrainIdentityId": "0",
        "LivingWorld.DebugCombatManaDrainTargetManaPct": "60",
        "LivingWorld.DebugCombatManaDrainIntervalMs": "1500",
    }
    MODULE_CONF.write_text(rewrite_module_config(original_conf, config_updates), encoding="utf-8")

    stamp = dt.datetime.now().strftime("%Y%m%d-%H%M%S")
    stdout_path = REPO_ROOT / f"{args.report_prefix}-{stamp}.stdout.log"
    stderr_path = REPO_ROOT / f"{args.report_prefix}-{stamp}.stderr.log"
    report_path = REPO_ROOT / f"{args.report_prefix}-{stamp}.report.txt"

    process = None
    try:
        with stdout_path.open("w", encoding="utf-8") as stdout_handle, stderr_path.open("w", encoding="utf-8") as stderr_handle:
            process = subprocess.Popen(
                [str(WORLD_EXE), "--config", "configs/worldserver.conf"],
                cwd=str(BUILD_ROOT),
                stdout=stdout_handle,
                stderr=stderr_handle,
                text=True,
            )
            wait_for_server_ready(process, stdout_path, stderr_path, args.startup_timeout)
            active_count = wait_for_reserve_activation(settings, args, max(30, args.startup_timeout // 2))
            time.sleep(args.runtime_seconds)
    finally:
        if process is not None and process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=20)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=20)

    final_active_count = query_scalar(
        settings,
        "SELECT COUNT(*) FROM living_world_bot_identity "
        f"WHERE population_role = 'city_reserve' AND reserve_city_zone_id = {args.city_zone_id} "
        f"AND faction = {args.faction} AND is_available = 0"
    )
    reserve_total = query_scalar(
        settings,
        "SELECT COUNT(*) FROM living_world_bot_identity "
        f"WHERE population_role = 'city_reserve' AND reserve_city_zone_id = {args.city_zone_id} "
        f"AND faction = {args.faction}"
    )
    report_rows = fetch_report_rows(settings, args, log_start_id)
    report = format_report(args, reserve_total, seeded_count, final_active_count, report_rows, stdout_path, stderr_path)
    report_path.write_text(report, encoding="utf-8")
    print(report_path)
    print(report)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
