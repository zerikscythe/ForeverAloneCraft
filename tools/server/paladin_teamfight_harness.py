#!/usr/bin/env python3
"""Spawn a level-80 Paladin world-bot sandbox, force them into combat, and log the fight.

The harness seeds exact reference loadouts for:
- Alliance: Retribution / Protection (optional Holy)
- Horde:    Retribution / Protection / Holy (+ optional extra Retribution pressure)

It reuses the same assigned-gear + enchantment path as the single-bot Paladin
dummy harness, but skips the training dummy entirely and lets real world bots
fight each other in a deterministic forced combat sandbox.
"""

from __future__ import annotations

import argparse
import datetime as dt
import os
import pathlib
import re
import subprocess
import sys
import time

from paladin_dummy_harness import (
    BUILD_ROOT,
    MODULE_CONF,
    REPO_ROOT,
    WORLD_EXE,
    build_item_enchantments,  # noqa: F401  # imported intentionally for reuse visibility
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


TEAM_BOTS = [
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
    {
        "name": "Dirtyseal",
        "spec_key": "paladin_ret",
        "loadout_key": "ret_toc",
        "faction": 2,
        "race_id": 10,
        "display_id": 15476,
        "gender": 0,
    },
    {
        "name": "Sunwallsteve",
        "spec_key": "paladin_prot",
        "loadout_key": "prot_toc",
        "faction": 2,
        "race_id": 10,
        "display_id": 15476,
        "gender": 0,
    },
    {
        "name": "Smellygrace",
        "spec_key": "paladin_holy",
        "loadout_key": "holy_toc",
        "faction": 2,
        "race_id": 10,
        "display_id": 15477,
        "gender": 1,
    },
    {
        "name": "Memecrusader",
        "spec_key": "paladin_ret",
        "loadout_key": "ret_toc",
        "faction": 2,
        "race_id": 10,
        "display_id": 15476,
        "gender": 0,
    },
    {
        "name": "Lightbonker",
        "spec_key": "paladin_ret",
        "loadout_key": "ret_toc",
        "faction": 2,
        "race_id": 10,
        "display_id": 15476,
        "gender": 0,
    },
]

COMBAT_SUMMARY_RE = re.compile(
    r"outgoing_damage=(?P<outgoing_damage>\d+)\s+"
    r"incoming_damage=(?P<incoming_damage>\d+)\s+"
    r"outgoing_healing=(?P<outgoing_healing>\d+)\s+"
    r"incoming_healing=(?P<incoming_healing>\d+)"
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--level", type=int, default=80)
    parser.add_argument("--seconds", type=int, default=90)
    parser.add_argument("--startup-timeout", type=int, default=180)
    parser.add_argument("--zone-id", type=int, default=210, help="Sandbox zone id")
    parser.add_argument("--map-id", type=int, default=571, help="Sandbox map id")
    parser.add_argument("--spawn-x", type=float, default=5655.3)
    parser.add_argument("--spawn-y", type=float, default=-5131.1)
    parser.add_argument("--spawn-z", type=float, default=824.8)
    parser.add_argument("--force-spawn-count", type=int, default=6)
    parser.add_argument(
        "--include-alliance-healer",
        action="store_true",
        help="Include the Alliance Holy Paladin in the sandbox roster.",
    )
    parser.add_argument(
        "--alliance-solo",
        action="store_true",
        help="Run the sandbox with only Aldricseal on the Alliance side.",
    )
    return parser.parse_args()


def wait_for_forced_identity_activity(
    settings: dict[str, str | int],
    baseline_id: int,
    identity_ids: list[int],
    timeout_seconds: int,
) -> None:
    deadline = time.time() + timeout_seconds
    placeholders = ",".join(str(identity_id) for identity_id in identity_ids)
    sql = (
        "SELECT COUNT(*) FROM living_world_bot_activity_log "
        f"WHERE id > {baseline_id} AND bot_guid IN ({placeholders})"
    )

    while time.time() < deadline:
        if query_scalar(settings, str(settings["characters_db"]), sql) > 0:
            return
        time.sleep(1)

    raise TimeoutError("Timed out waiting for Paladin sandbox activity rows after startup")


def ensure_team_identities(
    settings: dict[str, str | int],
    level: int,
    last_seen_zone: int,
    include_alliance_healer: bool,
    alliance_solo: bool,
) -> list[dict[str, object]]:
    seeded: list[dict[str, object]] = []
    for entry in TEAM_BOTS:
        if alliance_solo and int(entry["faction"]) == 1 and str(entry["name"]) != "Aldricseal":
            continue
        if not include_alliance_healer and str(entry["name"]) == "Katielight":
            continue
        if int(entry["faction"]) == 1:
            home_zone_id = 1519
            home_anchor = "stormwind_inn"
        else:
            home_zone_id = 1637
            home_anchor = "orgrimmar_inn"

        identity_id = ensure_identity_record(
            settings,
            name=str(entry["name"]),
            spec_key=str(entry["spec_key"]),
            level=level,
            faction=int(entry["faction"]),
            race_id=int(entry["race_id"]),
            display_id=int(entry["display_id"]),
            gender=int(entry["gender"]),
            home_zone_id=home_zone_id,
            home_anchor_point_key=home_anchor,
            home_bind_point_key=home_anchor,
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

    counts = run_mysql_query(
        settings,
        str(settings["characters_db"]),
        (
            "SELECT event_type, COUNT(*) FROM living_world_bot_activity_log "
            f"WHERE id > {baseline_id} AND bot_guid IN ({placeholders}) "
            "GROUP BY event_type ORDER BY COUNT(*) DESC, event_type ASC"
        ),
    )
    build_rows = run_mysql_query(
        settings,
        str(settings["characters_db"]),
        (
            "SELECT id, bot_name, event_type, zone_id, detail "
            "FROM living_world_bot_activity_log "
            f"WHERE id > {baseline_id} AND bot_guid IN ({placeholders}) "
            "AND event_type IN ('build_prepared', 'build_prepare_failed', 'combat_enter', 'combat_exit', 'session_complete') "
            "ORDER BY id DESC LIMIT 180"
        ),
    )
    combat_rows = run_mysql_query(
        settings,
        str(settings["characters_db"]),
        (
            "SELECT id, bot_name, event_type, zone_id, detail "
            "FROM living_world_bot_activity_log "
            f"WHERE id > {baseline_id} AND bot_guid IN ({placeholders}) "
            "AND event_type = 'combat_trace' "
            "ORDER BY id DESC LIMIT 300"
        ),
    )
    combat_summary_rows = run_mysql_query(
        settings,
        str(settings["characters_db"]),
        (
            "SELECT id, bot_name, event_type, zone_id, detail "
            "FROM living_world_bot_activity_log "
            f"WHERE id > {baseline_id} AND bot_guid IN ({placeholders}) "
            "AND event_type = 'combat_summary' "
            "ORDER BY id DESC LIMIT 180"
        ),
    )
    identity_rows = run_mysql_query(
        settings,
        str(settings["characters_db"]),
        (
            "SELECT id, name, faction, level, spec_key, runtime_state, runtime_detail, session_count, active_world_session_ms "
            "FROM living_world_bot_identity "
            f"WHERE id IN ({placeholders}) ORDER BY faction ASC, id ASC"
        ),
    )

    faction_by_name = {str(entry["name"]): int(entry["faction"]) for entry in team}
    side_totals = {
        1: {"outgoing_damage": 0, "incoming_damage": 0, "outgoing_healing": 0, "incoming_healing": 0},
        2: {"outgoing_damage": 0, "incoming_damage": 0, "outgoing_healing": 0, "incoming_healing": 0},
    }
    for row in combat_summary_rows:
        if len(row) < 5:
            continue
        bot_name = row[1]
        detail = row[4]
        match = COMBAT_SUMMARY_RE.search(detail)
        if not match:
            continue
        faction = faction_by_name.get(bot_name)
        if faction not in side_totals:
            continue
        for key, value in match.groupdict().items():
            side_totals[faction][key] += int(value)

    with report_path.open("w", encoding="utf-8") as handle:
        handle.write("=== CONFIG ===\n")
        handle.write(f"identities={','.join(str(v) for v in identity_ids)}\n")
        handle.write(f"stdout_log={stdout_path.name}\n")
        handle.write(f"stderr_log={stderr_path.name}\n\n")

        handle.write("=== TEAM ===\n")
        for entry in team:
            seeded_loadout = entry["seeded_loadout"]
            expected_summary = seeded_loadout.get("expected_summary")
            expected_text = (
                format_summary(expected_summary)
                if isinstance(expected_summary, dict)
                else ""
            )
            handle.write(
                f"id={entry['identity_id']} faction={entry['faction']} name={entry['name']} "
                f"spec={entry['spec_key']} loadout={entry['loadout_key']} "
                f"expected={expected_text}\n"
            )
        handle.write("\n")

        handle.write("=== EVENT_COUNTS ===\n")
        for row in counts:
            handle.write("\t".join(str(value) for value in row) + "\n")
        handle.write("\n")

        handle.write("=== IDENTITIES ===\n")
        for row in identity_rows:
            handle.write("\t".join(str(value) for value in row) + "\n")
        handle.write("\n")

        handle.write("=== SIDE_TOTALS ===\n")
        for faction, totals in sorted(side_totals.items()):
            label = "alliance" if faction == 1 else "horde"
            handle.write(
                f"{label}\toutgoing_damage={totals['outgoing_damage']}\t"
                f"incoming_damage={totals['incoming_damage']}\t"
                f"outgoing_healing={totals['outgoing_healing']}\t"
                f"incoming_healing={totals['incoming_healing']}\n"
            )
        handle.write("\n")

        handle.write("=== BUILD_AND_COMBAT_ROWS ===\n")
        for row in build_rows:
            handle.write("\t".join(str(value) for value in row) + "\n")
        handle.write("\n")

        handle.write("=== COMBAT_SUMMARY_ROWS ===\n")
        for row in combat_summary_rows:
            handle.write("\t".join(str(value) for value in row) + "\n")
        handle.write("\n")

        handle.write("=== COMBAT_TRACE_ROWS ===\n")
        for row in combat_rows:
            handle.write("\t".join(str(value) for value in row) + "\n")


def main() -> int:
    args = parse_args()
    ensure_paths()
    settings = load_db_settings()
    team = ensure_team_identities(
        settings,
        args.level,
        args.zone_id,
        args.include_alliance_healer,
        args.alliance_solo,
    )
    identity_ids = [int(entry["identity_id"]) for entry in team]

    timestamp = dt.datetime.now().strftime("%Y%m%d-%H%M%S")
    prefix = REPO_ROOT / f"paladin-teamfight-harness-{timestamp}"
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
        "LivingWorld.DebugForceSessionComposeAttempts": "128",
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

    build_report(settings, baseline_id, team, stdout_path, stderr_path, report_path)
    print(report_path)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:  # pragma: no cover - harness failure path
        print(f"ERROR: {exc}", file=sys.stderr)
        raise
