#!/usr/bin/env python3
"""Spawn a Frost Mage world bot and verify basic companion-pet summon/follow/assist behavior.

This probe keeps the scope intentionally small:
- force-spawn one Frost Mage world-bot identity
- let it idle long enough to maintain a Water Elemental
- optionally force it onto a training dummy
- capture DB activity rows related to pet control and combat
"""

from __future__ import annotations

import argparse
import datetime as dt
import pathlib
import subprocess
import sys
import time

from paladin_dummy_harness import (
    BUILD_ROOT,
    MODULE_CONF,
    REPO_ROOT,
    WORLD_EXE,
    ensure_identity_record,
    ensure_paths,
    load_db_settings,
    query_scalar,
    rewrite_module_config,
    run_mysql_query,
    stop_worldserver,
)
from paladin_teamfight_harness import wait_for_forced_identity_activity


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--name", default="Frostpetprobe")
    parser.add_argument("--level", type=int, default=80)
    parser.add_argument("--seconds", type=int, default=45)
    parser.add_argument("--startup-timeout", type=int, default=120)
    parser.add_argument("--zone-id", type=int, default=1519)
    parser.add_argument("--map-id", type=int, default=0)
    parser.add_argument("--spawn-x", type=float, default=-8772.5)
    parser.add_argument("--spawn-y", type=float, default=347.0)
    parser.add_argument("--spawn-z", type=float, default=101.1)
    parser.add_argument("--force-spawn-count", type=int, default=1)
    parser.add_argument("--dummy-entry", type=int, default=31144)
    parser.add_argument("--dummy-radius", type=float, default=30.0)
    parser.add_argument("--enable-combat", action="store_true")
    parser.add_argument("--loadout-key", default="")
    return parser.parse_args()


def ensure_mage_identity(
    settings: dict[str, str | int],
    *,
    name: str,
    level: int,
    last_seen_zone: int,
    loadout_key: str,
) -> int:
    return ensure_identity_record(
        settings,
        name=name,
        class_id=8,
        spec_key="mage_frost",
        loadout_key=loadout_key,
        level=level,
        faction=1,
        race_id=1,
        display_id=49,
        gender=0,
        home_zone_id=1519,
        home_anchor_point_key="stormwind_inn",
        home_bind_point_key="stormwind_inn",
        last_seen_zone=last_seen_zone,
    )


def build_report(
    settings: dict[str, str | int],
    baseline_id: int,
    identity_id: int,
    name: str,
    stdout_path: pathlib.Path,
    stderr_path: pathlib.Path,
    report_path: pathlib.Path,
) -> None:
    counts = run_mysql_query(
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
            "ORDER BY id DESC LIMIT 180"
        ),
    )
    pet_rows = run_mysql_query(
        settings,
        str(settings["characters_db"]),
        (
            "SELECT id, bot_name, event_type, zone_id, detail "
            "FROM living_world_bot_activity_log "
            f"WHERE id > {baseline_id} AND bot_guid = {identity_id} "
            "AND event_type IN ('pet_control', 'combat_enter', 'combat_exit', 'build_prepared', 'resource_snapshot', 'glyph_snapshot') "
            "ORDER BY id ASC"
        ),
    )
    identity_rows = run_mysql_query(
        settings,
        str(settings["characters_db"]),
        (
            "SELECT id, name, class_id, spec_key, level, runtime_state, runtime_detail "
            "FROM living_world_bot_identity "
            f"WHERE id = {identity_id} LIMIT 1"
        ),
    )

    with report_path.open("w", encoding="utf-8") as handle:
        handle.write(f"report_generated_at={dt.datetime.now().isoformat()}\n")
        handle.write(f"identity_id={identity_id}\n")
        handle.write(f"name={name}\n")
        handle.write(f"stdout_log={stdout_path.name}\n")
        handle.write(f"stderr_log={stderr_path.name}\n\n")

        handle.write("=== IDENTITY ===\n")
        for row in identity_rows:
            handle.write("\t".join(str(value) for value in row) + "\n")
        handle.write("\n")

        handle.write("=== EVENT_COUNTS ===\n")
        for row in counts:
            handle.write("\t".join(str(value) for value in row) + "\n")
        handle.write("\n")

        handle.write("=== PET_FOCUS_ROWS ===\n")
        for row in pet_rows:
            handle.write("\t".join(str(value) for value in row) + "\n")
        handle.write("\n")

        handle.write("=== RECENT_ROWS ===\n")
        for row in recent:
            handle.write("\t".join(str(value) for value in row) + "\n")


def main() -> int:
    args = parse_args()
    ensure_paths()
    settings = load_db_settings()
    identity_id = ensure_mage_identity(
        settings,
        name=args.name,
        level=args.level,
        last_seen_zone=args.zone_id,
        loadout_key=args.loadout_key,
    )

    timestamp = dt.datetime.now().strftime("%Y%m%d-%H%M%S")
    prefix = REPO_ROOT / f"mage-pet-harness-{timestamp}"
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
        "LivingWorld.DebugForceCombatTargetIdentityId": str(identity_id if args.enable_combat else 0),
        "LivingWorld.DebugForceCombatTargetEntry": str(args.dummy_entry if args.enable_combat else 0),
        "LivingWorld.DebugForceCombatTargetSearchRadius": str(args.dummy_radius if args.enable_combat else 0),
    }

    baseline_id = query_scalar(
        settings,
        str(settings["characters_db"]),
        "SELECT COALESCE(MAX(id), 0) FROM living_world_bot_activity_log",
    )

    original_config = MODULE_CONF.read_text(encoding="utf-8-sig")
    process: subprocess.Popen[bytes] | None = None
    try:
        MODULE_CONF.write_text(rewrite_module_config(original_config, updates), encoding="utf-8-sig")

        with stdout_path.open("wb") as stdout_handle, stderr_path.open("wb") as stderr_handle:
            process = subprocess.Popen(
                [str(WORLD_EXE)],
                cwd=str(BUILD_ROOT),
                stdout=stdout_handle,
                stderr=stderr_handle,
            )

            wait_for_forced_identity_activity(
                settings,
                baseline_id,
                [identity_id],
                timeout_seconds=max(1, args.startup_timeout),
            )
            time.sleep(max(1, args.seconds))
    finally:
        if process is not None:
            stop_worldserver(process)
        MODULE_CONF.write_text(original_config, encoding="utf-8-sig")

    build_report(settings, baseline_id, identity_id, args.name, stdout_path, stderr_path, report_path)
    print(report_path)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:  # pragma: no cover
        print(f"ERROR: {exc}", file=sys.stderr)
        raise
