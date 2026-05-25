#!/usr/bin/env python3
"""Probe a transition seam by routing a levelled debug harness bot between two authored route anchors.

This harness resolves a start/end anchor from the editor/runtime route JSON, launches
the existing debug route harness, and records the materialized travel trace so obscured
zone seams can be validated against actual in-world movement.
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
from dataclasses import dataclass

import route_travel_harness as rth


REPO_ROOT = pathlib.Path(__file__).resolve().parents[2]
EDITOR_ROUTES_DIR = REPO_ROOT / "tools" / "lw-zone-editor" / "data" / "editor_routes"
RUNTIME_ROUTES_DIR = REPO_ROOT / "tools" / "lw-zone-editor" / "data" / "exported_routes"

SUCCESS_EVENTS = {"travel_arrive", "session_complete", "activity_complete"}
FAIL_EVENTS = {"travel_timeout", "travel_stuck", "session_abort"}
TRACE_EVENTS = {"travel_start", "travel_plan", "travel_waypoint", "position_tick", "travel_arrive", "travel_stuck", "travel_timeout", "session_abort"}


@dataclass(frozen=True, slots=True)
class AnchorRef:
    zone_id: int
    path_key: str
    anchor_index: int


@dataclass(frozen=True, slots=True)
class AnchorNode:
    source_path: pathlib.Path
    map_id: int
    zone_id: int
    zone_name: str
    path_key: str
    anchor_index: int
    world_x: float
    world_y: float
    world_z: float
    target_zone_id: int | None


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--from-zone-id", type=int, required=True)
    parser.add_argument("--from-path-key", required=True)
    parser.add_argument("--from-anchor-index", type=int, required=True, help="Zero-based anchor index")
    parser.add_argument("--to-zone-id", type=int, required=True)
    parser.add_argument("--to-path-key", required=True)
    parser.add_argument("--to-anchor-index", type=int, required=True, help="Zero-based anchor index")
    parser.add_argument("--level", type=int, default=80)
    parser.add_argument("--race-id", type=int, default=1)
    parser.add_argument("--class-id", type=int, default=1)
    parser.add_argument("--gender", type=int, default=0)
    parser.add_argument("--interest-zone-id", type=int, default=0, help="Synthetic hot zone at run start; defaults to from-zone")
    parser.add_argument("--interest-switch-zone-id", type=int, default=0, help="Optional synthetic hot zone switch target")
    parser.add_argument("--interest-switch-ms", type=int, default=0, help="Optional synthetic hot zone switch delay")
    parser.add_argument("--linger-ms", type=int, default=90000, help="Hot->cold linger in milliseconds")
    parser.add_argument("--seconds", type=int, default=240, help="Maximum run time after progress starts")
    parser.add_argument("--startup-timeout", type=int, default=120)
    parser.add_argument("--idle-seconds", type=int, default=15, help="Idle hold after reaching destination")
    parser.add_argument("--explored-zone-ids", default="", help="Comma-separated explored zones to seed onto the debug bot")
    return parser.parse_args()


def iter_route_files() -> list[pathlib.Path]:
    files: list[pathlib.Path] = []
    for root in (EDITOR_ROUTES_DIR, RUNTIME_ROUTES_DIR):
        if not root.exists():
            continue
        files.extend(sorted(root.glob("*.json"), key=lambda path: path.name.lower()))
    return files


def load_anchor(ref: AnchorRef) -> AnchorNode:
    best_candidate: AnchorNode | None = None
    best_preference = -1
    for route_path in iter_route_files():
        if route_path.name.endswith("__connectors.json"):
            continue
        try:
            payload = json.loads(route_path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            continue
        if not isinstance(payload, dict):
            continue
        if int(payload.get("zone_id", 0) or 0) != ref.zone_id:
            continue
        map_id = int(payload.get("map_id", 0) or 0)
        zone_name = str(payload.get("zone_name", "") or f"zone_{ref.zone_id}")
        paths = payload.get("paths", [])
        if not isinstance(paths, list):
            continue
        for path_payload in paths:
            if not isinstance(path_payload, dict):
                continue
            if str(path_payload.get("path_key", "")).strip() != ref.path_key:
                continue
            anchors = path_payload.get("anchors", [])
            if not isinstance(anchors, list):
                continue
            if not (0 <= ref.anchor_index < len(anchors)):
                raise ValueError(
                    f"Anchor index {ref.anchor_index} is out of range for {ref.path_key} in zone {ref.zone_id}."
                )
            anchor = anchors[ref.anchor_index]
            if not isinstance(anchor, dict):
                raise ValueError(
                    f"Anchor index {ref.anchor_index} in {ref.path_key} zone {ref.zone_id} is not a valid anchor object."
                )
            transition = anchor.get("transition_node")
            target_zone_id = None
            if isinstance(transition, dict):
                raw_target = transition.get("target_zone_id")
                if raw_target not in (None, ""):
                    try:
                        target_zone_id = int(raw_target)
                    except (TypeError, ValueError):
                        target_zone_id = None
            candidate = AnchorNode(
                source_path=route_path,
                map_id=map_id,
                zone_id=ref.zone_id,
                zone_name=zone_name,
                path_key=ref.path_key,
                anchor_index=ref.anchor_index,
                world_x=float(anchor.get("world_x")),
                world_y=float(anchor.get("world_y")),
                world_z=float(anchor.get("world_z", 0.0)),
                target_zone_id=target_zone_id,
            )
            preference = 2 if route_path.name.endswith("__editor.json") else 1 if route_path.name.endswith("__routes.json") else 0
            if preference > best_preference:
                best_candidate = candidate
                best_preference = preference
    if best_candidate is None:
        raise FileNotFoundError(
            f"Could not resolve anchor {ref.zone_id}:{ref.path_key}:{ref.anchor_index} from editor/runtime route files."
        )
    return best_candidate


def fetch_rows_for_bot(settings: dict[str, str | int], bot_name: str) -> list[tuple[str, ...]]:
    return rth.run_mysql_query(
        settings,
        str(settings["characters_db"]),
        "SELECT bot_name, event_type, detail, map_id, zone_id, pos_x, pos_y, pos_z "
        "FROM living_world_bot_activity_log "
        f"WHERE bot_name = '{bot_name}' "
        "ORDER BY id ASC",
    )


def wait_for_terminal_event(settings: dict[str, str | int], bot_name: str, timeout_seconds: int) -> tuple[str | None, list[tuple[str, ...]]]:
    deadline = time.time() + timeout_seconds
    while time.time() < deadline:
        rows = fetch_rows_for_bot(settings, bot_name)
        for row in reversed(rows):
            event_type = row[1]
            if event_type in SUCCESS_EVENTS or event_type in FAIL_EVENTS:
                return event_type, rows
        time.sleep(1.0)
    return None, fetch_rows_for_bot(settings, bot_name)


def write_trace_csv(path: pathlib.Path, rows: list[tuple[str, ...]]) -> None:
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.writer(handle)
        writer.writerow(["bot_name", "event_type", "detail", "map_id", "zone_id", "pos_x", "pos_y", "pos_z"])
        for row in rows:
            if row[1] in TRACE_EVENTS:
                writer.writerow(row)


def extract_path_points(rows: list[tuple[str, ...]]) -> list[dict[str, object]]:
    points: list[dict[str, object]] = []
    last_key: tuple[int, int, float, float, float] | None = None
    for row in rows:
        event_type = row[1]
        if event_type not in TRACE_EVENTS:
            continue
        point = {
            "event_type": event_type,
            "detail": row[2],
            "map_id": int(row[3]),
            "zone_id": int(row[4]),
            "world_x": float(row[5]),
            "world_y": float(row[6]),
            "world_z": float(row[7]),
        }
        key = (
            int(point["map_id"]),
            int(point["zone_id"]),
            round(float(point["world_x"]), 3),
            round(float(point["world_y"]), 3),
            round(float(point["world_z"]), 3),
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


def write_path_points_json(path: pathlib.Path, *, start: AnchorNode, dest: AnchorNode, points: list[dict[str, object]]) -> None:
    payload = {
        "format": "lw_transition_probe_path_points",
        "generated_at": dt.datetime.now().isoformat(),
        "start": {
            "zone_id": start.zone_id,
            "path_key": start.path_key,
            "anchor_index": start.anchor_index,
            "world_x": start.world_x,
            "world_y": start.world_y,
            "world_z": start.world_z,
        },
        "destination": {
            "zone_id": dest.zone_id,
            "path_key": dest.path_key,
            "anchor_index": dest.anchor_index,
            "world_x": dest.world_x,
            "world_y": dest.world_y,
            "world_z": dest.world_z,
        },
        "points": points,
    }
    path.write_text(json.dumps(payload, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def write_trace_json(
    path: pathlib.Path,
    *,
    start: AnchorNode,
    dest: AnchorNode,
    terminal_event: str | None,
    rows: list[tuple[str, ...]],
) -> None:
    payload = {
        "format": "lw_transition_probe_trace",
        "generated_at": dt.datetime.now().isoformat(),
        "start": {
            "zone_id": start.zone_id,
            "zone_name": start.zone_name,
            "path_key": start.path_key,
            "anchor_index": start.anchor_index,
            "world_x": start.world_x,
            "world_y": start.world_y,
            "world_z": start.world_z,
            "target_zone_id": start.target_zone_id,
            "source_path": str(start.source_path),
        },
        "destination": {
            "zone_id": dest.zone_id,
            "zone_name": dest.zone_name,
            "path_key": dest.path_key,
            "anchor_index": dest.anchor_index,
            "world_x": dest.world_x,
            "world_y": dest.world_y,
            "world_z": dest.world_z,
            "target_zone_id": dest.target_zone_id,
            "source_path": str(dest.source_path),
        },
        "terminal_event": terminal_event or "",
        "points": [
            {
                "event_type": row[1],
                "detail": row[2],
                "map_id": int(row[3]),
                "zone_id": int(row[4]),
                "world_x": float(row[5]),
                "world_y": float(row[6]),
                "world_z": float(row[7]),
            }
            for row in rows
            if row[1] in TRACE_EVENTS
        ],
    }
    path.write_text(json.dumps(payload, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def build_probe_report(
    report_path: pathlib.Path,
    *,
    start: AnchorNode,
    dest: AnchorNode,
    terminal_event: str | None,
    stdout_path: pathlib.Path,
    stderr_path: pathlib.Path,
    trace_csv_path: pathlib.Path,
    trace_json_path: pathlib.Path,
    path_points_csv_path: pathlib.Path,
    path_points_json_path: pathlib.Path,
    rows: list[tuple[str, ...]],
) -> None:
    lines: list[str] = []
    lines.append(f"Transition probe generated {dt.datetime.now().isoformat()}")
    lines.append(f"start: {start.zone_id}:{start.path_key}:{start.anchor_index} @ ({start.world_x:.2f}, {start.world_y:.2f}, {start.world_z:.2f})")
    lines.append(f"dest:  {dest.zone_id}:{dest.path_key}:{dest.anchor_index} @ ({dest.world_x:.2f}, {dest.world_y:.2f}, {dest.world_z:.2f})")
    lines.append(f"terminal_event: {terminal_event or 'timeout'}")
    lines.append(f"stdout: {stdout_path}")
    lines.append(f"stderr: {stderr_path}")
    lines.append(f"trace csv: {trace_csv_path}")
    lines.append(f"trace json: {trace_json_path}")
    lines.append(f"path points csv: {path_points_csv_path}")
    lines.append(f"path points json: {path_points_json_path}")
    lines.append("")
    for row in rows:
        if row[1] not in TRACE_EVENTS:
            continue
        lines.append(
            f"{row[1]:>16} zone={row[4]} pos=({row[5]},{row[6]},{row[7]}) detail={row[2]}"
        )
    report_path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    args = parse_args()
    start_ref = AnchorRef(args.from_zone_id, args.from_path_key, args.from_anchor_index)
    dest_ref = AnchorRef(args.to_zone_id, args.to_path_key, args.to_anchor_index)
    start = load_anchor(start_ref)
    dest = load_anchor(dest_ref)
    if start.map_id != dest.map_id:
        raise RuntimeError(
            f"Start map {start.map_id} and destination map {dest.map_id} differ; this probe only supports same-map seams."
        )

    settings = rth.load_db_settings()
    rth.ensure_paths()

    timestamp = dt.datetime.now().strftime("%Y%m%d-%H%M%S")
    prefix = REPO_ROOT / f"transition-probe-{timestamp}"
    stdout_path = prefix.with_suffix(".stdout.log")
    stderr_path = prefix.with_suffix(".stderr.log")
    report_path = prefix.with_suffix(".report.txt")
    trace_csv_path = prefix.with_suffix(".trace.csv")
    trace_json_path = prefix.with_suffix(".trace.json")
    path_points_csv_path = prefix.with_suffix(".path_points.csv")
    path_points_json_path = prefix.with_suffix(".path_points.json")

    original_conf = rth.MODULE_CONF.read_text(encoding="utf-8")
    if rth.MODULE_CONF_BACKUP.exists():
        rth.MODULE_CONF.write_text(rth.MODULE_CONF_BACKUP.read_text(encoding="utf-8"), encoding="utf-8")
        rth.MODULE_CONF_BACKUP.unlink()
    rth.MODULE_CONF_BACKUP.write_text(original_conf, encoding="utf-8")

    interest_zone_id = args.interest_zone_id or start.zone_id
    updates = {
        "LivingWorld.AmbientPopulation": "0",
        "LivingWorld.AmbientForceSpawnCount": "0",
        "LivingWorld.DebugSyntheticInterestEnabled": "1",
        "LivingWorld.DebugSyntheticInterestMapId": str(start.map_id),
        "LivingWorld.DebugSyntheticInterestZoneId": str(interest_zone_id),
        "LivingWorld.DebugSyntheticInterestSwitchMapId": str(start.map_id if args.interest_switch_zone_id else 0),
        "LivingWorld.DebugSyntheticInterestSwitchZoneId": str(args.interest_switch_zone_id),
        "LivingWorld.DebugSyntheticInterestSwitchMs": str(args.interest_switch_ms),
        "LivingWorld.DebugSyntheticInterestClearMs": "0",
        "LivingWorld.DebugHotZoneCooldownMs": str(max(0, args.linger_ms)),
        "LivingWorld.DebugForceIdentityIds": "\"\"",
        "LivingWorld.DebugForceSessionZoneId": "0",
        "LivingWorld.DebugForceSessionComposeAttempts": "24",
        "LivingWorld.DebugRouteHarnessEnabled": "1",
        "LivingWorld.DebugRouteHarnessLevels": f"\"{args.level}\"",
        "LivingWorld.DebugRouteHarnessRaceId": str(args.race_id),
        "LivingWorld.DebugRouteHarnessClassId": str(args.class_id),
        "LivingWorld.DebugRouteHarnessGender": str(args.gender),
        "LivingWorld.DebugRouteHarnessMapId": str(start.map_id),
        "LivingWorld.DebugRouteHarnessStartX": f"{start.world_x}",
        "LivingWorld.DebugRouteHarnessStartY": f"{start.world_y}",
        "LivingWorld.DebugRouteHarnessStartZ": f"{start.world_z}",
        "LivingWorld.DebugRouteHarnessDestZoneId": str(dest.zone_id),
        "LivingWorld.DebugRouteHarnessDestX": f"{dest.world_x}",
        "LivingWorld.DebugRouteHarnessDestY": f"{dest.world_y}",
        "LivingWorld.DebugRouteHarnessDestZ": f"{dest.world_z}",
        "LivingWorld.DebugRouteHarnessTransitRouteKey": "\"\"",
        "LivingWorld.DebugRouteHarnessExploredZones": f"\"{args.explored_zone_ids}\"",
        "LivingWorld.DebugRouteHarnessBakeRouteZ": "0",
        "LivingWorld.DebugRouteHarnessBakeZoneIds": "\"\"",
        "LivingWorld.DebugRouteHarnessIdleDurationSec": str(args.idle_seconds),
        "LivingWorld.RouteExportDir": f"\"{(REPO_ROOT / 'tools' / 'lw-zone-editor' / 'data' / 'exported_routes').as_posix()}\"",
        "LivingWorld.RouteExportBakeZOnStartup": "0",
    }

    bot_name = f"RouteHarnessL{args.level}"
    rth.clear_prior_rows(settings, [bot_name])
    rth.MODULE_CONF.write_text(rth.rewrite_module_config(original_conf, updates), encoding="utf-8")

    process: subprocess.Popen[str] | None = None
    restored = False

    def cleanup() -> None:
        nonlocal process, restored
        if not restored:
            rth.MODULE_CONF.write_text(original_conf, encoding="utf-8")
            if rth.MODULE_CONF_BACKUP.exists():
                rth.MODULE_CONF_BACKUP.unlink()
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
            [str(rth.WORLD_EXE)],
            cwd=str(rth.BUILD_ROOT),
            stdout=stdout_handle,
            stderr=stderr_handle,
            text=True,
        )

        rth.wait_for_server_ready(process, stdout_path, stderr_path, settings, [bot_name], args.startup_timeout)
        if not rth.wait_for_any_activity_rows(settings, [bot_name], max(30, args.startup_timeout // 2)):
            raise RuntimeError("Transition probe bot did not emit any activity rows before timeout.")
        if not rth.wait_for_progress_rows(settings, [bot_name], max(45, args.startup_timeout // 2)):
            raise RuntimeError("Transition probe bot did not emit travel progress rows before timeout.")

        terminal_event, rows = wait_for_terminal_event(settings, bot_name, args.seconds)
        path_points = extract_path_points(rows)
        write_trace_csv(trace_csv_path, rows)
        write_trace_json(trace_json_path, start=start, dest=dest, terminal_event=terminal_event, rows=rows)
        write_path_points_csv(path_points_csv_path, path_points)
        write_path_points_json(path_points_json_path, start=start, dest=dest, points=path_points)
        build_probe_report(
            report_path,
            start=start,
            dest=dest,
            terminal_event=terminal_event,
            stdout_path=stdout_path,
            stderr_path=stderr_path,
            trace_csv_path=trace_csv_path,
            trace_json_path=trace_json_path,
            path_points_csv_path=path_points_csv_path,
            path_points_json_path=path_points_json_path,
            rows=rows,
        )
        print(report_path)
        return 0
    finally:
        cleanup()


if __name__ == "__main__":
    raise SystemExit(main())
