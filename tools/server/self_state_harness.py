#!/usr/bin/env python3
"""Probe world-bot self-state choices across controlled runtime contexts.

This harness is meant to answer:
- what self-state does the bot choose while idle?
- what self-state does the bot choose while treated as traveling?

It runs one identity through one or more short headless worldserver passes and
captures Living World activity rows such as:
- build_prepared
- self_state
- ooc_buff
- spellbook_snapshot
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
    "druid_balance": {
        "class_id": 11,
        "race_id": 4,
        "display_id": 37919,
        "gender": 0,
        "faction": 1,
    },
    "druid_feral": {
        "class_id": 11,
        "race_id": 4,
        "display_id": 37919,
        "gender": 0,
        "faction": 1,
    },
    "druid_resto": {
        "class_id": 11,
        "race_id": 4,
        "display_id": 37919,
        "gender": 0,
        "faction": 1,
    },
    "warrior_arms": {
        "class_id": 1,
        "race_id": 1,
        "display_id": 49,
        "gender": 0,
        "faction": 1,
    },
    "warrior_fury": {
        "class_id": 1,
        "race_id": 1,
        "display_id": 49,
        "gender": 0,
        "faction": 1,
    },
    "warrior_prot": {
        "class_id": 1,
        "race_id": 1,
        "display_id": 49,
        "gender": 0,
        "faction": 1,
    },
    "dk_blood": {
        "class_id": 6,
        "race_id": 1,
        "display_id": 49,
        "gender": 0,
        "faction": 1,
    },
    "dk_frost": {
        "class_id": 6,
        "race_id": 1,
        "display_id": 49,
        "gender": 0,
        "faction": 1,
    },
    "dk_unholy": {
        "class_id": 6,
        "race_id": 1,
        "display_id": 49,
        "gender": 0,
        "faction": 1,
    },
    "hunter_bm": {
        "class_id": 3,
        "race_id": 4,
        "display_id": 51,
        "gender": 0,
        "faction": 1,
    },
    "hunter_mm": {
        "class_id": 3,
        "race_id": 4,
        "display_id": 51,
        "gender": 0,
        "faction": 1,
    },
    "hunter_sv": {
        "class_id": 3,
        "race_id": 4,
        "display_id": 51,
        "gender": 0,
        "faction": 1,
    },
    "shaman_ele": {
        "class_id": 7,
        "race_id": 11,
        "display_id": 20323,
        "gender": 0,
        "faction": 1,
    },
    "shaman_enh": {
        "class_id": 7,
        "race_id": 11,
        "display_id": 20323,
        "gender": 0,
        "faction": 1,
    },
    "shaman_resto": {
        "class_id": 7,
        "race_id": 11,
        "display_id": 20323,
        "gender": 0,
        "faction": 1,
    },
    "warlock_afflic": {
        "class_id": 9,
        "race_id": 1,
        "display_id": 49,
        "gender": 0,
        "faction": 1,
    },
    "warlock_demo": {
        "class_id": 9,
        "race_id": 1,
        "display_id": 49,
        "gender": 0,
        "faction": 1,
    },
    "warlock_destro": {
        "class_id": 9,
        "race_id": 1,
        "display_id": 49,
        "gender": 0,
        "faction": 1,
    },
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--name", default="Selfstateprobe")
    parser.add_argument("--spec", choices=sorted(SPEC_CONFIG), required=True)
    parser.add_argument("--level", type=int, default=80)
    parser.add_argument("--contexts", default="idle,travel", help="Comma-separated contexts: idle,travel")
    parser.add_argument("--seconds", type=int, default=25)
    parser.add_argument("--startup-timeout", type=int, default=120)
    parser.add_argument("--zone-id", type=int, default=1519)
    parser.add_argument("--map-id", type=int, default=0)
    parser.add_argument("--spawn-x", type=float, default=-8772.5)
    parser.add_argument("--spawn-y", type=float, default=347.0)
    parser.add_argument("--spawn-z", type=float, default=101.1)
    parser.add_argument("--force-spawn-count", type=int, default=1)
    return parser.parse_args()


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


def ensure_probe_identity(
    settings: dict[str, str | int],
    *,
    name: str,
    spec_key: str,
    level: int,
    last_seen_zone: int,
) -> int:
    spec = SPEC_CONFIG[spec_key]
    return ensure_identity_record(
        settings,
        name=name,
        class_id=int(spec["class_id"]),
        spec_key=spec_key,
        loadout_key="",
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
                "AND event_type IN ('build_prepared', 'spellbook_snapshot', 'self_state', 'ooc_buff', "
                "'combat_enter', 'combat_exit', 'pet_control') "
                "ORDER BY id ASC"
            ),
        ),
        "identity": run_mysql_query(
            settings,
            str(settings["characters_db"]),
            (
                "SELECT id, name, class_id, spec_key, level, runtime_state, runtime_detail "
                "FROM living_world_bot_identity "
                f"WHERE id = {identity_id} LIMIT 1"
            ),
        ),
    }


def write_report(
    report_path: pathlib.Path,
    *,
    identity_id: int,
    name: str,
    spec_key: str,
    context_reports: list[dict[str, object]],
) -> None:
    with report_path.open("w", encoding="utf-8") as handle:
        handle.write(f"report_generated_at={dt.datetime.now().isoformat()}\n")
        handle.write(f"identity_id={identity_id}\n")
        handle.write(f"name={name}\n")
        handle.write(f"spec_key={spec_key}\n\n")

        for context_report in context_reports:
            context = str(context_report["context"])
            stdout_path = pathlib.Path(str(context_report["stdout"]))
            stderr_path = pathlib.Path(str(context_report["stderr"]))
            rows = context_report["rows"]
            handle.write(f"=== CONTEXT {context.upper()} ===\n")
            handle.write(f"stdout_log={stdout_path.name}\n")
            handle.write(f"stderr_log={stderr_path.name}\n\n")

            handle.write("[identity]\n")
            for row in rows["identity"]:
                handle.write("\t".join(str(value) for value in row) + "\n")
            handle.write("\n[event_counts]\n")
            for row in rows["counts"]:
                handle.write("\t".join(str(value) for value in row) + "\n")
            handle.write("\n[focus_rows]\n")
            for row in rows["focus"]:
                handle.write("\t".join(str(value) for value in row) + "\n")
            handle.write("\n")


def main() -> int:
    args = parse_args()
    contexts = parse_contexts(args.contexts)
    ensure_paths()
    settings = load_db_settings()
    identity_id = ensure_probe_identity(
        settings,
        name=args.name,
        spec_key=args.spec,
        level=args.level,
        last_seen_zone=args.zone_id,
    )

    timestamp = dt.datetime.now().strftime("%Y%m%d-%H%M%S")
    prefix = REPO_ROOT / f"self-state-harness-{args.spec}-{timestamp}"
    report_path = prefix.with_suffix(".report.txt")

    original_config = MODULE_CONF.read_text(encoding="utf-8-sig")
    context_reports: list[dict[str, object]] = []

    try:
        for context in contexts:
            stdout_path = prefix.with_name(prefix.name + f".{context}.stdout.log")
            stderr_path = prefix.with_name(prefix.name + f".{context}.stderr.log")

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

    finally:
        MODULE_CONF.write_text(original_config, encoding="utf-8-sig")

    write_report(
        report_path,
        identity_id=identity_id,
        name=args.name,
        spec_key=args.spec,
        context_reports=context_reports,
    )
    print(report_path)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:  # pragma: no cover
        print(f"ERROR: {exc}", file=sys.stderr)
        raise
