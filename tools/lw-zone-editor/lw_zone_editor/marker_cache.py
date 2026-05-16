from __future__ import annotations

from collections import Counter
from dataclasses import asdict, dataclass
from datetime import datetime, timezone
from functools import lru_cache
import json
from pathlib import Path
import shutil
import struct
import subprocess
from typing import Any

from .paths import APP_ROOT, MARKER_CACHE_DIR, PNG_ICONS_DIR, PNG_MAPS_DIR

CREATURE_NPC_FLAGS = {
    "quest_giver": 0x00000002,
    "repair": 0x00001000,
    "flightmaster": 0x00002000,
    "innkeeper": 0x00010000,
    "banker": 0x00020000,
    "battlemaster": 0x00100000,
    "auctioneer": 0x00200000,
    "stablemaster": 0x00400000,
}

MARKER_KIND_LABELS = {
    "quest_giver": "Quest Givers",
    "mailbox": "Mailboxes",
    "auctioneer": "Auctioneers",
    "banker": "Bankers",
    "flightmaster": "Flight Masters",
    "innkeeper": "Innkeepers",
    "repair": "Repair",
    "stablemaster": "Stable Masters",
    "battlemaster": "Battlemasters",
    "herb": "Herb Nodes",
    "ore": "Ore Nodes",
}

MARKER_KIND_ICON_RELATIVE_PATHS = {
    "quest_giver": "interface/minimap/objecticons.png",
    "mailbox": "interface/minimap/tracking/mailbox.png",
    "auctioneer": "interface/minimap/tracking/auctioneer.png",
    "banker": "interface/minimap/tracking/banker.png",
    "flightmaster": "interface/minimap/tracking/flightmaster.png",
    "innkeeper": "interface/minimap/tracking/innkeeper.png",
    "repair": "interface/minimap/tracking/repair.png",
    "stablemaster": "interface/minimap/tracking/stablemaster.png",
    "battlemaster": "interface/minimap/tracking/battlemaster.png",
    "herb": "",
    "ore": "",
}

DEFAULT_MARKER_CACHE_PATH = MARKER_CACHE_DIR / "world_markers.json"
MARKER_CACHE_CHUNK_SIZE = 5000

ATLAS_SPRITES = {
    "quest_giver": {
        "rows": 2,
        "columns": 8,
        "row": 2,
        "column": 2,
    },
    "quest_giver_daily": {
        "rows": 2,
        "columns": 8,
        "row": 2,
        "column": 4,
    },
}

OBJECTIVE_DEFAULT_RADIUS = 35.0
OBJECTIVE_CLUSTER_PADDING = 55.0
OBJECTIVE_QUERY_BATCH_SIZE = 500
QUEST_SORT_SEASONAL = 22
QUEST_SORT_SPECIAL = 284
QUEST_SORT_DARKMOON_FAIRE = 364
QUEST_SORT_AHN_QIRAJ_WAR = 365
QUEST_SORT_LUNAR_FESTIVAL = 366
QUEST_SORT_INVASION = 368
QUEST_SORT_MIDSUMMER = 369
QUEST_SORT_BREWFEST = 370
QUEST_SORT_NOBLEGARDEN = 374
QUEST_SORT_PILGRIMS_BOUNTY = 375
QUEST_SORT_LOVE_IS_IN_THE_AIR = 376
QUEST_TYPE_WORLD_EVENT = 82
QUEST_FLAGS_DAILY = 0x00001000
QUEST_FLAGS_WEEKLY = 0x00008000
QUEST_SPECIAL_FLAGS_MONTHLY = 0x0010

EVENT_QUEST_SORT_IDS = {
    QUEST_SORT_SEASONAL,
    QUEST_SORT_SPECIAL,
    QUEST_SORT_DARKMOON_FAIRE,
    QUEST_SORT_AHN_QIRAJ_WAR,
    QUEST_SORT_LUNAR_FESTIVAL,
    QUEST_SORT_INVASION,
    QUEST_SORT_MIDSUMMER,
    QUEST_SORT_BREWFEST,
    QUEST_SORT_NOBLEGARDEN,
    QUEST_SORT_PILGRIMS_BOUNTY,
    QUEST_SORT_LOVE_IS_IN_THE_AIR,
}

HERB_NAME_KEYWORDS = (
    "adder's tongue",
    "arthas' tears",
    "black lotus",
    "blindweed",
    "briarthorn",
    "bruiseweed",
    "dreamfoil",
    "dreaming glory",
    "earthroot",
    "fadeleaf",
    "felweed",
    "firebloom",
    "firethorn",
    "flame cap",
    "frozen herb",
    "ghost mushroom",
    "goldclover",
    "golden sansam",
    "goldthorn",
    "grave moss",
    "gromsblood",
    "icecap",
    "icethorn",
    "khadgar's whisker",
    "kingsblood",
    "lichbloom",
    "liferoot",
    "mageroyal",
    "mana thistle",
    "mountain silversage",
    "netherbloom",
    "nightmare vine",
    "peacebloom",
    "plaguebloom",
    "purple lotus",
    "ragveil",
    "silverleaf",
    "stranglekelp",
    "sungrass",
    "talandra's rose",
    "terocone",
    "tiger lily",
    "wild steelbloom",
    "wintersbite",
)

ORE_NAME_KEYWORDS = (
    "deposit",
    "mineral vein",
    "vein",
)

RESOURCE_NAME_FILTER_TERMS = tuple(sorted(set(HERB_NAME_KEYWORDS + ORE_NAME_KEYWORDS)))
RESOURCE_ORE_ITEM_NAME_BY_NODE = {
    "copper vein": "Copper Ore",
    "tin vein": "Tin Ore",
    "silver vein": "Silver Ore",
    "gold vein": "Gold Ore",
    "iron deposit": "Iron Ore",
    "mithril deposit": "Mithril Ore",
    "truesilver deposit": "Truesilver Ore",
    "dark iron deposit": "Dark Iron Ore",
    "small thorium vein": "Thorium Ore",
    "rich thorium vein": "Thorium Ore",
    "fel iron deposit": "Fel Iron Ore",
    "adamantite deposit": "Adamantite Ore",
    "rich adamantite deposit": "Adamantite Ore",
    "khorium vein": "Khorium Ore",
    "cobalt deposit": "Cobalt Ore",
    "rich cobalt deposit": "Cobalt Ore",
    "saronite deposit": "Saronite Ore",
    "rich saronite deposit": "Saronite Ore",
    "titanium vein": "Titanium Ore",
}
ITEM_DISPLAY_INFO_DBC_PATH = APP_ROOT.parent.parent / "var" / "extractors" / "dbc" / "ItemDisplayInfo.dbc"


@dataclass(frozen=True, slots=True)
class MarkerRecord:
    uid: str
    kind: str
    label: str
    object_type: str
    map_id: int
    world_x: float
    world_y: float
    world_z: float
    entry: int
    guid: int
    icon_relpath: str
    metadata: dict[str, Any]


def marker_icon_path(kind: str) -> Path | None:
    relative = MARKER_KIND_ICON_RELATIVE_PATHS.get(kind)
    if not relative:
        return None
    return PNG_MAPS_DIR / Path(relative)


def marker_icon_crop_box(kind: str, image_width: int, image_height: int) -> tuple[int, int, int, int] | None:
    atlas = ATLAS_SPRITES.get(kind)
    if not atlas:
        return None

    rows = int(atlas["rows"])
    columns = int(atlas["columns"])
    row_index = int(atlas["row"]) - 1
    column_index = int(atlas["column"]) - 1
    tile_width = image_width // columns
    tile_height = image_height // rows
    left = column_index * tile_width
    top = row_index * tile_height
    return left, top, left + tile_width, top + tile_height


def load_marker_cache(path: str | Path = DEFAULT_MARKER_CACHE_PATH) -> tuple[list[MarkerRecord], dict[str, Any]]:
    cache_path = Path(path)
    if not cache_path.is_file():
        return [], {}

    payload = json.loads(cache_path.read_text(encoding="utf-8"))
    marker_rows: list[dict[str, Any]]
    if "markers" in payload:
        marker_rows = list(payload.get("markers", []))
    else:
        marker_rows = []
        for chunk_name in payload.get("marker_chunks", []):
            chunk_path = _marker_chunk_dir_for_path(cache_path) / chunk_name
            if not chunk_path.is_file():
                continue
            chunk_payload = json.loads(chunk_path.read_text(encoding="utf-8"))
            marker_rows.extend(chunk_payload.get("markers", []))

    markers = [MarkerRecord(**row) for row in marker_rows]
    return markers, payload


def write_marker_cache(payload: dict[str, Any], path: str | Path = DEFAULT_MARKER_CACHE_PATH) -> Path:
    output_path = Path(path)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    markers = list(payload.get("markers", []))
    manifest = dict(payload)
    chunk_dir = _marker_chunk_dir_for_path(output_path)
    if chunk_dir.exists():
        shutil.rmtree(chunk_dir)

    if markers:
        chunk_dir.mkdir(parents=True, exist_ok=True)
        chunk_names: list[str] = []
        for chunk_index, chunk_rows in enumerate(_iter_marker_chunks(markers, MARKER_CACHE_CHUNK_SIZE), start=1):
            chunk_name = f"markers_{chunk_index:04d}.json"
            chunk_path = chunk_dir / chunk_name
            chunk_path.write_text(
                json.dumps({"markers": chunk_rows}, indent=2, ensure_ascii=False) + "\n",
                encoding="utf-8",
            )
            chunk_names.append(chunk_name)

        manifest.pop("markers", None)
        manifest["marker_chunk_size"] = MARKER_CACHE_CHUNK_SIZE
        manifest["marker_chunk_count"] = len(chunk_names)
        manifest["marker_chunks"] = chunk_names
    else:
        manifest["markers"] = []
        manifest.pop("marker_chunk_size", None)
        manifest.pop("marker_chunk_count", None)
        manifest.pop("marker_chunks", None)

    output_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return output_path


def _marker_chunk_dir_for_path(path: Path) -> Path:
    return path.parent / path.stem


def _iter_marker_chunks(
    markers: list[dict[str, Any]],
    chunk_size: int,
) -> list[list[dict[str, Any]]]:
    return [markers[index:index + chunk_size] for index in range(0, len(markers), chunk_size)]


def build_marker_cache(
    host: str,
    port: int,
    user: str,
    password: str,
    database: str = "acore_world",
    mysql_binary: str = "mysql",
) -> dict[str, Any]:
    quest_index = _query_quest_details(
        host=host,
        port=port,
        user=user,
        password=password,
        database=database,
        mysql_binary=mysql_binary,
    )
    objective_target_ids = _collect_creature_objective_target_ids(quest_index)
    item_requirement_ids = _collect_item_requirement_ids(quest_index)
    item_source_creatures = _query_item_requirement_creature_sources(
        host=host,
        port=port,
        user=user,
        password=password,
        database=database,
        mysql_binary=mysql_binary,
        item_ids=item_requirement_ids,
    )
    item_source_creature_entries = {
        _int_or_zero(source.get("entry"))
        for sources in item_source_creatures.values()
        for source in sources
        if _int_or_zero(source.get("entry"))
    }
    objective_spawn_index = _query_creature_objective_spawns(
        host=host,
        port=port,
        user=user,
        password=password,
        database=database,
        mysql_binary=mysql_binary,
        entries=objective_target_ids | item_source_creature_entries,
    )
    objective_area_index = _build_objective_area_index(objective_spawn_index)
    objective_area_index.update(_build_item_source_area_index(item_source_creatures, objective_spawn_index))
    _annotate_quests_with_objective_area_data(quest_index, objective_area_index)
    quest_route_graph = _build_quest_route_graph(quest_index)
    creature_rows = _query_creature_markers(
        host=host,
        port=port,
        user=user,
        password=password,
        database=database,
        mysql_binary=mysql_binary,
    )
    resource_rows = _query_resource_markers(
        host=host,
        port=port,
        user=user,
        password=password,
        database=database,
        mysql_binary=mysql_binary,
    )
    resource_item_icons = _query_resource_item_icon_relpaths(
        host=host,
        port=port,
        user=user,
        password=password,
        database=database,
        mysql_binary=mysql_binary,
        resource_rows=resource_rows,
    )
    resource_loot_by_entry = _query_resource_loot_by_entry(
        host=host,
        port=port,
        user=user,
        password=password,
        database=database,
        mysql_binary=mysql_binary,
        resource_rows=resource_rows,
    )
    mailbox_rows = _query_mailbox_markers(
        host=host,
        port=port,
        user=user,
        password=password,
        database=database,
        mysql_binary=mysql_binary,
    )

    markers: list[MarkerRecord] = []
    for row in creature_rows:
        markers.extend(_markers_from_creature_row(row, quest_index))
    for row in resource_rows:
        resource_marker = _marker_from_resource_row(row, resource_item_icons, resource_loot_by_entry)
        if resource_marker is not None:
            markers.append(resource_marker)
    for row in mailbox_rows:
        markers.append(_marker_from_mailbox_row(row))

    markers.sort(key=lambda item: (item.kind, item.label.lower(), item.map_id, item.guid))
    counts = Counter(marker.kind for marker in markers)
    return {
        "version": 1,
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "source": {
            "host": host,
            "port": port,
            "database": database,
        },
        "marker_counts": dict(sorted(counts.items())),
        "objective_area_index": objective_area_index,
        "quest_route_graph": quest_route_graph,
        "markers": [asdict(marker) for marker in markers],
    }


def _collect_creature_objective_target_ids(quest_index: dict[int, list[dict[str, Any]]]) -> set[int]:
    entries: set[int] = set()
    for quests in quest_index.values():
        for quest in quests:
            for requirement in quest.get("target_requirements", []):
                target_id = _int_or_zero(requirement.get("target_id"))
                if target_id and str(requirement.get("target_kind", "")) != "object":
                    entries.add(target_id)
    return entries


def _collect_item_requirement_ids(quest_index: dict[int, list[dict[str, Any]]]) -> set[int]:
    item_ids: set[int] = set()
    for quests in quest_index.values():
        for quest in quests:
            for requirement in quest.get("item_requirements", []):
                item_id = _int_or_zero(requirement.get("item_id"))
                if item_id:
                    item_ids.add(item_id)
    return item_ids


def _query_item_requirement_creature_sources(
    host: str,
    port: int,
    user: str,
    password: str,
    database: str,
    mysql_binary: str,
    item_ids: set[int],
) -> dict[int, list[dict[str, Any]]]:
    if not item_ids:
        return {}

    sources_by_item: dict[int, list[dict[str, Any]]] = {}
    sorted_item_ids = sorted(item_ids)
    for start_index in range(0, len(sorted_item_ids), OBJECTIVE_QUERY_BATCH_SIZE):
        batch = sorted_item_ids[start_index:start_index + OBJECTIVE_QUERY_BATCH_SIZE]
        sql = f"""
SELECT
    clt.Item,
    ct.entry,
    ct.name
FROM creature_loot_template clt
JOIN creature_template ct ON ct.lootid = clt.Entry
WHERE clt.Item IN ({','.join(str(value) for value in batch)})
ORDER BY clt.Item, ct.entry
"""
        rows = _run_mysql_query(mysql_binary, host, port, user, password, database, sql)
        seen_pairs: set[tuple[int, int]] = set()
        for row in rows:
            item_id = _int_or_zero(row[0])
            entry = _int_or_zero(row[1])
            if not item_id or not entry:
                continue
            dedupe_key = (item_id, entry)
            if dedupe_key in seen_pairs:
                continue
            seen_pairs.add(dedupe_key)
            sources_by_item.setdefault(item_id, []).append(
                {
                    "entry": entry,
                    "name": row[2] or f"Creature #{entry}",
                }
            )
    return sources_by_item


def _query_creature_objective_spawns(
    host: str,
    port: int,
    user: str,
    password: str,
    database: str,
    mysql_binary: str,
    entries: set[int],
) -> dict[int, list[dict[str, Any]]]:
    if not entries:
        return {}

    spawns_by_entry: dict[int, list[dict[str, Any]]] = {}
    sorted_entries = sorted(entries)
    for start_index in range(0, len(sorted_entries), OBJECTIVE_QUERY_BATCH_SIZE):
        batch = sorted_entries[start_index:start_index + OBJECTIVE_QUERY_BATCH_SIZE]
        sql = f"""
SELECT
    c.id1,
    c.map,
    c.position_x,
    c.position_y,
    c.position_z,
    c.wander_distance,
    c.MovementType,
    c.phaseMask
FROM creature c
WHERE c.id1 IN ({','.join(str(value) for value in batch)})
ORDER BY c.id1, c.map, c.guid
"""
        rows = _run_mysql_query(mysql_binary, host, port, user, password, database, sql)
        for row in rows:
            entry_value = _int_or_zero(row[0])
            if not entry_value:
                continue
            spawns_by_entry.setdefault(entry_value, []).append(
                {
                    "map_id": _int_or_zero(row[1]),
                    "world_x": _float_or_zero(row[2]),
                    "world_y": _float_or_zero(row[3]),
                    "world_z": _float_or_zero(row[4]),
                    "wander_distance": max(0.0, _float_or_zero(row[5])),
                    "movement_type": _int_or_zero(row[6]),
                    "phase_mask": _int_or_zero(row[7]),
                }
            )
    return spawns_by_entry


def _build_objective_area_index(spawns_by_entry: dict[int, list[dict[str, Any]]]) -> dict[str, Any]:
    objective_area_index: dict[str, Any] = {}
    for target_id, spawns in spawns_by_entry.items():
        overlay_id = f"creature:{target_id}"
        areas = _cluster_objective_spawns(spawns)
        objective_area_index[overlay_id] = {
            "overlay_id": overlay_id,
            "target_id": target_id,
            "target_kind": "creature",
            "spawn_count_total": len(spawns),
            "map_ids": sorted({int(area["map_id"]) for area in areas}),
            "areas": areas,
        }
    return objective_area_index


def _build_item_source_area_index(
    item_source_creatures: dict[int, list[dict[str, Any]]],
    spawns_by_entry: dict[int, list[dict[str, Any]]],
) -> dict[str, Any]:
    overlays: dict[str, Any] = {}
    for item_id, sources in item_source_creatures.items():
        combined_spawns: list[dict[str, Any]] = []
        source_creatures: list[dict[str, Any]] = []
        for source in sources:
            entry = _int_or_zero(source.get("entry"))
            if not entry:
                continue
            source_creatures.append({"entry": entry, "name": str(source.get("name", "") or f"Creature #{entry}")})
            combined_spawns.extend(spawns_by_entry.get(entry, []))
        if not combined_spawns:
            continue
        overlay_id = f"item_source:{item_id}"
        areas = _cluster_objective_spawns(combined_spawns)
        overlays[overlay_id] = {
            "overlay_id": overlay_id,
            "target_id": item_id,
            "target_kind": "item_source",
            "spawn_count_total": len(combined_spawns),
            "map_ids": sorted({int(area["map_id"]) for area in areas}),
            "areas": areas,
            "source_creatures": source_creatures,
        }
    return overlays


def _annotate_quests_with_objective_area_data(
    quest_index: dict[int, list[dict[str, Any]]],
    objective_area_index: dict[str, Any],
) -> None:
    for giver_entry, quests in quest_index.items():
        for quest in quests:
            objective_overlay_ids: list[str] = []
            branch_candidates: list[dict[str, Any]] = []
            for requirement in quest.get("target_requirements", []):
                target_id = _int_or_zero(requirement.get("target_id"))
                if not target_id or str(requirement.get("target_kind", "")) == "object":
                    continue
                overlay_id = f"creature:{target_id}"
                overlay = objective_area_index.get(overlay_id)
                requirement["objective_overlay_id"] = overlay_id
                requirement["objective_overlay_available"] = bool(overlay)
                if not overlay:
                    continue
                overlay.setdefault("target_name", str(requirement.get("name", "") or f"Creature #{target_id}"))
                requirement["objective_spawn_count"] = int(overlay.get("spawn_count_total", 0))
                requirement["objective_area_count"] = len(list(overlay.get("areas", [])))
                requirement["objective_map_ids"] = list(overlay.get("map_ids", []))
                if overlay_id not in objective_overlay_ids:
                    objective_overlay_ids.append(overlay_id)
                branch_candidates.append(
                    {
                        "branch_kind": "objective_area",
                        "branch_id": overlay_id,
                        "label": str(requirement.get("name", "") or f"Creature #{target_id}"),
                        "weight": 1.0,
                        "giver_entry": giver_entry,
                    }
                )

            for requirement in quest.get("item_requirements", []):
                item_id = _int_or_zero(requirement.get("item_id"))
                if not item_id:
                    continue
                overlay_id = f"item_source:{item_id}"
                overlay = objective_area_index.get(overlay_id)
                requirement["objective_overlay_id"] = overlay_id
                requirement["objective_overlay_available"] = bool(overlay)
                if not overlay:
                    continue
                requirement["objective_spawn_count"] = int(overlay.get("spawn_count_total", 0))
                requirement["objective_area_count"] = len(list(overlay.get("areas", [])))
                requirement["objective_map_ids"] = list(overlay.get("map_ids", []))
                requirement["source_creatures"] = list(overlay.get("source_creatures", []))
                if overlay_id not in objective_overlay_ids:
                    objective_overlay_ids.append(overlay_id)
                branch_candidates.append(
                    {
                        "branch_kind": "item_source",
                        "branch_id": overlay_id,
                        "label": str(requirement.get("name", "") or f"Item #{item_id}"),
                        "weight": 1.0,
                        "giver_entry": giver_entry,
                    }
                )

            for related in quest.get("followup_quests", []):
                related_id = _int_or_zero(related.get("quest_id"))
                if not related_id:
                    continue
                branch_candidates.append(
                    {
                        "branch_kind": "followup_quest",
                        "branch_id": related_id,
                        "label": str(related.get("title", "") or f"Quest #{related_id}"),
                        "weight": 1.0,
                        "relation": str(related.get("relation", "next")),
                    }
                )

            quest["objective_overlay_ids"] = objective_overlay_ids
            quest["branch_candidates"] = branch_candidates
            quest["branch_metadata"] = {
                "branch_mode": "branching" if len(branch_candidates) > 1 else "linear",
                "branch_count": len(branch_candidates),
                "return_to_giver_assumed": True,
            }


def _build_quest_route_graph(quest_index: dict[int, list[dict[str, Any]]]) -> list[dict[str, Any]]:
    routes: list[dict[str, Any]] = []
    for giver_entry, quests in sorted(quest_index.items()):
        for quest in quests:
            routes.append(
                {
                    "route_id": f"giver:{giver_entry}:quest:{quest['quest_id']}",
                    "giver_entry": giver_entry,
                    "quest_id": int(quest.get("quest_id", 0)),
                    "quest_title": str(quest.get("title", "")),
                    "quest_level": int(quest.get("quest_level", 0)),
                    "objective_overlay_ids": list(quest.get("objective_overlay_ids", [])),
                    "prerequisite_quest_ids": [
                        _int_or_zero(item.get("quest_id")) for item in quest.get("prerequisite_quests", []) if _int_or_zero(item.get("quest_id"))
                    ],
                    "followup_quest_ids": [
                        _int_or_zero(item.get("quest_id")) for item in quest.get("followup_quests", []) if _int_or_zero(item.get("quest_id"))
                    ],
                    "breadcrumb_for_quest_ids": [
                        _int_or_zero(item.get("quest_id")) for item in quest.get("breadcrumb_for_quests", []) if _int_or_zero(item.get("quest_id"))
                    ],
                    "branch_candidates": list(quest.get("branch_candidates", [])),
                    "branch_metadata": dict(quest.get("branch_metadata", {})),
                }
            )
    return routes


def _cluster_objective_spawns(spawns: list[dict[str, Any]]) -> list[dict[str, Any]]:
    grouped_by_map: dict[int, list[dict[str, Any]]] = {}
    for spawn in spawns:
        grouped_by_map.setdefault(_int_or_zero(spawn.get("map_id")), []).append(spawn)

    areas: list[dict[str, Any]] = []
    for map_id, map_spawns in sorted(grouped_by_map.items()):
        clusters: list[list[dict[str, Any]]] = []
        centers: list[tuple[float, float]] = []
        radii: list[float] = []
        for spawn in map_spawns:
            spawn_x = _float_or_zero(spawn.get("world_x"))
            spawn_y = _float_or_zero(spawn.get("world_y"))
            spawn_radius = max(OBJECTIVE_DEFAULT_RADIUS, _float_or_zero(spawn.get("wander_distance")))
            matched_index: int | None = None
            for index, center in enumerate(centers):
                if _distance_2d(center[0], center[1], spawn_x, spawn_y) <= radii[index] + spawn_radius + OBJECTIVE_CLUSTER_PADDING:
                    matched_index = index
                    break
            if matched_index is None:
                clusters.append([spawn])
                centers.append((spawn_x, spawn_y))
                radii.append(spawn_radius)
                continue

            clusters[matched_index].append(spawn)
            center_x, center_y, cluster_radius = _recalculate_cluster_geometry(clusters[matched_index])
            centers[matched_index] = (center_x, center_y)
            radii[matched_index] = cluster_radius

        for cluster_index, cluster in enumerate(clusters, start=1):
            center_x, center_y, cluster_radius = _recalculate_cluster_geometry(cluster)
            center_z = sum(_float_or_zero(item.get("world_z")) for item in cluster) / float(len(cluster))
            areas.append(
                {
                    "area_id": f"map:{map_id}:cluster:{cluster_index}",
                    "map_id": map_id,
                    "center_x": round(center_x, 2),
                    "center_y": round(center_y, 2),
                    "center_z": round(center_z, 2),
                    "radius": round(cluster_radius, 2),
                    "spawn_count": len(cluster),
                    "movement_types": sorted({_int_or_zero(item.get("movement_type")) for item in cluster}),
                    "phase_masks": sorted({_int_or_zero(item.get("phase_mask")) for item in cluster}),
                    "sample_spawns": [
                        {
                            "world_x": round(_float_or_zero(item.get("world_x")), 2),
                            "world_y": round(_float_or_zero(item.get("world_y")), 2),
                            "world_z": round(_float_or_zero(item.get("world_z")), 2),
                            "wander_distance": round(_float_or_zero(item.get("wander_distance")), 2),
                        }
                        for item in cluster[:5]
                    ],
                }
            )
    return areas


def _recalculate_cluster_geometry(cluster: list[dict[str, Any]]) -> tuple[float, float, float]:
    center_x = sum(_float_or_zero(item.get("world_x")) for item in cluster) / float(len(cluster))
    center_y = sum(_float_or_zero(item.get("world_y")) for item in cluster) / float(len(cluster))
    radius = OBJECTIVE_DEFAULT_RADIUS
    for item in cluster:
        radius = max(
            radius,
            _distance_2d(center_x, center_y, _float_or_zero(item.get("world_x")), _float_or_zero(item.get("world_y")))
            + max(OBJECTIVE_DEFAULT_RADIUS, _float_or_zero(item.get("wander_distance"))),
        )
    return center_x, center_y, radius


def _distance_2d(ax: float, ay: float, bx: float, by: float) -> float:
    return ((ax - bx) ** 2 + (ay - by) ** 2) ** 0.5


def _query_creature_markers(
    host: str,
    port: int,
    user: str,
    password: str,
    database: str,
    mysql_binary: str,
) -> list[list[str]]:
    service_mask = 0
    for flag in CREATURE_NPC_FLAGS.values():
        service_mask |= flag

    sql = f"""
SELECT
    c.guid,
    c.id1,
    ct.name,
    c.map,
    c.position_x,
    c.position_y,
    c.position_z,
    c.npcflag,
    ct.npcflag,
    COALESCE(qs.quest_count, 0) AS quest_count
FROM creature c
JOIN creature_template ct ON ct.entry = c.id1
LEFT JOIN (
    SELECT id, COUNT(*) AS quest_count
    FROM creature_queststarter
    GROUP BY id
) qs ON qs.id = c.id1
WHERE ((c.npcflag | ct.npcflag) & {service_mask}) != 0
   OR COALESCE(qs.quest_count, 0) > 0
"""
    return _run_mysql_query(mysql_binary, host, port, user, password, database, sql)


def _query_mailbox_markers(
    host: str,
    port: int,
    user: str,
    password: str,
    database: str,
    mysql_binary: str,
) -> list[list[str]]:
    sql = """
SELECT
    g.guid,
    g.id,
    gt.name,
    g.map,
    g.position_x,
    g.position_y,
    g.position_z
FROM gameobject g
JOIN gameobject_template gt ON gt.entry = g.id
WHERE LOWER(gt.name) LIKE '%mailbox%'
"""
    return _run_mysql_query(mysql_binary, host, port, user, password, database, sql)


def _query_resource_markers(
    host: str,
    port: int,
    user: str,
    password: str,
    database: str,
    mysql_binary: str,
) -> list[list[str]]:
    sql = f"""
SELECT
    g.guid,
    g.id,
    gt.name,
    g.map,
    g.position_x,
    g.position_y,
    g.position_z,
    gt.Data0,
    gt.Data1
FROM gameobject g
JOIN gameobject_template gt ON gt.entry = g.id
WHERE gt.type = 3
  AND ({_resource_name_filter_sql('LOWER(gt.name)')})
ORDER BY gt.name, g.map, g.guid
"""
    return _run_mysql_query(mysql_binary, host, port, user, password, database, sql)


def _query_resource_item_icon_relpaths(
    host: str,
    port: int,
    user: str,
    password: str,
    database: str,
    mysql_binary: str,
    resource_rows: list[list[str]],
) -> dict[str, str]:
    candidate_names: set[str] = set()
    for row in resource_rows:
        if len(row) < 3:
            continue
        resource_name = row[2]
        kind = _resource_kind_from_name(resource_name)
        if kind is None:
            continue
        item_name = _resource_item_name_from_node_name(resource_name, kind)
        if item_name:
            candidate_names.add(item_name)

    if not candidate_names:
        return {}

    icon_name_by_display_id = _load_item_display_icon_index_from_dbc(ITEM_DISPLAY_INFO_DBC_PATH)
    if not icon_name_by_display_id:
        return {}

    ordered_names = sorted(candidate_names)
    sql_names = ", ".join(_sql_quote_literal(name) for name in ordered_names)
    sql = f"""
SELECT
    name,
    displayid
FROM item_template
WHERE name IN ({sql_names})
  AND displayid > 0
"""
    rows = _run_mysql_query(mysql_binary, host, port, user, password, database, sql)

    result: dict[str, str] = {}
    for row in rows:
        if len(row) < 2:
            continue
        item_name = row[0]
        display_id = _int_or_zero(row[1])
        if not item_name or display_id <= 0:
            continue
        icon_name = icon_name_by_display_id.get(display_id, "")
        if not icon_name:
            continue
        icon_filename = f"{icon_name.lower()}.png"
        icon_relpath = Path("interface") / "icons" / icon_filename
        if (PNG_ICONS_DIR / icon_relpath).is_file():
            result[item_name] = icon_relpath.as_posix()
    return result


def _query_resource_loot_by_entry(
    host: str,
    port: int,
    user: str,
    password: str,
    database: str,
    mysql_binary: str,
    resource_rows: list[list[str]],
) -> dict[int, list[dict[str, Any]]]:
    loot_entry_ids = sorted(
        {
            _int_or_zero(row[8])
            for row in resource_rows
            if len(row) >= 9 and _int_or_zero(row[8]) > 0
        }
    )
    if not loot_entry_ids:
        return {}

    icon_name_by_display_id = _load_item_display_icon_index_from_dbc(ITEM_DISPLAY_INFO_DBC_PATH)
    sql_entries = ", ".join(str(entry_id) for entry_id in loot_entry_ids)
    sql = f"""
SELECT
    glt.Entry,
    glt.Item,
    glt.Chance,
    glt.QuestRequired,
    glt.GroupId,
    glt.MinCount,
    glt.MaxCount,
    COALESCE(glt.Comment, ''),
    COALESCE(it.name, CONCAT('Item #', glt.Item)),
    COALESCE(it.displayid, 0)
FROM gameobject_loot_template glt
LEFT JOIN item_template it ON it.entry = glt.Item
WHERE glt.Entry IN ({sql_entries})
  AND glt.Item > 0
  AND glt.Reference = 0
ORDER BY glt.Entry, glt.GroupId, glt.Chance DESC, glt.Item
"""
    rows = _run_mysql_query(mysql_binary, host, port, user, password, database, sql)
    by_entry: dict[int, list[dict[str, Any]]] = {}
    for row in rows:
        if len(row) < 10:
            continue
        loot_entry = _int_or_zero(row[0])
        item_id = _int_or_zero(row[1])
        display_id = _int_or_zero(row[9])
        icon_name = icon_name_by_display_id.get(display_id, "")
        icon_relpath = ""
        if icon_name:
            icon_candidate = Path("interface") / "icons" / f"{icon_name.lower()}.png"
            if (PNG_ICONS_DIR / icon_candidate).is_file():
                icon_relpath = icon_candidate.as_posix()
        item_payload = {
            "item_id": item_id,
            "name": row[8],
            "chance": _float_or_zero(row[2]),
            "quest_required": bool(_int_or_zero(row[3])),
            "group_id": _int_or_zero(row[4]),
            "min_count": max(1, _int_or_zero(row[5])),
            "max_count": max(1, _int_or_zero(row[6])),
            "comment": row[7],
            "icon_relpath": icon_relpath,
        }
        by_entry.setdefault(loot_entry, []).append(item_payload)

    for loot_entry, items in by_entry.items():
        _annotate_resource_loot_chances(items)
        by_entry[loot_entry] = items
    return by_entry


def _annotate_resource_loot_chances(items: list[dict[str, Any]]) -> None:
    groups: dict[int, list[dict[str, Any]]] = {}
    for item in items:
        groups.setdefault(_int_or_zero(item.get("group_id")), []).append(item)

    for group_id, group_items in groups.items():
        explicit_positive = [entry for entry in group_items if _float_or_zero(entry.get("chance")) > 0.0]
        equal_roll_items = [entry for entry in group_items if _float_or_zero(entry.get("chance")) <= 0.0]
        equal_roll_chance = None
        if group_id > 0 and equal_roll_items and not explicit_positive:
            equal_roll_chance = 100.0 / float(len(equal_roll_items))

        for entry in group_items:
            listed_chance = _float_or_zero(entry.get("chance"))
            estimated_chance = listed_chance if listed_chance > 0.0 else equal_roll_chance
            if estimated_chance is not None:
                entry["estimated_chance"] = round(float(estimated_chance), 3)
            else:
                entry["estimated_chance"] = None


def _query_quest_details(
    host: str,
    port: int,
    user: str,
    password: str,
    database: str,
    mysql_binary: str,
) -> dict[int, list[dict[str, Any]]]:
    def safe_text_sql(expression: str) -> str:
        return (
            f"REPLACE(REPLACE(REPLACE(COALESCE({expression}, ''), CHAR(13), ' '), CHAR(10), ' '), CHAR(9), ' ')"
        )

    sql = f"""
SELECT
    qs.id,
    q.ID,
    {safe_text_sql('q.LogTitle')},
    q.QuestLevel,
    q.MinLevel,
    q.AllowableRaces,
    {safe_text_sql('q.LogDescription')},
    {safe_text_sql('q.QuestDescription')},
    {safe_text_sql('q.AreaDescription')},
    {safe_text_sql('q.QuestCompletionLog')},
    {safe_text_sql('q.ObjectiveText1')},
    {safe_text_sql('q.ObjectiveText2')},
    {safe_text_sql('q.ObjectiveText3')},
    {safe_text_sql('q.ObjectiveText4')},
    q.RequiredNpcOrGo1,
    q.RequiredNpcOrGoCount1,
    {safe_text_sql('req_target1.name')},
    q.RequiredNpcOrGo2,
    q.RequiredNpcOrGoCount2,
    {safe_text_sql('req_target2.name')},
    q.RequiredNpcOrGo3,
    q.RequiredNpcOrGoCount3,
    {safe_text_sql('req_target3.name')},
    q.RequiredNpcOrGo4,
    q.RequiredNpcOrGoCount4,
    {safe_text_sql('req_target4.name')},
    q.RequiredItemId1,
    q.RequiredItemCount1,
    {safe_text_sql('req1.name')},
    q.RequiredItemId2,
    q.RequiredItemCount2,
    {safe_text_sql('req2.name')},
    q.RequiredItemId3,
    q.RequiredItemCount3,
    {safe_text_sql('req3.name')},
    q.RequiredItemId4,
    q.RequiredItemCount4,
    {safe_text_sql('req4.name')},
    q.RequiredItemId5,
    q.RequiredItemCount5,
    {safe_text_sql('req5.name')},
    q.RequiredItemId6,
    q.RequiredItemCount6,
    {safe_text_sql('req6.name')},
    q.RewardMoney,
    q.RewardItem1,
    q.RewardAmount1,
    {safe_text_sql('rew1.name')},
    q.RewardItem2,
    q.RewardAmount2,
    {safe_text_sql('rew2.name')},
    q.RewardItem3,
    q.RewardAmount3,
    {safe_text_sql('rew3.name')},
    q.RewardItem4,
    q.RewardAmount4,
    {safe_text_sql('rew4.name')},
    q.RewardChoiceItemID1,
    q.RewardChoiceItemQuantity1,
    {safe_text_sql('choice1.name')},
    q.RewardChoiceItemID2,
    q.RewardChoiceItemQuantity2,
    {safe_text_sql('choice2.name')},
    q.RewardChoiceItemID3,
    q.RewardChoiceItemQuantity3,
    {safe_text_sql('choice3.name')},
    q.RewardChoiceItemID4,
    q.RewardChoiceItemQuantity4,
    {safe_text_sql('choice4.name')},
    q.RewardChoiceItemID5,
    q.RewardChoiceItemQuantity5,
    {safe_text_sql('choice5.name')},
    q.RewardChoiceItemID6,
    q.RewardChoiceItemQuantity6,
    {safe_text_sql('choice6.name')},
    COALESCE(qta.PrevQuestID, 0),
    {safe_text_sql('prev_q.LogTitle')},
    COALESCE(qta.NextQuestID, 0),
    {safe_text_sql('next_q.LogTitle')},
    q.RewardNextQuest,
    {safe_text_sql('reward_next_q.LogTitle')},
    COALESCE(qta.BreadcrumbForQuestId, 0),
    {safe_text_sql('breadcrumb_q.LogTitle')},
    q.QuestSortID,
    q.QuestType,
    q.Flags,
    COALESCE(qta.SpecialFlags, 0),
    CASE WHEN event_q.quest_id IS NOT NULL OR q.QuestType = {QUEST_TYPE_WORLD_EVENT} THEN 1 ELSE 0 END,
    CASE WHEN starter_event_creature.creature_id IS NOT NULL THEN 1 ELSE 0 END
FROM creature_queststarter qs
JOIN quest_template q ON q.ID = qs.quest
LEFT JOIN quest_template_addon qta ON qta.ID = q.ID
LEFT JOIN (
    SELECT DISTINCT questId AS quest_id FROM game_event_seasonal_questrelation
    UNION
    SELECT DISTINCT quest AS quest_id FROM game_event_creature_quest
    UNION
    SELECT DISTINCT quest AS quest_id FROM game_event_gameobject_quest
) event_q ON event_q.quest_id = q.ID
LEFT JOIN (
    SELECT DISTINCT c.id1 AS creature_id
    FROM creature c
    JOIN game_event_creature gec ON gec.guid = c.guid
) starter_event_creature ON starter_event_creature.creature_id = qs.id
LEFT JOIN (
    SELECT entry, name FROM creature_template
    UNION ALL
    SELECT -entry, name FROM gameobject_template
) req_target1 ON req_target1.entry = q.RequiredNpcOrGo1
LEFT JOIN (
    SELECT entry, name FROM creature_template
    UNION ALL
    SELECT -entry, name FROM gameobject_template
) req_target2 ON req_target2.entry = q.RequiredNpcOrGo2
LEFT JOIN (
    SELECT entry, name FROM creature_template
    UNION ALL
    SELECT -entry, name FROM gameobject_template
) req_target3 ON req_target3.entry = q.RequiredNpcOrGo3
LEFT JOIN (
    SELECT entry, name FROM creature_template
    UNION ALL
    SELECT -entry, name FROM gameobject_template
) req_target4 ON req_target4.entry = q.RequiredNpcOrGo4
LEFT JOIN item_template req1 ON req1.entry = q.RequiredItemId1
LEFT JOIN item_template req2 ON req2.entry = q.RequiredItemId2
LEFT JOIN item_template req3 ON req3.entry = q.RequiredItemId3
LEFT JOIN item_template req4 ON req4.entry = q.RequiredItemId4
LEFT JOIN item_template req5 ON req5.entry = q.RequiredItemId5
LEFT JOIN item_template req6 ON req6.entry = q.RequiredItemId6
LEFT JOIN item_template rew1 ON rew1.entry = q.RewardItem1
LEFT JOIN item_template rew2 ON rew2.entry = q.RewardItem2
LEFT JOIN item_template rew3 ON rew3.entry = q.RewardItem3
LEFT JOIN item_template rew4 ON rew4.entry = q.RewardItem4
LEFT JOIN item_template choice1 ON choice1.entry = q.RewardChoiceItemID1
LEFT JOIN item_template choice2 ON choice2.entry = q.RewardChoiceItemID2
LEFT JOIN item_template choice3 ON choice3.entry = q.RewardChoiceItemID3
LEFT JOIN item_template choice4 ON choice4.entry = q.RewardChoiceItemID4
LEFT JOIN item_template choice5 ON choice5.entry = q.RewardChoiceItemID5
LEFT JOIN item_template choice6 ON choice6.entry = q.RewardChoiceItemID6
LEFT JOIN quest_template prev_q ON prev_q.ID = ABS(COALESCE(qta.PrevQuestID, 0))
LEFT JOIN quest_template next_q ON next_q.ID = COALESCE(qta.NextQuestID, 0)
LEFT JOIN quest_template reward_next_q ON reward_next_q.ID = q.RewardNextQuest
LEFT JOIN quest_template breadcrumb_q ON breadcrumb_q.ID = COALESCE(qta.BreadcrumbForQuestId, 0)
ORDER BY qs.id, q.ID
"""
    rows = _run_mysql_query(mysql_binary, host, port, user, password, database, sql)
    by_creature: dict[int, list[dict[str, Any]]] = {}
    for row in rows:
        if len(row) < 89:
            raise ValueError(f"quest detail row expected 89 columns but received {len(row)}: {row!r}")
        creature_entry = int(row[0])
        by_creature.setdefault(creature_entry, []).append(_build_quest_payload_from_row(row))
    return by_creature


def _run_mysql_query(
    mysql_binary: str,
    host: str,
    port: int,
    user: str,
    password: str,
    database: str,
    sql: str,
) -> list[list[str]]:
    command = [
        mysql_binary,
        "-h",
        host,
        "-P",
        str(port),
        "-u",
        user,
        f"--password={password}",
        "-D",
        database,
        "-N",
        "-B",
        "--raw",
        "-e",
        sql,
    ]
    completed = subprocess.run(command, check=True, capture_output=True, text=True)
    rows: list[list[str]] = []
    for line in completed.stdout.splitlines():
        preserved = line.rstrip("\r\n")
        if preserved:
            rows.append(["" if value in {"NULL", r"\N"} else value for value in preserved.split("\t")])
    return rows


def _markers_from_creature_row(row: list[str], quest_index: dict[int, list[dict[str, Any]]]) -> list[MarkerRecord]:
    guid, entry, name, map_id, x, y, z, spawn_npcflag, template_npcflag, quest_count = row
    guid_value = int(guid)
    entry_value = int(entry)
    map_id_value = int(map_id)
    world_x = float(x)
    world_y = float(y)
    world_z = float(z)
    spawn_npcflag_value = int(spawn_npcflag)
    template_npcflag_value = int(template_npcflag)
    npcflag_value = spawn_npcflag_value | template_npcflag_value
    quest_count_value = int(quest_count)
    lowered_name = name.lower()
    if lowered_name.startswith("[dnd]") or "pedestal" in lowered_name:
        return []
    quests = quest_index.get(entry_value, [])

    base_metadata = {
        "npcflag": npcflag_value,
        "spawn_npcflag": spawn_npcflag_value,
        "template_npcflag": template_npcflag_value,
        "quest_count": quest_count_value,
        "quests": quests,
    }

    markers: list[MarkerRecord] = []
    if quest_count_value > 0:
        markers.append(
            MarkerRecord(
                uid=f"creature:{guid_value}:quest_giver",
                kind="quest_giver",
                label=name,
                object_type="creature",
                map_id=map_id_value,
                world_x=world_x,
                world_y=world_y,
                world_z=world_z,
                entry=entry_value,
                guid=guid_value,
                icon_relpath=MARKER_KIND_ICON_RELATIVE_PATHS["quest_giver"],
                metadata=base_metadata,
            )
        )

    for kind, flag in CREATURE_NPC_FLAGS.items():
        if kind == "quest_giver":
            continue
        if npcflag_value & flag:
            markers.append(
                MarkerRecord(
                    uid=f"creature:{guid_value}:{kind}",
                    kind=kind,
                    label=name,
                    object_type="creature",
                    map_id=map_id_value,
                    world_x=world_x,
                    world_y=world_y,
                    world_z=world_z,
                    entry=entry_value,
                    guid=guid_value,
                    icon_relpath=MARKER_KIND_ICON_RELATIVE_PATHS[kind],
                    metadata=base_metadata,
                )
            )
    return markers


def _marker_from_mailbox_row(row: list[str]) -> MarkerRecord:
    guid, entry, name, map_id, x, y, z = row
    return MarkerRecord(
        uid=f"gameobject:{guid}:mailbox",
        kind="mailbox",
        label=name,
        object_type="gameobject",
        map_id=int(map_id),
        world_x=float(x),
        world_y=float(y),
        world_z=float(z),
        entry=int(entry),
        guid=int(guid),
        icon_relpath=MARKER_KIND_ICON_RELATIVE_PATHS["mailbox"],
        metadata={},
    )


def _marker_from_resource_row(
    row: list[str],
    resource_item_icons: dict[str, str],
    resource_loot_by_entry: dict[int, list[dict[str, Any]]],
) -> MarkerRecord | None:
    guid, entry, name, map_id, x, y, z, lock_id, loot_entry_id = row
    kind = _resource_kind_from_name(name)
    if kind is None:
        return None
    item_name = _resource_item_name_from_node_name(name, kind)
    icon_relpath = resource_item_icons.get(item_name, MARKER_KIND_ICON_RELATIVE_PATHS[kind]) if item_name else ""
    loot_entry = _int_or_zero(loot_entry_id)
    resource_loot = [dict(item) for item in resource_loot_by_entry.get(loot_entry, [])]
    return MarkerRecord(
        uid=f"gameobject:{guid}:{kind}",
        kind=kind,
        label=name,
        object_type="gameobject",
        map_id=int(map_id),
        world_x=float(x),
        world_y=float(y),
        world_z=float(z),
        entry=int(entry),
        guid=int(guid),
        icon_relpath=icon_relpath,
        metadata={
            "lock_id": _int_or_zero(lock_id),
            "loot_entry_id": loot_entry,
            "resource_kind": kind,
            "resource_item_name": item_name or "",
            "resource_item_icon_relpath": icon_relpath,
            "resource_loot": resource_loot,
        },
    )


def _build_quest_payload_from_row(row: list[str]) -> dict[str, Any]:
    quest_id = _int_or_zero(row[1])
    quest_title = row[2]
    quest_level = _int_or_zero(row[3])
    min_level = _int_or_zero(row[4])
    allowable_races = _int_or_zero(row[5])

    objective_texts = [value for value in row[10:14] if value]

    requirement_lines: list[str] = []
    target_requirements = _build_target_requirements(row[14:26])
    item_requirements = _build_item_entries(row[26:44])
    requirement_lines.extend(_target_requirement_line(item) for item in target_requirements)
    requirement_lines.extend(_item_requirement_line(item) for item in item_requirements)

    reward_money = _int_or_zero(row[44])
    fixed_rewards = _build_item_entries(row[45:57])
    choice_rewards = _build_item_entries(row[57:75])
    reward_lines = []
    if reward_money:
        reward_lines.append(_format_money(reward_money))
    reward_lines.extend(_reward_line(item) for item in fixed_rewards)
    reward_lines.extend(_reward_line(item) for item in choice_rewards)

    prerequisite_quests = _build_related_quests(
        [
            {
                "quest_id": abs(_int_or_zero(row[75])),
                "title": row[76],
                "relation": "prerequisite",
                "raw_quest_id": _int_or_zero(row[75]),
            }
        ]
    )
    followup_quests = _build_related_quests(
        [
            {
                "quest_id": _int_or_zero(row[77]),
                "title": row[78],
                "relation": "next",
            },
            {
                "quest_id": _int_or_zero(row[79]),
                "title": row[80],
                "relation": "reward_next",
            },
        ]
    )
    breadcrumb_for = _build_related_quests(
        [
            {
                "quest_id": _int_or_zero(row[81]),
                "title": row[82],
                "relation": "breadcrumb_for",
            }
        ]
    )
    quest_sort_id = abs(_value_at(row, 83))
    quest_type = _value_at(row, 84)
    quest_flags = _value_at(row, 85)
    special_flags = _value_at(row, 86)
    has_event_relation = bool(_value_at(row, 87))
    has_event_starter = bool(_value_at(row, 88))
    is_event_quest = _is_event_quest(
        quest_type=quest_type,
        quest_sort_id=quest_sort_id,
        has_event_relation=has_event_relation,
        has_event_starter=has_event_starter,
    )
    is_daily_quest = bool(quest_flags & QUEST_FLAGS_DAILY)
    is_weekly_quest = bool(quest_flags & QUEST_FLAGS_WEEKLY)
    is_monthly_quest = bool(special_flags & QUEST_SPECIAL_FLAGS_MONTHLY)
    classification_tags = _quest_classification_tags(
        is_daily=is_daily_quest,
        is_weekly=is_weekly_quest,
        is_monthly=is_monthly_quest,
        is_event=is_event_quest,
        quest_sort_id=quest_sort_id,
        quest_type=quest_type,
    )

    return {
        "quest_id": quest_id,
        "title": quest_title,
        "quest_level": quest_level,
        "min_level": min_level,
        "faction": _faction_label_from_race_mask(allowable_races),
        "log_description": row[6],
        "quest_description": row[7],
        "area_description": row[8],
        "completion_log": row[9],
        "objective_texts": objective_texts,
        "target_requirements": target_requirements,
        "item_requirements": item_requirements,
        "requirement_lines": requirement_lines,
        "reward_money": reward_money,
        "fixed_rewards": fixed_rewards,
        "choice_rewards": choice_rewards,
        "reward_lines": reward_lines,
        "prerequisite_quests": prerequisite_quests,
        "followup_quests": followup_quests,
        "breadcrumb_for_quests": breadcrumb_for,
        "quest_sort_id": quest_sort_id,
        "quest_type": quest_type,
        "quest_flags": quest_flags,
        "special_flags": special_flags,
        "is_daily_quest": is_daily_quest,
        "is_weekly_quest": is_weekly_quest,
        "is_monthly_quest": is_monthly_quest,
        "is_event_quest": is_event_quest,
        "classification_tags": classification_tags,
    }


def _build_item_entries(values: list[str]) -> list[dict[str, Any]]:
    items: list[dict[str, Any]] = []
    for index in range(0, len(values), 3):
        if index + 2 >= len(values):
            break
        item_id = _int_or_zero(values[index])
        quantity = _int_or_zero(values[index + 1])
        item_name = values[index + 2]
        if item_id and quantity:
            items.append(
                {
                    "item_id": item_id,
                    "quantity": quantity,
                    "name": item_name or f"Item #{item_id}",
                }
            )
    return items


def _build_target_requirements(values: list[str]) -> list[dict[str, Any]]:
    requirements: list[dict[str, Any]] = []
    for index in range(0, len(values), 3):
        if index + 2 >= len(values):
            break
        required_id = _int_or_zero(values[index])
        required_count = _int_or_zero(values[index + 1])
        required_name = values[index + 2]
        if required_id and required_count:
            target_kind = "object" if required_id < 0 else "creature"
            requirements.append(
                {
                    "target_id": abs(required_id),
                    "quantity": required_count,
                    "name": required_name or f"{target_kind.title()} #{abs(required_id)}",
                    "target_kind": target_kind,
                }
            )
    return requirements


def _build_related_quests(values: list[dict[str, Any]]) -> list[dict[str, Any]]:
    related: list[dict[str, Any]] = []
    seen_ids: set[tuple[int, str]] = set()
    for value in values:
        quest_id = abs(_int_or_zero(value.get("quest_id", 0)))
        relation = str(value.get("relation", "related"))
        if not quest_id:
            continue
        dedupe_key = (quest_id, relation)
        if dedupe_key in seen_ids:
            continue
        seen_ids.add(dedupe_key)
        related.append(
            {
                "quest_id": quest_id,
                "title": str(value.get("title", "") or f"Quest #{quest_id}"),
                "relation": relation,
                "raw_quest_id": _int_or_zero(value.get("raw_quest_id", quest_id)),
            }
        )
    return related


def _target_requirement_line(item: dict[str, Any]) -> str:
    return f"0/{item['quantity']} {item['name']}"


def _item_requirement_line(item: dict[str, Any]) -> str:
    return f"0/{item['quantity']} {item['name']}"


def _reward_line(item: dict[str, Any]) -> str:
    return f"{item['quantity']}x {item['name']}"


def _int_or_zero(value: Any) -> int:
    try:
        if value in (None, ""):
            return 0
        return int(value)
    except (TypeError, ValueError):
        return 0


def _value_at(values: list[Any], index: int) -> int:
    if index >= len(values):
        return 0
    return _int_or_zero(values[index])


def _float_or_zero(value: Any) -> float:
    try:
        if value in (None, ""):
            return 0.0
        return float(value)
    except (TypeError, ValueError):
        return 0.0


def _quest_classification_tags(
    *,
    is_daily: bool,
    is_weekly: bool,
    is_monthly: bool,
    is_event: bool,
    quest_sort_id: int,
    quest_type: int,
) -> list[str]:
    normalized_quest_sort_id = abs(quest_sort_id)
    tags: list[str] = []
    if is_event or quest_type == QUEST_TYPE_WORLD_EVENT or normalized_quest_sort_id in EVENT_QUEST_SORT_IDS:
        tags.append("Event")
    if is_daily:
        tags.append("Daily")
    if is_weekly:
        tags.append("Weekly")
    if is_monthly:
        tags.append("Monthly")
    return tags


def _is_event_quest(*, quest_type: int, quest_sort_id: int, has_event_relation: bool, has_event_starter: bool) -> bool:
    normalized_quest_sort_id = abs(quest_sort_id)
    return (
        has_event_relation
        or has_event_starter
        or quest_type == QUEST_TYPE_WORLD_EVENT
        or normalized_quest_sort_id in EVENT_QUEST_SORT_IDS
    )


def _format_money(copper: int) -> str:
    gold, remainder = divmod(copper, 10000)
    silver, copper_value = divmod(remainder, 100)
    parts: list[str] = []
    if gold:
        parts.append(f"{gold}g")
    if silver:
        parts.append(f"{silver}s")
    if copper_value or not parts:
        parts.append(f"{copper_value}c")
    return " ".join(parts)


def _resource_kind_from_name(name: str) -> str | None:
    normalized_name = name.strip().lower()
    if not normalized_name:
        return None
    if any(keyword in normalized_name for keyword in ORE_NAME_KEYWORDS):
        return "ore"
    if any(keyword in normalized_name for keyword in HERB_NAME_KEYWORDS):
        return "herb"
    return None


def _resource_item_name_from_node_name(name: str, kind: str) -> str | None:
    normalized_name = name.strip()
    if not normalized_name:
        return None
    lowered_name = normalized_name.lower()
    if kind == "herb":
        return normalized_name
    if kind == "ore":
        return RESOURCE_ORE_ITEM_NAME_BY_NODE.get(lowered_name)
    return None


def _sql_quote_literal(value: str) -> str:
    return "'" + value.replace("\\", "\\\\").replace("'", "''") + "'"


@lru_cache(maxsize=4)
def _load_item_display_icon_index_from_dbc(dbc_path: Path) -> dict[int, str]:
    if not dbc_path.is_file():
        return {}

    with dbc_path.open("rb") as handle:
        magic, record_count, field_count, record_size, string_size = struct.unpack("<4s4I", handle.read(20))
        if magic != b"WDBC":
            return {}
        records = handle.read(record_count * record_size)
        string_block = handle.read(string_size)

    def read_string(offset: int) -> str:
        if not offset:
            return ""
        end = string_block.find(b"\x00", offset)
        if end < 0:
            end = len(string_block)
        return string_block[offset:end].decode("utf-8", errors="ignore").strip()

    result: dict[int, str] = {}
    unpack_format = f"<{field_count}I"
    for index in range(record_count):
        row = struct.unpack_from(unpack_format, records, index * record_size)
        display_id = int(row[0])
        icon_name = read_string(int(row[5]))
        if display_id > 0 and icon_name:
            result[display_id] = icon_name
    return result


def _resource_name_filter_sql(expression: str) -> str:
    return " OR ".join(
        f"{expression} LIKE '%{keyword.replace("'", "''")}%'" for keyword in RESOURCE_NAME_FILTER_TERMS
    )


def _faction_label_from_race_mask(mask: int) -> str:
    if mask == 0:
        return "Alliance & Horde"
    alliance_mask = 1 | 4 | 8 | 64 | 1024
    horde_mask = 2 | 16 | 32 | 128 | 512
    has_alliance = bool(mask & alliance_mask)
    has_horde = bool(mask & horde_mask)
    if has_alliance and has_horde:
        return "Alliance & Horde"
    if has_alliance:
        return "Alliance"
    if has_horde:
        return "Horde"
    return f"Race mask {mask}"
