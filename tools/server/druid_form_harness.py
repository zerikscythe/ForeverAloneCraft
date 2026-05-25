#!/usr/bin/env python3
"""Probe level-80 Druid world-bot forms and travel-state behavior per spec.

This harness answers:
- does each Druid spec materialize with a sane profile/loadout snapshot?
- does its preferred baseline form appear while idle?
- does it switch into Travel Form when we force travel state?

It intentionally stays lighter than a full combat harness. For Druid, the first
question is whether the self-state substrate is selecting the right form and
whether the build/loadout surface looks sane per spec. Combat doctrine work can
layer on top once that footing is proven.
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


SPEC_CONFIG = {
    "balance": {
        "identity_name": "Druidbalprobe",
        "spec_key": "druid_balance",
        "loadout_key": "test_state_balance",
        "class_id": 11,
        "race_id": 4,
        "display_id": 37919,
        "gender": 0,
        "faction": 1,
        "expected_form_idle": "Moonkin",
        "expected_loadout": "Druid Balance Tier 1",
    },
    "feral_bear": {
        "identity_name": "Druidbearprobe",
        "spec_key": "druid_feral",
        "loadout_key": "test_state_bear",
        "class_id": 11,
        "race_id": 4,
        "display_id": 37919,
        "gender": 0,
        "faction": 1,
        "expected_form_idle": "Bear",
        "expected_loadout": "Druid Feral Tier 1",
    },
    "feral_cat": {
        "identity_name": "Druidcatprobe",
        "spec_key": "druid_feral",
        "loadout_key": "Druid_Feral_PVE_01",
        "class_id": 11,
        "race_id": 4,
        "display_id": 37919,
        "gender": 0,
        "faction": 1,
        "expected_form_idle": "Cat",
        "expected_loadout": "Druid Feral Cat PvE",
    },
    "restoration": {
        "identity_name": "Druidrestoprobe",
        "spec_key": "druid_resto",
        "loadout_key": "test_state_resto",
        "class_id": 11,
        "race_id": 4,
        "display_id": 37919,
        "gender": 0,
        "faction": 1,
        "expected_form_idle": "Tree",
        "expected_loadout": "Druid Restoration Tier 1",
    },
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--specs",
        default="balance,feral_bear,feral_cat,restoration",
        help="Comma-separated spec keys or 'all'",
    )
    parser.add_argument("--level", type=int, default=80)
    parser.add_argument("--contexts", default="idle,travel")
    parser.add_argument("--seconds", type=int, default=25)
    parser.add_argument("--startup-timeout", type=int, default=120)
    parser.add_argument("--zone-id", type=int, default=1519)
    parser.add_argument("--map-id", type=int, default=0)
    parser.add_argument("--spawn-x", type=float, default=-8772.5)
    parser.add_argument("--spawn-y", type=float, default=347.0)
    parser.add_argument("--spawn-z", type=float, default=101.1)
    parser.add_argument("--force-spawn-count", type=int, default=1)
    return parser.parse_args()


def parse_specs(raw: str) -> list[str]:
    if raw.strip().lower() == "all":
        return list(SPEC_CONFIG.keys())

    specs: list[str] = []
    for token in raw.split(","):
        token = token.strip().lower()
        if not token:
            continue
        if token not in SPEC_CONFIG:
            raise ValueError(f"Unsupported spec '{token}'")
        if token not in specs:
            specs.append(token)
    if not specs:
        raise ValueError("At least one spec is required")
    return specs


def parse_contexts(raw: str) -> list[str]:
    contexts: list[str] = []
    for token in raw.split(","):
        token = token.strip().lower()
        if not token:
            continue
        if token not in {"idle", "travel"}:
            raise ValueError(f"Unsupported context '{token}'")
        if token not in contexts:
            contexts.append(token)
    if not contexts:
        raise ValueError("At least one context is required")
    return contexts


def ensure_druid_test_profiles(settings: dict[str, str | int]) -> None:
    sql = """
DELETE FROM living_world_bot_combat_default_condition WHERE entry_id IN (11010,11020,11030,11040);
DELETE FROM living_world_bot_combat_default_action WHERE entry_id IN (11010,11020,11030,11040);
DELETE FROM living_world_bot_combat_default_entry WHERE entry_id IN (11010,11020,11030,11040);
DELETE FROM living_world_bot_combat_default_profile WHERE default_profile_id IN (1101,1102,1103,1104);

INSERT INTO living_world_bot_combat_default_profile
    (default_profile_id, spec_key, role_key, class_key, context_key, variant_key, display_name, description,
     conservation_mode, resource_low_water, resource_high_water, enable_down_rank, down_rank_floor,
     default_aoe_mode, default_aoe_min_targets, default_aoe_scan_radius,
     targeting_mode, current_target_bias, assist_target_bias, focus_fire_bias,
     protect_ally_bias, prefer_healer_bias, prefer_dps_bias, avoid_tank_bias)
VALUES
    (1101, 'Balance', 'DPS',  'Druid', 'PvE', 'test_state_balance', 'Druid Balance State Probe',
     'Minimal test profile used by druid_form_harness to validate Moonkin and Travel Form state selection.',
     1, 35, 70, 0, 0, 0, 3, 10, 1, 100, 150, 80, 150, 200, 150, 150),
    (1102, 'Feral',   'TANK', 'Druid', 'PvE', 'test_state_bear', 'Druid Feral Bear State Probe',
     'Minimal test profile used by druid_form_harness to validate Bear and Travel Form state selection.',
     1, 35, 70, 0, 0, 0, 3, 10, 1, 100, 150, 80, 150, 200, 150, 150),
    (1103, 'Feral',   'DPS',  'Druid', 'PvE', 'Druid_Feral_PVE_01', 'Druid Feral Cat State Probe',
     'Minimal test profile used by druid_form_harness to validate Cat Form and the cat-focused feral loadout.',
     1, 35, 70, 0, 0, 0, 3, 10, 1, 100, 150, 80, 150, 200, 150, 150),
    (1104, 'Restoration', 'HEAL', 'Druid', 'PvE', 'test_state_resto', 'Druid Restoration State Probe',
     'Minimal test profile used by druid_form_harness to validate Tree of Life and Travel Form state selection.',
     1, 35, 70, 0, 0, 0, 3, 10, 1, 100, 150, 80, 150, 200, 150, 150);

INSERT INTO living_world_bot_combat_default_entry
    (entry_id, default_profile_id, priority, label, is_interrupt, breaks_current_cast, enabled, condition_logic)
VALUES
    (11010, 1101, 10, 'Wrath filler',            0, 0, 1, 0),
    (11020, 1102, 10, 'Mangle (Bear)',           0, 0, 1, 0),
    (11030, 1103, 10, 'Mangle (Cat)',            0, 0, 1, 0),
    (11040, 1104, 10, 'Rejuvenation (self)',     0, 0, 1, 0);

INSERT INTO living_world_bot_combat_default_action
    (action_id, entry_id, slot, action_type, spell_base_id, item_id, rank_mode, rank_value, target_key, aoe_mode, aoe_min_targets, aoe_radius)
VALUES
    (110100, 11010, 0, 0, 5176,  0, 0, 0, 'enemy_primary', NULL, NULL, NULL),
    (110200, 11020, 0, 0, 33917, 0, 0, 0, 'enemy_primary', NULL, NULL, NULL),
    (110300, 11030, 0, 0, 33876, 0, 0, 0, 'enemy_primary', NULL, NULL, NULL),
    (110400, 11040, 0, 0, 774,   0, 0, 0, 'self',          NULL, NULL, NULL);
"""
    run_mysql_query(settings, "acore_world", sql)


def ensure_probe_identity(
    settings: dict[str, str | int],
    *,
    spec_name: str,
    level: int,
    last_seen_zone: int,
) -> int:
    spec = SPEC_CONFIG[spec_name]
    identity_id = ensure_identity_record(
        settings,
        name=str(spec["identity_name"]),
        class_id=int(spec["class_id"]),
        spec_key=str(spec["spec_key"]),
        loadout_key=str(spec["loadout_key"]),
        level=level,
        faction=int(spec["faction"]),
        race_id=int(spec["race_id"]),
        display_id=int(spec["display_id"]),
        gender=int(spec["gender"]),
        home_zone_id=1519,
        home_anchor_point_key="stormwind_inn",
        home_bind_point_key="stormwind_inn",
        last_seen_zone=last_seen_zone,
    )

    # This harness is meant to validate fresh-80 Druid spec surfaces, so pin the
    # probe identities to the stage-0 / tier-1 rung instead of the hotter tier-3
    # defaults many other combat harnesses use.
    run_mysql_query(
        settings,
        str(settings["characters_db"]),
        (
            "UPDATE living_world_bot_identity SET "
            "gear_tier = 1, gear_refresh_pending = 1, last_gear_refresh_band = 0, "
            "world_online_ms_since_level = 0, post_max_world_online_ms = 0, "
            "active_world_session_ms = 0, total_world_online_ms = 0 "
            f"WHERE id = {identity_id}"
        ),
    )

    return identity_id


def collect_focus_rows(
    settings: dict[str, str | int],
    baseline_id: int,
    identity_id: int,
) -> dict[str, list[tuple[str, ...]]]:
    return {
        "counts": run_mysql_query(
            settings,
            str(settings["characters_db"]),
            (
                "SELECT event_type, COUNT(*) FROM living_world_bot_activity_log "
                f"WHERE id > {baseline_id} AND bot_guid = {identity_id} "
                "GROUP BY event_type ORDER BY COUNT(*) DESC, event_type ASC"
            ),
        ),
        "focus": run_mysql_query(
            settings,
            str(settings["characters_db"]),
            (
                "SELECT id, bot_name, event_type, zone_id, detail "
                "FROM living_world_bot_activity_log "
                f"WHERE id > {baseline_id} AND bot_guid = {identity_id} "
                "AND event_type IN ('build_prepared', 'build_prepare_failed', 'spellbook_snapshot', "
                "'self_state', 'resource_snapshot', 'weapon_loadout_snapshot', 'ooc_buff') "
                "ORDER BY id ASC"
            ),
        ),
        "identity": run_mysql_query(
            settings,
            str(settings["characters_db"]),
            (
                "SELECT id, name, class_id, spec_key, loadout_key, level, runtime_state, runtime_detail "
                "FROM living_world_bot_identity "
                f"WHERE id = {identity_id} LIMIT 1"
            ),
        ),
    }


def write_report(
    report_path: pathlib.Path,
    *,
    spec_reports: list[dict[str, object]],
) -> None:
    with report_path.open("w", encoding="utf-8") as handle:
        handle.write(f"report_generated_at={dt.datetime.now().isoformat()}\n\n")
        for spec_report in spec_reports:
            spec_name = str(spec_report["spec_name"])
            spec_cfg = SPEC_CONFIG[spec_name]
            handle.write(f"=== SPEC {spec_name.upper()} ===\n")
            handle.write(f"expected_idle_form={spec_cfg['expected_form_idle']}\n")
            handle.write(f"expected_idle_loadout={spec_cfg['expected_loadout']}\n")
            handle.write(f"identity_id={spec_report['identity_id']}\n\n")
            for context_report in spec_report["contexts"]:
                context = str(context_report["context"])
                stdout_path = pathlib.Path(str(context_report["stdout"]))
                stderr_path = pathlib.Path(str(context_report["stderr"]))
                rows = context_report["rows"]
                handle.write(f"[context:{context}]\n")
                handle.write(f"stdout_log={stdout_path.name}\n")
                handle.write(f"stderr_log={stderr_path.name}\n")
                handle.write("[identity]\n")
                for row in rows["identity"]:
                    handle.write("\t".join(str(value) for value in row) + "\n")
                handle.write("[event_counts]\n")
                for row in rows["counts"]:
                    handle.write("\t".join(str(value) for value in row) + "\n")
                handle.write("[focus_rows]\n")
                for row in rows["focus"]:
                    handle.write("\t".join(str(value) for value in row) + "\n")
                handle.write("\n")


def main() -> int:
    args = parse_args()
    specs = parse_specs(args.specs)
    contexts = parse_contexts(args.contexts)
    ensure_paths()
    settings = load_db_settings()
    ensure_druid_test_profiles(settings)

    timestamp = dt.datetime.now().strftime("%Y%m%d-%H%M%S")
    prefix = REPO_ROOT / f"druid-form-harness-{timestamp}"
    report_path = prefix.with_suffix(".report.txt")

    original_config = MODULE_CONF.read_text(encoding="utf-8-sig")
    spec_reports: list[dict[str, object]] = []

    try:
        for spec_name in specs:
            identity_id = ensure_probe_identity(
                settings,
                spec_name=spec_name,
                level=args.level,
                last_seen_zone=args.zone_id,
            )

            context_reports: list[dict[str, object]] = []
            for context in contexts:
                stdout_path = prefix.with_name(prefix.name + f".{spec_name}.{context}.stdout.log")
                stderr_path = prefix.with_name(prefix.name + f".{spec_name}.{context}.stderr.log")

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
                    "LivingWorld.DebugForceTravelStateIdentityId": str(identity_id if context == "travel" else 0),
                    "LivingWorld.DebugSuppressTravelStateIdentityId": str(identity_id if context == "idle" else 0),
                }

                baseline_id = query_scalar(
                    settings,
                    str(settings["characters_db"]),
                    "SELECT COALESCE(MAX(id), 0) FROM living_world_bot_activity_log",
                )

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

                context_reports.append(
                    {
                        "context": context,
                        "stdout": stdout_path,
                        "stderr": stderr_path,
                        "rows": collect_focus_rows(settings, baseline_id, identity_id),
                    }
                )

            spec_reports.append(
                {
                    "spec_name": spec_name,
                    "identity_id": identity_id,
                    "contexts": context_reports,
                }
            )

    finally:
        MODULE_CONF.write_text(original_config, encoding="utf-8-sig")

    write_report(report_path, spec_reports=spec_reports)
    print(report_path)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:  # pragma: no cover
        print(f"ERROR: {exc}", file=sys.stderr)
        raise
