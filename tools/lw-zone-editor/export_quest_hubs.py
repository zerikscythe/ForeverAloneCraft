#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
import math
import re
import sys
from collections import Counter, defaultdict
from dataclasses import dataclass
from pathlib import Path
from typing import Any


APP_ROOT = Path(__file__).resolve().parent
if str(APP_ROOT) not in sys.path:
    sys.path.insert(0, str(APP_ROOT))

from lw_zone_editor.client_assets import (  # noqa: E402
    AREA_TABLE_DBC_PATH,
    WORLD_MAP_AREA_DBC_PATH,
    load_area_table_names,
    load_world_map_area_records,
)
from lw_zone_editor.marker_cache import DEFAULT_MARKER_CACHE_PATH, load_marker_cache  # noqa: E402


DEFAULT_OUTPUT_DIR = APP_ROOT / "data" / "quest_hubs"
DEFAULT_CLUSTER_RADIUS_YARDS = 60.0
MAX_TASK_AREA_RADIUS_YARDS = 220.0
MAX_TASK_AREAS_PER_HUB = 6
MAX_TASK_AREAS_PER_OVERLAY = 2
MAX_TASK_AREA_TARGET_ENTRIES = 6
SLUG_NON_ALNUM_RE = re.compile(r"[^a-z0-9]+")


@dataclass(frozen=True, slots=True)
class ZoneBounds:
    zone_id: int
    zone_name: str
    map_id: int
    left: float
    right: float
    top: float
    bottom: float

    def contains(self, map_id: int, world_x: float, world_y: float) -> bool:
        if self.map_id != map_id:
            return False
        min_x = min(self.left, self.right)
        max_x = max(self.left, self.right)
        min_y = min(self.top, self.bottom)
        max_y = max(self.top, self.bottom)
        return min_x <= world_y <= max_x and min_y <= world_x <= max_y


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Export per-zone quest hub runtime JSON from the lw-zone-editor marker cache."
    )
    parser.add_argument(
        "--marker-cache",
        default=str(DEFAULT_MARKER_CACHE_PATH),
        help="Path to world_markers.json.",
    )
    parser.add_argument(
        "--output-dir",
        default=str(DEFAULT_OUTPUT_DIR),
        help="Directory where per-zone quest hub JSON files will be written.",
    )
    parser.add_argument(
        "--zone-id",
        type=int,
        action="append",
        dest="zone_ids",
        help="Optional zone id filter. Can be passed multiple times.",
    )
    parser.add_argument(
        "--zone-name",
        action="append",
        dest="zone_names",
        help="Optional case-insensitive zone-name substring filter. Can be passed multiple times.",
    )
    parser.add_argument(
        "--cluster-radius",
        type=float,
        default=DEFAULT_CLUSTER_RADIUS_YARDS,
        help="Distance in yards for grouping nearby quest givers into one hub.",
    )
    return parser


def _slugify(value: str) -> str:
    lowered = value.strip().lower()
    lowered = SLUG_NON_ALNUM_RE.sub("_", lowered)
    return lowered.strip("_") or "zone"


def _distance_2d(left: tuple[float, float], right: tuple[float, float]) -> float:
    dx = left[0] - right[0]
    dy = left[1] - right[1]
    return math.sqrt((dx * dx) + (dy * dy))


def _round_position(map_id: int, world_x: float, world_y: float, world_z: float) -> dict[str, float | int]:
    return {
        "mapId": int(map_id),
        "x": round(float(world_x), 3),
        "y": round(float(world_y), 3),
        "z": round(float(world_z), 3),
    }


def _is_runtime_eligible_quest(quest: dict[str, Any]) -> bool:
    return not (
        bool(quest.get("is_event_quest", False))
        or bool(quest.get("is_daily_quest", False))
        or bool(quest.get("is_weekly_quest", False))
        or bool(quest.get("is_monthly_quest", False))
    )


def _load_zone_bounds() -> list[ZoneBounds]:
    area_names = load_area_table_names(AREA_TABLE_DBC_PATH)
    bounds: list[ZoneBounds] = []
    for record in load_world_map_area_records(WORLD_MAP_AREA_DBC_PATH):
        zone_id = int(record["zone_id"])
        zone_name = area_names.get(zone_id)
        if zone_id <= 0 or not zone_name:
            continue
        bounds.append(
            ZoneBounds(
                zone_id=zone_id,
                zone_name=zone_name,
                map_id=int(record["map_id"]),
                left=float(record["left"]),
                right=float(record["right"]),
                top=float(record["top"]),
                bottom=float(record["bottom"]),
            )
        )
    return bounds


def _index_zone_bounds(bounds: list[ZoneBounds]) -> dict[int, list[ZoneBounds]]:
    by_zone_id: dict[int, list[ZoneBounds]] = defaultdict(list)
    for entry in bounds:
        by_zone_id[entry.zone_id].append(entry)
    return by_zone_id


def _resolve_zone(bounds: list[ZoneBounds], map_id: int, world_x: float, world_y: float) -> ZoneBounds | None:
    matches = [entry for entry in bounds if entry.contains(map_id, world_x, world_y)]
    if not matches:
        return None
    matches.sort(
        key=lambda entry: (
            abs(entry.left - entry.right) * abs(entry.top - entry.bottom),
            entry.zone_id,
        )
    )
    return matches[0]


def _resolve_zone_from_quests(
    bounds_by_zone_id: dict[int, list[ZoneBounds]],
    map_id: int,
    quests: list[dict[str, Any]],
) -> ZoneBounds | None:
    quest_sort_counts: Counter[int] = Counter()
    for quest in quests:
        quest_sort_id = int(quest.get("quest_sort_id", 0) or 0)
        if quest_sort_id > 0:
            quest_sort_counts[quest_sort_id] += 1
    for zone_id, _count in quest_sort_counts.most_common():
        candidates = [
            entry for entry in bounds_by_zone_id.get(zone_id, [])
            if entry.map_id == map_id
        ]
        if candidates:
            candidates.sort(
                key=lambda entry: (
                    abs(entry.left - entry.right) * abs(entry.top - entry.bottom),
                    entry.zone_id,
                )
            )
            return candidates[0]
    return None


def _index_quest_routes(world_markers_payload: dict[str, Any]) -> tuple[dict[int, list[dict[str, Any]]], dict[int, set[int]]]:
    route_by_quest_id: dict[int, list[dict[str, Any]]] = defaultdict(list)
    giver_entries_by_quest_id: dict[int, set[int]] = defaultdict(set)
    for route in world_markers_payload.get("quest_route_graph", []):
        quest_id = int(route.get("quest_id", 0) or 0)
        giver_entry = int(route.get("giver_entry", 0) or 0)
        if quest_id <= 0 or giver_entry <= 0:
            continue
        route_by_quest_id[quest_id].append(route)
        giver_entries_by_quest_id[quest_id].add(giver_entry)
    return route_by_quest_id, giver_entries_by_quest_id


def _build_quest_giver_rows(marker_cache_path: Path, bounds: list[ZoneBounds]) -> tuple[list[dict[str, Any]], dict[str, Any]]:
    markers, payload = load_marker_cache(marker_cache_path)
    route_by_quest_id, giver_entries_by_quest_id = _index_quest_routes(payload)
    bounds_by_zone_id = _index_zone_bounds(bounds)
    rows: list[dict[str, Any]] = []
    for marker in markers:
        if marker.kind != "quest_giver":
            continue
        quests = [quest for quest in marker.metadata.get("quests", []) if _is_runtime_eligible_quest(quest)]
        if not quests:
            continue
        zone = _resolve_zone_from_quests(bounds_by_zone_id, marker.map_id, quests)
        if zone is None:
            zone = _resolve_zone(bounds, marker.map_id, marker.world_x, marker.world_y)
        if zone is None:
            continue
        rows.append(
            {
                "zone_id": zone.zone_id,
                "zone_name": zone.zone_name,
                "map_id": marker.map_id,
                "entry": int(marker.entry),
                "guid": int(marker.guid),
                "label": marker.label,
                "world_x": float(marker.world_x),
                "world_y": float(marker.world_y),
                "world_z": float(marker.world_z),
                "quests": quests,
                "route_by_quest_id": route_by_quest_id,
                "giver_entries_by_quest_id": giver_entries_by_quest_id,
            }
        )
    return rows, dict(payload.get("objective_area_index", {}))


def _cluster_zone_rows(zone_rows: list[dict[str, Any]], cluster_radius: float) -> list[list[dict[str, Any]]]:
    remaining = sorted(zone_rows, key=lambda row: (row["world_x"], row["world_y"], row["guid"]))
    clusters: list[list[dict[str, Any]]] = []
    while remaining:
        seed = remaining.pop(0)
        cluster = [seed]
        changed = True
        while changed:
            changed = False
            keep: list[dict[str, Any]] = []
            cluster_points = [(row["world_x"], row["world_y"]) for row in cluster]
            for candidate in remaining:
                candidate_point = (candidate["world_x"], candidate["world_y"])
                if any(_distance_2d(candidate_point, point) <= cluster_radius for point in cluster_points):
                    cluster.append(candidate)
                    cluster_points.append(candidate_point)
                    changed = True
                else:
                    keep.append(candidate)
            remaining = keep
        clusters.append(cluster)
    return clusters


def _quest_signature(row: dict[str, Any]) -> tuple[int, ...]:
    return tuple(
        sorted(
            int(quest.get("quest_id", 0) or 0)
            for quest in row.get("quests", [])
            if int(quest.get("quest_id", 0) or 0) > 0
        )
    )


def _should_skip_cluster_as_scattered_duplicate(
    cluster_rows: list[dict[str, Any]],
    repeated_signature_counts: Counter[tuple[int, tuple[int, ...]]],
) -> bool:
    if not cluster_rows:
        return True
    entries = {int(row["entry"]) for row in cluster_rows}
    signatures = {_quest_signature(row) for row in cluster_rows}
    if len(entries) != 1 or len(signatures) != 1:
        return False
    signature = next(iter(signatures))
    if len(signature) != 1:
        return False
    repeated_count = repeated_signature_counts[(next(iter(entries)), signature)]
    return repeated_count >= 3


def _choose_cluster_name(zone_name: str, rows: list[dict[str, Any]]) -> str:
    quest_labels = [row["label"] for row in rows if row.get("quests")]
    if not quest_labels:
        return f"{zone_name} Hub"
    counts = Counter(quest_labels)
    return counts.most_common(1)[0][0]


def _build_task_areas_for_cluster(
    cluster_rows: list[dict[str, Any]],
    objective_area_index: dict[str, Any],
    bounds: list[ZoneBounds],
    zone_id: int,
    zone_map_id: int,
) -> list[dict[str, Any]]:
    hub_center_x = sum(float(row["world_x"]) for row in cluster_rows) / len(cluster_rows)
    hub_center_y = sum(float(row["world_y"]) for row in cluster_rows) / len(cluster_rows)
    area_payloads: dict[tuple[str, str], dict[str, Any]] = {}
    for row in cluster_rows:
        for quest in row["quests"]:
            quest_id = int(quest.get("quest_id", 0) or 0)
            if quest_id <= 0:
                continue

            overlay_labels: dict[str, set[str]] = defaultdict(set)
            overlay_target_entries: dict[str, set[int]] = defaultdict(set)

            for requirement in quest.get("target_requirements", []):
                overlay_id = str(requirement.get("objective_overlay_id", "") or "")
                if not overlay_id:
                    continue
                name = str(requirement.get("name", "") or "").strip()
                if name:
                    overlay_labels[overlay_id].add(name)
                target_id = int(requirement.get("target_id", 0) or 0)
                if target_id > 0:
                    overlay_target_entries[overlay_id].add(target_id)

            for requirement in quest.get("item_requirements", []):
                overlay_id = str(requirement.get("objective_overlay_id", "") or "")
                if not overlay_id:
                    continue
                name = str(requirement.get("name", "") or "").strip()
                if name:
                    overlay_labels[overlay_id].add(name)
                for source in requirement.get("source_creatures", []):
                    source_entry = int(source.get("entry", 0) or 0)
                    if source_entry > 0:
                        overlay_target_entries[overlay_id].add(source_entry)
                    source_name = str(source.get("name", "") or "").strip()
                    if source_name:
                        overlay_labels[overlay_id].add(source_name)

            for overlay_id in quest.get("objective_overlay_ids", []):
                overlay = objective_area_index.get(overlay_id)
                if not overlay:
                    continue
                target_kind = str(overlay.get("target_kind", "") or "")
                area_kind = "kill" if target_kind == "creature" else "collect"
                target_id = int(overlay.get("target_id", 0) or 0)
                if target_kind == "creature" and target_id > 0:
                    overlay_target_entries[overlay_id].add(target_id)
                target_name = str(overlay.get("target_name", "") or "").strip()
                if target_name:
                    overlay_labels[overlay_id].add(target_name)
                for source in overlay.get("source_creatures", []):
                    source_entry = int(source.get("entry", 0) or 0)
                    if source_entry > 0:
                        overlay_target_entries[overlay_id].add(source_entry)
                    source_name = str(source.get("name", "") or "").strip()
                    if source_name:
                        overlay_labels[overlay_id].add(source_name)

                for area in overlay.get("areas", []):
                    area_id = str(area.get("area_id", "") or "")
                    center_x = float(area.get("center_x", 0.0) or 0.0)
                    center_y = float(area.get("center_y", 0.0) or 0.0)
                    center_z = float(area.get("center_z", 0.0) or 0.0)
                    map_id = int(area.get("map_id", 0) or 0)
                    radius = float(area.get("radius", 0.0) or 0.0)
                    if map_id != zone_map_id:
                        continue
                    if radius <= 0.0 or radius > MAX_TASK_AREA_RADIUS_YARDS:
                        continue
                    resolved_zone = _resolve_zone(bounds, map_id, center_x, center_y)
                    if resolved_zone is None or resolved_zone.zone_id != zone_id:
                        continue

                    key = (overlay_id, area_id)
                    payload = area_payloads.setdefault(
                        key,
                        {
                            "taskAreaId": f"{overlay_id}:{area_id}",
                            "overlayId": overlay_id,
                            "kind": area_kind,
                            "position": _round_position(map_id, center_x, center_y, center_z),
                            "radius": round(radius, 3),
                            "relatedQuestIds": set(),
                            "targetEntries": set(),
                            "distanceToHub": round(_distance_2d((center_x, center_y), (hub_center_x, hub_center_y)), 3),
                        },
                    )
                    payload["relatedQuestIds"].add(quest_id)
                    payload["targetEntries"].update(overlay_target_entries.get(overlay_id, set()))

    task_areas: list[dict[str, Any]] = []
    for payload in area_payloads.values():
        related_quest_count = len(payload["relatedQuestIds"])
        if related_quest_count <= 0:
            continue
        task_areas.append(
            {
                "taskAreaId": payload["taskAreaId"],
                "overlayId": payload["overlayId"],
                "kind": payload["kind"],
                "position": payload["position"],
                "radius": payload["radius"],
                "weight": max(1, related_quest_count),
                "relatedQuestCount": related_quest_count,
                "targetEntries": sorted(payload["targetEntries"])[:MAX_TASK_AREA_TARGET_ENTRIES],
                "targetCount": len(payload["targetEntries"]),
                "distanceToHub": payload["distanceToHub"],
            }
        )
    task_areas.sort(
        key=lambda area: (
            -int(area["weight"]),
            float(area["distanceToHub"]),
            float(area["radius"]),
            str(area["kind"]),
            str(area["taskAreaId"]),
        )
    )
    per_overlay_counts: Counter[str] = Counter()
    trimmed_task_areas: list[dict[str, Any]] = []
    for task_area in task_areas:
        overlay_id = str(task_area["overlayId"])
        if per_overlay_counts[overlay_id] >= MAX_TASK_AREAS_PER_OVERLAY:
            continue
        per_overlay_counts[overlay_id] += 1
        trimmed = dict(task_area)
        trimmed.pop("overlayId", None)
        trimmed.pop("distanceToHub", None)
        trimmed_task_areas.append(trimmed)
        if len(trimmed_task_areas) >= MAX_TASK_AREAS_PER_HUB:
            break
    return trimmed_task_areas


def _build_branches_for_cluster(
    cluster_index: int,
    cluster_rows: list[dict[str, Any]],
    giver_entry_to_cluster: dict[int, int],
    cluster_hub_ids: dict[int, str],
    zone_rows_by_cluster: dict[int, list[dict[str, Any]]],
) -> list[dict[str, Any]]:
    target_counts: dict[int, dict[str, Any]] = {}
    seen_edges: set[tuple[int, int, int]] = set()
    for row in cluster_rows:
        for quest in row["quests"]:
            quest_id = int(quest.get("quest_id", 0) or 0)
            if quest_id <= 0:
                continue
            for followup in quest.get("followup_quests", []):
                followup_quest_id = int(followup.get("quest_id", 0) or 0)
                if followup_quest_id <= 0:
                    continue
                target_entries = row["giver_entries_by_quest_id"].get(followup_quest_id, set())
                for target_entry in sorted(target_entries):
                    target_cluster_index = giver_entry_to_cluster.get(target_entry)
                    if target_cluster_index is None or target_cluster_index == cluster_index:
                        continue
                    edge_key = (quest_id, followup_quest_id, target_entry)
                    if edge_key in seen_edges:
                        continue
                    seen_edges.add(edge_key)
                    target_rows = zone_rows_by_cluster[target_cluster_index]
                    target_sample = target_rows[0]
                    payload = target_counts.setdefault(
                        target_cluster_index,
                        {
                            "targetHubId": cluster_hub_ids[target_cluster_index],
                            "targetZoneId": int(target_sample["zone_id"]),
                            "targetZoneName": target_sample["zone_name"],
                            "targetNpcEntries": sorted({int(item["entry"]) for item in target_rows}),
                            "targetNpcLabels": sorted({str(item["label"]) for item in target_rows}),
                            "targetPosition": {
                                "mapId": int(target_sample["map_id"]),
                                "x": round(float(target_sample["world_x"]), 3),
                                "y": round(float(target_sample["world_y"]), 3),
                                "z": round(float(target_sample["world_z"]), 3),
                            },
                            "leadCount": 0,
                            "sourceQuestIds": set(),
                            "followupQuestIds": set(),
                        },
                    )
                    payload["leadCount"] += 1
                    payload["sourceQuestIds"].add(quest_id)
                    payload["followupQuestIds"].add(followup_quest_id)

    branches: list[dict[str, Any]] = []
    for payload in target_counts.values():
        lead_count = max(1, int(payload["leadCount"]))
        branches.append(
            {
                "hubId": payload["targetHubId"],
                "zoneId": payload["targetZoneId"],
                "zoneName": payload["targetZoneName"],
                "questGivers": payload["targetNpcEntries"],
                "position": payload["targetPosition"],
                "weight": lead_count,
            }
        )
    branches.sort(key=lambda branch: (-int(branch["weight"]), int(branch["zoneId"]), str(branch["hubId"])))
    return branches


def _build_zone_payload(
    zone_name: str,
    zone_id: int,
    zone_rows: list[dict[str, Any]],
    cluster_radius: float,
    objective_area_index: dict[str, Any],
    bounds: list[ZoneBounds],
) -> dict[str, Any]:
    clusters = _cluster_zone_rows(zone_rows, cluster_radius)
    repeated_signature_counts: Counter[tuple[int, tuple[int, ...]]] = Counter(
        (int(row["entry"]), _quest_signature(row))
        for row in zone_rows
    )
    giver_entry_to_cluster: dict[int, int] = {}
    zone_rows_by_cluster: dict[int, list[dict[str, Any]]] = {}
    cluster_hub_ids: dict[int, str] = {}

    zone_slug = _slugify(zone_name)
    filtered_clusters: list[list[dict[str, Any]]] = []
    for cluster_rows in clusters:
        if _should_skip_cluster_as_scattered_duplicate(cluster_rows, repeated_signature_counts):
            continue
        filtered_clusters.append(cluster_rows)

    for cluster_index, cluster_rows in enumerate(filtered_clusters, start=1):
        zone_rows_by_cluster[cluster_index] = cluster_rows
        cluster_hub_ids[cluster_index] = f"{zone_slug}_hub_{cluster_index:02d}"
        for row in cluster_rows:
            giver_entry_to_cluster[int(row["entry"])] = cluster_index

    hubs: list[dict[str, Any]] = []
    for cluster_index, cluster_rows in enumerate(filtered_clusters, start=1):
        hub_id = cluster_hub_ids[cluster_index]
        quests = [quest for row in cluster_rows for quest in row["quests"]]
        quest_ids = sorted({int(quest.get("quest_id", 0) or 0) for quest in quests if int(quest.get("quest_id", 0) or 0) > 0})
        quest_levels = [int(quest.get("quest_level", 0) or 0) for quest in quests if int(quest.get("quest_level", 0) or 0) > 0]
        min_levels = [int(quest.get("min_level", 0) or 0) for quest in quests if int(quest.get("min_level", 0) or 0) > 0]
        factions = Counter(str(quest.get("faction", "") or "") for quest in quests if str(quest.get("faction", "") or ""))
        branch_candidates = _build_branches_for_cluster(
            cluster_index,
            cluster_rows,
            giver_entry_to_cluster,
            cluster_hub_ids,
            zone_rows_by_cluster,
        )
        task_areas = _build_task_areas_for_cluster(
            cluster_rows,
            objective_area_index,
            bounds,
            zone_id,
            int(cluster_rows[0]["map_id"]),
        )

        mean_x = sum(float(row["world_x"]) for row in cluster_rows) / len(cluster_rows)
        mean_y = sum(float(row["world_y"]) for row in cluster_rows) / len(cluster_rows)
        mean_z = sum(float(row["world_z"]) for row in cluster_rows) / len(cluster_rows)
        unique_quest_count = len(quest_ids)
        estimated_minutes = max(10, unique_quest_count * 5)
        primary_faction = factions.most_common(1)[0][0] if factions else "Both"
        effective_levels = quest_levels or min_levels
        level_avg = int(round(sum(effective_levels) / len(effective_levels))) if effective_levels else 1

        hubs.append(
            {
                "hubId": hub_id,
                "position": _round_position(int(cluster_rows[0]["map_id"]), mean_x, mean_y, mean_z),
                "questGivers": sorted({int(row["entry"]) for row in cluster_rows}),
                "levelRange": {
                    "min": min(min_levels) if min_levels else 1,
                    "max": max(effective_levels) if effective_levels else 80,
                    "avg": level_avg,
                },
                "faction": primary_faction,
                "totalQuests": unique_quest_count,
                "estimatedMinutes": estimated_minutes,
                "taskAreas": task_areas,
                "nextHubs": branch_candidates,
            }
        )

    hubs.sort(key=lambda hub: (hub["position"]["x"], hub["position"]["y"], hub["hubId"]))
    return {
        "version": 1,
        "zoneId": zone_id,
        "zoneName": zone_name,
        "mapId": int(zone_rows[0]["map_id"]),
        "hubCount": len(hubs),
        "hubs": hubs,
    }


def _zone_selected(zone_name: str, zone_id: int, zone_ids: set[int], zone_name_filters: list[str]) -> bool:
    if zone_ids and zone_id not in zone_ids:
        return False
    if zone_name_filters:
        lowered = zone_name.lower()
        if not any(token in lowered for token in zone_name_filters):
            return False
    return True


def export_quest_hubs(
    marker_cache_path: Path,
    output_dir: Path,
    cluster_radius: float,
    zone_ids: set[int] | None = None,
    zone_name_filters: list[str] | None = None,
) -> list[Path]:
    bounds = _load_zone_bounds()
    quest_giver_rows, objective_area_index = _build_quest_giver_rows(marker_cache_path, bounds)
    by_zone: dict[tuple[int, str], list[dict[str, Any]]] = defaultdict(list)
    for row in quest_giver_rows:
        key = (int(row["zone_id"]), str(row["zone_name"]))
        by_zone[key].append(row)

    output_dir.mkdir(parents=True, exist_ok=True)
    selected_zone_ids = zone_ids or set()
    selected_zone_names = [token.lower() for token in (zone_name_filters or []) if token]
    written: list[Path] = []
    for (zone_id, zone_name), zone_rows in sorted(by_zone.items(), key=lambda item: (item[0][1].lower(), item[0][0])):
        if not _zone_selected(zone_name, zone_id, selected_zone_ids, selected_zone_names):
            continue
        payload = _build_zone_payload(zone_name, zone_id, zone_rows, cluster_radius, objective_area_index, bounds)
        output_path = output_dir / f"quest_hubs_{_slugify(zone_name)}.json"
        output_path.write_text(json.dumps(payload, indent=2), encoding="utf-8")
        written.append(output_path)
    return written


def main() -> int:
    args = build_parser().parse_args()
    written = export_quest_hubs(
        marker_cache_path=Path(args.marker_cache),
        output_dir=Path(args.output_dir),
        cluster_radius=float(args.cluster_radius),
        zone_ids=set(args.zone_ids or []),
        zone_name_filters=list(args.zone_names or []),
    )
    print(f"[lw-zone-editor] wrote {len(written)} quest hub file(s)")
    for path in written[:20]:
        print(f"  - {path}")
    if len(written) > 20:
        print(f"  ... {len(written) - 20} more")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
