#!/usr/bin/env python3
"""Spawn a level-80 Paladin world bot in Stormwind and force it onto a training dummy.

This is a cheap combat-validation harness for doctrine work:
- ensure one chosen ledger identity is a Paladin spec at the requested level
- force-spawn that identity near the Stormwind Cathedral training dummies
- enable a small debug AI hook that acquires the nearest training dummy by entry
- collect activity/combat trace rows so we can inspect spell choices quickly
"""

from __future__ import annotations

import argparse
import configparser
import csv
import datetime as dt
import io
import pathlib
import shutil
import subprocess
import time


REPO_ROOT = pathlib.Path(__file__).resolve().parents[2]
BUILD_ROOT = REPO_ROOT / "out" / "build-vs2022" / "bin" / "Debug"
WORLD_EXE = BUILD_ROOT / "worldserver.exe"
MODULE_CONF = BUILD_ROOT / "configs" / "modules" / "mod-living-world.conf"
DBCONFIG = REPO_ROOT / "tools" / "lw-editor" / "config.ini"
MYSQL_EXE = pathlib.Path(shutil.which("mysql") or r"D:\mysql-8.0.46-winx64\bin\mysql.exe")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--name", default="PallyDummy80", help="Ledger identity name to create/update")
    parser.add_argument("--spec", default="paladin_ret", choices=["paladin_ret", "paladin_prot", "paladin_holy"])
    parser.add_argument("--level", type=int, default=80)
    parser.add_argument("--seconds", type=int, default=75)
    parser.add_argument("--startup-timeout", type=int, default=120)
    parser.add_argument("--zone-id", type=int, default=1519)
    parser.add_argument("--map-id", type=int, default=0)
    parser.add_argument("--spawn-x", type=float, default=-8772.5)
    parser.add_argument("--spawn-y", type=float, default=347.0)
    parser.add_argument("--spawn-z", type=float, default=101.1)
    parser.add_argument("--dummy-entry", type=int, default=31144, help="Grandmaster's Training Dummy by default")
    parser.add_argument("--dummy-radius", type=float, default=30.0)
    parser.add_argument("--force-spawn-count", type=int, default=1)
    return parser.parse_args()


def ensure_paths() -> None:
    if not WORLD_EXE.exists():
        raise FileNotFoundError(f"worldserver.exe not found: {WORLD_EXE}")
    if not MODULE_CONF.exists():
        raise FileNotFoundError(f"Module config not found: {MODULE_CONF}")
    if not MYSQL_EXE.exists():
        raise FileNotFoundError(f"mysql.exe not found: {MYSQL_EXE}")


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
        str(MYSQL_EXE),
        "-h", str(settings["host"]),
        "-P", str(settings["port"]),
        "-u", str(settings["user"]),
        f"-p{settings['password']}",
        "--batch",
        "--raw",
        "--skip-column-names",
        "-D", database,
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


def query_scalar(settings: dict[str, str | int], database: str, sql: str) -> int:
    rows = run_mysql_query(settings, database, sql)
    if not rows or not rows[0]:
        return 0
    return int(rows[0][0])


def sql_quote(value: object) -> str:
    if value is None:
        return "NULL"
    if isinstance(value, bool):
        return "1" if value else "0"
    if isinstance(value, (int, float)):
        return str(value)
    text = str(value)
    return "'" + text.replace("\\", "\\\\").replace("'", "\\'") + "'"


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


def ensure_identity(settings: dict[str, str | int], args: argparse.Namespace) -> int:
    rows = run_mysql_query(
        settings,
        str(settings["characters_db"]),
        f"SELECT id FROM living_world_bot_identity WHERE name = {sql_quote(args.name)} LIMIT 1",
    )

    if rows:
        identity_id = int(rows[0][0])
        run_mysql_query(
            settings,
            str(settings["characters_db"]),
            (
                "UPDATE living_world_bot_identity SET "
                f"race_id=1, class_id=2, spec_key={sql_quote(args.spec)}, loadout_key='', "
                f"faction=1, display_id=49, gender=0, level={args.level}, gear_tier=3, "
                "population_role='world', reserve_city_zone_id=NULL, home_zone_id=1519, "
                "home_anchor_point_key='stormwind_inn', home_bind_point_key='stormwind_inn', "
                "is_available=1, session_count=0, total_world_online_ms=0, "
                "world_online_ms_since_level=0, post_max_world_online_ms=0, active_world_session_ms=0, "
                "runtime_state='', runtime_detail='', last_session_source_kind='', last_session_source_key='', "
                "last_task_family='', last_task_target_zone=NULL, gear_refresh_pending=1, last_gear_refresh_band=0, "
                "active_world_session_start=NULL, is_retired=0, successor_spawned=0, retired_at=NULL, "
                "last_seen_zone=1519, last_seen_at=NULL "
                f"WHERE id={identity_id}"
            ),
        )
        return identity_id

    run_mysql_query(
        settings,
        str(settings["characters_db"]),
        (
            "INSERT INTO living_world_bot_identity "
            "(name, race_id, class_id, spec_key, loadout_key, faction, display_id, gender, level, gear_tier, "
            "population_role, home_zone_id, home_anchor_point_key, home_bind_point_key, is_available, is_retired, last_seen_zone) VALUES "
            f"({sql_quote(args.name)}, 1, 2, {sql_quote(args.spec)}, '', 1, 49, 0, {args.level}, 3, "
            "'world', 1519, 'stormwind_inn', 'stormwind_inn', 1, 0, 1519)"
        ),
    )
    rows = run_mysql_query(
        settings,
        str(settings["characters_db"]),
        f"SELECT id FROM living_world_bot_identity WHERE name = {sql_quote(args.name)} LIMIT 1",
    )
    if not rows:
        raise RuntimeError("Failed to create harness identity")
    return int(rows[0][0])


def wait_for_identity_activity(
    settings: dict[str, str | int],
    baseline_id: int,
    identity_id: int,
    timeout_seconds: int,
) -> None:
    deadline = time.time() + timeout_seconds
    sql = (
        "SELECT COUNT(*) FROM living_world_bot_activity_log "
        f"WHERE id > {baseline_id} AND bot_guid = {identity_id}"
    )
    while time.time() < deadline:
        if query_scalar(settings, str(settings["characters_db"]), sql) > 0:
            return
        time.sleep(1)
    raise TimeoutError("Timed out waiting for harness bot activity rows")


def stop_worldserver(process: subprocess.Popen[bytes]) -> None:
    if process.poll() is not None:
        return
    process.terminate()
    try:
        process.wait(timeout=20)
        return
    except subprocess.TimeoutExpired:
        pass
    process.kill()
    process.wait(timeout=15)


def build_report(
    settings: dict[str, str | int],
    baseline_id: int,
    identity_id: int,
    name: str,
    stdout_path: pathlib.Path,
    stderr_path: pathlib.Path,
    report_path: pathlib.Path,
) -> None:
    rows = run_mysql_query(
        settings,
        str(settings["characters_db"]),
        (
            "SELECT event_type, COUNT(*) FROM living_world_bot_activity_log "
            f"WHERE id > {baseline_id} AND bot_guid = {identity_id} "
            "GROUP BY event_type ORDER BY COUNT(*) DESC, event_type ASC"
        ),
    )
    recent = run_mysql_query(
        settings,
        str(settings["characters_db"]),
        (
            "SELECT id, bot_name, event_type, zone_id, detail "
            "FROM living_world_bot_activity_log "
            f"WHERE id > {baseline_id} AND bot_guid = {identity_id} "
            "ORDER BY id DESC LIMIT 120"
        ),
    )
    identity_rows = run_mysql_query(
        settings,
        str(settings["characters_db"]),
        (
            "SELECT id, name, level, spec_key, runtime_state, runtime_detail, session_count, active_world_session_ms "
            "FROM living_world_bot_identity "
            f"WHERE id = {identity_id} LIMIT 1"
        ),
    )

    with report_path.open("w", encoding="utf-8") as handle:
        handle.write("=== CONFIG ===\n")
        handle.write(f"identity_id={identity_id}\n")
        handle.write(f"name={name}\n")
        handle.write(f"stdout_log={stdout_path.name}\n")
        handle.write(f"stderr_log={stderr_path.name}\n\n")

        handle.write("=== IDENTITY ===\n")
        for row in identity_rows:
            handle.write("\t".join(str(value) for value in row) + "\n")
        handle.write("\n")

        handle.write("=== EVENT_COUNTS ===\n")
        for row in rows:
            handle.write("\t".join(str(value) for value in row) + "\n")
        handle.write("\n")

        handle.write("=== RECENT_ROWS ===\n")
        for row in recent:
            handle.write("\t".join(str(value) for value in row) + "\n")


def main() -> int:
    args = parse_args()
    ensure_paths()
    settings = load_db_settings()
    identity_id = ensure_identity(settings, args)

    timestamp = dt.datetime.now().strftime("%Y%m%d-%H%M%S")
    prefix = REPO_ROOT / f"paladin-dummy-harness-{timestamp}"
    stdout_path = prefix.with_suffix(".stdout.log")
    stderr_path = prefix.with_suffix(".stderr.log")
    report_path = prefix.with_suffix(".report.txt")

    updates = {
        "LivingWorld.AmbientPopulation": str(args.force_spawn_count),
        "LivingWorld.AmbientPopulationTickMs": "10000",
        "LivingWorld.AmbientForceSpawnCount": str(args.force_spawn_count),
        "LivingWorld.AmbientForceSpawnMapId": str(args.map_id),
        "LivingWorld.AmbientForceSpawnX": str(args.spawn_x),
        "LivingWorld.AmbientForceSpawnY": str(args.spawn_y),
        "LivingWorld.AmbientForceSpawnZ": str(args.spawn_z),
        "LivingWorld.DebugSyntheticInterestEnabled": "1",
        "LivingWorld.DebugSyntheticInterestMapId": str(args.map_id),
        "LivingWorld.DebugSyntheticInterestZoneId": str(args.zone_id),
        "LivingWorld.DebugSyntheticInterestSwitchMapId": "0",
        "LivingWorld.DebugSyntheticInterestSwitchZoneId": "0",
        "LivingWorld.DebugSyntheticInterestSwitchMs": "0",
        "LivingWorld.DebugSyntheticInterestClearMs": "0",
        "LivingWorld.DebugHotZoneCooldownMs": "10000",
        "LivingWorld.DebugForceIdentityIds": f"\"{identity_id}\"",
        "LivingWorld.DebugForceSessionZoneId": str(args.zone_id),
        "LivingWorld.DebugForceSessionComposeAttempts": "64",
        "LivingWorld.DebugForceCombatTargetIdentityId": str(identity_id),
        "LivingWorld.DebugForceCombatTargetEntry": str(args.dummy_entry),
        "LivingWorld.DebugForceCombatTargetSearchRadius": str(args.dummy_radius),
    }

    baseline_id = query_scalar(
        settings,
        str(settings["characters_db"]),
        "SELECT COALESCE(MAX(id), 0) FROM living_world_bot_activity_log",
    )

    original_config = MODULE_CONF.read_text(encoding="utf-8")
    process: subprocess.Popen[bytes] | None = None
    try:
        MODULE_CONF.write_text(rewrite_module_config(original_config, updates), encoding="utf-8")

        with stdout_path.open("wb") as stdout_handle, stderr_path.open("wb") as stderr_handle:
            process = subprocess.Popen(
                [str(WORLD_EXE)],
                cwd=str(BUILD_ROOT),
                stdout=stdout_handle,
                stderr=stderr_handle,
            )

            wait_for_identity_activity(settings, baseline_id, identity_id, args.startup_timeout)
            time.sleep(args.seconds)
    finally:
        if process is not None:
            stop_worldserver(process)
        MODULE_CONF.write_text(original_config, encoding="utf-8")

    build_report(settings, baseline_id, identity_id, args.name, stdout_path, stderr_path, report_path)
    print(report_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
