#!/usr/bin/env python3
"""Run a deterministic forced world-bot combat sandbox and report Mana Gem usage.

This harness reuses the existing LivingWorld forced ambient/world-bot sandbox:
- forces a chosen set of world-bot identities
- materializes them in a known sandbox zone
- enables a debug combat mana-drain hook for one chosen mage identity
- captures `worldserver` stdout/stderr and DB-backed activity/combat trace rows

By default it does not modify the world DB. Pass `--apply-doctrine` if you want it
to import the Arcane profile/template/doctrine SQL slices first.
"""

from __future__ import annotations

import argparse
import configparser
import datetime as dt
import csv
import io
import os
import pathlib
import subprocess
import sys
import time
from typing import Iterable


REPO_ROOT = pathlib.Path(__file__).resolve().parents[2]
BUILD_ROOT = REPO_ROOT / "out" / "build-vs2022" / "bin" / "Debug"
WORLD_EXE = BUILD_ROOT / "worldserver.exe"
MODULE_CONF = BUILD_ROOT / "configs" / "modules" / "mod-living-world.conf"
DBCONFIG = REPO_ROOT / "tools" / "lw-editor" / "config.ini"
ARCANE_SQL = (
    REPO_ROOT
    / "data"
    / "sql"
    / "updates"
    / "pending_db_world"
    / "rev_living_world_028_arcane_phase2_doctrine.sql"
)
ARCANE_PROFILE_SQL = (
    REPO_ROOT
    / "data"
    / "sql"
    / "updates"
    / "pending_db_world"
    / "rev_living_world_027_additional_default_dps_profiles.sql"
)
ARCANE_TEMPLATE_SQL = (
    REPO_ROOT
    / "data"
    / "sql"
    / "updates"
    / "pending_db_world"
    / "rev_living_world_029_arcane_mage_talent_template.sql"
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--identities", default="2,4", help="Comma-separated world bot identity ids")
    parser.add_argument("--drain-identity", type=int, default=2, help="Identity id to mana-drain during combat")
    parser.add_argument("--seconds", type=int, default=120, help="How long to leave worldserver running")
    parser.add_argument(
        "--startup-timeout",
        type=int,
        default=240,
        help="How long to wait for forced-identity activity rows after startup",
    )
    parser.add_argument("--zone-id", type=int, default=3518, help="Forced session zone id")
    parser.add_argument("--map-id", type=int, default=530, help="Forced spawn / synthetic interest map id")
    parser.add_argument("--spawn-x", type=float, default=-1228.6)
    parser.add_argument("--spawn-y", type=float, default=7312.4)
    parser.add_argument("--spawn-z", type=float, default=-3.7)
    parser.add_argument("--drain-target-mana-pct", type=int, default=0)
    parser.add_argument("--drain-interval-ms", type=int, default=250)
    parser.add_argument("--force-spawn-count", type=int, default=2)
    parser.add_argument(
        "--apply-doctrine",
        action="store_true",
        help="Apply Arcane default-profile, talent-template, and doctrine SQL before startup",
    )
    return parser.parse_args()


def parse_identity_ids(raw: str) -> list[int]:
    ids: list[int] = []
    for token in raw.split(","):
        token = token.strip()
        if not token:
            continue
        value = int(token)
        if value > 0 and value not in ids:
            ids.append(value)
    if not ids:
        raise ValueError("At least one identity id is required")
    return ids


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
        "world_db": "acore_world",
    }


def mysql_base_command(settings: dict[str, str | int], database: str) -> list[str]:
    return [
        "mysql",
        "-h",
        str(settings["host"]),
        "-P",
        str(settings["port"]),
        "-u",
        str(settings["user"]),
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
            f"mysql query failed for {database}:\n"
            + result.stdout
            + result.stderr
        )

    output = result.stdout.strip()
    if not output:
        return []

    reader = csv.reader(io.StringIO(output), delimiter="\t")
    return [tuple(row) for row in reader]


def ensure_paths() -> None:
    if not WORLD_EXE.exists():
        raise FileNotFoundError(f"worldserver.exe not found: {WORLD_EXE}")
    if not MODULE_CONF.exists():
        raise FileNotFoundError(f"Module config not found: {MODULE_CONF}")


def apply_world_sql(settings: dict[str, str | int], sql_path: pathlib.Path) -> None:
    if not sql_path.exists():
        raise FileNotFoundError(f"Doctrine SQL not found: {sql_path}")

    command = mysql_base_command(settings, str(settings["world_db"]))
    with sql_path.open("rb") as handle:
        result = subprocess.run(command, stdin=handle, capture_output=True)
    if result.returncode != 0:
        raise RuntimeError(
            "Failed to apply doctrine SQL:\n"
            + result.stdout.decode(errors="ignore")
            + result.stderr.decode(errors="ignore")
        )


def query_scalar(connection, sql: str, params: Iterable[object] = ()) -> int:
    raise NotImplementedError("query_scalar(connection, ...) should not be used")


def query_scalar_sql(
    settings: dict[str, str | int],
    database: str,
    sql: str,
) -> int:
    rows = run_mysql_query(settings, database, sql)
    if not rows or not rows[0]:
        return 0
    return int(rows[0][0])


def doctrine_present(settings: dict[str, str | int]) -> bool:
    count = query_scalar_sql(
        settings,
        str(settings["world_db"]),
        "SELECT COUNT(*) FROM living_world_bot_combat_default_action WHERE action_id = 2700 AND item_id = 33312",
    )
    return count > 0


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


def sql_quote(value: object) -> str:
    if value is None:
        return "NULL"
    if isinstance(value, bool):
        return "1" if value else "0"
    if isinstance(value, (int, float)):
        return str(value)

    text = str(value)
    return "'" + text.replace("\\", "\\\\").replace("'", "\\'") + "'"


def fetch_rows_sql(
    settings: dict[str, str | int],
    database: str,
    sql_template: str,
    params: Iterable[object] = (),
) -> list[tuple[str, ...]]:
    sql = sql_template
    for param in params:
        sql = sql.replace("%s", sql_quote(param), 1)
    return run_mysql_query(settings, database, sql)


def build_report(
    settings: dict[str, str | int],
    baseline_id: int,
    identity_ids: list[int],
    drain_identity: int,
    stdout_path: pathlib.Path,
    stderr_path: pathlib.Path,
    report_path: pathlib.Path,
    doctrine_was_present: bool,
) -> None:
    placeholders = ",".join(["%s"] * len(identity_ids))
    counts = fetch_rows_sql(
        settings,
        str(settings["characters_db"]),
        "SELECT event_type, COUNT(*) FROM living_world_bot_activity_log WHERE id > %s GROUP BY event_type ORDER BY COUNT(*) DESC, event_type ASC",
        (baseline_id,),
    )
    forced_rows = fetch_rows_sql(
        settings,
        str(settings["characters_db"]),
        f"SELECT id, bot_name, event_type, detail FROM living_world_bot_activity_log WHERE id > %s AND bot_guid IN ({placeholders}) ORDER BY id DESC LIMIT 250",
        (baseline_id, *identity_ids),
    )
    gem_rows = fetch_rows_sql(
        settings,
        str(settings["characters_db"]),
        f"SELECT id, bot_name, event_type, detail FROM living_world_bot_activity_log WHERE id > %s AND bot_guid IN ({placeholders}) AND event_type = 'combat_trace' AND detail LIKE %s ORDER BY id DESC LIMIT 50",
        (baseline_id, *identity_ids, "%Item(33312)%"),
    )
    simulated_item_rows = fetch_rows_sql(
        settings,
        str(settings["characters_db"]),
        f"SELECT id, bot_name, event_type, detail FROM living_world_bot_activity_log WHERE id > %s AND bot_guid IN ({placeholders}) AND event_type = 'combat_trace' AND detail LIKE %s ORDER BY id DESC LIMIT 80",
        (baseline_id, *identity_ids, "%simulated_item_use=1%"),
    )
    build_rows = fetch_rows_sql(
        settings,
        str(settings["characters_db"]),
        f"SELECT id, bot_name, event_type, detail FROM living_world_bot_activity_log WHERE id > %s AND bot_guid IN ({placeholders}) AND event_type IN ('build_prepared', 'build_prepare_failed', 'combat_enter', 'combat_exit') ORDER BY id DESC LIMIT 80",
        (baseline_id, *identity_ids),
    )

    with report_path.open("w", encoding="utf-8") as handle:
        handle.write("=== CONFIG ===\n")
        handle.write(f"identities={','.join(str(v) for v in identity_ids)}\n")
        handle.write(f"drain_identity={drain_identity}\n")
        handle.write(f"doctrine_present={1 if doctrine_was_present else 0}\n")
        handle.write(f"stdout_log={stdout_path.name}\n")
        handle.write(f"stderr_log={stderr_path.name}\n\n")

        handle.write("=== COUNTS_SINCE_BASELINE ===\n")
        for event_type, count in counts:
            handle.write(f"{event_type}\t{count}\n")
        handle.write("\n")

        handle.write("=== BUILD_AND_COMBAT_BOUNDARY_ROWS ===\n")
        for row in build_rows:
            handle.write("\t".join(str(value) for value in row) + "\n")
        handle.write("\n")

        handle.write("=== MANA_GEM_ROWS ===\n")
        for row in gem_rows:
            handle.write("\t".join(str(value) for value in row) + "\n")
        handle.write("\n")

        handle.write("=== SIMULATED_ITEM_USE_ROWS ===\n")
        for row in simulated_item_rows:
            handle.write("\t".join(str(value) for value in row) + "\n")
        handle.write("\n")

        handle.write("=== FORCED_IDENTITY_ROWS ===\n")
        for row in forced_rows:
            handle.write("\t".join(str(value) for value in row) + "\n")


def wait_for_forced_identity_activity(
    settings: dict[str, str | int],
    baseline_id: int,
    identity_ids: list[int],
    timeout_seconds: int = 180,
) -> None:
    deadline = time.time() + timeout_seconds
    placeholders = ",".join(str(identity_id) for identity_id in identity_ids)
    sql = (
        "SELECT COUNT(*) FROM living_world_bot_activity_log "
        f"WHERE id > {baseline_id} AND bot_guid IN ({placeholders})"
    )

    while time.time() < deadline:
        count = query_scalar_sql(settings, str(settings["characters_db"]), sql)
        if count > 0:
            return
        time.sleep(1)

    raise TimeoutError(
        "Timed out waiting for forced-identity activity rows after startup"
    )


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


def main() -> int:
    args = parse_args()
    ensure_paths()
    settings = load_db_settings()
    identity_ids = parse_identity_ids(args.identities)
    timestamp = dt.datetime.now().strftime("%Y%m%d-%H%M%S")
    prefix = REPO_ROOT / f"combat-sandbox-mana-gem-{timestamp}"
    stdout_path = prefix.with_suffix(".stdout.log")
    stderr_path = prefix.with_suffix(".stderr.log")
    report_path = prefix.with_suffix(".report.txt")

    if args.drain_identity not in identity_ids:
        raise ValueError("--drain-identity must be one of --identities")

    if args.apply_doctrine:
        apply_world_sql(settings, ARCANE_PROFILE_SQL)
        apply_world_sql(settings, ARCANE_TEMPLATE_SQL)
        apply_world_sql(settings, ARCANE_SQL)

    doctrine_was_present = doctrine_present(settings)

    updates = {
        "LivingWorld.AmbientPopulation": str(args.force_spawn_count),
        "LivingWorld.AmbientPopulationTickMs": "15000",
        "LivingWorld.AmbientForceSpawnCount": str(args.force_spawn_count),
        "LivingWorld.AmbientForceSpawnMapId": str(args.map_id),
        "LivingWorld.AmbientForceSpawnX": f"{args.spawn_x}",
        "LivingWorld.AmbientForceSpawnY": f"{args.spawn_y}",
        "LivingWorld.AmbientForceSpawnZ": f"{args.spawn_z}",
        "LivingWorld.DebugSyntheticInterestEnabled": "1",
        "LivingWorld.DebugSyntheticInterestMapId": str(args.map_id),
        "LivingWorld.DebugSyntheticInterestZoneId": str(args.zone_id),
        "LivingWorld.DebugSyntheticInterestSwitchMapId": "0",
        "LivingWorld.DebugSyntheticInterestSwitchZoneId": "0",
        "LivingWorld.DebugSyntheticInterestSwitchMs": "0",
        "LivingWorld.DebugSyntheticInterestClearMs": "0",
        "LivingWorld.DebugHotZoneCooldownMs": "10000",
        "LivingWorld.DebugForceIdentityIds": f'"{",".join(str(v) for v in identity_ids)}"',
        "LivingWorld.DebugForceSessionZoneId": str(args.zone_id),
        "LivingWorld.DebugForceSessionComposeAttempts": "64",
        "LivingWorld.DebugCombatManaDrainIdentityId": str(args.drain_identity),
        "LivingWorld.DebugCombatManaDrainTargetManaPct": str(args.drain_target_mana_pct),
        "LivingWorld.DebugCombatManaDrainIntervalMs": str(args.drain_interval_ms),
    }

    original_config = MODULE_CONF.read_text(encoding="utf-8")
    baseline_id = 0
    process: subprocess.Popen[bytes] | None = None

    try:
        MODULE_CONF.write_text(rewrite_module_config(original_config, updates), encoding="utf-8")

        baseline_id = query_scalar_sql(
            settings,
            str(settings["characters_db"]),
            "SELECT COALESCE(MAX(id), 0) FROM living_world_bot_activity_log",
        )

        with stdout_path.open("wb") as stdout_handle, stderr_path.open("wb") as stderr_handle:
            process = subprocess.Popen(
                [str(WORLD_EXE)],
                cwd=str(BUILD_ROOT),
                stdout=stdout_handle,
                stderr=stderr_handle,
            )
            stdout_handle.flush()
            os.fsync(stdout_handle.fileno())
            wait_for_forced_identity_activity(
                settings,
                baseline_id,
                identity_ids,
                timeout_seconds=max(1, args.startup_timeout),
            )
            time.sleep(max(1, args.seconds))
            stop_worldserver(process)

        build_report(
            settings,
            baseline_id,
            identity_ids,
            args.drain_identity,
            stdout_path,
            stderr_path,
            report_path,
            doctrine_was_present,
        )

        print(report_path)
        return 0
    finally:
        if process is not None and process.poll() is None:
            stop_worldserver(process)
        MODULE_CONF.write_text(original_config, encoding="utf-8")


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:  # pragma: no cover - harness failure path
        print(f"ERROR: {exc}", file=sys.stderr)
        raise