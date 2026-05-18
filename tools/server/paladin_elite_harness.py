#!/usr/bin/env python3
"""Spawn a level-80 Alliance Paladin trio beside a real hostile creature or pack and log the fight.

This harness is meant to answer the "real tank" questions that bot-vs-bot
skirmishes blur together:
- does Protection become the one getting hit?
- does Holy keep the tank alive instead of playing tourist?
- does Retribution add pressure without turning the pull into chaos?

It intentionally uses an existing hostile world elite rather than a synthetic
 bot-only duel target, so we exercise the normal creature combat path.
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
    format_summary,
    load_db_settings,
    query_scalar,
    rewrite_module_config,
    run_mysql_query,
    seed_reference_loadout,
    stop_worldserver,
)
from paladin_teamfight_harness import build_report, wait_for_forced_identity_activity


ALLIANCE_TEAM = [
    {
        "name": "Aldricseal",
        "spec_key": "paladin_ret",
        "loadout_key": "ret_toc",
        "faction": 1,
        "race_id": 1,
        "display_id": 49,
        "gender": 0,
    },
    {
        "name": "Bromwall",
        "spec_key": "paladin_prot",
        "loadout_key": "prot_toc",
        "faction": 1,
        "race_id": 3,
        "display_id": 132,
        "gender": 0,
    },
    {
        "name": "Katielight",
        "spec_key": "paladin_holy",
        "loadout_key": "holy_toc",
        "faction": 1,
        "race_id": 11,
        "display_id": 16126,
        "gender": 1,
    },
]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--level", type=int, default=80)
    parser.add_argument("--seconds", type=int, default=90)
    parser.add_argument("--startup-timeout", type=int, default=180)
    parser.add_argument("--zone-id", type=int, default=67, help="Synthetic interest zone id")
    parser.add_argument(
        "--session-zone-id",
        type=int,
        default=210,
        help="Forced session compose zone id; can differ from the physical spawn zone for combat-only tests",
    )
    parser.add_argument("--map-id", type=int, default=571, help="Sandbox map id")
    parser.add_argument("--spawn-x", type=float, default=8386.5)
    parser.add_argument("--spawn-y", type=float, default=-1189.1)
    parser.add_argument("--spawn-z", type=float, default=927.5)
    parser.add_argument("--force-spawn-count", type=int, default=3)
    parser.add_argument("--elite-guid", type=int, default=152010, help="Existing hostile elite creature guid")
    parser.add_argument("--elite-entry", type=int, default=32500, help="Creature entry for report context")
    parser.add_argument("--elite-name", default="Dirkee")
    parser.add_argument(
        "--temp-pack-source-guid",
        type=int,
        default=0,
        help="Existing creature guid to clone into a temporary nearby pack for multi-target threat tests",
    )
    parser.add_argument(
        "--temp-pack-count",
        type=int,
        default=0,
        help="How many temporary pack members to create from the source guid",
    )
    return parser.parse_args()


def ensure_alliance_team(
    settings: dict[str, str | int],
    level: int,
    last_seen_zone: int,
) -> list[dict[str, object]]:
    seeded: list[dict[str, object]] = []
    for entry in ALLIANCE_TEAM:
        identity_id = ensure_identity_record(
            settings,
            name=str(entry["name"]),
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
        loadout = seed_reference_loadout(settings, identity_id, level, str(entry["loadout_key"]))
        seeded.append(
            {
                **entry,
                "identity_id": identity_id,
                "seeded_loadout": loadout,
            }
        )

    leader_identity_id = next(
        int(entry["identity_id"]) for entry in seeded if str(entry["spec_key"]) == "paladin_prot"
    )
    ambient_group_id = leader_identity_id
    role_by_spec = {
        "paladin_prot": "tank",
        "paladin_holy": "healer",
        "paladin_ret": "melee_dps",
    }

    for entry in seeded:
        identity_id = int(entry["identity_id"])
        spec_key = str(entry["spec_key"])
        ambient_group_role = role_by_spec.get(spec_key, "support")
        run_mysql_query(
            settings,
            str(settings["characters_db"]),
            (
                "UPDATE living_world_bot_identity "
                f"SET ambient_group_id = {ambient_group_id}, "
                f"ambient_group_leader_identity_id = {leader_identity_id}, "
                f"ambient_group_role = '{ambient_group_role}' "
                f"WHERE id = {identity_id}"
            ),
        )
    return seeded


def spawn_temp_pack(
    settings: dict[str, str | int],
    source_guid: int,
    map_id: int,
    spawn_x: float,
    spawn_y: float,
    spawn_z: float,
    count: int,
) -> list[int]:
    if source_guid <= 0 or count <= 0:
        return []

    next_guid = query_scalar(
        settings,
        "acore_world",
        "SELECT COALESCE(MAX(guid), 0) + 1 FROM creature",
    )

    offsets = [
        (8.0, 0.0, 0.0),
        (-8.0, 6.0, 0.0),
        (-6.0, -7.0, 0.0),
        (11.0, 5.0, 0.0),
        (-11.0, 3.0, 0.0),
    ]
    guids: list[int] = []
    for index in range(count):
        guid = int(next_guid) + index
        offset_x, offset_y, offset_z = offsets[index % len(offsets)]
        x = spawn_x + offset_x
        y = spawn_y + offset_y
        z = spawn_z + offset_z
        sql = (
            "INSERT INTO creature "
            "(guid, id1, id2, id3, map, zoneId, areaId, spawnMask, phaseMask, equipment_id, "
            "position_x, position_y, position_z, orientation, spawntimesecs, wander_distance, "
            "currentwaypoint, curhealth, curmana, MovementType, npcflag, unit_flags, dynamicflags, "
            "ScriptName, VerifiedBuild, CreateObject, Comment) "
            "SELECT "
            f"{guid}, id1, id2, id3, {map_id}, zoneId, areaId, spawnMask, phaseMask, equipment_id, "
            f"{x}, {y}, {z}, orientation, spawntimesecs, 0, currentwaypoint, curhealth, curmana, "
            "MovementType, npcflag, unit_flags, dynamicflags, ScriptName, VerifiedBuild, CreateObject, "
            f"'paladin_elite_harness temp pack from {source_guid}' "
            "FROM creature "
            f"WHERE guid = {source_guid}"
        )
        run_mysql_query(settings, "acore_world", sql)
        guids.append(guid)
    return guids


def delete_temp_pack(settings: dict[str, str | int], guids: list[int]) -> None:
    if not guids:
        return
    guid_list = ",".join(str(guid) for guid in guids)
    run_mysql_query(settings, "acore_world", f"DELETE FROM creature WHERE guid IN ({guid_list})")


def build_elite_report(
    settings: dict[str, str | int],
    baseline_id: int,
    team: list[dict[str, object]],
    stdout_path: pathlib.Path,
    stderr_path: pathlib.Path,
    report_path: pathlib.Path,
    elite_guid: int,
    elite_entry: int,
    elite_name: str,
    temp_pack_guids: list[int],
) -> None:
    build_report(settings, baseline_id, team, stdout_path, stderr_path, report_path)

    elite_rows = run_mysql_query(
        settings,
        "acore_world",
        (
            "SELECT c.guid, c.id1, ct.name, ct.minlevel, ct.maxlevel, ct.rank, ct.faction, "
            "c.map, c.position_x, c.position_y, c.position_z "
            "FROM creature c "
            "JOIN creature_template ct ON ct.entry = c.id1 "
            f"WHERE c.guid = {elite_guid}"
        ),
    )

    with report_path.open("r+", encoding="utf-8") as handle:
        existing = handle.read()
        handle.seek(0)
        handle.write("=== ELITE ===\n")
        if elite_rows:
            for row in elite_rows:
                handle.write("\t".join(str(value) for value in row) + "\n")
        else:
            handle.write(
                f"guid={elite_guid}\tentry={elite_entry}\tname={elite_name}\tstatus=missing_in_db_snapshot\n"
            )
        handle.write("\n")
        handle.write("=== TEMP_PACK ===\n")
        if temp_pack_guids:
            handle.write(f"guids={','.join(str(guid) for guid in temp_pack_guids)}\n")
        else:
            handle.write("guids=\n")
        handle.write("\n")
        handle.write(existing)


def main() -> int:
    args = parse_args()
    ensure_paths()
    settings = load_db_settings()
    team = ensure_alliance_team(settings, args.level, args.zone_id)
    identity_ids = [int(entry["identity_id"]) for entry in team]
    pull_identity_id = next(
        int(entry["identity_id"]) for entry in team if str(entry["spec_key"]) == "paladin_prot"
    )
    temp_pack_guids = spawn_temp_pack(
        settings,
        args.temp_pack_source_guid,
        args.map_id,
        args.spawn_x,
        args.spawn_y,
        args.spawn_z,
        args.temp_pack_count,
    )

    timestamp = dt.datetime.now().strftime("%Y%m%d-%H%M%S")
    prefix = REPO_ROOT / f"paladin-elite-harness-{timestamp}"
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
        "LivingWorld.DebugForceSessionZoneId": str(args.session_zone_id),
        "LivingWorld.DebugForceSessionComposeAttempts": "128",
        "LivingWorld.DebugForceCombatTargetIdentityId": str(pull_identity_id),
        "LivingWorld.DebugForceCombatTargetEntry": str(args.elite_entry),
        "LivingWorld.DebugForceCombatTargetSearchRadius": "45",
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
        MODULE_CONF.write_text(original_config, encoding="utf-8")
        delete_temp_pack(settings, temp_pack_guids)

    build_elite_report(
        settings,
        baseline_id,
        team,
        stdout_path,
        stderr_path,
        report_path,
        elite_guid=args.elite_guid,
        elite_entry=args.elite_entry,
        elite_name=args.elite_name,
        temp_pack_guids=temp_pack_guids,
    )
    print(report_path)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:  # pragma: no cover - harness failure path
        print(f"ERROR: {exc}", file=sys.stderr)
        raise
