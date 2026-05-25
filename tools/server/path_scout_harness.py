#!/usr/bin/env python3
"""Launch a small dispersed pack of debug path scouts and dump their walked breadcrumbs.

Unlike the route travel harness, this tool is meant for missing-road discovery.
It enables the server-side `debug_path_scout` mode, which asks core pathfinding
for a real ground path up front and rejects runs that only degrade to direct/
shortcut fallback movement.
"""

from __future__ import annotations

import argparse
import atexit
import csv
import datetime as dt
import json
import pathlib
import signal
import subprocess
import time

import route_travel_harness as rth


REPO_ROOT = pathlib.Path(__file__).resolve().parents[2]
BUILD_ROOT = REPO_ROOT / "out" / "build-vs2022" / "bin" / "Debug"
WORLD_EXE = BUILD_ROOT / "worldserver.exe"
MODULE_CONF = BUILD_ROOT / "configs" / "modules" / "mod-living-world.conf"
MODULE_CONF_BACKUP = BUILD_ROOT / "configs" / "modules" / "mod-living-world.conf.route-harness.bak"

TRACE_EVENTS = {
    "travel_start",
    "travel_plan",
    "travel_waypoint",
    "position_tick",
    "travel_arrive",
    "travel_stuck",
    "travel_timeout",
    "travel_scout_rejected",
    "session_abort",
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--count", type=int, default=4, help="How many scouts to spawn")
    parser.add_argument("--level", type=int, default=80, help="Nominal top scout level")
    parser.add_argument("--race-id", type=int, default=1)
    parser.add_argument("--class-id", type=int, default=1)
    parser.add_argument("--gender", type=int, default=0)
    parser.add_argument("--map-id", type=int, default=0)
    parser.add_argument("--start-x", type=float, required=True)
    parser.add_argument("--start-y", type=float, required=True)
    parser.add_argument("--start-z", type=float, default=0.0)
    parser.add_argument("--dest-zone-id", type=int, required=True)
    parser.add_argument("--dest-x", type=float, required=True)
    parser.add_argument("--dest-y", type=float, required=True)
    parser.add_argument("--dest-z", type=float, default=0.0)
    parser.add_argument("--spacing-yards", type=float, default=5.0)
    parser.add_argument("--interest-zone-id", type=int, default=0, help="Synthetic hot zone; defaults to destination zone")
    parser.add_argument("--interest-switch-zone-id", type=int, default=0)
    parser.add_argument("--interest-switch-ms", type=int, default=0)
    parser.add_argument("--explored-zone-ids", default="", help="Comma-separated explored zones to seed onto each scout")
    parser.add_argument("--idle-seconds", type=int, default=20)
    parser.add_argument("--seconds", type=int, default=300)
    parser.add_argument("--startup-timeout", type=int, default=180)
    return parser.parse_args()


def build_levels(level: int, count: int) -> list[int]:
    clamped_level = max(1, min(80, level))
    clamped_count = max(1, count)
    levels: list[int] = []
    for offset in range(clamped_count):
        candidate = max(1, clamped_level - (clamped_count - 1) + offset)
        if candidate not in levels:
            levels.append(candidate)
    return levels


def write_trace_csv(path: pathlib.Path, rows: list[tuple[str, ...]]) -> None:
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.writer(handle)
        writer.writerow(["bot_name", "event_type", "detail", "map_id", "zone_id", "pos_x", "pos_y", "pos_z"])
        for row in rows:
            if row[1] in TRACE_EVENTS:
                writer.writerow(row)


def write_trace_json(path: pathlib.Path, rows: list[tuple[str, ...]]) -> None:
    payload = []
    for row in rows:
        if row[1] not in TRACE_EVENTS:
            continue
        payload.append(
            {
                "bot_name": row[0],
                "event_type": row[1],
                "detail": row[2],
                "map_id": int(row[3]),
                "zone_id": int(row[4]),
                "world_x": float(row[5]),
                "world_y": float(row[6]),
                "world_z": float(row[7]),
            }
        )
    path.write_text(json.dumps(payload, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def extract_path_points(rows: list[tuple[str, ...]], bot_name: str) -> list[dict[str, object]]:
    points: list[dict[str, object]] = []
    last_key: tuple[int, int, float, float, float] | None = None
    for row in rows:
        if row[0] != bot_name or row[1] not in TRACE_EVENTS:
            continue
        point = {
            "bot_name": row[0],
            "event_type": row[1],
            "detail": row[2],
            "map_id": int(row[3]),
            "zone_id": int(row[4]),
            "world_x": float(row[5]),
            "world_y": float(row[6]),
            "world_z": float(row[7]),
        }
        key = (
            point["map_id"],
            point["zone_id"],
            round(point["world_x"], 3),
            round(point["world_y"], 3),
            round(point["world_z"], 3),
        )
        if key == last_key:
            continue
        points.append(point)
        last_key = key
    return points


def write_path_points_csv(path: pathlib.Path, points: list[dict[str, object]]) -> None:
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.writer(handle)
        writer.writerow(["index", "event_type", "map_id", "zone_id", "world_x", "world_y", "world_z", "detail"])
        for index, point in enumerate(points):
            writer.writerow(
                [
                    index,
                    point["event_type"],
                    point["map_id"],
                    point["zone_id"],
                    point["world_x"],
                    point["world_y"],
                    point["world_z"],
                    point["detail"],
                ]
            )


def write_path_points_json(path: pathlib.Path, args: argparse.Namespace, points: list[dict[str, object]]) -> None:
    payload = {
        "format": "lw_path_scout_points",
        "generated_at": dt.datetime.now().isoformat(),
        "start": {
            "map_id": args.map_id,
            "world_x": args.start_x,
            "world_y": args.start_y,
            "world_z": args.start_z,
        },
        "destination": {
            "zone_id": args.dest_zone_id,
            "world_x": args.dest_x,
            "world_y": args.dest_y,
            "world_z": args.dest_z,
        },
        "points": points,
    }
    path.write_text(json.dumps(payload, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def classify_bot_mode(rows: list[tuple[str, ...]], bot_name: str) -> str:
    bot_rows = [row for row in rows if row[0] == bot_name]
    if any(row[1] == "travel_scout_rejected" for row in bot_rows):
        return "rejected"
    if any(row[1] == "travel_plan" and "scout nav path" in row[2] for row in bot_rows):
        return "nav_path"
    if any(row[1] == "travel_start" and "direct target=" in row[2] for row in bot_rows):
        return "direct_fallback"
    return "unknown"


def build_report(
    report_path: pathlib.Path,
    stdout_path: pathlib.Path,
    stderr_path: pathlib.Path,
    identity_rows: list[tuple[str, ...]],
    activity_rows: list[tuple[str, ...]],
    names: list[str],
) -> None:
    lines: list[str] = []
    lines.append(f"Path scout harness report generated {dt.datetime.now().isoformat()}")
    lines.append(f"stdout: {stdout_path}")
    lines.append(f"stderr: {stderr_path}")
    lines.append("")

    if identity_rows:
        lines.append("[identities]")
        for row in identity_rows:
            lines.append(
                "  "
                f"name={row[0]} level={row[1]} is_available={row[2]} "
                f"session_count={row[3]} active_world_session_ms={row[4]} "
                f"last_seen_zone={row[5]} runtime_state={row[6]} runtime_detail={row[7]}"
            )
        lines.append("")

    for name in names:
        bot_rows = [row for row in activity_rows if row[0] == name]
        mode = classify_bot_mode(activity_rows, name)
        points = extract_path_points(activity_rows, name)
        lines.append(f"[{name}] mode={mode} points={len(points)}")
        for event_type in ("travel_plan", "travel_scout_rejected", "travel_start", "travel_waypoint", "travel_arrive", "travel_stuck", "travel_timeout", "session_abort"):
            matching = [row for row in bot_rows if row[1] == event_type]
            if not matching:
                continue
            lines.append(f"  {event_type}: {len(matching)}")
            for row in matching[:6]:
                lines.append(
                    f"    zone={row[4]} pos=({row[5]},{row[6]},{row[7]}) detail={row[2]}"
                )
        lines.append("")

    report_path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    args = parse_args()
    levels = build_levels(args.level, args.count)
    names = [f"RouteHarnessL{level}" for level in levels]
    rth.ensure_paths()
    settings = rth.load_db_settings()

    timestamp = dt.datetime.now().strftime("%Y%m%d-%H%M%S")
    prefix = REPO_ROOT / f"path-scout-{timestamp}"
    stdout_path = prefix.with_suffix(".stdout.log")
    stderr_path = prefix.with_suffix(".stderr.log")
    report_path = prefix.with_suffix(".report.txt")
    trace_csv_path = prefix.with_suffix(".trace.csv")
    trace_json_path = prefix.with_suffix(".trace.json")

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
        "LivingWorld.DebugSyntheticInterestSwitchMapId": str(args.map_id),
        "LivingWorld.DebugSyntheticInterestSwitchZoneId": str(args.interest_switch_zone_id),
        "LivingWorld.DebugSyntheticInterestSwitchMs": str(args.interest_switch_ms),
        "LivingWorld.DebugSyntheticInterestClearMs": "0",
        "LivingWorld.DebugHotZoneCooldownMs": "90000",
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
        "LivingWorld.DebugRouteHarnessMode": "\"path_scout\"",
        "LivingWorld.DebugRouteHarnessSpacingYards": str(args.spacing_yards),
        "LivingWorld.DebugRouteHarnessTransitRouteKey": "\"\"",
        "LivingWorld.DebugRouteHarnessExploredZones": f"\"{args.explored_zone_ids}\"",
        "LivingWorld.DebugRouteHarnessBakeRouteZ": "0",
        "LivingWorld.DebugRouteHarnessBakeZoneIds": "\"\"",
        "LivingWorld.DebugRouteHarnessIdleDurationSec": str(args.idle_seconds),
        "LivingWorld.RouteExportDir": f"\"{(REPO_ROOT / 'tools' / 'lw-zone-editor' / 'data' / 'exported_routes').as_posix()}\"",
        "LivingWorld.RouteExportBakeZOnStartup": "0",
    }

    rth.clear_prior_rows(settings, names)
    MODULE_CONF.write_text(rth.rewrite_module_config(original_conf, updates), encoding="utf-8")

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

        rth.wait_for_server_ready(process, stdout_path, stderr_path, settings, names, args.startup_timeout)
        any_activity = rth.wait_for_any_activity_rows(settings, names, max(30, args.startup_timeout // 2))
        if not any_activity:
            raise RuntimeError("Path scouts did not emit any activity rows before timeout.")
        progressed = rth.wait_for_progress_rows(settings, names, max(45, args.startup_timeout // 2))
        if not progressed:
            raise RuntimeError("Path scouts spawned but never emitted travel progress rows.")

        time.sleep(args.seconds)
        identity_rows = rth.fetch_identity_rows(settings, names)
        activity_rows = rth.fetch_activity_rows(settings, names)

        build_report(report_path, stdout_path, stderr_path, identity_rows, activity_rows, names)
        write_trace_csv(trace_csv_path, activity_rows)
        write_trace_json(trace_json_path, activity_rows)

        for name in names:
            points = extract_path_points(activity_rows, name)
            safe_name = name.replace(" ", "_")
            write_path_points_csv(prefix.with_name(f"{prefix.name}.{safe_name}.path_points.csv"), points)
            write_path_points_json(prefix.with_name(f"{prefix.name}.{safe_name}.path_points.json"), args, points)

        print(report_path)
        return 0
    finally:
        cleanup()


if __name__ == "__main__":
    raise SystemExit(main())
