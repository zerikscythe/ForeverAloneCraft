#!/usr/bin/env python3
"""Spawn three level-80 world-bot rogues and record their MH/OH/ranged assignments.

This is a lightweight verification harness for the generator-backed rogue path:
- Assassination should strongly prefer daggers and a thrown weapon
- Combat should prefer a slow MH / fast OH and a gun/crossbow leaning ranged slot
- Subtlety should prefer dagger/light rogue weapons and a bow-leaning ranged slot
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


ROGUE_BOTS = [
    {
        "name": "Stabitha",
        "spec_key": "rogue_assassination",
        "faction": 1,
        "race_id": 1,
        "display_id": 49,
        "gender": 1,
    },
    {
        "name": "Bladewall",
        "spec_key": "rogue_combat",
        "faction": 1,
        "race_id": 1,
        "display_id": 49,
        "gender": 0,
    },
    {
        "name": "Shadefeint",
        "spec_key": "rogue_subtlety",
        "faction": 1,
        "race_id": 1,
        "display_id": 49,
        "gender": 0,
    },
]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--level", type=int, default=80)
    parser.add_argument("--seconds", type=int, default=20)
    parser.add_argument("--startup-timeout", type=int, default=120)
    parser.add_argument("--zone-id", type=int, default=1519)
    parser.add_argument("--map-id", type=int, default=0)
    parser.add_argument("--spawn-x", type=float, default=-8772.5)
    parser.add_argument("--spawn-y", type=float, default=347.0)
    parser.add_argument("--spawn-z", type=float, default=101.1)
    parser.add_argument("--force-spawn-count", type=int, default=3)
    return parser.parse_args()


def ensure_rogue_identities(
    settings: dict[str, str | int],
    level: int,
    last_seen_zone: int,
) -> list[dict[str, object]]:
    seeded: list[dict[str, object]] = []
    for entry in ROGUE_BOTS:
        identity_id = ensure_identity_record(
            settings,
            name=str(entry["name"]),
            class_id=4,
            spec_key=str(entry["spec_key"]),
            level=level,
            faction=int(entry["faction"]),
            race_id=int(entry["race_id"]),
            display_id=int(entry["display_id"]),
            gender=int(entry["gender"]),
            home_zone_id=1519,
            home_anchor_point_key="stormwind_inn",
            home_bind_point_key="stormwind_inn",
            last_seen_zone=last_seen_zone,
        )

        run_mysql_query(
            settings,
            str(settings["characters_db"]),
            f"DELETE FROM living_world_bot_assigned_gear WHERE identity_id = {identity_id}",
        )
        run_mysql_query(
            settings,
            str(settings["characters_db"]),
            (
                "UPDATE living_world_bot_identity SET "
                "gear_refresh_pending = 1, "
                "last_gear_refresh_band = 0 "
                f"WHERE id = {identity_id}"
            ),
        )

        seeded.append({**entry, "identity_id": identity_id})

    return seeded


def build_report(
    settings: dict[str, str | int],
    baseline_id: int,
    team: list[dict[str, object]],
    stdout_path: pathlib.Path,
    stderr_path: pathlib.Path,
    report_path: pathlib.Path,
) -> None:
    identity_ids = [int(entry["identity_id"]) for entry in team]
    placeholders = ",".join(str(identity_id) for identity_id in identity_ids)

    identity_rows = run_mysql_query(
        settings,
        str(settings["characters_db"]),
        (
            "SELECT id, name, class_id, spec_key, level, gear_tier, last_gear_refresh_band "
            "FROM living_world_bot_identity "
            f"WHERE id IN ({placeholders}) "
            "ORDER BY id"
        ),
    )
    recent = run_mysql_query(
        settings,
        str(settings["characters_db"]),
        (
            "SELECT bot_name, event_type, detail "
            "FROM living_world_bot_activity_log "
            f"WHERE id > {baseline_id} AND bot_guid IN ({placeholders}) "
            "AND event_type IN ('build_prepared', 'resource_snapshot', 'weapon_loadout_snapshot') "
            "ORDER BY id ASC"
        ),
    )

    with report_path.open("w", encoding="utf-8") as handle:
        handle.write(f"report_generated_at={dt.datetime.now().isoformat()}\n")
        handle.write(f"stdout_log={stdout_path.name}\n")
        handle.write(f"stderr_log={stderr_path.name}\n\n")

        handle.write("=== IDENTITIES ===\n")
        for row in identity_rows:
            handle.write("\t".join(str(value) for value in row) + "\n")
        handle.write("\n")

        handle.write("=== SNAPSHOTS ===\n")
        for row in recent:
            handle.write("\t".join(str(value) for value in row) + "\n")


def main() -> int:
    args = parse_args()
    ensure_paths()
    settings = load_db_settings()
    team = ensure_rogue_identities(settings, args.level, args.zone_id)
    identity_ids = [int(entry["identity_id"]) for entry in team]

    timestamp = dt.datetime.now().strftime("%Y%m%d-%H%M%S")
    prefix = REPO_ROOT / f"rogue-weapon-probe-{timestamp}"
    stdout_path = prefix.with_suffix(".stdout.log")
    stderr_path = prefix.with_suffix(".stderr.log")
    report_path = prefix.with_suffix(".report.txt")

    updates = {
        "LivingWorld.AmbientPopulation": str(max(args.force_spawn_count, len(identity_ids))),
        "LivingWorld.AmbientPopulationTickMs": "10000",
        "LivingWorld.AmbientForceSpawnCount": str(max(args.force_spawn_count, len(identity_ids))),
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
        "LivingWorld.DebugForceIdentityIds": f"\"{','.join(str(v) for v in identity_ids)}\"",
        "LivingWorld.DebugForceSessionZoneId": str(args.zone_id),
        "LivingWorld.DebugForceSessionComposeAttempts": "64",
        "LivingWorld.DebugForceCombatTargetIdentityId": "0",
        "LivingWorld.DebugForceCombatTargetEntry": "0",
        "LivingWorld.DebugForceCombatTargetSearchRadius": "0",
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
                identity_ids,
                timeout_seconds=max(1, args.startup_timeout),
            )
            time.sleep(max(1, args.seconds))
    finally:
        if process is not None:
            stop_worldserver(process)
        MODULE_CONF.write_text(original_config, encoding="utf-8-sig")

    build_report(settings, baseline_id, team, stdout_path, stderr_path, report_path)
    print(report_path)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:  # pragma: no cover
        print(f"ERROR: {exc}", file=sys.stderr)
        raise
