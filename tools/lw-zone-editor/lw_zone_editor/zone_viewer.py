from __future__ import annotations

import argparse
from collections import defaultdict
import copy
import json
import tkinter as tk
from dataclasses import dataclass, field
from pathlib import Path
from tkinter import ttk
import re
from typing import Sequence

from PIL import Image, ImageDraw, ImageTk

from .client_assets import load_area_table_names, load_world_map_area_records
from .image_info import read_image_size
from .marker_cache import MARKER_KIND_LABELS, MarkerRecord, load_marker_cache, marker_icon_crop_box
from .paths import APP_ROOT, COMPOSITE_MAPS_DIR, PNG_ICONS_DIR, PNG_MAPS_DIR
from .settings import MARKER_DISPLAY_SETTINGS, ROUTE_SAMPLING_SETTINGS, ROUTE_STORAGE_SETTINGS

COMPOSITE_FILENAME_RE = re.compile(r"^(?P<zone_name>.+) - (?P<zone_id>\d+)\.png$", re.IGNORECASE)
REPO_ROOT = APP_ROOT.parents[1]
DBC_DIR = REPO_ROOT / "var" / "extractors" / "dbc"
MAP_DBC_PATH = DBC_DIR / "Map.dbc"
EDITOR_ROUTES_DIR = ROUTE_STORAGE_SETTINGS.editor_routes_dir
EXPORTED_ROUTES_DIR = ROUTE_STORAGE_SETTINGS.exported_routes_dir

MAP_COMMON = 0
MAP_INSTANCE = 1
MAP_RAID = 2
MAP_BATTLEGROUND = 3
MAP_ARENA = 4

EXPANSION_LABELS = {
    0: "Vanilla",
    1: "TBC",
    2: "WotLK",
}

CONTINENT_CATEGORY_BY_MAP_ID = {
    0: "Eastern Kingdoms",
    1: "Kalimdor",
    530: "Outland",
    571: "Northrend",
}

CATEGORY_ORDER = {
    "Eastern Kingdoms": 10,
    "Kalimdor": 20,
    "Outland": 30,
    "Northrend": 40,
    "Dungeons": 50,
    "Raids": 60,
    "Battlegrounds": 70,
    "Arenas": 80,
    "Other": 90,
}

EXPANSION_ORDER = {
    "Vanilla": 10,
    "TBC": 20,
    "WotLK": 30,
    "Other": 90,
}
TREE_SORT_ALPHA = "alpha"
TREE_SORT_ZONE = "zone"

ZOOM_MIN = 1.0
ZOOM_MAX = 8.0
ZOOM_STEP = 1.2
IMAGE_RESAMPLE = Image.Resampling.BICUBIC if hasattr(Image, "Resampling") else Image.BICUBIC
IMAGE_EXTENT = Image.Transform.EXTENT if hasattr(Image, "Transform") else Image.EXTENT
IMAGE_FLIP_LEFT_RIGHT = Image.Transpose.FLIP_LEFT_RIGHT if hasattr(Image, "Transpose") else Image.FLIP_LEFT_RIGHT
IMAGE_FLIP_TOP_BOTTOM = Image.Transpose.FLIP_TOP_BOTTOM if hasattr(Image, "Transpose") else Image.FLIP_TOP_BOTTOM
GENERATED_MARKER_ICON_SIZE = 18
DEFAULT_EDIT_SAMPLE_SPACING_YARDS = ROUTE_SAMPLING_SETTINGS.base_spacing_yards
DEFAULT_EDIT_BEZIER_SUBDIVISIONS = 24
EDIT_ANCHOR_HIT_RADIUS = 8.0
EDIT_HANDLE_HIT_RADIUS = 7.0
EDIT_SEGMENT_INSERT_HIT_RADIUS = 10.0
ADAPTIVE_SPACING_MIN_FACTOR = ROUTE_SAMPLING_SETTINGS.min_factor
ADAPTIVE_SPACING_MAX_FACTOR = ROUTE_SAMPLING_SETTINGS.max_factor
SHIFT_MASK = 0x0001
CONTROL_MASK = 0x0004
EDITOR_PATH_COLORS = (
    "#00e5ff",
    "#66bb6a",
    "#ffb300",
    "#ab47bc",
    "#ef5350",
)


@dataclass(slots=True)
class EditorAnchor:
    image_x: float
    image_y: float
    handle_in_x: float | None = None
    handle_in_y: float | None = None
    handle_out_x: float | None = None
    handle_out_y: float | None = None


@dataclass(slots=True)
class EditorPath:
    anchors: list[EditorAnchor] = field(default_factory=list)
    sampled_points: list[dict[str, object]] = field(default_factory=list)
    finalized: bool = False
    start_connection: EditorPathConnection | None = None
    end_connection: EditorPathConnection | None = None


@dataclass(slots=True)
class EditorPathState:
    paths: list[EditorPath] = field(default_factory=list)
    active_path_index: int | None = None
    export_payload: dict[str, object] | None = None


@dataclass(frozen=True, slots=True)
class EditorPathConnection:
    path_index: int
    anchor_index: int


@dataclass(frozen=True, slots=True)
class ZoneCoordinateTransform:
    world_map_area_id: int
    map_id: int
    zone_id: int
    world_y1: float
    world_y2: float
    world_x1: float
    world_x2: float

    def zone_to_world(self, zone_x: float, zone_y: float) -> tuple[float, float]:
        world_x = zone_y * ((self.world_x2 - self.world_x1) / 100.0) + self.world_x1
        world_y = zone_x * ((self.world_y2 - self.world_y1) / 100.0) + self.world_y1
        return world_x, world_y

    def world_to_zone(self, world_x: float, world_y: float) -> tuple[float, float]:
        zone_x = (world_y - self.world_y1) / ((self.world_y2 - self.world_y1) / 100.0)
        zone_y = (world_x - self.world_x1) / ((self.world_x2 - self.world_x1) / 100.0)
        return zone_x, zone_y

    def image_to_zone(self, image_x: float, image_y: float, image_width: int, image_height: int) -> tuple[float, float]:
        if image_width <= 0 or image_height <= 0:
            raise ValueError("image dimensions must be positive")
        zone_x = image_x * 100.0 / float(image_width)
        zone_y = image_y * 100.0 / float(image_height)
        return zone_x, zone_y

    def image_to_world(self, image_x: float, image_y: float, image_width: int, image_height: int) -> tuple[float, float]:
        zone_x, zone_y = self.image_to_zone(image_x, image_y, image_width, image_height)
        return self.zone_to_world(zone_x, zone_y)


@dataclass(frozen=True, slots=True)
class ZoneCompositeAsset:
    source_zone_name: str
    zone_name: str
    zone_id: int
    path: Path
    width: int
    height: int
    map_id: int | None = None
    map_name: str = ""
    map_type: int = MAP_COMMON
    expansion_id: int = 0
    expansion_label: str = "Vanilla"
    category_label: str = "Other"
    world_map_area_id: int | None = None
    floor: int = 0
    coordinate_transform: ZoneCoordinateTransform | None = None

    @property
    def label(self) -> str:
        return f"{self.zone_name} ({self.zone_id})"


@dataclass(frozen=True, slots=True)
class ZoneTreeChild:
    label: str
    asset: ZoneCompositeAsset


@dataclass(frozen=True, slots=True)
class ZoneTreeGroup:
    label: str
    children: tuple[ZoneTreeChild, ...]


@dataclass(frozen=True, slots=True)
class ZoneRenderState:
    canvas_width: int
    canvas_height: int
    draw_offset_x: int
    draw_offset_y: int
    draw_width: int
    draw_height: int
    viewport_x: float
    viewport_y: float
    viewport_width: float
    viewport_height: float
    display_scale: float

    def contains_canvas_point(self, canvas_x: float, canvas_y: float) -> bool:
        return (
            self.draw_offset_x <= canvas_x <= self.draw_offset_x + self.draw_width
            and self.draw_offset_y <= canvas_y <= self.draw_offset_y + self.draw_height
        )

    def canvas_to_image(self, canvas_x: float, canvas_y: float) -> tuple[float, float] | None:
        if not self.contains_canvas_point(canvas_x, canvas_y):
            return None
        local_x = (canvas_x - self.draw_offset_x) / float(self.draw_width)
        local_y = (canvas_y - self.draw_offset_y) / float(self.draw_height)
        image_x = self.viewport_x + local_x * self.viewport_width
        image_y = self.viewport_y + local_y * self.viewport_height
        return image_x, image_y

    def image_to_canvas(self, image_x: float, image_y: float) -> tuple[float, float] | None:
        if not (
            self.viewport_x <= image_x <= self.viewport_x + self.viewport_width
            and self.viewport_y <= image_y <= self.viewport_y + self.viewport_height
        ):
            return None
        local_x = (image_x - self.viewport_x) / float(self.viewport_width)
        local_y = (image_y - self.viewport_y) / float(self.viewport_height)
        canvas_x = self.draw_offset_x + local_x * self.draw_width
        canvas_y = self.draw_offset_y + local_y * self.draw_height
        return canvas_x, canvas_y


def compute_render_state(
    image_width: int,
    image_height: int,
    canvas_width: int,
    canvas_height: int,
    zoom_factor: float,
    viewport_x: float,
    viewport_y: float,
) -> ZoneRenderState:
    if image_width <= 0 or image_height <= 0:
        raise ValueError("image dimensions must be positive")
    if canvas_width <= 0 or canvas_height <= 0:
        raise ValueError("canvas dimensions must be positive")

    base_scale = min(canvas_width / float(image_width), canvas_height / float(image_height))
    display_scale = base_scale * zoom_factor
    viewport_width = min(float(image_width), canvas_width / display_scale)
    viewport_height = min(float(image_height), canvas_height / display_scale)

    max_viewport_x = max(0.0, float(image_width) - viewport_width)
    max_viewport_y = max(0.0, float(image_height) - viewport_height)
    clamped_viewport_x = min(max(0.0, viewport_x), max_viewport_x)
    clamped_viewport_y = min(max(0.0, viewport_y), max_viewport_y)

    draw_width = max(1, int(round(viewport_width * display_scale)))
    draw_height = max(1, int(round(viewport_height * display_scale)))
    draw_offset_x = max(0, (canvas_width - draw_width) // 2)
    draw_offset_y = max(0, (canvas_height - draw_height) // 2)

    return ZoneRenderState(
        canvas_width=canvas_width,
        canvas_height=canvas_height,
        draw_offset_x=draw_offset_x,
        draw_offset_y=draw_offset_y,
        draw_width=draw_width,
        draw_height=draw_height,
        viewport_x=clamped_viewport_x,
        viewport_y=clamped_viewport_y,
        viewport_width=viewport_width,
        viewport_height=viewport_height,
        display_scale=display_scale,
    )


def discover_zone_composites(composite_dir: str | Path = COMPOSITE_MAPS_DIR) -> list[ZoneCompositeAsset]:
    root = Path(composite_dir)
    if not root.is_dir():
        return []

    zone_metadata_index = _build_zone_metadata_index()

    assets: list[ZoneCompositeAsset] = []
    for path in sorted(root.glob("*.png"), key=lambda item: item.name.lower()):
        match = COMPOSITE_FILENAME_RE.match(path.name)
        if not match:
            continue

        zone_name = match.group("zone_name")
        zone_id = int(match.group("zone_id"))
        metadata = zone_metadata_index.get((zone_name.lower(), zone_id), {})
        if not metadata:
            metadata = zone_metadata_index.get((_normalize_name_for_lookup(zone_name), zone_id), {})
        if not metadata:
            metadata = zone_metadata_index.get((_normalize_name_for_lookup(zone_name), None), {})
        if not metadata:
            metadata = zone_metadata_index.get((_normalize_folderish_lookup(zone_name), None), {})

        display_zone_name = metadata.get("display_name", zone_name) if metadata else zone_name
        expansion_label, category_label = classify_zone_group(
            map_id=metadata.get("map_id"),
            map_type=metadata.get("map_type", MAP_COMMON),
            expansion_id=metadata.get("expansion_id", 0),
        )
        width, height = read_image_size(path)
        assets.append(
            ZoneCompositeAsset(
                source_zone_name=zone_name,
                zone_name=display_zone_name,
                zone_id=zone_id,
                path=path,
                width=width,
                height=height,
                map_id=metadata.get("map_id"),
                map_name=metadata.get("map_name", ""),
                map_type=metadata.get("map_type", MAP_COMMON),
                expansion_id=metadata.get("expansion_id", 0),
                expansion_label=expansion_label,
                category_label=category_label,
                world_map_area_id=metadata.get("world_map_area_id"),
                floor=metadata.get("floor", 0),
                coordinate_transform=metadata.get("coordinate_transform"),
            )
        )

    return assets


def classify_zone_group(map_id: int | None, map_type: int, expansion_id: int) -> tuple[str, str]:
    expansion_label = EXPANSION_LABELS.get(expansion_id, "Other")

    if map_type == MAP_INSTANCE:
        return expansion_label, "Dungeons"
    if map_type == MAP_RAID:
        return expansion_label, "Raids"
    if map_type == MAP_BATTLEGROUND:
        return expansion_label, "Battlegrounds"
    if map_type == MAP_ARENA:
        return expansion_label, "Arenas"

    category = CONTINENT_CATEGORY_BY_MAP_ID.get(map_id, "Other")
    return expansion_label, category


def _tree_asset_sort_key(asset: ZoneCompositeAsset, sort_mode: str) -> tuple[object, ...]:
    if sort_mode == TREE_SORT_ZONE:
        return (asset.zone_id, asset.zone_name.lower(), asset.source_zone_name.lower(), asset.path.name.lower())
    return (asset.zone_name.lower(), asset.zone_id, asset.source_zone_name.lower(), asset.path.name.lower())


def _tree_group_sort_key(group: ZoneTreeGroup, sort_mode: str) -> tuple[object, ...]:
    if not group.children:
        return (group.label.lower(), 0)
    root_asset = group.children[0].asset
    if sort_mode == TREE_SORT_ZONE:
        return (root_asset.zone_id, group.label.lower())
    return (group.label.lower(), root_asset.zone_id)


def _category_root_asset(category_label: str, assets: Sequence[ZoneCompositeAsset]) -> ZoneCompositeAsset | None:
    normalized_category = _normalize_name_for_lookup(category_label)
    exact_matches = [asset for asset in assets if _normalize_name_for_lookup(asset.zone_name) == normalized_category]
    if exact_matches:
        return exact_matches[0]
    if category_label == "Eastern Kingdoms":
        for asset in assets:
            if _normalize_name_for_lookup(asset.zone_name) == "azeroth":
                return asset
    return None


def _expansion_root_asset(expansion_label: str, assets: Sequence[ZoneCompositeAsset]) -> ZoneCompositeAsset | None:
    if expansion_label != "Vanilla":
        return None
    for candidate_name in ("World", "Azeroth"):
        normalized_candidate = _normalize_name_for_lookup(candidate_name)
        for asset in assets:
            if _normalize_name_for_lookup(asset.zone_name) == normalized_candidate:
                return asset
    return None


def group_assets_for_tree(assets: Sequence[ZoneCompositeAsset], *, sort_mode: str = TREE_SORT_ALPHA) -> dict[str, dict[str, list[ZoneCompositeAsset]]]:
    grouped: dict[str, dict[str, list[ZoneCompositeAsset]]] = defaultdict(lambda: defaultdict(list))
    for asset in assets:
        grouped[asset.expansion_label][asset.category_label].append(asset)

    ordered: dict[str, dict[str, list[ZoneCompositeAsset]]] = {}
    for expansion in sorted(grouped, key=lambda value: (EXPANSION_ORDER.get(value, 999), value.lower())):
        ordered_categories: dict[str, list[ZoneCompositeAsset]] = {}
        for category in sorted(grouped[expansion], key=lambda value: (CATEGORY_ORDER.get(value, 999), value.lower())):
            ordered_categories[category] = sorted(grouped[expansion][category], key=lambda asset: _tree_asset_sort_key(asset, sort_mode))
        ordered[expansion] = ordered_categories
    return ordered


def group_category_assets(assets: Sequence[ZoneCompositeAsset], *, sort_mode: str = TREE_SORT_ALPHA) -> tuple[list[ZoneCompositeAsset], list[ZoneTreeGroup]]:
    leaf_assets: list[ZoneCompositeAsset] = []
    multilevel_buckets: dict[str, dict[str, object]] = {}

    for asset in sorted(assets, key=lambda item: _tree_asset_sort_key(item, sort_mode)):
        if asset.category_label not in {"Dungeons", "Raids"}:
            leaf_assets.append(asset)
            continue

        parsed = _parse_multilevel_source_name(asset.source_zone_name)
        if parsed is None:
            leaf_assets.append(asset)
            continue

        base_label, level_index = parsed
        key = _normalize_name_for_lookup(base_label)
        bucket = multilevel_buckets.setdefault(
            key,
            {
                "label": _humanize_world_map_name(base_label),
                "indexed": [],
                "plain": [],
            },
        )
        cast_indexed = bucket["indexed"]
        assert isinstance(cast_indexed, list)
        cast_indexed.append((level_index, asset))

    grouped_assets: list[ZoneTreeGroup] = []
    for key in sorted(multilevel_buckets, key=str.lower):
        bucket = multilevel_buckets[key]
        label = str(bucket["label"])
        indexed_entries = list(bucket["indexed"])
        indexed_entries.sort(key=lambda item: (item[0], item[1].path.name.lower()))
        children = tuple(
            ZoneTreeChild(label=f"Level {level_index}", asset=asset)
            for level_index, asset in indexed_entries
        )
        grouped_assets.append(ZoneTreeGroup(label=label, children=children))

    floor_leaf_assets, floor_groups = _group_floor_variant_assets(leaf_assets)
    grouped_assets.extend(floor_groups)
    floor_leaf_assets.sort(key=lambda item: _tree_asset_sort_key(item, sort_mode))
    grouped_assets.sort(key=lambda item: _tree_group_sort_key(item, sort_mode))
    return floor_leaf_assets, grouped_assets


def _group_floor_variant_assets(assets: Sequence[ZoneCompositeAsset]) -> tuple[list[ZoneCompositeAsset], list[ZoneTreeGroup]]:
    buckets: dict[tuple[int | None, int, str], list[ZoneCompositeAsset]] = defaultdict(list)
    for asset in assets:
        buckets[(asset.map_id, asset.zone_id, _normalize_name_for_lookup(asset.zone_name))].append(asset)

    leaf_assets: list[ZoneCompositeAsset] = []
    grouped_assets: list[ZoneTreeGroup] = []
    for key, bucket_assets in sorted(
        buckets.items(),
        key=lambda item: (item[1][0].zone_name.lower(), item[1][0].zone_id, item[1][0].path.name.lower()),
    ):
        del key
        ordered_assets = sorted(bucket_assets, key=lambda asset: (asset.floor, asset.path.name.lower()))
        should_group = len(ordered_assets) > 1 and any(asset.floor > 0 for asset in ordered_assets)
        if not should_group:
            leaf_assets.extend(ordered_assets)
            continue
        grouped_assets.append(
            ZoneTreeGroup(
                label=ordered_assets[0].zone_name,
                children=tuple(
                    ZoneTreeChild(label=_asset_floor_tree_label(asset), asset=asset)
                    for asset in ordered_assets
                ),
            )
        )
    return leaf_assets, grouped_assets


def _asset_floor_tree_label(asset: ZoneCompositeAsset) -> str:
    if asset.floor <= 0:
        return "Surface"
    return f"Floor {asset.floor}"


def _related_overlay_assets_for_edit(
    assets: Sequence[ZoneCompositeAsset],
    current_asset: ZoneCompositeAsset,
) -> list[ZoneCompositeAsset]:
    if current_asset.coordinate_transform is None:
        return []
    related_assets = [
        asset
        for asset in assets
        if asset.path != current_asset.path
        and asset.coordinate_transform is not None
        and asset.map_id == current_asset.map_id
        and asset.zone_id == current_asset.zone_id
    ]
    return sorted(related_assets, key=lambda asset: (asset.floor, asset.zone_name.lower(), asset.path.name.lower()))


def _project_overlay_asset_into_base_image(
    overlay_asset: ZoneCompositeAsset,
    base_asset: ZoneCompositeAsset,
) -> tuple[int, int, int, int, bool, bool] | None:
    overlay_transform = overlay_asset.coordinate_transform
    base_transform = base_asset.coordinate_transform
    if overlay_transform is None or base_transform is None:
        return None

    def _base_image_coords_for_overlay_point(image_x: float, image_y: float) -> tuple[float, float]:
        world_x, world_y = overlay_transform.image_to_world(image_x, image_y, overlay_asset.width, overlay_asset.height)
        zone_x, zone_y = base_transform.world_to_zone(world_x, world_y)
        return (
            zone_x * base_asset.width / 100.0,
            zone_y * base_asset.height / 100.0,
        )

    top_left = _base_image_coords_for_overlay_point(0.0, 0.0)
    top_right = _base_image_coords_for_overlay_point(float(overlay_asset.width), 0.0)
    bottom_left = _base_image_coords_for_overlay_point(0.0, float(overlay_asset.height))
    bottom_right = _base_image_coords_for_overlay_point(float(overlay_asset.width), float(overlay_asset.height))
    projected_points = (top_left, top_right, bottom_left, bottom_right)
    left = int(round(min(point[0] for point in projected_points)))
    right = int(round(max(point[0] for point in projected_points)))
    top = int(round(min(point[1] for point in projected_points)))
    bottom = int(round(max(point[1] for point in projected_points)))
    width = max(1, right - left)
    height = max(1, bottom - top)
    flip_x = top_right[0] < top_left[0]
    flip_y = bottom_left[1] < top_left[1]
    return left, top, width, height, flip_x, flip_y


def load_map_records(dbc_path: str | Path = MAP_DBC_PATH) -> dict[int, dict]:
    path = Path(dbc_path)
    if not path.is_file():
        return {}

    data = path.read_bytes()
    record_count, field_count, record_size, string_size = __import__("struct").unpack("<4I", data[4:20])
    records = data[20:20 + record_count * record_size]
    strings = data[20 + record_count * record_size:20 + record_count * record_size + string_size]

    def get_string(offset: int) -> str:
        if offset <= 0 or offset >= len(strings):
            return ""
        end = strings.find(b"\0", offset)
        if end == -1:
            end = len(strings)
        return strings[offset:end].decode("utf-8", errors="ignore")

    records_by_id: dict[int, dict] = {}
    for index in range(record_count):
        row = __import__("struct").unpack_from(f"<{field_count}I", records, index * record_size)
        map_id = int(row[0])
        records_by_id[map_id] = {
            "map_id": map_id,
            "map_type": int(row[2]),
            "name": get_string(int(row[5])).strip(),
            "linked_zone": int(row[22]),
            "multimap_id": int(row[57]),
            "expansion_id": int(row[63]),
            "max_players": int(row[65]),
        }
    return records_by_id


def _build_zone_metadata_index() -> dict[tuple[str, int], dict]:
    map_records = load_map_records()
    area_names = load_area_table_names()

    index: dict[tuple[str, int], dict] = {}
    for record in load_world_map_area_records():
        zone_id = int(record["zone_id"])
        zone_name = area_names.get(zone_id) or _humanize_world_map_name(str(record["internal_name"]))
        map_id = int(record["map_id"])
        map_record = map_records.get(map_id, {})
        payload = {
            "world_map_area_id": int(record["world_map_area_id"]),
            "map_id": map_id,
            "map_type": int(map_record.get("map_type", MAP_COMMON)),
            "map_name": str(map_record.get("name", "")),
            "expansion_id": int(map_record.get("expansion_id", 0)),
            "display_name": zone_name,
            "floor": int(record["floor"]),
            "coordinate_transform": _build_zone_coordinate_transform(record),
        }
        index[(zone_name.lower(), zone_id)] = payload
        index[(_normalize_name_for_lookup(zone_name), zone_id)] = payload
        index.setdefault((_normalize_name_for_lookup(zone_name), None), payload)
        index.setdefault((_normalize_folderish_lookup(zone_name), None), payload)
    return index


def _build_zone_coordinate_transform(record: dict) -> ZoneCoordinateTransform:
    return ZoneCoordinateTransform(
        world_map_area_id=int(record["world_map_area_id"]),
        map_id=int(record["map_id"]),
        zone_id=int(record["zone_id"]),
        world_y1=float(record["left"]),
        world_y2=float(record["right"]),
        world_x1=float(record["top"]),
        world_x2=float(record["bottom"]),
    )


def _humanize_world_map_name(name: str) -> str:
    humanized = name.replace("_", " ")
    humanized = re.sub(r"(?<=[a-z])(?=[A-Z])", " ", humanized)
    humanized = re.sub(r"(?<=[A-Za-z])(?=[0-9])", " ", humanized)
    humanized = re.sub(r"(?<=[0-9])(?=[A-Za-z])", " ", humanized)
    return re.sub(r"\s+", " ", humanized).strip()


def _normalize_name_for_lookup(name: str) -> str:
    return re.sub(r"[^a-z0-9]+", "", name.lower())


def _normalize_folderish_lookup(name: str) -> str:
    lowered = _normalize_name_for_lookup(name)
    return re.sub(r"\d+$", "", lowered)


def _slugify_path_component(name: str) -> str:
    slug = re.sub(r"[^a-z0-9]+", "_", name.lower()).strip("_")
    return slug or "value"


def _route_filename_base(zone_name: str, zone_id: int, map_id: int | None) -> str:
    zone_slug = _slugify_path_component(zone_name)
    map_fragment = f"map_{map_id:03d}" if map_id is not None else "map_unknown"
    return f"{map_fragment}__zone_{zone_id}__{zone_slug}"


def _asset_world_bounds(asset: ZoneCompositeAsset) -> tuple[float, float, float, float] | None:
    transform = asset.coordinate_transform
    if transform is None:
        return None
    return (
        min(transform.world_x1, transform.world_x2),
        max(transform.world_x1, transform.world_x2),
        min(transform.world_y1, transform.world_y2),
        max(transform.world_y1, transform.world_y2),
    )


def _bounds_intersect(
    first: tuple[float, float, float, float],
    second: tuple[float, float, float, float],
) -> bool:
    first_min_x, first_max_x, first_min_y, first_max_y = first
    second_min_x, second_max_x, second_min_y, second_max_y = second
    return not (
        first_max_x < second_min_x
        or second_max_x < first_min_x
        or first_max_y < second_min_y
        or second_max_y < first_min_y
    )


def _route_payload_world_bounds(payload: dict[str, object]) -> tuple[float, float, float, float] | None:
    raw_paths = payload.get("paths", [])
    if not isinstance(raw_paths, list):
        return None

    min_world_x: float | None = None
    max_world_x: float | None = None
    min_world_y: float | None = None
    max_world_y: float | None = None

    def _update_bounds(world_x: float, world_y: float) -> None:
        nonlocal min_world_x, max_world_x, min_world_y, max_world_y
        min_world_x = world_x if min_world_x is None else min(min_world_x, world_x)
        max_world_x = world_x if max_world_x is None else max(max_world_x, world_x)
        min_world_y = world_y if min_world_y is None else min(min_world_y, world_y)
        max_world_y = world_y if max_world_y is None else max(max_world_y, world_y)

    for raw_path in raw_paths:
        if not isinstance(raw_path, dict):
            continue
        for collection_name in ("movement_points", "anchors"):
            raw_points = raw_path.get(collection_name, [])
            if not isinstance(raw_points, list):
                continue
            for point in raw_points:
                if not isinstance(point, dict):
                    continue
                try:
                    _update_bounds(float(point.get("world_x")), float(point.get("world_y")))
                except (TypeError, ValueError):
                    continue

    if min_world_x is None or max_world_x is None or min_world_y is None or max_world_y is None:
        return None
    return (min_world_x, max_world_x, min_world_y, max_world_y)


def _route_payload_overlaps_asset(payload: dict[str, object], asset: ZoneCompositeAsset) -> bool:
    asset_bounds = _asset_world_bounds(asset)
    payload_bounds = _route_payload_world_bounds(payload)
    if asset_bounds is None or payload_bounds is None:
        return False
    return _bounds_intersect(asset_bounds, payload_bounds)


def _merge_view_route_payloads(payloads: Sequence[dict[str, object]]) -> dict[str, object]:
    merged_paths: list[dict[str, object]] = []
    movement_point_count = 0
    for payload in payloads:
        raw_paths = payload.get("paths", [])
        if not isinstance(raw_paths, list):
            continue
        for raw_path in raw_paths:
            if isinstance(raw_path, dict):
                merged_paths.append(raw_path)
        try:
            movement_point_count += int(payload.get("movement_point_count", 0))
        except (TypeError, ValueError):
            continue

    return {
        "path_count": len(merged_paths),
        "movement_point_count": movement_point_count,
        "paths": merged_paths,
    }


def _route_export_group_key(route_path: Path) -> str:
    return route_path.name.rsplit("__", 1)[0]


def _route_export_preference(route_path: Path) -> tuple[int, float]:
    try:
        modified_time = route_path.stat().st_mtime
    except OSError:
        modified_time = 0.0
    is_canonical_runtime = route_path.name.endswith("__routes.json")
    return (1 if is_canonical_runtime else 0, modified_time)


def _route_point_to_asset_image_coords(
    point: dict[str, object],
    asset: ZoneCompositeAsset,
) -> tuple[float, float] | None:
    transform = asset.coordinate_transform
    if transform is None:
        return None

    try:
        world_x = float(point.get("world_x"))
        world_y = float(point.get("world_y"))
    except (TypeError, ValueError):
        try:
            return float(point.get("image_x")), float(point.get("image_y"))
        except (TypeError, ValueError):
            return None

    zone_x, zone_y = transform.world_to_zone(world_x, world_y)
    return (
        zone_x * asset.width / 100.0,
        zone_y * asset.height / 100.0,
    )


def _build_route_editor_filename(zone_name: str, zone_id: int, map_id: int | None) -> str:
    return f"{_route_filename_base(zone_name, zone_id, map_id)}__editor.json"


def _build_route_runtime_filename(zone_name: str, zone_id: int, map_id: int | None) -> str:
    return f"{_route_filename_base(zone_name, zone_id, map_id)}__routes.json"


def _build_route_export_filename(
    zone_name: str,
    zone_id: int,
    map_id: int | None,
    route_group_key: str,
) -> str:
    route_slug = _slugify_path_component(route_group_key)
    return f"{_route_filename_base(zone_name, zone_id, map_id)}__{route_slug}.json"


def _editor_path_export_key(route_group_key: str, path_index: int) -> str:
    return f"{route_group_key}_{path_index + 1:02d}"


def _parse_multilevel_source_name(name: str) -> tuple[str, int] | None:
    match = re.match(r"^(?P<base>.+?)\s+(?P<index>\d+)$", name.strip())
    if not match:
        return None
    return match.group("base"), int(match.group("index"))


class ZoneViewerApp(tk.Tk):
    def __init__(self, assets: list[ZoneCompositeAsset], initial_zone: str | None = None) -> None:
        super().__init__()
        self.title("LivingWorld Zone Viewer")
        self.geometry("1280x900")
        self.minsize(960, 640)

        self.assets = assets
        self.filtered_assets = list(assets)
        self.tree_asset_by_item: dict[str, ZoneCompositeAsset] = {}
        self.tree_special_item_ids: dict[str, str] = {}
        self.current_asset: ZoneCompositeAsset | None = None
        self.current_image: Image.Image | None = None
        self.current_photo: ImageTk.PhotoImage | None = None
        self.current_render_state: ZoneRenderState | None = None
        self.zoom_factor = ZOOM_MIN
        self.viewport_x = 0.0
        self.viewport_y = 0.0
        self.drag_state: dict[str, float | bool] | None = None
        self.marker_photo_cache: dict[str, ImageTk.PhotoImage] = {}
        self.marker_image_cache: dict[str, Image.Image] = {}
        self.editor_overlay_image_cache: dict[Path, Image.Image] = {}
        self.visible_marker_hitboxes: list[tuple[tuple[float, float, float, float], MarkerRecord]] = []
        self.all_markers, self.marker_cache_payload = load_marker_cache()
        self.objective_area_index = dict(self.marker_cache_payload.get("objective_area_index", {}))
        self.quest_route_graph = list(self.marker_cache_payload.get("quest_route_graph", []))
        self.quest_starter_marker_index = _build_quest_starter_marker_index(self.all_markers)
        self.current_zone_markers: list[MarkerRecord] = []
        self.current_view_route_payload: dict[str, object] | None = None
        self.current_view_route_path: Path | None = None
        self.selected_marker: MarkerRecord | None = None
        self.active_objective_overlay_id: str | None = None
        self.properties_tree_payload_by_item: dict[str, dict] = {}
        self.edit_mode_enabled = False
        self.editor_handles_visible = True
        self.editor_path_state = EditorPathState()
        self.editor_undo_history: list[EditorPathState] = []
        self.editor_drag_target: tuple[str, int, int] | None = None
        self.editor_path_name_var = tk.StringVar(value="route_001")
        self.properties_header_var = tk.StringVar(value="Properties")
        self.properties_text_header_var = tk.StringVar(value="Quest Details")

        self.search_var = tk.StringVar()
        self.tree_sort_var = tk.StringVar(value=TREE_SORT_ALPHA)
        self.status_var = tk.StringVar(value="Ready")
        self.details_var = tk.StringVar(value="Select a zone composite to load it.")
        self.marker_details_var = tk.StringVar(value="Markers: no selection")
        self.local_coords_var = tk.StringVar(value="Local: —")
        self.world_coords_var = tk.StringVar(value="World: —")
        self.marker_visibility_vars = {
            kind: tk.BooleanVar(value=True)
            for kind in MARKER_KIND_LABELS
        }
        self.event_quest_visibility_var = tk.BooleanVar(value=False)
        self.view_route_points_var = tk.BooleanVar(value=False)
        self.quest_level_min_var = tk.StringVar(value="")
        self.quest_level_max_var = tk.StringVar(value="")
        self.quest_level_min_var.trace_add("write", lambda *_args: self._on_marker_visibility_changed())
        self.quest_level_max_var.trace_add("write", lambda *_args: self._on_marker_visibility_changed())

        self._build_layout()
        self._populate_tree()
        self._select_initial_asset(initial_zone)

    def _build_layout(self) -> None:
        self.columnconfigure(0, weight=1)
        self.rowconfigure(0, weight=1)

        container = ttk.Panedwindow(self, orient=tk.HORIZONTAL)
        container.grid(row=0, column=0, sticky="nsew")

        left_frame = ttk.Frame(container, padding=10)
        left_frame.columnconfigure(0, weight=1)
        left_frame.rowconfigure(3, weight=1)
        container.add(left_frame, weight=0)

        ttk.Label(left_frame, text="Zones", font=("Segoe UI", 11, "bold")).grid(row=0, column=0, sticky="w")
        sort_frame = ttk.Frame(left_frame)
        sort_frame.grid(row=1, column=0, sticky="w", pady=(8, 0))
        ttk.Label(sort_frame, text="Sort:").grid(row=0, column=0, sticky="w")
        ttk.Radiobutton(
            sort_frame,
            text="Alph",
            value=TREE_SORT_ALPHA,
            variable=self.tree_sort_var,
            command=self._on_tree_sort_changed,
        ).grid(row=0, column=1, sticky="w", padx=(8, 0))
        ttk.Radiobutton(
            sort_frame,
            text="Zone",
            value=TREE_SORT_ZONE,
            variable=self.tree_sort_var,
            command=self._on_tree_sort_changed,
        ).grid(row=0, column=2, sticky="w", padx=(8, 0))

        search_entry = ttk.Entry(left_frame, textvariable=self.search_var)
        search_entry.grid(row=2, column=0, sticky="ew", pady=(8, 8))
        search_entry.bind("<KeyRelease>", self._on_search_changed)

        tree_frame = ttk.Frame(left_frame)
        tree_frame.grid(row=3, column=0, sticky="nsew")
        tree_frame.columnconfigure(0, weight=1)
        tree_frame.rowconfigure(0, weight=1)

        self.zone_tree = ttk.Treeview(tree_frame, show="tree", selectmode="browse")
        self.zone_tree.grid(row=0, column=0, sticky="nsew")
        self.zone_tree.bind("<<TreeviewSelect>>", self._on_zone_selected)

        zone_scrollbar = ttk.Scrollbar(tree_frame, orient=tk.VERTICAL, command=self.zone_tree.yview)
        zone_scrollbar.grid(row=0, column=1, sticky="ns")
        self.zone_tree.configure(yscrollcommand=zone_scrollbar.set)

        center_frame = ttk.Frame(container, padding=10)
        center_frame.columnconfigure(0, weight=1)
        center_frame.rowconfigure(3, weight=1)
        container.add(center_frame, weight=1)

        self.properties_frame = ttk.Frame(container, padding=10)
        self.properties_frame.columnconfigure(0, weight=1)
        self.properties_frame.columnconfigure(1, weight=0)
        self.properties_frame.rowconfigure(2, weight=1)
        self.properties_frame.rowconfigure(4, weight=1)
        container.add(self.properties_frame, weight=0)

        ttk.Label(center_frame, textvariable=self.details_var, wraplength=900, justify=tk.LEFT).grid(
            row=0, column=0, sticky="ew", pady=(0, 8)
        )

        self.marker_toggle_frame = ttk.LabelFrame(center_frame, text="Markers", padding=8)
        self.marker_toggle_frame.grid(row=2, column=0, sticky="ew", pady=(0, 8))
        marker_checkbox_columns = 4
        for column in range(marker_checkbox_columns):
            self.marker_toggle_frame.columnconfigure(column, weight=1)

        marker_toggle_items = [
            *MARKER_KIND_LABELS.items(),
            ("event_quests", "Event Quests"),
        ]
        for index, (kind, label) in enumerate(marker_toggle_items):
            variable = (
                self.event_quest_visibility_var if kind == "event_quests" else self.marker_visibility_vars[kind]
            )
            ttk.Checkbutton(
                self.marker_toggle_frame,
                text=label,
                variable=variable,
                command=self._on_marker_visibility_changed,
            ).grid(
                row=index // marker_checkbox_columns,
                column=index % marker_checkbox_columns,
                sticky="w",
                padx=(0, 18),
                pady=(0, 6),
            )

        quest_level_row = (len(marker_toggle_items) + marker_checkbox_columns - 1) // marker_checkbox_columns
        quest_level_frame = ttk.Frame(self.marker_toggle_frame)
        quest_level_frame.grid(
            row=quest_level_row,
            column=0,
            columnspan=marker_checkbox_columns,
            sticky="w",
            pady=(8, 0),
        )
        ttk.Label(quest_level_frame, text="Quest Level:").grid(row=0, column=0, sticky="w")
        ttk.Entry(quest_level_frame, textvariable=self.quest_level_min_var, width=6).grid(
            row=0,
            column=1,
            sticky="w",
            padx=(8, 0),
        )
        ttk.Label(quest_level_frame, text="to").grid(row=0, column=2, sticky="w", padx=(8, 8))
        ttk.Entry(quest_level_frame, textvariable=self.quest_level_max_var, width=6).grid(
            row=0,
            column=3,
            sticky="w",
        )

        ttk.Checkbutton(
            quest_level_frame,
            text="Show Route",
            variable=self.view_route_points_var,
            command=self._on_view_route_overlay_changed,
        ).grid(row=0, column=4, sticky="w", padx=(18, 0))

        self.image_frame = ttk.Frame(center_frame, relief=tk.SUNKEN)
        self.image_frame.grid(row=3, column=0, sticky="nsew")
        self.image_frame.columnconfigure(0, weight=1)
        self.image_frame.rowconfigure(0, weight=1)
        self.image_frame.bind("<Configure>", self._on_image_frame_configured)

        self.image_canvas = tk.Canvas(self.image_frame, background="#1e1e1e", highlightthickness=0)
        self.image_canvas.grid(row=0, column=0, sticky="nsew")
        self.image_canvas.bind("<Motion>", self._on_image_motion)
        self.image_canvas.bind("<Leave>", self._on_image_leave)
        self.image_canvas.bind("<ButtonPress-1>", self._on_drag_started)
        self.image_canvas.bind("<ButtonPress-3>", self._on_secondary_click)
        self.image_canvas.bind("<B1-Motion>", self._on_drag_moved)
        self.image_canvas.bind("<ButtonRelease-1>", self._on_drag_ended)
        self.image_canvas.bind("<ButtonPress-2>", self._on_middle_drag_started)
        self.image_canvas.bind("<B2-Motion>", self._on_middle_drag_moved)
        self.image_canvas.bind("<ButtonRelease-2>", self._on_middle_drag_ended)
        self.image_canvas.bind("<MouseWheel>", self._on_mouse_wheel)
        self.bind("<Control-z>", self._on_editor_undo_shortcut)
        self.bind("<Control-Z>", self._on_editor_undo_shortcut)
        self.bind("<Control-h>", self._on_editor_toggle_handles_shortcut)
        self.bind("<Control-H>", self._on_editor_toggle_handles_shortcut)
        self.bind("<Return>", self._on_editor_finalize_path_shortcut)

        coords_frame = ttk.Frame(self.image_frame, padding=(8, 4))
        coords_frame.grid(row=1, column=0, sticky="ew")
        coords_frame.columnconfigure(0, weight=1)
        coords_frame.columnconfigure(1, weight=1)

        ttk.Label(coords_frame, textvariable=self.local_coords_var, anchor=tk.W).grid(row=0, column=0, sticky="w")
        ttk.Label(coords_frame, textvariable=self.world_coords_var, anchor=tk.E).grid(row=0, column=1, sticky="e")

        ttk.Label(self.properties_frame, textvariable=self.properties_header_var, font=("Segoe UI", 11, "bold")).grid(row=0, column=0, sticky="w")
        self.edit_mode_button = ttk.Button(self.properties_frame, text="Enter Edit Mode", command=self._toggle_edit_mode)
        self.edit_mode_button.grid(row=0, column=1, sticky="e", padx=(8, 0))
        ttk.Label(self.properties_frame, textvariable=self.marker_details_var, wraplength=300, justify=tk.LEFT).grid(
            row=1, column=0, sticky="ew", pady=(8, 8)
        )

        self.property_tree_frame = ttk.Frame(self.properties_frame)
        self.property_tree_frame.grid(row=2, column=0, columnspan=2, sticky="nsew")
        self.property_tree_frame.columnconfigure(0, weight=1)
        self.property_tree_frame.rowconfigure(0, weight=1)

        self.properties_tree = ttk.Treeview(self.property_tree_frame, show="tree", selectmode="browse")
        self.properties_tree.grid(row=0, column=0, sticky="nsew")
        self.properties_tree.bind("<<TreeviewSelect>>", self._on_properties_tree_selected)
        self.properties_tree.bind("<Shift-ButtonRelease-1>", self._on_properties_tree_shift_click)

        property_tree_scrollbar = ttk.Scrollbar(self.property_tree_frame, orient=tk.VERTICAL, command=self.properties_tree.yview)
        property_tree_scrollbar.grid(row=0, column=1, sticky="ns")
        self.properties_tree.configure(yscrollcommand=property_tree_scrollbar.set)

        self.editor_toolbox_frame = ttk.LabelFrame(self.properties_frame, text="Path Toolbox", padding=8)
        self.editor_toolbox_frame.columnconfigure(1, weight=1)
        ttk.Label(self.editor_toolbox_frame, text="Path Key").grid(row=0, column=0, sticky="w")
        ttk.Entry(self.editor_toolbox_frame, textvariable=self.editor_path_name_var).grid(row=0, column=1, sticky="ew", padx=(8, 0))
        ttk.Button(self.editor_toolbox_frame, text="Undo Last Edit", command=self._undo_editor_anchor).grid(row=1, column=0, sticky="ew", pady=(10, 0))
        ttk.Button(self.editor_toolbox_frame, text="Clear Path", command=self._clear_editor_path).grid(row=1, column=1, sticky="ew", padx=(8, 0), pady=(10, 0))
        ttk.Button(self.editor_toolbox_frame, text="Save Route Files", command=self._save_editor_export_json).grid(row=2, column=0, columnspan=2, sticky="ew", pady=(8, 0))
        ttk.Label(
            self.editor_toolbox_frame,
            text=(
                "Left-click to add anchors. Drag white anchors to move them. Drag blue handle dots to shape curves. "
                "Shift-click an existing anchor to start a branch from it, snap the current path onto it, or close a loop by snapping back to an earlier anchor on the same path. "
                f"Movement nodes are rebuilt on save with the smart adaptive sampler (base {int(DEFAULT_EDIT_SAMPLE_SPACING_YARDS)} yd). "
                "Ctrl+Left-click a curve to insert an anchor there, and Ctrl+Right-click an anchor to delete it. "
                "Middle-mouse drag pans while editing. Press Enter to finish the current path, Ctrl+Z to undo, "
                "and Ctrl+H to hide or show curve handles."
            ),
            wraplength=280,
            justify=tk.LEFT,
        ).grid(row=3, column=0, columnspan=2, sticky="ew", pady=(10, 0))

        ttk.Label(self.properties_frame, textvariable=self.properties_text_header_var, font=("Segoe UI", 10, "bold")).grid(row=3, column=0, sticky="w", pady=(8, 4))

        self.text_frame = ttk.Frame(self.properties_frame)
        self.text_frame.grid(row=4, column=0, columnspan=2, sticky="nsew")
        self.text_frame.columnconfigure(0, weight=1)
        self.text_frame.rowconfigure(0, weight=1)

        self.properties_text = tk.Text(self.text_frame, wrap=tk.WORD, height=16, width=42, cursor="xterm")
        self.properties_text.grid(row=0, column=0, sticky="nsew")
        self.properties_text.bind("<KeyPress>", self._on_properties_text_keypress)
        self.properties_text.bind("<Control-c>", self._copy_properties_text_selection)
        self.properties_text.bind("<Control-C>", self._copy_properties_text_selection)
        self.properties_text.bind("<Control-a>", self._select_all_properties_text)
        self.properties_text.bind("<Control-A>", self._select_all_properties_text)

        properties_text_scrollbar = ttk.Scrollbar(self.text_frame, orient=tk.VERTICAL, command=self.properties_text.yview)
        properties_text_scrollbar.grid(row=0, column=1, sticky="ns")
        self.properties_text.configure(yscrollcommand=properties_text_scrollbar.set)

        self.editor_toolbox_frame.grid_remove()

        status = ttk.Label(self, textvariable=self.status_var, anchor=tk.W, relief=tk.GROOVE)
        status.grid(row=1, column=0, sticky="ew")

    def _populate_tree(self) -> None:
        self.tree_asset_by_item.clear()
        self.tree_special_item_ids.clear()
        for item_id in self.zone_tree.get_children():
            self.zone_tree.delete(item_id)

        sort_mode = self.tree_sort_var.get()
        grouped = group_assets_for_tree(self.filtered_assets, sort_mode=sort_mode)
        for expansion, categories in grouped.items():
            expansion_id = self.zone_tree.insert("", tk.END, text=expansion, open=True)
            expansion_root_asset = _expansion_root_asset(
                expansion,
                [asset for category_assets in categories.values() for asset in category_assets],
            )
            if expansion_root_asset is not None:
                self.tree_asset_by_item[expansion_id] = expansion_root_asset
                self.tree_special_item_ids[f"expansion:{expansion}"] = expansion_id
            for category, assets in categories.items():
                category_id = self.zone_tree.insert(expansion_id, tk.END, text=category, open=True)
                category_root_asset = _category_root_asset(category, assets)
                if category_root_asset is not None:
                    self.tree_asset_by_item[category_id] = category_root_asset
                    self.tree_special_item_ids[f"category:{expansion}:{category}"] = category_id

                leaf_assets, grouped_assets = group_category_assets(assets, sort_mode=sort_mode)
                for asset in leaf_assets:
                    if asset == expansion_root_asset or asset == category_root_asset:
                        continue
                    item_id = self.zone_tree.insert(category_id, tk.END, text=asset.label)
                    self.tree_asset_by_item[item_id] = asset
                for asset_group in grouped_assets:
                    visible_children = [
                        child
                        for child in asset_group.children
                        if child.asset != expansion_root_asset and child.asset != category_root_asset
                    ]
                    if not visible_children:
                        continue
                    group_id = self.zone_tree.insert(category_id, tk.END, text=asset_group.label, open=True)
                    for child in visible_children:
                        item_id = self.zone_tree.insert(group_id, tk.END, text=child.label)
                        self.tree_asset_by_item[item_id] = child.asset

    def _select_initial_asset(self, initial_zone: str | None) -> None:
        if not self.filtered_assets:
            self.status_var.set("No explored zone composites were found.")
            return

        index = 0
        if initial_zone:
            lowered = initial_zone.lower()
            for candidate_index, asset in enumerate(self.filtered_assets):
                if lowered in asset.zone_name.lower() or lowered == str(asset.zone_id):
                    index = candidate_index
                    break
            self._select_tree_asset(self.filtered_assets[index])
            return

        vanilla_item_id = self.tree_special_item_ids.get("expansion:Vanilla")
        if vanilla_item_id is not None:
            self._select_tree_item(vanilla_item_id)
            return

        self._select_tree_asset(self.filtered_assets[index])

    def _on_search_changed(self, _event: tk.Event | None = None) -> None:
        previous_asset = self.current_asset
        query = self.search_var.get().strip().lower()
        if not query:
            self.filtered_assets = list(self.assets)
        else:
            self.filtered_assets = [
                asset
                for asset in self.assets
                if query in asset.zone_name.lower() or query in str(asset.zone_id)
            ]

        self._populate_tree()
        if self.filtered_assets:
            if previous_asset in self.filtered_assets:
                self._select_tree_asset(previous_asset)
            else:
                self._select_initial_asset(None)
        else:
            self.current_asset = None
            self.current_image = None
            self.current_photo = None
            self.current_render_state = None
            self.current_zone_markers = []
            self.selected_marker = None
            self.image_canvas.delete("all")
            self.details_var.set("No matching zones found.")
            self.status_var.set("Filter returned no zones.")
            self.marker_details_var.set("Markers: no selection")
            self._clear_properties_panel()
            self._reset_coordinate_readouts()

    def _on_zone_selected(self, _event: tk.Event | None = None) -> None:
        selection = self.zone_tree.selection()
        if not selection:
            return
        item_id = selection[0]
        asset = self.tree_asset_by_item.get(item_id)
        if asset is None:
            return
        self._apply_marker_defaults_for_tree_item(item_id)
        self._load_asset(asset)

    def _load_asset(self, asset: ZoneCompositeAsset) -> None:
        self.current_asset = asset
        self.current_image = Image.open(asset.path).convert("RGBA")
        self.zoom_factor = ZOOM_MIN
        self.viewport_x = 0.0
        self.viewport_y = 0.0
        self.drag_state = None
        self.editor_drag_target = None
        self.current_view_route_payload = None
        self.current_view_route_path = None
        self.current_zone_markers = self._filter_markers_for_asset(asset)
        self.selected_marker = None
        self.active_objective_overlay_id = None
        self.editor_path_state = EditorPathState()
        self.editor_undo_history = []
        floor_suffix = f" | Floor: {asset.floor}" if asset.floor else ""
        world_map_area_suffix = f" | WorldMapArea: {asset.world_map_area_id}" if asset.world_map_area_id is not None else ""
        self.details_var.set(
            f"{asset.zone_name} — zone {asset.zone_id} — {asset.width}x{asset.height}px\n"
            f"Expansion: {asset.expansion_label} | Group: {asset.category_label} | Map ID: {asset.map_id if asset.map_id is not None else '?'}{floor_suffix}{world_map_area_suffix}\n"
            f"{asset.path}"
        )
        loaded_route_path: Path | None = None
        loaded_view_route_summary = self._load_saved_view_route_overlay()
        if self.edit_mode_enabled:
            loaded_route_path = self._load_saved_editor_route(force=True)
            self._refresh_edit_mode_ui()
        else:
            self.marker_details_var.set(
                f"Markers: {len(self.current_zone_markers)} cached in this zone. Click an icon to inspect it."
            )
            self._populate_properties_for_marker(None)
        if loaded_route_path is not None:
            self.status_var.set(f"Loaded {asset.label} with route {loaded_route_path.name}.")
        elif loaded_view_route_summary is not None:
            self.status_var.set(f"Loaded {asset.label} with route overlay {loaded_view_route_summary}.")
        else:
            self.status_var.set(f"Loaded {asset.label}")
        self._reset_coordinate_readouts()
        self._render_current_image()

    def _select_tree_item(self, item_id: str) -> None:
        asset = self.tree_asset_by_item.get(item_id)
        if asset is None:
            return
        self.zone_tree.selection_set(item_id)
        self.zone_tree.focus(item_id)
        self.zone_tree.see(item_id)
        self._apply_marker_defaults_for_tree_item(item_id)
        self._load_asset(asset)

    def _apply_marker_defaults_for_tree_item(self, item_id: str) -> None:
        self.event_quest_visibility_var.set(False)
        heavy_root_item_ids = {
            self.tree_special_item_ids.get("expansion:Vanilla"),
            self.tree_special_item_ids.get("category:Vanilla:Eastern Kingdoms"),
            self.tree_special_item_ids.get("category:Vanilla:Kalimdor"),
        }
        if item_id not in heavy_root_item_ids:
            return
        for variable in self.marker_visibility_vars.values():
            variable.set(False)

    def _select_tree_asset(self, asset: ZoneCompositeAsset) -> None:
        for item_id, candidate in self.tree_asset_by_item.items():
            if candidate == asset:
                self._select_tree_item(item_id)
                return

    def _on_tree_sort_changed(self) -> None:
        previous_asset = self.current_asset
        self._populate_tree()
        if previous_asset in self.filtered_assets:
            self._select_tree_asset(previous_asset)
            return
        self._select_initial_asset(None)

    def _on_editor_toggle_handles_shortcut(self, _event: tk.Event | None = None) -> str:
        self.editor_handles_visible = not self.editor_handles_visible
        visibility_label = "shown" if self.editor_handles_visible else "hidden"
        if self.edit_mode_enabled and self.current_asset is not None and self.current_image is not None:
            self._render_current_image()
        self.status_var.set(f"Curve handles {visibility_label}.")
        return "break"

    def _toggle_edit_mode(self) -> None:
        if self.current_asset is None:
            self.status_var.set("Select a zone map before entering edit mode.")
            return
        self.edit_mode_enabled = not self.edit_mode_enabled
        if not self.edit_mode_enabled:
            self.editor_drag_target = None
            self.drag_state = None
            self.selected_marker = None
            self.image_canvas.configure(cursor="")
        else:
            loaded_route_path = self._load_saved_editor_route(force=not self.editor_path_state.paths)
            if loaded_route_path is not None:
                self.status_var.set(f"Loaded route {loaded_route_path.name} for {self.current_asset.label}.")
        self._refresh_edit_mode_ui()
        self._render_current_image()

    def _saved_editor_route_path(self, asset: ZoneCompositeAsset) -> Path:
        return EDITOR_ROUTES_DIR / _build_route_editor_filename(asset.zone_name, asset.zone_id, asset.map_id)

    def _saved_runtime_route_path(self, asset: ZoneCompositeAsset) -> Path:
        return EXPORTED_ROUTES_DIR / _build_route_runtime_filename(asset.zone_name, asset.zone_id, asset.map_id)

    def _find_loadable_route_path(self, asset: ZoneCompositeAsset) -> Path | None:
        editor_path = self._saved_editor_route_path(asset)
        if editor_path.exists():
            return editor_path

        runtime_path = self._saved_runtime_route_path(asset)
        if runtime_path.exists():
            return runtime_path

        legacy_pattern = f"{_route_filename_base(asset.zone_name, asset.zone_id, asset.map_id)}__*.json"
        legacy_paths = [
            path
            for path in EXPORTED_ROUTES_DIR.glob(legacy_pattern)
            if path.name != runtime_path.name
        ]
        if not legacy_paths:
            return None
        return max(legacy_paths, key=lambda candidate: candidate.stat().st_mtime)

    def _load_saved_editor_route(self, *, force: bool = False) -> Path | None:
        if self.current_asset is None or self.current_asset.coordinate_transform is None:
            return None
        if self.editor_path_state.paths and not force:
            return None

        route_path = self._find_loadable_route_path(self.current_asset)
        if route_path is None:
            return None

        try:
            payload = json.loads(route_path.read_text(encoding="utf-8"))
            path_state, route_group_key, _sample_spacing_yards = _deserialize_editor_path_state(payload)
        except (OSError, json.JSONDecodeError, ValueError) as exc:
            self.status_var.set(f"Could not load route file {route_path.name}: {exc}")
            return None

        self.editor_path_state = path_state
        self.editor_undo_history = []
        self.editor_drag_target = None
        self.editor_path_name_var.set(route_group_key)
        return route_path

    def _load_saved_view_route_overlay(self) -> str | None:
        if self.current_asset is None or self.current_asset.coordinate_transform is None:
            return None

        allowed_map_id = self.current_asset.map_id
        if allowed_map_id is None:
            allowed_map_id = self.current_asset.coordinate_transform.map_id

        matched_by_group: dict[str, tuple[Path, dict[str, object]]] = {}
        for route_path in sorted(EXPORTED_ROUTES_DIR.glob("*.json"), key=lambda candidate: candidate.name.lower()):
            if route_path.name.endswith("__editor.json"):
                continue
            try:
                payload = json.loads(route_path.read_text(encoding="utf-8"))
            except (OSError, json.JSONDecodeError):
                continue
            if not isinstance(payload, dict) or not isinstance(payload.get("paths"), list):
                continue
            try:
                payload_map_id = int(payload.get("map_id"))
            except (TypeError, ValueError):
                continue
            if payload_map_id != allowed_map_id:
                continue
            if not _route_payload_overlaps_asset(payload, self.current_asset):
                continue

            group_key = _route_export_group_key(route_path)
            existing = matched_by_group.get(group_key)
            if existing is not None and _route_export_preference(existing[0]) >= _route_export_preference(route_path):
                continue
            matched_by_group[group_key] = (route_path, payload)

        matched_paths = [entry[0] for entry in matched_by_group.values()]
        matched_payloads = [entry[1] for entry in matched_by_group.values()]

        if not matched_payloads:
            self.current_view_route_payload = None
            self.current_view_route_path = None
            return None

        if len(matched_payloads) == 1:
            self.current_view_route_payload = matched_payloads[0]
            self.current_view_route_path = matched_paths[0]
            return matched_paths[0].name

        self.current_view_route_payload = _merge_view_route_payloads(matched_payloads)
        self.current_view_route_path = None
        return f"{len(matched_payloads)} route files"

    def _refresh_edit_mode_ui(self) -> None:
        if self.edit_mode_enabled:
            self.properties_header_var.set("Path Toolbox")
            self.properties_text_header_var.set("Movement Path Export")
            self.edit_mode_button.configure(text="Exit Edit Mode")
            self.property_tree_frame.grid_remove()
            self.editor_toolbox_frame.grid(row=2, column=0, columnspan=2, sticky="nsew")
            self.image_canvas.configure(cursor="crosshair")
            zone_label = self.current_asset.label if self.current_asset is not None else "No zone loaded"
            self.marker_details_var.set(
                "Edit Mode active. Click the map to place anchors, drag white points to move anchors, "
                f"drag blue handle dots to shape curves, and Shift-click an existing anchor to branch from it or close a loop. Current zone: {zone_label}."
            )
            self._rebuild_editor_export()
        else:
            self.properties_header_var.set("Properties")
            self.properties_text_header_var.set("Quest Details")
            self.edit_mode_button.configure(text="Enter Edit Mode")
            self.editor_toolbox_frame.grid_remove()
            self.property_tree_frame.grid(row=2, column=0, columnspan=2, sticky="nsew")
            self.image_canvas.configure(cursor="")
            if self.selected_marker is not None:
                self.marker_details_var.set(self._format_marker_details(self.selected_marker))
                self._populate_properties_for_marker(self.selected_marker)
            else:
                self.marker_details_var.set(
                    f"Markers: {len(self.current_zone_markers)} cached in this zone. Click an icon to inspect it."
                )
                self._clear_properties_panel()

    def _editor_sample_spacing_yards(self) -> float:
        return DEFAULT_EDIT_SAMPLE_SPACING_YARDS

    def _on_view_route_overlay_changed(self) -> None:
        if self.current_asset is not None and self.current_image is not None:
            self._render_current_image()

    def _editor_paths(self) -> list[EditorPath]:
        return self.editor_path_state.paths

    def _push_editor_undo_state(self) -> None:
        snapshot = copy.deepcopy(self.editor_path_state)
        snapshot.export_payload = None
        self.editor_undo_history.append(snapshot)
        if len(self.editor_undo_history) > 100:
            self.editor_undo_history.pop(0)

    def _restore_editor_undo_state(self) -> bool:
        if not self.editor_undo_history:
            return False
        self.editor_path_state = self.editor_undo_history.pop()
        self.editor_drag_target = None
        self._rebuild_editor_export()
        self._render_current_image()
        return True

    def _active_editor_path(self) -> EditorPath | None:
        active_index = self.editor_path_state.active_path_index
        if active_index is None:
            return None
        if 0 <= active_index < len(self.editor_path_state.paths):
            return self.editor_path_state.paths[active_index]
        self.editor_path_state.active_path_index = None
        return None

    def _ensure_active_editor_path(self) -> EditorPath:
        active_path = self._active_editor_path()
        if active_path is not None and not active_path.finalized:
            return active_path
        active_path = EditorPath()
        self.editor_path_state.paths.append(active_path)
        self.editor_path_state.active_path_index = len(self.editor_path_state.paths) - 1
        return active_path

    def _active_editor_path_index(self) -> int | None:
        active_path = self._active_editor_path()
        if active_path is None:
            return None
        return self.editor_path_state.active_path_index

    def _editor_overlay_assets(self) -> list[ZoneCompositeAsset]:
        if self.current_asset is None:
            return []
        return _related_overlay_assets_for_edit(self.assets, self.current_asset)

    def _append_editor_anchor_to_path(self, path: EditorPath, image_x: float, image_y: float) -> EditorAnchor:
        new_anchor = EditorAnchor(image_x=image_x, image_y=image_y)
        if path.anchors:
            previous = path.anchors[-1]
            default_out_x, default_out_y, default_in_x, default_in_y = _default_curve_handles_between_points(
                previous.image_x,
                previous.image_y,
                image_x,
                image_y,
            )
            previous.handle_out_x = default_out_x
            previous.handle_out_y = default_out_y
            new_anchor.handle_in_x = default_in_x
            new_anchor.handle_in_y = default_in_y
        path.anchors.append(new_anchor)
        path.finalized = False
        path.end_connection = None
        return new_anchor

    def _add_editor_anchor(self, image_x: float, image_y: float) -> None:
        self._push_editor_undo_state()
        path = self._ensure_active_editor_path()
        self._append_editor_anchor_to_path(path, image_x, image_y)
        self._rebuild_editor_export()
        active_index = self._active_editor_path_index()
        path_label = f"path {active_index + 1}" if active_index is not None else "path"
        self.status_var.set(f"Added {path_label} anchor #{len(path.anchors)}")
        self._render_current_image()

    def _start_branch_from_existing_anchor(self, source_path_index: int, source_anchor_index: int) -> None:
        self._push_editor_undo_state()
        source_anchor = self.editor_path_state.paths[source_path_index].anchors[source_anchor_index]
        branch_path = EditorPath(
            anchors=[EditorAnchor(image_x=source_anchor.image_x, image_y=source_anchor.image_y)],
            start_connection=EditorPathConnection(path_index=source_path_index, anchor_index=source_anchor_index),
        )
        self.editor_path_state.paths.append(branch_path)
        self.editor_path_state.active_path_index = len(self.editor_path_state.paths) - 1
        self._rebuild_editor_export()
        self.status_var.set(
            f"Started branch path {self.editor_path_state.active_path_index + 1} from P{source_path_index + 1}-A{source_anchor_index + 1}."
        )
        self._render_current_image()

    def _attach_active_path_to_anchor(self, source_path_index: int, source_anchor_index: int) -> None:
        active_path = self._active_editor_path()
        active_path_index = self._active_editor_path_index()
        if active_path is None or active_path_index is None:
            self._start_branch_from_existing_anchor(source_path_index, source_anchor_index)
            return
        if not active_path.anchors:
            self._start_branch_from_existing_anchor(source_path_index, source_anchor_index)
            return
        is_self_attachment = active_path_index == source_path_index
        if is_self_attachment:
            last_anchor_index = len(active_path.anchors) - 1
            if source_anchor_index == last_anchor_index:
                self.status_var.set("Shift-click an earlier anchor on the same path to close a loop.")
                return
            if len(active_path.anchors) < 2:
                self.status_var.set("Add at least one more point before closing a loop.")
                return
        source_anchor = self.editor_path_state.paths[source_path_index].anchors[source_anchor_index]
        last_anchor = active_path.anchors[-1]
        self._push_editor_undo_state()
        if not _points_nearly_equal((last_anchor.image_x, last_anchor.image_y), (source_anchor.image_x, source_anchor.image_y)):
            self._append_editor_anchor_to_path(active_path, source_anchor.image_x, source_anchor.image_y)
        active_path.end_connection = EditorPathConnection(path_index=source_path_index, anchor_index=source_anchor_index)
        if len(active_path.anchors) < 2:
            self.status_var.set("Add at least one more point before attaching a connection.")
            return
        active_path.finalized = True
        self.editor_path_state.active_path_index = None
        self._rebuild_editor_export()
        if is_self_attachment:
            self.status_var.set(
                f"Closed loop by attaching path {active_path_index + 1} to P{source_path_index + 1}-A{source_anchor_index + 1}."
            )
        else:
            self.status_var.set(
                f"Attached branch to P{source_path_index + 1}-A{source_anchor_index + 1} and finished path {active_path_index + 1}."
            )
        self._render_current_image()

    def _undo_editor_anchor(self) -> None:
        if not self._restore_editor_undo_state():
            self.status_var.set("No edit-mode changes to undo.")
            return
        self.status_var.set("Undid last edit.")

    def _clear_editor_path(self) -> None:
        if self.editor_path_state.paths:
            self._push_editor_undo_state()
        self.editor_path_state = EditorPathState()
        self.editor_drag_target = None
        self._rebuild_editor_export()
        self.status_var.set("Cleared edit-mode path.")
        self._render_current_image()

    def _save_editor_export_json(self) -> None:
        if self.current_asset is None or self.current_asset.coordinate_transform is None:
            self.status_var.set("Load a zone map before saving routes.")
            return

        active_path = self._active_editor_path()
        if active_path is not None:
            if len(active_path.anchors) >= 2:
                self._finalize_active_editor_path()
            else:
                self.status_var.set(
                    "The active path has fewer than 2 anchors. Add another anchor or undo it before saving."
                )
                return

        _mark_complete_paths_finalized(self.editor_path_state)
        self._rebuild_editor_export()

        payload = self.editor_path_state.export_payload
        if not payload:
            self.status_var.set("No sampled path export is available yet.")
            return

        route_group_key = self.editor_path_name_var.get().strip() or "route_001"
        sample_spacing_yards = self._editor_sample_spacing_yards()
        editor_payload = _serialize_editor_path_state(
            self.editor_path_state,
            route_group_key=route_group_key,
            sample_spacing_yards=sample_spacing_yards,
            asset=self.current_asset,
        )

        EDITOR_ROUTES_DIR.mkdir(parents=True, exist_ok=True)
        EXPORTED_ROUTES_DIR.mkdir(parents=True, exist_ok=True)

        editor_path = self._saved_editor_route_path(self.current_asset)
        export_path = self._saved_runtime_route_path(self.current_asset)
        editor_path.write_text(json.dumps(editor_payload, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
        export_path.write_text(json.dumps(payload, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
        self.current_view_route_payload = payload
        self.current_view_route_path = export_path
        total_points = sum(len(path.sampled_points) for path in self.editor_path_state.paths)
        self.status_var.set(
            f"Saved route source to {editor_path.name} and {total_points} movement points to {export_path.name}."
        )

    def _finalize_active_editor_path(self) -> None:
        path = self._active_editor_path()
        active_index = self._active_editor_path_index()
        if path is None or active_index is None:
            self.status_var.set("No active path is being edited.")
            return
        if len(path.anchors) < 2:
            self.status_var.set("Add at least 2 anchors before finishing a path.")
            return
        self._push_editor_undo_state()
        path.finalized = True
        self.editor_path_state.active_path_index = None
        self._rebuild_editor_export()
        self.status_var.set(f"Finished path {active_index + 1}. Next left-click starts a new path.")
        self._render_current_image()

    def _on_editor_undo_shortcut(self, _event: tk.Event | None = None) -> str | None:
        if not self.edit_mode_enabled:
            return None
        self._undo_editor_anchor()
        return "break"

    def _on_editor_finalize_path_shortcut(self, _event: tk.Event | None = None) -> str | None:
        if not self.edit_mode_enabled:
            return None
        self._finalize_active_editor_path()
        return "break"

    def _rebuild_editor_export(self) -> None:
        if not self.edit_mode_enabled or self.current_asset is None or self.current_asset.coordinate_transform is None:
            return

        if not self.editor_path_state.paths:
            self.editor_path_state.export_payload = None
            self._set_properties_text(
                "Edit Mode\n\nLeft-click to add anchors. Press Enter to finish the current path.\n"
                "Each finished path is resampled into adaptive world-space movement points on save."
            )
            return

        path_exports: list[dict[str, object]] = []
        total_points = 0
        complete_path_count = 0
        group_key = self.editor_path_name_var.get().strip() or "route_001"
        for path_index, path in enumerate(self.editor_path_state.paths):
            if len(path.anchors) >= 2:
                path.sampled_points = _sample_editor_path(
                    path.anchors,
                    transform=self.current_asset.coordinate_transform,
                    image_width=self.current_asset.width,
                    image_height=self.current_asset.height,
                    map_id=self.current_asset.map_id,
                    spacing_yards=self._editor_sample_spacing_yards(),
                    subdivisions=DEFAULT_EDIT_BEZIER_SUBDIVISIONS,
                )
                complete_path_count += 1
                total_points += len(path.sampled_points)
            else:
                path.sampled_points = []

            anchor_payload = [
                _editor_anchor_payload(
                    anchor,
                    transform=self.current_asset.coordinate_transform,
                    image_width=self.current_asset.width,
                    image_height=self.current_asset.height,
                )
                for anchor in path.anchors
            ]
            path_exports.append(
                {
                    "path_index": path_index,
                    "path_key": _editor_path_export_key(group_key, path_index),
                    "finalized": path.finalized,
                    "start_connection": _editor_path_connection_payload(path.start_connection, group_key),
                    "end_connection": _editor_path_connection_payload(path.end_connection, group_key),
                    "anchors": anchor_payload,
                    "movement_points": list(path.sampled_points),
                }
            )

        if complete_path_count == 0:
            self.editor_path_state.export_payload = None
            self._set_properties_text(
                "Edit Mode\n\nAdd at least 2 anchors to a path before it can export movement points.\n"
                "Press Enter to finish a path so the next left-click begins a new one."
            )
            return

        export_payload = {
            "route_group_key": group_key,
            "zone_name": self.current_asset.zone_name,
            "zone_id": self.current_asset.zone_id,
            "map_id": self.current_asset.map_id,
            "world_map_area_id": self.current_asset.world_map_area_id,
            "sample_spacing_yards": self._editor_sample_spacing_yards(),
            "path_count": len(self.editor_path_state.paths),
            "movement_point_count": total_points,
            "paths": path_exports,
        }
        self.editor_path_state.export_payload = export_payload
        self._set_properties_text(json.dumps(export_payload, indent=2, ensure_ascii=False))

    def _render_editor_overlay(self) -> None:
        if not self.edit_mode_enabled or self.current_render_state is None or self.current_asset is None:
            return

        for overlay_asset in self._editor_overlay_assets():
            projected = _project_overlay_asset_into_base_image(overlay_asset, self.current_asset)
            if projected is None:
                continue
            left, top, width, height, _flip_x, _flip_y = projected
            top_left = self.current_render_state.image_to_canvas(float(left), float(top))
            bottom_right = self.current_render_state.image_to_canvas(float(left + width), float(top + height))
            if top_left is None or bottom_right is None:
                continue
            self.image_canvas.create_rectangle(
                top_left[0],
                top_left[1],
                bottom_right[0],
                bottom_right[1],
                outline="#90caf9",
                dash=(6, 4),
                width=2,
            )
            self.image_canvas.create_text(
                top_left[0] + 6,
                top_left[1] + 6,
                anchor=tk.NW,
                text=_asset_floor_tree_label(overlay_asset),
                fill="#e3f2fd",
                font=("Segoe UI", 9, "bold"),
            )

        for path_index, path in enumerate(self.editor_path_state.paths):
            if not path.anchors:
                continue
            path_color = EDITOR_PATH_COLORS[path_index % len(EDITOR_PATH_COLORS)]
            dense_points = _sample_bezier_polyline(path.anchors, DEFAULT_EDIT_BEZIER_SUBDIVISIONS)
            canvas_points: list[tuple[float, float]] = []
            for image_x, image_y in dense_points:
                canvas_point = self.current_render_state.image_to_canvas(image_x, image_y)
                if canvas_point is not None:
                    canvas_points.append(canvas_point)
            if len(canvas_points) >= 2:
                flattened = [coordinate for point in canvas_points for coordinate in point]
                self.image_canvas.create_line(*flattened, fill=path_color, width=3, smooth=True)

            for sampled_point in path.sampled_points:
                image_x = float(sampled_point.get("image_x", 0.0))
                image_y = float(sampled_point.get("image_y", 0.0))
                canvas_point = self.current_render_state.image_to_canvas(image_x, image_y)
                if canvas_point is None:
                    continue
                self.image_canvas.create_oval(
                    canvas_point[0] - 3,
                    canvas_point[1] - 3,
                    canvas_point[0] + 3,
                    canvas_point[1] + 3,
                    fill="#ffee58",
                    outline="#f57f17",
                    width=1,
                )

            for anchor_index, anchor in enumerate(path.anchors):
                anchor_canvas = self.current_render_state.image_to_canvas(anchor.image_x, anchor.image_y)
                if anchor_canvas is None:
                    continue
                canvas_x, canvas_y = anchor_canvas
                if self.editor_handles_visible and anchor.handle_in_x is not None and anchor.handle_in_y is not None:
                    handle_canvas = self.current_render_state.image_to_canvas(anchor.handle_in_x, anchor.handle_in_y)
                    if handle_canvas is not None:
                        self.image_canvas.create_line(handle_canvas[0], handle_canvas[1], canvas_x, canvas_y, fill="#5c6bc0", dash=(4, 3), width=2)
                        self._draw_editor_handle(*handle_canvas)
                if self.editor_handles_visible and anchor.handle_out_x is not None and anchor.handle_out_y is not None:
                    handle_canvas = self.current_render_state.image_to_canvas(anchor.handle_out_x, anchor.handle_out_y)
                    if handle_canvas is not None:
                        self.image_canvas.create_line(canvas_x, canvas_y, handle_canvas[0], handle_canvas[1], fill="#5c6bc0", dash=(4, 3), width=2)
                        self._draw_editor_handle(*handle_canvas)
                self.image_canvas.create_oval(
                    canvas_x - EDIT_ANCHOR_HIT_RADIUS,
                    canvas_y - EDIT_ANCHOR_HIT_RADIUS,
                    canvas_x + EDIT_ANCHOR_HIT_RADIUS,
                    canvas_y + EDIT_ANCHOR_HIT_RADIUS,
                    fill="#ffffff",
                    outline="#263238",
                    width=2,
                )
                self.image_canvas.create_text(
                    canvas_x + 12,
                    canvas_y - 10,
                    text=f"P{path_index + 1}-A{anchor_index + 1}",
                    fill="#ffffff",
                    font=("Segoe UI", 9, "bold"),
                    anchor=tk.W,
                )

    def _render_view_route_overlay(self) -> None:
        if self.edit_mode_enabled or self.current_render_state is None:
            return
        if self.current_view_route_payload is None or self.current_asset is None:
            return
        if not self.view_route_points_var.get():
            return

        raw_paths = self.current_view_route_payload.get("paths", [])
        if not isinstance(raw_paths, list):
            return

        for raw_path in raw_paths:
            if not isinstance(raw_path, dict):
                continue
            raw_points = raw_path.get("movement_points", [])
            if not isinstance(raw_points, list):
                continue

            canvas_points: list[tuple[float, float]] = []
            for point in raw_points:
                if not isinstance(point, dict):
                    continue
                image_coords = _route_point_to_asset_image_coords(point, self.current_asset)
                if image_coords is None:
                    continue
                image_x, image_y = image_coords
                canvas_point = self.current_render_state.image_to_canvas(image_x, image_y)
                if canvas_point is not None:
                    canvas_points.append(canvas_point)

            for canvas_x, canvas_y in canvas_points:
                self.image_canvas.create_oval(
                    canvas_x - 2,
                    canvas_y - 2,
                    canvas_x + 2,
                    canvas_y + 2,
                    fill="#ffee58",
                    outline="#f57f17",
                    width=1,
                )

    def _draw_editor_handle(self, canvas_x: float, canvas_y: float) -> None:
        self.image_canvas.create_oval(
            canvas_x - EDIT_HANDLE_HIT_RADIUS,
            canvas_y - EDIT_HANDLE_HIT_RADIUS,
            canvas_x + EDIT_HANDLE_HIT_RADIUS,
            canvas_y + EDIT_HANDLE_HIT_RADIUS,
            fill="#64b5f6",
            outline="#0d47a1",
            width=2,
        )

    def _hit_test_editor_target(self, canvas_x: float, canvas_y: float) -> tuple[str, int, int] | None:
        if self.current_render_state is None:
            return None
        for path_index in range(len(self.editor_path_state.paths) - 1, -1, -1):
            path = self.editor_path_state.paths[path_index]
            for anchor_index in range(len(path.anchors) - 1, -1, -1):
                anchor = path.anchors[anchor_index]
                hit_targets = [("anchor", anchor.image_x, anchor.image_y, EDIT_ANCHOR_HIT_RADIUS + 2.0)]
                if self.editor_handles_visible:
                    hit_targets = [
                        ("handle_in", anchor.handle_in_x, anchor.handle_in_y, EDIT_HANDLE_HIT_RADIUS + 2.0),
                        ("handle_out", anchor.handle_out_x, anchor.handle_out_y, EDIT_HANDLE_HIT_RADIUS + 2.0),
                        *hit_targets,
                    ]
                for target_kind, handle_x, handle_y, radius in hit_targets:
                    if handle_x is None or handle_y is None:
                        continue
                    canvas_point = self.current_render_state.image_to_canvas(handle_x, handle_y)
                    if canvas_point is None:
                        continue
                    if _distance_between_points(canvas_x, canvas_y, canvas_point[0], canvas_point[1]) <= radius:
                        return target_kind, path_index, anchor_index
        return None

    def _move_editor_target(self, target_kind: str, path_index: int, anchor_index: int, image_x: float, image_y: float) -> None:
        anchor = self.editor_path_state.paths[path_index].anchors[anchor_index]
        self.editor_path_state.active_path_index = path_index
        self.editor_path_state.paths[path_index].finalized = False
        if target_kind == "anchor":
            _move_anchor_with_handles(anchor, image_x, image_y)
            _propagate_connected_anchor_position(self.editor_path_state, path_index, anchor_index)
        elif target_kind == "handle_in":
            anchor.handle_in_x = image_x
            anchor.handle_in_y = image_y
        elif target_kind == "handle_out":
            anchor.handle_out_x = image_x
            anchor.handle_out_y = image_y

    def _find_editor_segment_insert_target(
        self,
        canvas_x: float,
        canvas_y: float,
    ) -> tuple[int, int, float, float] | None:
        if self.current_render_state is None:
            return None

        best_match: tuple[int, int, float, float] | None = None
        best_distance = EDIT_SEGMENT_INSERT_HIT_RADIUS + 0.01
        for path_index in range(len(self.editor_path_state.paths) - 1, -1, -1):
            path = self.editor_path_state.paths[path_index]
            if len(path.anchors) < 2:
                continue
            sampled_points = _sample_bezier_polyline_with_segment_indices(path.anchors, DEFAULT_EDIT_BEZIER_SUBDIVISIONS)
            previous_point = sampled_points[0]
            previous_canvas = self.current_render_state.image_to_canvas(previous_point[0], previous_point[1])
            for current_point in sampled_points[1:]:
                current_canvas = self.current_render_state.image_to_canvas(current_point[0], current_point[1])
                if previous_canvas is None or current_canvas is None:
                    previous_point = current_point
                    previous_canvas = current_canvas
                    continue
                distance, _projected_canvas_x, _projected_canvas_y, ratio = _project_point_onto_segment(
                    canvas_x,
                    canvas_y,
                    previous_canvas[0],
                    previous_canvas[1],
                    current_canvas[0],
                    current_canvas[1],
                )
                if distance < best_distance:
                    projected_image_x = previous_point[0] + (current_point[0] - previous_point[0]) * ratio
                    projected_image_y = previous_point[1] + (current_point[1] - previous_point[1]) * ratio
                    best_distance = distance
                    best_match = (path_index, current_point[2], projected_image_x, projected_image_y)
                previous_point = current_point
                previous_canvas = current_canvas
        return best_match

    def _insert_editor_anchor_at_canvas(self, canvas_x: float, canvas_y: float) -> bool:
        insert_target = self._find_editor_segment_insert_target(canvas_x, canvas_y)
        if insert_target is None:
            self.status_var.set("Ctrl+Left-click near a curve segment to insert an anchor.")
            return False

        path_index, insert_after_anchor_index, image_x, image_y = insert_target
        self._push_editor_undo_state()
        if not _insert_anchor_into_path(self.editor_path_state, path_index, insert_after_anchor_index, image_x, image_y):
            self.editor_undo_history.pop()
            self.status_var.set("Could not insert an anchor at that curve position.")
            return False

        self.editor_path_state.active_path_index = path_index
        self.editor_path_state.paths[path_index].finalized = False
        self._rebuild_editor_export()
        self.status_var.set(f"Inserted anchor into path {path_index + 1}.")
        self._render_current_image()
        return True

    def _delete_editor_anchor(self, path_index: int, anchor_index: int) -> bool:
        if _anchor_has_connected_path_references(self.editor_path_state, path_index, anchor_index):
            self.status_var.set("That anchor is part of a branch/loop connection. Move or reconnect it before deleting.")
            return False
        self._push_editor_undo_state()
        if not _delete_anchor_from_path(self.editor_path_state, path_index, anchor_index):
            self.editor_undo_history.pop()
            self.status_var.set("Could not delete that anchor.")
            return False

        if 0 <= path_index < len(self.editor_path_state.paths):
            self.editor_path_state.active_path_index = path_index
            self.editor_path_state.paths[path_index].finalized = False
        self._rebuild_editor_export()
        self.status_var.set("Deleted anchor.")
        self._render_current_image()
        return True

    def _on_middle_drag_started(self, event: tk.Event) -> None:
        if self.current_render_state is None:
            return
        if not self.current_render_state.contains_canvas_point(float(event.x), float(event.y)):
            return
        self.drag_state = {
            "start_x": float(event.x),
            "start_y": float(event.y),
            "viewport_x": self.viewport_x,
            "viewport_y": self.viewport_y,
            "moved": False,
        }
        self.image_canvas.configure(cursor="fleur")

    def _on_middle_drag_moved(self, event: tk.Event) -> None:
        if self.current_render_state is None or self.drag_state is None:
            return
        start_x = float(self.drag_state["start_x"])
        start_y = float(self.drag_state["start_y"])
        start_viewport_x = float(self.drag_state["viewport_x"])
        start_viewport_y = float(self.drag_state["viewport_y"])
        delta_canvas_x = float(event.x - start_x)
        delta_canvas_y = float(event.y - start_y)
        if abs(delta_canvas_x) > 3 or abs(delta_canvas_y) > 3:
            self.drag_state["moved"] = True
        delta_image_x = delta_canvas_x * self.current_render_state.viewport_width / float(self.current_render_state.draw_width)
        delta_image_y = delta_canvas_y * self.current_render_state.viewport_height / float(self.current_render_state.draw_height)
        self.viewport_x = start_viewport_x - delta_image_x
        self.viewport_y = start_viewport_y - delta_image_y
        self._render_current_image()

    def _on_middle_drag_ended(self, _event: tk.Event | None = None) -> None:
        self.drag_state = None
        self.image_canvas.configure(cursor="crosshair" if self.edit_mode_enabled else "")

    def _on_image_frame_configured(self, _event: tk.Event | None = None) -> None:
        if self.current_asset and self.current_image is not None:
            self._render_current_image()

    def _render_current_image(self) -> None:
        if self.current_image is None or self.current_asset is None:
            return

        available_width = max(200, self.image_canvas.winfo_width())
        available_height = max(200, self.image_canvas.winfo_height())
        render_state = compute_render_state(
            image_width=self.current_asset.width,
            image_height=self.current_asset.height,
            canvas_width=available_width,
            canvas_height=available_height,
            zoom_factor=self.zoom_factor,
            viewport_x=self.viewport_x,
            viewport_y=self.viewport_y,
        )
        self.current_render_state = render_state
        self.viewport_x = render_state.viewport_x
        self.viewport_y = render_state.viewport_y

        source_image = self._edit_overlay_composite_image()
        image = source_image.transform(
            (render_state.draw_width, render_state.draw_height),
            IMAGE_EXTENT,
            (
                render_state.viewport_x,
                render_state.viewport_y,
                render_state.viewport_x + render_state.viewport_width,
                render_state.viewport_y + render_state.viewport_height,
            ),
            resample=IMAGE_RESAMPLE,
        )
        self.current_photo = ImageTk.PhotoImage(image)
        self.image_canvas.delete("all")
        self.image_canvas.create_image(
            render_state.draw_offset_x,
            render_state.draw_offset_y,
            anchor=tk.NW,
            image=self.current_photo,
        )
        self._render_objective_overlay()
        self._render_view_route_overlay()
        self._render_markers()
        self._render_editor_overlay()
        self._reset_coordinate_readouts()

    def _edit_overlay_composite_image(self) -> Image.Image:
        if not self.edit_mode_enabled or self.current_image is None or self.current_asset is None:
            return self.current_image

        overlay_assets = self._editor_overlay_assets()
        if not overlay_assets:
            return self.current_image

        composite = self.current_image.copy()
        for overlay_asset in overlay_assets:
            projected = _project_overlay_asset_into_base_image(overlay_asset, self.current_asset)
            if projected is None:
                continue
            left, top, width, height, flip_x, flip_y = projected
            if width <= 0 or height <= 0:
                continue
            right = left + width
            bottom = top + height
            if right <= 0 or bottom <= 0 or left >= composite.width or top >= composite.height:
                continue
            overlay_image = self._load_editor_overlay_image(overlay_asset)
            if overlay_image is None:
                continue
            overlay_image = overlay_image.copy()
            if flip_x:
                overlay_image = overlay_image.transpose(IMAGE_FLIP_LEFT_RIGHT)
            if flip_y:
                overlay_image = overlay_image.transpose(IMAGE_FLIP_TOP_BOTTOM)
            overlay_image = overlay_image.resize((width, height), resample=IMAGE_RESAMPLE)
            overlay_image.putalpha(110)

            crop_left = max(0, -left)
            crop_top = max(0, -top)
            crop_right = width - max(0, right - composite.width)
            crop_bottom = height - max(0, bottom - composite.height)
            if crop_right <= crop_left or crop_bottom <= crop_top:
                continue
            cropped = overlay_image.crop((crop_left, crop_top, crop_right, crop_bottom))
            paste_x = max(0, left)
            paste_y = max(0, top)
            composite.alpha_composite(cropped, (paste_x, paste_y))
        return composite

    def _load_editor_overlay_image(self, asset: ZoneCompositeAsset) -> Image.Image | None:
        cached = self.editor_overlay_image_cache.get(asset.path)
        if cached is not None:
            return cached
        try:
            image = Image.open(asset.path).convert("RGBA")
        except OSError:
            return None
        self.editor_overlay_image_cache[asset.path] = image
        return image

    def _on_image_motion(self, event: tk.Event) -> None:
        if self.current_asset is None or self.current_photo is None or self.current_render_state is None:
            return

        image_coords = self.current_render_state.canvas_to_image(float(event.x), float(event.y))
        if image_coords is None:
            self._reset_coordinate_readouts()
            return

        image_x, image_y = image_coords

        transform = self.current_asset.coordinate_transform
        if transform is None:
            self.local_coords_var.set(f"Local: px=({image_x:.1f}, {image_y:.1f})")
            self.world_coords_var.set("World: unavailable")
            return

        zone_x, zone_y = transform.image_to_zone(image_x, image_y, self.current_asset.width, self.current_asset.height)
        world_x, world_y = transform.image_to_world(image_x, image_y, self.current_asset.width, self.current_asset.height)
        self.local_coords_var.set(f"Zone: x={zone_x:.2f}, y={zone_y:.2f} | px=({image_x:.1f}, {image_y:.1f})")
        self.world_coords_var.set(f"World: x={world_x:.2f}, y={world_y:.2f}")

    def _on_image_leave(self, _event: tk.Event | None = None) -> None:
        self._reset_coordinate_readouts()

    def _on_drag_started(self, event: tk.Event) -> None:
        if self.current_render_state is None:
            return
        if not self.current_render_state.contains_canvas_point(float(event.x), float(event.y)):
            return
        if self.edit_mode_enabled:
            image_coords = self.current_render_state.canvas_to_image(float(event.x), float(event.y))
            if image_coords is None:
                return
            control_pressed = bool(event.state & CONTROL_MASK)
            if control_pressed:
                self._insert_editor_anchor_at_canvas(float(event.x), float(event.y))
                self.image_canvas.configure(cursor="crosshair")
                return
            hit_target = self._hit_test_editor_target(float(event.x), float(event.y))
            if hit_target is not None:
                shift_pressed = bool(event.state & SHIFT_MASK)
                target_kind, path_index, anchor_index = hit_target
                if shift_pressed and target_kind == "anchor":
                    if self._active_editor_path() is None:
                        self._start_branch_from_existing_anchor(path_index, anchor_index)
                        self.image_canvas.configure(cursor="crosshair")
                        return
                    active_path_index = self._active_editor_path_index()
                    if active_path_index is not None and active_path_index != path_index:
                        self._attach_active_path_to_anchor(path_index, anchor_index)
                        self.image_canvas.configure(cursor="crosshair")
                        return
                self._push_editor_undo_state()
                self.editor_drag_target = hit_target
                self.image_canvas.configure(cursor="hand2")
                return
            self._add_editor_anchor(*image_coords)
            self.image_canvas.configure(cursor="crosshair")
            return
        self.drag_state = {
            "start_x": float(event.x),
            "start_y": float(event.y),
            "viewport_x": self.viewport_x,
            "viewport_y": self.viewport_y,
            "moved": False,
        }
        self.image_canvas.configure(cursor="fleur")

    def _on_secondary_click(self, event: tk.Event) -> None:
        if not self.edit_mode_enabled or self.current_render_state is None:
            return
        if not self.current_render_state.contains_canvas_point(float(event.x), float(event.y)):
            return
        if not bool(event.state & CONTROL_MASK):
            return
        hit_target = self._hit_test_editor_target(float(event.x), float(event.y))
        if hit_target is None or hit_target[0] != "anchor":
            self.status_var.set("Ctrl+Right-click an anchor to delete it.")
            return
        _, path_index, anchor_index = hit_target
        self._delete_editor_anchor(path_index, anchor_index)
        self.image_canvas.configure(cursor="crosshair")

    def _on_drag_moved(self, event: tk.Event) -> None:
        if self.current_render_state is None:
            return
        if self.edit_mode_enabled:
            if self.editor_drag_target is None:
                return
            image_coords = self.current_render_state.canvas_to_image(float(event.x), float(event.y))
            if image_coords is None:
                return
            target_kind, path_index, anchor_index = self.editor_drag_target
            self._move_editor_target(target_kind, path_index, anchor_index, *image_coords)
            self._rebuild_editor_export()
            self._render_current_image()
            return
        if self.drag_state is None:
            return

        start_x = float(self.drag_state["start_x"])
        start_y = float(self.drag_state["start_y"])
        start_viewport_x = float(self.drag_state["viewport_x"])
        start_viewport_y = float(self.drag_state["viewport_y"])
        delta_canvas_x = float(event.x - start_x)
        delta_canvas_y = float(event.y - start_y)
        if abs(delta_canvas_x) > 3 or abs(delta_canvas_y) > 3:
            self.drag_state["moved"] = True
        delta_image_x = delta_canvas_x * self.current_render_state.viewport_width / float(self.current_render_state.draw_width)
        delta_image_y = delta_canvas_y * self.current_render_state.viewport_height / float(self.current_render_state.draw_height)
        self.viewport_x = start_viewport_x - delta_image_x
        self.viewport_y = start_viewport_y - delta_image_y
        self._render_current_image()

    def _on_drag_ended(self, event: tk.Event | None = None) -> None:
        if self.edit_mode_enabled:
            self.editor_drag_target = None
            self.image_canvas.configure(cursor="crosshair")
            return
        if event is not None and self.drag_state is not None and not bool(self.drag_state["moved"]):
            self._select_marker_at_canvas(float(event.x), float(event.y))
        self.drag_state = None
        self.image_canvas.configure(cursor="")

    def _on_mouse_wheel(self, event: tk.Event) -> None:
        if self.current_asset is None or self.current_render_state is None:
            return

        if event.delta == 0:
            return

        scale_factor = ZOOM_STEP if event.delta > 0 else 1.0 / ZOOM_STEP
        new_zoom = min(ZOOM_MAX, max(ZOOM_MIN, self.zoom_factor * scale_factor))
        if abs(new_zoom - self.zoom_factor) < 1e-9:
            return

        current_image_coords = self.current_render_state.canvas_to_image(float(event.x), float(event.y))
        self.zoom_factor = new_zoom

        if current_image_coords is None:
            self._render_current_image()
            return

        next_state = compute_render_state(
            image_width=self.current_asset.width,
            image_height=self.current_asset.height,
            canvas_width=max(200, self.image_canvas.winfo_width()),
            canvas_height=max(200, self.image_canvas.winfo_height()),
            zoom_factor=self.zoom_factor,
            viewport_x=self.viewport_x,
            viewport_y=self.viewport_y,
        )
        image_x, image_y = current_image_coords
        self.viewport_x = image_x - ((float(event.x) - next_state.draw_offset_x) * next_state.viewport_width / float(next_state.draw_width))
        self.viewport_y = image_y - ((float(event.y) - next_state.draw_offset_y) * next_state.viewport_height / float(next_state.draw_height))
        self._render_current_image()

    def _reset_coordinate_readouts(self) -> None:
        if self.current_asset is None:
            self.local_coords_var.set("Local: —")
            self.world_coords_var.set("World: —")
            return

        zoom_percent = int(round(self.zoom_factor * 100))
        self.local_coords_var.set(f"Zone: move the mouse over the map | Zoom: {zoom_percent}%")
        if self.current_asset.coordinate_transform is None:
            self.world_coords_var.set("World: unavailable for this map")
        else:
            self.world_coords_var.set("World: move the mouse over the map")

    def _render_markers(self) -> None:
        self.visible_marker_hitboxes.clear()
        if self.current_render_state is None or self.current_asset is None:
            return

        for marker in self.current_zone_markers:
            visible_quests = self._visible_quests_for_marker(marker)
            if marker.kind == "quest_giver" and not visible_quests:
                continue
            if not self.marker_visibility_vars.get(marker.kind, tk.BooleanVar(value=False)).get():
                continue
            photo = self._get_marker_photo(marker, icon_kind=_quest_marker_icon_kind(marker.kind, visible_quests))
            if photo is None:
                continue

            transform = self.current_asset.coordinate_transform
            if transform is None:
                continue
            zone_x, zone_y = transform.world_to_zone(marker.world_x, marker.world_y)
            image_x = zone_x * self.current_asset.width / 100.0
            image_y = zone_y * self.current_asset.height / 100.0
            canvas_coords = self.current_render_state.image_to_canvas(image_x, image_y)
            if canvas_coords is None:
                continue
            canvas_x, canvas_y = canvas_coords
            self.image_canvas.create_image(canvas_x, canvas_y, image=photo, anchor=tk.CENTER)
            width = photo.width()
            height = photo.height()
            self.visible_marker_hitboxes.append(
                ((canvas_x - width / 2, canvas_y - height / 2, canvas_x + width / 2, canvas_y + height / 2), marker)
            )

    def _render_objective_overlay(self) -> None:
        if self.current_render_state is None or self.current_asset is None or not self.active_objective_overlay_id:
            return

        overlay = self.objective_area_index.get(self.active_objective_overlay_id)
        if not isinstance(overlay, dict):
            return
        transform = self.current_asset.coordinate_transform
        if transform is None:
            return

        allowed_map_id = self.current_asset.map_id if self.current_asset.map_id is not None else transform.map_id
        rendered_any = False
        for area in overlay.get("areas", []):
            if _int_from_value(area.get("map_id")) != allowed_map_id:
                continue
            rendered_any = True
            center_image = _objective_center_image_coords(
                transform,
                float(area.get("center_x", 0.0)),
                float(area.get("center_y", 0.0)),
                self.current_asset.width,
                self.current_asset.height,
            )
            center_canvas = self.current_render_state.image_to_canvas(*center_image)
            if center_canvas is None:
                continue

            radius_x, radius_y = _objective_area_canvas_radii(
                transform=transform,
                render_state=self.current_render_state,
                area=area,
                image_width=self.current_asset.width,
                image_height=self.current_asset.height,
            )
            canvas_x, canvas_y = center_canvas
            self.image_canvas.create_oval(
                canvas_x - radius_x,
                canvas_y - radius_y,
                canvas_x + radius_x,
                canvas_y + radius_y,
                outline="#ff6f00",
                width=4,
                fill="#ffd54f",
                stipple="gray25",
            )
            self.image_canvas.create_oval(
                canvas_x - radius_x * 0.6,
                canvas_y - radius_y * 0.6,
                canvas_x + radius_x * 0.6,
                canvas_y + radius_y * 0.6,
                outline="#fff59d",
                width=2,
                dash=(8, 6),
            )
            self.image_canvas.create_line(canvas_x - 10, canvas_y, canvas_x + 10, canvas_y, fill="#fff59d", width=2)
            self.image_canvas.create_line(canvas_x, canvas_y - 10, canvas_x, canvas_y + 10, fill="#fff59d", width=2)
            self.image_canvas.create_text(
                canvas_x,
                canvas_y - max(radius_y, 16) - 10,
                text=f"{_objective_overlay_label(overlay)} • {int(area.get('spawn_count', 0))} spawns",
                fill="#fff8e1",
                font=("Segoe UI", 9, "bold"),
            )

        if not rendered_any:
            self.image_canvas.create_text(
                24,
                24,
                anchor=tk.NW,
                text=f"Objective area exists, but not on this zone map: {_objective_overlay_label(overlay)}",
                fill="#fff176",
                font=("Segoe UI", 9, "bold"),
            )

    def _get_marker_photo(self, marker: MarkerRecord, icon_kind: str | None = None) -> ImageTk.PhotoImage | None:
        resolved_kind = icon_kind or marker.kind
        cache_key = f"{marker.icon_relpath}|{resolved_kind}"
        cached = self.marker_photo_cache.get(cache_key)
        if cached is not None:
            return cached

        if marker.icon_relpath:
            icon_path = PNG_MAPS_DIR / Path(marker.icon_relpath)
            if not icon_path.is_file():
                icon_path = PNG_ICONS_DIR / Path(marker.icon_relpath)
            if icon_path.is_file():
                with Image.open(icon_path) as image:
                    icon = image.convert("RGBA")
                if resolved_kind in {"herb", "ore"}:
                    icon = _remove_black_icon_background(icon)
                crop_box = marker_icon_crop_box(resolved_kind, icon.width, icon.height)
                if crop_box is not None:
                    icon = icon.crop(crop_box)
                target_size = _marker_icon_target_size(resolved_kind)
                icon.thumbnail((target_size, target_size))
                photo = ImageTk.PhotoImage(icon)
                self.marker_image_cache[cache_key] = icon
                self.marker_photo_cache[cache_key] = photo
                return photo

        generated_icon = _build_generated_marker_icon(resolved_kind)
        if generated_icon is None:
            return None
        photo = ImageTk.PhotoImage(generated_icon)
        self.marker_image_cache[cache_key] = generated_icon
        self.marker_photo_cache[cache_key] = photo
        return photo

    def _filter_markers_for_asset(self, asset: ZoneCompositeAsset) -> list[MarkerRecord]:
        transform = asset.coordinate_transform
        if transform is None:
            return []

        min_world_x = min(transform.world_x1, transform.world_x2)
        max_world_x = max(transform.world_x1, transform.world_x2)
        min_world_y = min(transform.world_y1, transform.world_y2)
        max_world_y = max(transform.world_y1, transform.world_y2)
        allowed_map_id = asset.map_id if asset.map_id is not None else transform.map_id
        filtered: list[MarkerRecord] = []
        for marker in self.all_markers:
            if marker.map_id != allowed_map_id:
                continue
            if not (min_world_x <= marker.world_x <= max_world_x and min_world_y <= marker.world_y <= max_world_y):
                continue
            filtered.append(marker)
        return filtered

    def _on_marker_visibility_changed(self) -> None:
        if self.current_asset is None:
            return
        if self.selected_marker is not None:
            self.marker_details_var.set(self._format_marker_details(self.selected_marker))
            self._populate_properties_for_marker(self.selected_marker)
        self._render_current_image()

    def _select_marker_at_canvas(self, canvas_x: float, canvas_y: float) -> None:
        for hitbox, marker in reversed(self.visible_marker_hitboxes):
            left, top, right, bottom = hitbox
            if left <= canvas_x <= right and top <= canvas_y <= bottom:
                self.selected_marker = marker
                self._set_active_objective_overlay(None, rerender=False)
                self.marker_details_var.set(self._format_marker_details(marker))
                self._populate_properties_for_marker(marker)
                return
        self.selected_marker = None
        self._set_active_objective_overlay(None)
        self.marker_details_var.set(
            f"Markers: {len(self.current_zone_markers)} cached in this zone. Click an icon to inspect it."
        )
        self._populate_properties_for_marker(None)

    def _set_active_objective_overlay(self, overlay_id: str | None, rerender: bool = True) -> None:
        if overlay_id == self.active_objective_overlay_id:
            return
        self.active_objective_overlay_id = overlay_id
        if overlay_id:
            overlay = self.objective_area_index.get(overlay_id, {})
            self.status_var.set(f"Objective overlay active: {_objective_overlay_label(overlay)}")
        elif self.current_asset is not None:
            self.status_var.set(f"Loaded {self.current_asset.label}")
        if rerender and self.current_asset is not None and self.current_image is not None:
            self._render_current_image()

    def _format_marker_details(self, marker: MarkerRecord) -> str:
        marker_type = MARKER_KIND_LABELS.get(marker.kind, marker.kind.replace("_", " ").title())
        lines = [
            f"Selected marker: {marker.label}",
            f"Type: {marker_type} | Object: {marker.object_type}",
            f"Entry: {marker.entry} | GUID: {marker.guid} | Map: {marker.map_id}",
            f"World: x={marker.world_x:.2f}, y={marker.world_y:.2f}, z={marker.world_z:.2f}",
        ]
        if marker.kind in {"herb", "ore"}:
            resource_item_name = str(marker.metadata.get("resource_item_name", "")).strip()
            resource_loot = list(marker.metadata.get("resource_loot", []))
            if resource_item_name:
                lines.append(f"Primary item: {resource_item_name}")
            if resource_loot:
                lines.append(f"Possible loot entries: {len(resource_loot)}")
        if marker.kind == "quest_giver":
            raw_quests = list(marker.metadata.get("quests", []))
            visible_quests = self._visible_quests_for_marker(marker)
            lines.append(f"Quest count: {len(visible_quests)} visible / {len(raw_quests)} total")
            active_filters = self._active_quest_filter_descriptions()
            if active_filters:
                lines.append(f"Active quest filters: {', '.join(active_filters)}")
        return "\n".join(lines)

    def _visible_quests_for_marker(self, marker: MarkerRecord) -> list[dict]:
        if marker.kind != "quest_giver":
            return []
        min_level, max_level = self._current_quest_level_filters()
        return _filter_visible_quests(
            list(marker.metadata.get("quests", [])),
            include_event_quests=self.event_quest_visibility_var.get(),
            min_level=min_level,
            max_level=max_level,
        )

    def _current_quest_level_filters(self) -> tuple[int | None, int | None]:
        min_level = _parse_optional_level_filter(self.quest_level_min_var.get())
        max_level = _parse_optional_level_filter(self.quest_level_max_var.get())
        if min_level is not None and max_level is not None and min_level > max_level:
            min_level, max_level = max_level, min_level
        return min_level, max_level

    def _active_quest_filter_descriptions(self) -> list[str]:
        descriptions: list[str] = []
        if not self.event_quest_visibility_var.get():
            descriptions.append("event quests hidden")
        min_level, max_level = self._current_quest_level_filters()
        if min_level is not None or max_level is not None:
            lower = str(min_level) if min_level is not None else "Any"
            upper = str(max_level) if max_level is not None else "Any"
            descriptions.append(f"quest level {lower}-{upper}")
        return descriptions

    def _populate_properties_for_marker(self, marker: MarkerRecord | None) -> None:
        self.properties_tree_payload_by_item.clear()
        for item_id in self.properties_tree.get_children():
            self.properties_tree.delete(item_id)
        self._set_active_objective_overlay(None, rerender=False)

        if marker is None:
            self._set_properties_text("Click a marker on the map to inspect it.")
            return

        marker_item = self.properties_tree.insert(
            "",
            tk.END,
            text=f"{'NPC' if marker.object_type == 'creature' else 'Object'}: {marker.label}",
            open=True,
        )
        self.properties_tree_payload_by_item[marker_item] = {"type": "marker", "marker": marker}

        summary_item = self.properties_tree.insert(
            marker_item,
            tk.END,
            text=(
                f"{MARKER_KIND_LABELS.get(marker.kind, marker.kind.replace('_', ' ').title())}"
                f" | Entry {marker.entry} | GUID {marker.guid}"
            ),
        )
        self.properties_tree_payload_by_item[summary_item] = {"type": "marker", "marker": marker}

        resource_loot = list(marker.metadata.get("resource_loot", [])) if marker.kind in {"herb", "ore"} else []
        if resource_loot:
            resource_loot_item = self.properties_tree.insert(
                marker_item,
                tk.END,
                text=f"Possible Loot ({len(resource_loot)})",
                open=False,
            )
            self.properties_tree_payload_by_item[resource_loot_item] = {
                "type": "resource_loot_section",
                "marker": marker,
            }
            for loot in resource_loot:
                loot_item = self.properties_tree.insert(
                    resource_loot_item,
                    tk.END,
                    text=_format_resource_loot_tree_label(loot),
                )
                self.properties_tree_payload_by_item[loot_item] = {
                    "type": "resource_loot_item",
                    "marker": marker,
                    "loot": loot,
                }

        raw_quests = list(marker.metadata.get("quests", [])) if marker.kind == "quest_giver" else []
        quests = self._visible_quests_for_marker(marker)
        if not quests:
            text = self._format_marker_properties_text(marker)
            active_filters = self._active_quest_filter_descriptions()
            if raw_quests and active_filters:
                text += (
                    "\n\nAll visible quests for this marker are currently hidden by the active filters: "
                    f"{', '.join(active_filters)}."
                )
            self._set_properties_text(text)
            return

        quests_item = self.properties_tree.insert(marker_item, tk.END, text=f"Quests ({len(quests)})", open=True)
        self.properties_tree_payload_by_item[quests_item] = {"type": "marker", "marker": marker}
        self._set_properties_text(
            self._format_marker_properties_text(marker)
            + "\n\nSelect a quest or a quest section in the tree above to inspect its details."
        )
        for quest in quests:
            quest_item = self.properties_tree.insert(
                quests_item,
                tk.END,
                text=_format_quest_tree_label(quest),
                open=False,
            )
            self.properties_tree_payload_by_item[quest_item] = {"type": "quest", "quest": quest}
            faction_item = self.properties_tree.insert(quest_item, tk.END, text=f"Faction: {quest.get('faction', 'Alliance & Horde')}")
            self.properties_tree_payload_by_item[faction_item] = {"type": "quest_section", "quest": quest, "section": "summary"}

            prerequisite_quests = list(quest.get("prerequisite_quests", []))
            if prerequisite_quests:
                prerequisites_item = self.properties_tree.insert(quest_item, tk.END, text="Prerequisites", open=False)
                self.properties_tree_payload_by_item[prerequisites_item] = {
                    "type": "quest_section",
                    "quest": quest,
                    "section": "prerequisites",
                }
                for related in prerequisite_quests:
                    related_item = self.properties_tree.insert(
                        prerequisites_item,
                        tk.END,
                        text=_format_related_quest_tree_label(related),
                    )
                    self.properties_tree_payload_by_item[related_item] = {
                        "type": "related_quest",
                        "quest": quest,
                        "related": related,
                    }

            objectives = list(quest.get("objective_texts", []))
            if objectives:
                objectives_item = self.properties_tree.insert(quest_item, tk.END, text="Quest Text & Objectives", open=False)
                self.properties_tree_payload_by_item[objectives_item] = {
                    "type": "quest_section",
                    "quest": quest,
                    "section": "objectives",
                }
                for objective in objectives:
                    objective_item = self.properties_tree.insert(objectives_item, tk.END, text=str(objective))
                    self.properties_tree_payload_by_item[objective_item] = {
                        "type": "quest_section",
                        "quest": quest,
                        "section": "objectives",
                    }

            requirement_lines = list(quest.get("requirement_lines", []))
            if requirement_lines:
                requirements_item = self.properties_tree.insert(quest_item, tk.END, text="Requirements", open=False)
                self.properties_tree_payload_by_item[requirements_item] = {
                    "type": "quest_section",
                    "quest": quest,
                    "section": "requirements",
                }
                target_requirements = list(quest.get("target_requirements", []))
                item_requirements = list(quest.get("item_requirements", []))
                for requirement in target_requirements:
                    requirement_item = self.properties_tree.insert(
                        requirements_item,
                        tk.END,
                        text=_format_requirement_tree_label(requirement),
                    )
                    self.properties_tree_payload_by_item[requirement_item] = {
                        "type": "requirement_item",
                        "quest": quest,
                        "requirement": requirement,
                    }
                for requirement in item_requirements:
                    requirement_item = self.properties_tree.insert(
                        requirements_item,
                        tk.END,
                        text=_format_requirement_tree_label(requirement),
                    )
                    self.properties_tree_payload_by_item[requirement_item] = {
                        "type": "requirement_item",
                        "quest": quest,
                        "requirement": requirement,
                    }
                if not target_requirements and not item_requirements:
                    for line in requirement_lines:
                        requirement_item = self.properties_tree.insert(requirements_item, tk.END, text=str(line))
                        self.properties_tree_payload_by_item[requirement_item] = {
                            "type": "quest_section",
                            "quest": quest,
                            "section": "requirements",
                        }

            rewards_item = self.properties_tree.insert(quest_item, tk.END, text="Rewards", open=False)
            self.properties_tree_payload_by_item[rewards_item] = {
                "type": "quest_section",
                "quest": quest,
                "section": "rewards",
            }
            if quest.get("reward_money"):
                reward_item = self.properties_tree.insert(rewards_item, tk.END, text=f"Money: {_format_money_line(int(quest['reward_money']))}")
                self.properties_tree_payload_by_item[reward_item] = {"type": "quest_section", "quest": quest, "section": "rewards"}
            for reward in quest.get("fixed_rewards", []):
                reward_item = self.properties_tree.insert(rewards_item, tk.END, text=f"Reward: {reward['quantity']}x {reward['name']}")
                self.properties_tree_payload_by_item[reward_item] = {"type": "quest_section", "quest": quest, "section": "rewards"}
            for reward in quest.get("choice_rewards", []):
                reward_item = self.properties_tree.insert(rewards_item, tk.END, text=f"Choice: {reward['quantity']}x {reward['name']}")
                self.properties_tree_payload_by_item[reward_item] = {"type": "quest_section", "quest": quest, "section": "rewards"}

            related_groups = [
                ("Follow-up Quests", list(quest.get("followup_quests", []))),
                ("Breadcrumb For", list(quest.get("breadcrumb_for_quests", []))),
            ]
            for label, related_values in related_groups:
                if not related_values:
                    continue
                related_parent = self.properties_tree.insert(quest_item, tk.END, text=label, open=False)
                self.properties_tree_payload_by_item[related_parent] = {
                    "type": "quest_section",
                    "quest": quest,
                    "section": label.lower().replace("-", " "),
                }
                for related in related_values:
                    related_item = self.properties_tree.insert(
                        related_parent,
                        tk.END,
                        text=_format_related_quest_tree_label(related),
                    )
                    self.properties_tree_payload_by_item[related_item] = {
                        "type": "related_quest",
                        "quest": quest,
                        "related": related,
                    }

    def _on_properties_tree_selected(self, _event: tk.Event | None = None) -> None:
        selection = self.properties_tree.selection()
        if not selection:
            return
        payload = self.properties_tree_payload_by_item.get(selection[0])
        if not payload:
            return
        payload_type = str(payload.get("type", ""))
        marker = payload.get("marker")
        if payload_type == "marker" and isinstance(marker, MarkerRecord):
            self._set_active_objective_overlay(None)
            self._set_properties_text(self._format_marker_properties_text(marker))
            return
        if payload_type == "resource_loot_section" and isinstance(marker, MarkerRecord):
            self._set_active_objective_overlay(None)
            self._set_properties_text(self._format_resource_loot_section_text(marker))
            return
        if payload_type == "resource_loot_item" and isinstance(marker, MarkerRecord):
            self._set_active_objective_overlay(None)
            loot = payload.get("loot")
            if isinstance(loot, dict):
                self._set_properties_text(self._format_resource_loot_item_text(marker, loot))
                return
        quest = payload.get("quest")
        section = str(payload.get("section", ""))
        if payload_type == "requirement_item":
            requirement = payload.get("requirement")
            if isinstance(quest, dict) and isinstance(requirement, dict):
                self._set_active_objective_overlay(_overlay_id_from_requirement(requirement))
                self._set_properties_text(self._format_requirement_text(quest, requirement))
                return
        if payload_type == "related_quest":
            self._set_active_objective_overlay(None)
            related = payload.get("related")
            if isinstance(related, dict) and isinstance(quest, dict):
                self._set_properties_text(self._format_related_quest_text(quest, related))
                return
        if not isinstance(quest, dict):
            return
        self._set_active_objective_overlay(None)
        if payload_type == "quest_section" and section:
            if section == "requirements" and isinstance(quest, dict):
                self._set_active_objective_overlay(_single_overlay_id_from_quest_requirements(quest))
            self._set_properties_text(self._format_quest_section_text(quest, section))
            return
        self._set_properties_text(self._format_quest_details_text(quest))

    def _on_properties_tree_shift_click(self, event: tk.Event | None = None) -> str | None:
        if event is None:
            return None
        item_id = self.properties_tree.identify_row(event.y)
        if not item_id:
            return None
        payload = self.properties_tree_payload_by_item.get(item_id)
        if not payload or str(payload.get("type", "")) != "related_quest":
            return None
        related = payload.get("related")
        if not isinstance(related, dict):
            return None
        self.properties_tree.selection_set(item_id)
        self.properties_tree.focus(item_id)
        self._jump_to_related_quest(related)
        return "break"

    def _jump_to_related_quest(self, related: dict) -> bool:
        quest_id = int(related.get("quest_id", 0))
        if quest_id <= 0:
            self.status_var.set("Related quest has no valid quest id.")
            return False
        target = _find_best_related_quest_target(
            self.assets,
            self.current_asset,
            self.quest_starter_marker_index,
            quest_id,
        )
        if target is None:
            self.status_var.set(f"No starter NPC marker found for related quest {quest_id}.")
            return False
        asset, marker = target
        self._select_tree_asset(asset)
        self.selected_marker = marker
        self._set_active_objective_overlay(None, rerender=False)
        self.marker_details_var.set(self._format_marker_details(marker))
        self._populate_properties_for_marker(marker)
        self.status_var.set(f"Jumped to quest starter [{quest_id}] at {marker.label} in {asset.label}.")
        self._render_current_image()
        return True

    def _clear_properties_panel(self) -> None:
        self.properties_tree_payload_by_item.clear()
        for item_id in self.properties_tree.get_children():
            self.properties_tree.delete(item_id)
        self._set_properties_text("Click a marker on the map to inspect it.")

    def _set_properties_text(self, text: str) -> None:
        self.properties_text.delete("1.0", tk.END)
        self.properties_text.insert("1.0", text)
        self.properties_text.mark_set(tk.INSERT, "1.0")
        self.properties_text.tag_remove(tk.SEL, "1.0", tk.END)

    def _on_properties_text_keypress(self, event: tk.Event) -> str | None:
        navigation_keys = {
            "Left",
            "Right",
            "Up",
            "Down",
            "Home",
            "End",
            "Prior",
            "Next",
        }
        if event.keysym in navigation_keys:
            return None
        if event.state & 0x4 and event.keysym.lower() in {"a", "c"}:
            return None
        return "break"

    def _copy_properties_text_selection(self, _event: tk.Event | None = None) -> str:
        try:
            selected_text = self.properties_text.get(tk.SEL_FIRST, tk.SEL_LAST)
        except tk.TclError:
            selected_text = self.properties_text.get("1.0", "end-1c")
        if not selected_text:
            return "break"
        self.clipboard_clear()
        self.clipboard_append(selected_text)
        self.status_var.set("Copied property panel text.")
        return "break"

    def _select_all_properties_text(self, _event: tk.Event | None = None) -> str:
        self.properties_text.tag_add(tk.SEL, "1.0", "end-1c")
        self.properties_text.mark_set(tk.INSERT, "1.0")
        self.properties_text.see("1.0")
        return "break"

    def _format_marker_properties_text(self, marker: MarkerRecord) -> str:
        lines = [self._format_marker_details(marker)]
        resource_loot = list(marker.metadata.get("resource_loot", [])) if marker.kind in {"herb", "ore"} else []
        if resource_loot:
            lines.append("")
            lines.append("Possible Loot:")
            lines.extend(_format_resource_loot_line(item) for item in resource_loot)
        quests = self._visible_quests_for_marker(marker)
        if quests:
            factions = sorted({str(quest.get("faction", "Alliance & Horde")) for quest in quests})
            lines.append("")
            lines.append(f"Quest Giver Faction: {', '.join(factions)}")
            lines.append("Quests:")
            lines.extend(f"- {_format_quest_tree_label(quest)}" for quest in quests)
        return "\n".join(lines).strip()

    def _format_quest_details_text(self, quest: dict) -> str:
        lines = [
            f"[{quest['quest_id']}] {quest['title']}",
            f"Faction: {quest.get('faction', 'Alliance & Horde')}",
            f"Quest Level: {quest.get('quest_level', 0)} | Minimum Level: {quest.get('min_level', 0)}",
            "",
        ]

        prerequisite_quests = list(quest.get("prerequisite_quests", []))
        if prerequisite_quests:
            lines.append("Immediate Prerequisites:")
            lines.extend(_format_related_quest_tree_label(item) for item in prerequisite_quests)
            lines.append("")

        if quest.get("log_description"):
            lines.append("Log Description:")
            lines.append(str(quest["log_description"]))
            lines.append("")

        if quest.get("quest_description"):
            lines.append("Quest Text:")
            lines.append(str(quest["quest_description"]))
            lines.append("")

        if quest.get("objective_texts"):
            lines.append("Objectives:")
            lines.extend(str(value) for value in quest["objective_texts"])
            lines.append("")

        if quest.get("requirement_lines"):
            lines.append("Requirements:")
            lines.extend(str(value) for value in quest["requirement_lines"])
            lines.append("")

        if quest.get("reward_lines"):
            lines.append("Rewards:")
            lines.extend(str(value) for value in quest["reward_lines"])
            lines.append("")

        branch_candidates = list(quest.get("branch_candidates", []))
        if branch_candidates:
            lines.append("Likely Branches:")
            lines.extend(_format_branch_candidate_line(item) for item in branch_candidates)
            lines.append("")

        followup_quests = list(quest.get("followup_quests", []))
        if followup_quests:
            lines.append("Follow-up Quests:")
            lines.extend(_format_related_quest_tree_label(item) for item in followup_quests)
            lines.append("")

        breadcrumb_for_quests = list(quest.get("breadcrumb_for_quests", []))
        if breadcrumb_for_quests:
            lines.append("Breadcrumb For:")
            lines.extend(_format_related_quest_tree_label(item) for item in breadcrumb_for_quests)
        return "\n".join(lines).strip()

    def _format_quest_section_text(self, quest: dict, section: str) -> str:
        normalized = section.lower().strip()
        if normalized == "summary":
            return self._format_quest_details_text(quest)
        if normalized == "objectives":
            lines = [f"[{quest['quest_id']}] {quest['title']}", "", "Quest Text & Objectives:"]
            if quest.get("quest_description"):
                lines.append(str(quest["quest_description"]))
                lines.append("")
            lines.extend(str(value) for value in quest.get("objective_texts", []))
            return "\n".join(lines).strip()
        if normalized == "requirements":
            lines = [f"[{quest['quest_id']}] {quest['title']}", "", "Requirements:"]
            lines.extend(str(value) for value in quest.get("requirement_lines", []))
            return "\n".join(lines).strip()
        if normalized == "rewards":
            lines = [f"[{quest['quest_id']}] {quest['title']}", "", "Rewards:"]
            lines.extend(str(value) for value in quest.get("reward_lines", []))
            return "\n".join(lines).strip()
        if normalized == "prerequisites":
            lines = [f"[{quest['quest_id']}] {quest['title']}", "", "Immediate Prerequisites:"]
            lines.extend(_format_related_quest_tree_label(item) for item in quest.get("prerequisite_quests", []))
            return "\n".join(lines).strip()
        if normalized in {"follow up quests", "follow-up quests"}:
            lines = [f"[{quest['quest_id']}] {quest['title']}", "", "Follow-up Quests:"]
            lines.extend(_format_related_quest_tree_label(item) for item in quest.get("followup_quests", []))
            return "\n".join(lines).strip()
        if normalized == "breadcrumb for":
            lines = [f"[{quest['quest_id']}] {quest['title']}", "", "Breadcrumb For:"]
            lines.extend(_format_related_quest_tree_label(item) for item in quest.get("breadcrumb_for_quests", []))
            return "\n".join(lines).strip()
        return self._format_quest_details_text(quest)

    def _format_related_quest_text(self, quest: dict, related: dict) -> str:
        relation = _humanize_relation(str(related.get("relation", "related")))
        related_id = int(related.get("quest_id", 0))
        related_title = str(related.get("title", "") or f"Quest #{related_id}")
        lines = [
            relation,
            f"[{related_id}] {related_title}",
            "",
            f"Context quest: [{quest['quest_id']}] {quest['title']}",
        ]
        starter_targets = _find_related_quest_targets(
            self.assets,
            self.current_asset,
            self.quest_starter_marker_index,
            related_id,
        )
        if starter_targets:
            lines.append("")
            lines.append("Known starter NPCs:")
            for asset, marker in starter_targets[:8]:
                location_note = "this zone" if self.current_asset is not None and asset == self.current_asset else asset.label
                lines.append(f"- {marker.label} ({location_note})")
            lines.append("")
            lines.append("Shift-click this related quest in the tree to jump to the best starter NPC.")
        else:
            lines.append("")
            lines.append("No starter NPC marker found for this quest in the current marker cache.")
        return "\n".join(lines).strip()

    def _format_requirement_text(self, quest: dict, requirement: dict) -> str:
        lines = [
            f"[{quest['quest_id']}] {quest['title']}",
            "",
            _format_requirement_tree_label(requirement),
        ]
        overlay_id = _overlay_id_from_requirement(requirement)
        if overlay_id:
            lines.append("")
            lines.append("Objective Overlay:")
            lines.append(f"Overlay ID: {overlay_id}")
            lines.append(
                f"Approximate areas: {int(requirement.get('objective_area_count', 0))} | "
                f"Spawn count: {int(requirement.get('objective_spawn_count', 0))}"
            )
            map_ids = list(requirement.get("objective_map_ids", []))
            if map_ids:
                lines.append(f"Maps: {', '.join(str(value) for value in map_ids)}")
            source_creatures = list(requirement.get("source_creatures", []))
            if source_creatures:
                lines.append("Possible sources:")
                lines.extend(
                    f"- [{int(source.get('entry', 0))}] {source.get('name', 'Unknown Creature')}"
                    for source in source_creatures[:8]
                )
            lines.append("Selecting this requirement highlights rough spawn circles on the map.")
        return "\n".join(lines).strip()

    def _format_resource_loot_section_text(self, marker: MarkerRecord) -> str:
        lines = [self._format_marker_details(marker), "", "Possible Loot:"]
        lines.extend(_format_resource_loot_line(item) for item in marker.metadata.get("resource_loot", []))
        return "\n".join(lines).strip()

    def _format_resource_loot_item_text(self, marker: MarkerRecord, loot: dict) -> str:
        lines = [
            marker.label,
            _format_resource_loot_tree_label(loot),
            "",
            f"Item ID: {int(loot.get('item_id', 0))}",
        ]
        chance_detail = _format_resource_loot_chance_detail(loot)
        if chance_detail:
            lines.append(f"Odds: {chance_detail}")
        if loot.get("comment"):
            lines.append(f"Source: {loot['comment']}")
        if bool(loot.get("quest_required", False)):
            lines.append("Quest Required: yes")
        return "\n".join(lines).strip()


def _format_resource_loot_tree_label(loot: dict) -> str:
    item_name = str(loot.get("name", "") or f"Item #{int(loot.get('item_id', 0))}")
    min_count = max(1, int(loot.get("min_count", 1)))
    max_count = max(1, int(loot.get("max_count", min_count)))
    count_label = f"{min_count}" if min_count == max_count else f"{min_count}-{max_count}"
    chance_label = _format_resource_loot_chance_summary(loot)
    suffix = f" [{chance_label}]" if chance_label else ""
    return f"{count_label}x {item_name}{suffix}"


def _format_resource_loot_line(loot: dict) -> str:
    line = f"- {_format_resource_loot_tree_label(loot)}"
    if bool(loot.get("quest_required", False)):
        line += " (quest)"
    return line


def _format_resource_loot_chance_summary(loot: dict) -> str:
    estimated = loot.get("estimated_chance")
    if estimated is None:
        group_id = int(loot.get("group_id", 0))
        if group_id > 0:
            return f"group {group_id}"
        return ""
    return f"{float(estimated):.1f}%"


def _format_resource_loot_chance_detail(loot: dict) -> str:
    estimated = loot.get("estimated_chance")
    listed = float(loot.get("chance", 0.0))
    group_id = int(loot.get("group_id", 0))
    if estimated is not None and group_id > 0 and listed <= 0.0:
        return f"Estimated equal roll in group {group_id}: {float(estimated):.1f}%"
    if estimated is not None and listed > 0.0 and group_id > 0:
        return f"Listed {float(estimated):.1f}% in group {group_id}"
    if estimated is not None:
        return f"{float(estimated):.1f}%"
    if group_id > 0:
        return f"Grouped loot table {group_id} (variable)"
    return ""


def _format_money_line(copper: int) -> str:
    gold, remainder = divmod(int(copper), 10000)
    silver, copper_value = divmod(remainder, 100)
    parts: list[str] = []
    if gold:
        parts.append(f"{gold}g")
    if silver:
        parts.append(f"{silver}s")
    if copper_value or not parts:
        parts.append(f"{copper_value}c")
    return " ".join(parts)


def _format_quest_tree_label(quest: dict) -> str:
    tags = list(quest.get("classification_tags", []))
    tag_suffix = f" [{' | '.join(tags)}]" if tags else ""
    return (
        f"[{quest['quest_id']}] {quest['title']} "
        f"(Level {quest.get('quest_level', 0)} | Req {quest.get('min_level', 0)}){tag_suffix}"
    )


def _format_related_quest_tree_label(related: dict) -> str:
    quest_id = int(related.get("quest_id", 0))
    title = str(related.get("title", "") or f"Quest #{quest_id}")
    relation = _humanize_relation(str(related.get("relation", "related")))
    return f"{relation}: [{quest_id}] {title}"


def _humanize_relation(relation: str) -> str:
    normalized = relation.strip().replace("_", " ")
    return normalized[:1].upper() + normalized[1:]


def _build_quest_starter_marker_index(markers: Sequence[MarkerRecord]) -> dict[int, list[MarkerRecord]]:
    index: dict[int, list[MarkerRecord]] = defaultdict(list)
    for marker in markers:
        if marker.kind != "quest_giver":
            continue
        for quest in marker.metadata.get("quests", []):
            quest_id = int(quest.get("quest_id", 0))
            if quest_id > 0:
                index[quest_id].append(marker)
    return index


def _asset_contains_marker(asset: ZoneCompositeAsset, marker: MarkerRecord) -> bool:
    transform = asset.coordinate_transform
    if transform is None:
        return False
    allowed_map_id = asset.map_id if asset.map_id is not None else transform.map_id
    if marker.map_id != allowed_map_id:
        return False
    min_world_x = min(transform.world_x1, transform.world_x2)
    max_world_x = max(transform.world_x1, transform.world_x2)
    min_world_y = min(transform.world_y1, transform.world_y2)
    max_world_y = max(transform.world_y1, transform.world_y2)
    return min_world_x <= marker.world_x <= max_world_x and min_world_y <= marker.world_y <= max_world_y


def _find_related_quest_targets(
    assets: Sequence[ZoneCompositeAsset],
    current_asset: ZoneCompositeAsset | None,
    quest_starter_marker_index: dict[int, list[MarkerRecord]],
    quest_id: int,
) -> list[tuple[ZoneCompositeAsset, MarkerRecord]]:
    starter_markers = quest_starter_marker_index.get(int(quest_id), [])
    matches: list[tuple[ZoneCompositeAsset, MarkerRecord]] = []
    seen_keys: set[tuple[Path, int, float, float]] = set()
    for marker in starter_markers:
        for asset in assets:
            if not _asset_contains_marker(asset, marker):
                continue
            dedupe_key = (asset.path, marker.entry, marker.world_x, marker.world_y)
            if dedupe_key not in seen_keys:
                matches.append((asset, marker))
                seen_keys.add(dedupe_key)
            break

    def _sort_key(item: tuple[ZoneCompositeAsset, MarkerRecord]) -> tuple[int, int, int, str, str]:
        asset, marker = item
        same_asset = 0 if current_asset is not None and asset == current_asset else 1
        same_zone = 0 if current_asset is not None and asset.zone_id == current_asset.zone_id else 1
        same_map = 0 if current_asset is not None and asset.map_id == current_asset.map_id else 1
        return same_asset, same_zone, same_map, asset.label.lower(), marker.label.lower()

    return sorted(matches, key=_sort_key)


def _find_best_related_quest_target(
    assets: Sequence[ZoneCompositeAsset],
    current_asset: ZoneCompositeAsset | None,
    quest_starter_marker_index: dict[int, list[MarkerRecord]],
    quest_id: int,
) -> tuple[ZoneCompositeAsset, MarkerRecord] | None:
    matches = _find_related_quest_targets(assets, current_asset, quest_starter_marker_index, quest_id)
    return matches[0] if matches else None


def _format_requirement_tree_label(requirement: dict) -> str:
    quantity = int(requirement.get("quantity", 0))
    name = str(requirement.get("name", "") or "Unknown Requirement")
    target_kind = str(requirement.get("target_kind", "item"))
    if target_kind == "creature":
        return f"Kill: 0/{quantity} {name}"
    if target_kind == "object":
        return f"Objective: 0/{quantity} {name}"
    if requirement.get("objective_overlay_available"):
        return f"Collect: 0/{quantity} {name} (mapped source area)"
    return f"Collect: 0/{quantity} {name}"


def _overlay_id_from_requirement(requirement: dict) -> str | None:
    overlay_id = requirement.get("objective_overlay_id")
    if not overlay_id:
        return None
    return str(overlay_id)


def _format_branch_candidate_line(candidate: dict) -> str:
    branch_kind = str(candidate.get("branch_kind", "branch")).replace("_", " ")
    label = str(candidate.get("label", "Unknown"))
    return f"- {_humanize_relation(branch_kind)}: {label}"


def _single_overlay_id_from_quest_requirements(quest: dict) -> str | None:
    overlay_ids: list[str] = []
    for requirement in list(quest.get("target_requirements", [])) + list(quest.get("item_requirements", [])):
        overlay_id = _overlay_id_from_requirement(requirement)
        if overlay_id and overlay_id not in overlay_ids:
            overlay_ids.append(overlay_id)
    if len(overlay_ids) == 1:
        return overlay_ids[0]
    return None


def _objective_overlay_label(overlay: object) -> str:
    if not isinstance(overlay, dict):
        return "Objective Area"
    if overlay.get("target_name"):
        return str(overlay.get("target_name"))
    source_creatures = list(overlay.get("source_creatures", []))
    if source_creatures:
        first = source_creatures[0]
        return f"{len(source_creatures)} source creatures, e.g. {first.get('name', 'Unknown')}"
    return str(overlay.get("overlay_id", "Objective Area"))


def _filter_event_quests(quests: list[dict], include_event_quests: bool) -> list[dict]:
    if include_event_quests:
        return list(quests)
    return [quest for quest in quests if not bool(quest.get("is_event_quest", False))]


def _filter_visible_quests(
    quests: list[dict],
    *,
    include_event_quests: bool,
    min_level: int | None,
    max_level: int | None,
) -> list[dict]:
    filtered = _filter_event_quests(quests, include_event_quests)
    return [quest for quest in filtered if _quest_matches_level_filter(quest, min_level=min_level, max_level=max_level)]


def _quest_matches_level_filter(quest: dict, *, min_level: int | None, max_level: int | None) -> bool:
    quest_level = _quest_filter_level_value(quest)
    if min_level is not None and quest_level < min_level:
        return False
    if max_level is not None and quest_level > max_level:
        return False
    return True


def _quest_filter_level_value(quest: dict) -> int:
    quest_level = _int_from_value(quest.get("quest_level"))
    minimum_level = _int_from_value(quest.get("min_level"))
    if quest_level > 0:
        return quest_level
    if minimum_level > 0:
        return minimum_level
    return 0


def _parse_optional_level_filter(value: object) -> int | None:
    if value is None:
        return None
    stripped = str(value).strip()
    if not stripped:
        return None
    try:
        return max(0, int(stripped))
    except ValueError:
        return None


def _quest_marker_icon_kind(marker_kind: str, quests: list[dict]) -> str:
    if marker_kind == "quest_giver" and any(bool(quest.get("is_daily_quest", False)) for quest in quests):
        return "quest_giver_daily"
    return marker_kind


def _build_generated_marker_icon(kind: str) -> Image.Image | None:
    if kind == "herb":
        icon = Image.new("RGBA", (GENERATED_MARKER_ICON_SIZE, GENERATED_MARKER_ICON_SIZE), (0, 0, 0, 0))
        draw = ImageDraw.Draw(icon)
        draw.ellipse((2, 2, 15, 15), fill=(34, 139, 34, 235), outline=(220, 255, 220, 255), width=2)
        draw.line((9, 4, 9, 14), fill=(245, 255, 245, 255), width=2)
        draw.line((9, 8, 5, 6), fill=(245, 255, 245, 255), width=2)
        draw.line((9, 10, 13, 7), fill=(245, 255, 245, 255), width=2)
        return icon
    if kind == "ore":
        icon = Image.new("RGBA", (GENERATED_MARKER_ICON_SIZE, GENERATED_MARKER_ICON_SIZE), (0, 0, 0, 0))
        draw = ImageDraw.Draw(icon)
        draw.polygon(((9, 1), (16, 9), (9, 17), (2, 9)), fill=(120, 130, 145, 240), outline=(235, 240, 245, 255))
        draw.ellipse((6, 6, 11, 11), fill=(255, 214, 64, 255), outline=(255, 245, 180, 255))
        return icon
    return None


def _marker_icon_target_size(kind: str) -> int:
    base_size = 18
    if kind not in {"herb", "ore"}:
        return base_size
    scaled = int(round(base_size * MARKER_DISPLAY_SETTINGS.gather_node_icon_size_scale))
    return max(8, scaled)


def _remove_black_icon_background(icon: Image.Image) -> Image.Image:
    rgba = icon.convert("RGBA")
    cleaned = rgba.copy()
    pixels = cleaned.load()
    width, height = cleaned.size
    for y in range(height):
        for x in range(width):
            r, g, b, a = pixels[x, y]
            if a <= 0:
                continue
            if r <= 16 and g <= 16 and b <= 16:
                pixels[x, y] = (r, g, b, 0)
    bbox = cleaned.getbbox()
    if bbox is None:
        return rgba
    return cleaned.crop(bbox)


def _objective_center_image_coords(
    transform: ZoneCoordinateTransform,
    world_x: float,
    world_y: float,
    image_width: int,
    image_height: int,
) -> tuple[float, float]:
    zone_x, zone_y = transform.world_to_zone(world_x, world_y)
    return zone_x * image_width / 100.0, zone_y * image_height / 100.0


def _objective_area_canvas_radii(
    transform: ZoneCoordinateTransform,
    render_state: ZoneRenderState,
    area: dict,
    image_width: int,
    image_height: int,
) -> tuple[float, float]:
    center_x = float(area.get("center_x", 0.0))
    center_y = float(area.get("center_y", 0.0))
    radius = max(1.0, float(area.get("radius", 1.0)))
    center_image_x, center_image_y = _objective_center_image_coords(transform, center_x, center_y, image_width, image_height)
    edge_x_image_x, edge_x_image_y = _objective_center_image_coords(transform, center_x + radius, center_y, image_width, image_height)
    edge_y_image_x, edge_y_image_y = _objective_center_image_coords(transform, center_x, center_y + radius, image_width, image_height)
    scale_x = render_state.draw_width / float(render_state.viewport_width)
    scale_y = render_state.draw_height / float(render_state.viewport_height)
    radius_x = max(8.0, abs(edge_x_image_x - center_image_x) * scale_x)
    radius_y = max(8.0, abs(edge_y_image_y - center_image_y) * scale_y)
    return radius_x, radius_y


def _int_from_value(value: object) -> int:
    try:
        return int(value)  # type: ignore[arg-type]
    except (TypeError, ValueError):
        return 0


def _default_curve_handles_between_points(
    start_x: float,
    start_y: float,
    end_x: float,
    end_y: float,
) -> tuple[float, float, float, float]:
    delta_x = end_x - start_x
    delta_y = end_y - start_y
    return (
        start_x + delta_x / 3.0,
        start_y + delta_y / 3.0,
        start_x + (2.0 * delta_x) / 3.0,
        start_y + (2.0 * delta_y) / 3.0,
    )


def _sample_bezier_polyline(anchors: Sequence[EditorAnchor], subdivisions: int) -> list[tuple[float, float]]:
    if not anchors:
        return []
    if len(anchors) == 1:
        return [(anchors[0].image_x, anchors[0].image_y)]

    point_count = max(2, int(subdivisions))
    sampled_points: list[tuple[float, float]] = [(anchors[0].image_x, anchors[0].image_y)]
    for start_anchor, end_anchor in zip(anchors, anchors[1:]):
        control_1 = (
            start_anchor.handle_out_x if start_anchor.handle_out_x is not None else start_anchor.image_x,
            start_anchor.handle_out_y if start_anchor.handle_out_y is not None else start_anchor.image_y,
        )
        control_2 = (
            end_anchor.handle_in_x if end_anchor.handle_in_x is not None else end_anchor.image_x,
            end_anchor.handle_in_y if end_anchor.handle_in_y is not None else end_anchor.image_y,
        )
        for step in range(1, point_count + 1):
            t = step / float(point_count)
            sampled_points.append(
                _cubic_bezier_point(
                    (start_anchor.image_x, start_anchor.image_y),
                    control_1,
                    control_2,
                    (end_anchor.image_x, end_anchor.image_y),
                    t,
                )
            )
    return sampled_points


def _sample_bezier_polyline_with_segment_indices(
    anchors: Sequence[EditorAnchor],
    subdivisions: int,
) -> list[tuple[float, float, int]]:
    if not anchors:
        return []
    if len(anchors) == 1:
        return [(anchors[0].image_x, anchors[0].image_y, 0)]

    point_count = max(2, int(subdivisions))
    sampled_points: list[tuple[float, float, int]] = [(anchors[0].image_x, anchors[0].image_y, 0)]
    for segment_index, (start_anchor, end_anchor) in enumerate(zip(anchors, anchors[1:])):
        control_1 = (
            start_anchor.handle_out_x if start_anchor.handle_out_x is not None else start_anchor.image_x,
            start_anchor.handle_out_y if start_anchor.handle_out_y is not None else start_anchor.image_y,
        )
        control_2 = (
            end_anchor.handle_in_x if end_anchor.handle_in_x is not None else end_anchor.image_x,
            end_anchor.handle_in_y if end_anchor.handle_in_y is not None else end_anchor.image_y,
        )
        for step in range(1, point_count + 1):
            t = step / float(point_count)
            point_x, point_y = _cubic_bezier_point(
                (start_anchor.image_x, start_anchor.image_y),
                control_1,
                control_2,
                (end_anchor.image_x, end_anchor.image_y),
                t,
            )
            sampled_points.append((point_x, point_y, segment_index))
    return sampled_points


def _cubic_bezier_point(
    p0: tuple[float, float],
    p1: tuple[float, float],
    p2: tuple[float, float],
    p3: tuple[float, float],
    t: float,
) -> tuple[float, float]:
    inverse = 1.0 - t
    x = (
        (inverse ** 3) * p0[0]
        + 3.0 * (inverse ** 2) * t * p1[0]
        + 3.0 * inverse * (t ** 2) * p2[0]
        + (t ** 3) * p3[0]
    )
    y = (
        (inverse ** 3) * p0[1]
        + 3.0 * (inverse ** 2) * t * p1[1]
        + 3.0 * inverse * (t ** 2) * p2[1]
        + (t ** 3) * p3[1]
    )
    return x, y


def _sample_editor_path(
    anchors: Sequence[EditorAnchor],
    *,
    transform: ZoneCoordinateTransform,
    image_width: int,
    image_height: int,
    map_id: int | None,
    spacing_yards: float,
    subdivisions: int,
) -> list[dict[str, object]]:
    dense_points = _sample_bezier_polyline(anchors, subdivisions)
    if not dense_points:
        return []
    dense_world_points = [
        transform.image_to_world(image_x, image_y, image_width, image_height)
        for image_x, image_y in dense_points
    ]

    resolved_map_id = map_id if map_id is not None else transform.map_id
    sampled: list[dict[str, object]] = []
    start_x, start_y = dense_points[0]
    sampled.append(_movement_point_payload(start_x, start_y, transform, image_width, image_height, resolved_map_id, 0, 0.0))
    distance_since_last_sample = 0.0
    prev_image_x, prev_image_y = start_x, start_y
    prev_world_x, prev_world_y = dense_world_points[0]
    distance_from_start = 0.0

    for point_index, (image_x, image_y) in enumerate(dense_points[1:], start=1):
        current_world_x, current_world_y = dense_world_points[point_index]
        segment_length = _distance_between_points(prev_world_x, prev_world_y, current_world_x, current_world_y)
        if segment_length <= 1e-6:
            prev_image_x, prev_image_y = image_x, image_y
            prev_world_x, prev_world_y = current_world_x, current_world_y
            continue

        remaining_length = segment_length
        segment_start_image_x = prev_image_x
        segment_start_image_y = prev_image_y
        segment_start_world_x = prev_world_x
        segment_start_world_y = prev_world_y
        desired_spacing_yards = _adaptive_spacing_yards(dense_world_points, point_index, spacing_yards)
        while distance_since_last_sample + remaining_length >= desired_spacing_yards:
            ratio = (desired_spacing_yards - distance_since_last_sample) / remaining_length
            sample_image_x = segment_start_image_x + (image_x - segment_start_image_x) * ratio
            sample_image_y = segment_start_image_y + (image_y - segment_start_image_y) * ratio
            sample_world_x = segment_start_world_x + (current_world_x - segment_start_world_x) * ratio
            sample_world_y = segment_start_world_y + (current_world_y - segment_start_world_y) * ratio
            distance_from_start += desired_spacing_yards - distance_since_last_sample
            sampled.append(
                _movement_point_payload(
                    sample_image_x,
                    sample_image_y,
                    transform,
                    image_width,
                    image_height,
                    resolved_map_id,
                    len(sampled),
                    distance_from_start,
                    world_override=(sample_world_x, sample_world_y),
                )
            )
            segment_start_image_x = sample_image_x
            segment_start_image_y = sample_image_y
            segment_start_world_x = sample_world_x
            segment_start_world_y = sample_world_y
            remaining_length = _distance_between_points(segment_start_world_x, segment_start_world_y, current_world_x, current_world_y)
            distance_since_last_sample = 0.0
            desired_spacing_yards = _adaptive_spacing_yards(dense_world_points, point_index, spacing_yards)
            if remaining_length <= 1e-6:
                break

        distance_since_last_sample += remaining_length
        distance_from_start += remaining_length
        prev_image_x, prev_image_y = image_x, image_y
        prev_world_x, prev_world_y = current_world_x, current_world_y

    final_image_x, final_image_y = dense_points[-1]
    if not _points_nearly_equal((sampled[-1]["image_x"], sampled[-1]["image_y"]), (final_image_x, final_image_y)):
        sampled.append(
            _movement_point_payload(
                final_image_x,
                final_image_y,
                transform,
                image_width,
                image_height,
                resolved_map_id,
                len(sampled),
                distance_from_start,
            )
        )
    return sampled


def _movement_point_payload(
    image_x: float,
    image_y: float,
    transform: ZoneCoordinateTransform,
    image_width: int,
    image_height: int,
    map_id: int,
    point_index: int,
    distance_from_start_yards: float,
    *,
    world_override: tuple[float, float] | None = None,
) -> dict[str, object]:
    zone_x, zone_y = transform.image_to_zone(image_x, image_y, image_width, image_height)
    if world_override is None:
        world_x, world_y = transform.image_to_world(image_x, image_y, image_width, image_height)
    else:
        world_x, world_y = world_override
    return {
        "point_index": point_index,
        "map_id": map_id,
        "image_x": round(image_x, 2),
        "image_y": round(image_y, 2),
        "zone_x": round(zone_x, 4),
        "zone_y": round(zone_y, 4),
        "world_x": round(world_x, 4),
        "world_y": round(world_y, 4),
        "distance_from_start_yards": round(distance_from_start_yards, 2),
    }


def _editor_anchor_payload(
    anchor: EditorAnchor,
    *,
    transform: ZoneCoordinateTransform,
    image_width: int,
    image_height: int,
) -> dict[str, object]:
    payload = _point_projection_payload(anchor.image_x, anchor.image_y, transform, image_width, image_height)
    payload["handle_in"] = (
        _point_projection_payload(anchor.handle_in_x, anchor.handle_in_y, transform, image_width, image_height)
        if anchor.handle_in_x is not None and anchor.handle_in_y is not None
        else None
    )
    payload["handle_out"] = (
        _point_projection_payload(anchor.handle_out_x, anchor.handle_out_y, transform, image_width, image_height)
        if anchor.handle_out_x is not None and anchor.handle_out_y is not None
        else None
    )
    return payload


def _editor_path_connection_payload(
    connection: EditorPathConnection | None,
    route_group_key: str,
) -> dict[str, object] | None:
    if connection is None:
        return None
    return {
        "path_index": connection.path_index,
        "path_key": _editor_path_export_key(route_group_key, connection.path_index),
        "anchor_index": connection.anchor_index,
        "anchor_label": f"P{connection.path_index + 1}-A{connection.anchor_index + 1}",
    }


def _editor_connection_from_payload(payload: object) -> EditorPathConnection | None:
    if not isinstance(payload, dict):
        return None
    try:
        path_index = int(payload["path_index"])
        anchor_index = int(payload["anchor_index"])
    except (KeyError, TypeError, ValueError):
        return None
    return EditorPathConnection(path_index=path_index, anchor_index=anchor_index)


def _editor_anchor_from_payload(payload: object) -> EditorAnchor | None:
    if not isinstance(payload, dict):
        return None
    try:
        image_x = float(payload["image_x"])
        image_y = float(payload["image_y"])
    except (KeyError, TypeError, ValueError):
        return None

    handle_in = payload.get("handle_in")
    handle_out = payload.get("handle_out")

    def _handle_coords(handle_payload: object) -> tuple[float | None, float | None]:
        if not isinstance(handle_payload, dict):
            return None, None
        try:
            return float(handle_payload["image_x"]), float(handle_payload["image_y"])
        except (KeyError, TypeError, ValueError):
            return None, None

    handle_in_x, handle_in_y = _handle_coords(handle_in)
    handle_out_x, handle_out_y = _handle_coords(handle_out)
    return EditorAnchor(
        image_x=image_x,
        image_y=image_y,
        handle_in_x=handle_in_x,
        handle_in_y=handle_in_y,
        handle_out_x=handle_out_x,
        handle_out_y=handle_out_y,
    )


def _serialize_editor_path_state(
    path_state: EditorPathState,
    *,
    route_group_key: str,
    sample_spacing_yards: float,
    asset: ZoneCompositeAsset,
) -> dict[str, object]:
    path_payloads: list[dict[str, object]] = []
    transform = asset.coordinate_transform
    if transform is None:
        raise ValueError("asset is missing coordinate transform")

    for path_index, path in enumerate(path_state.paths):
        path_payloads.append(
            {
                "path_index": path_index,
                "path_key": _editor_path_export_key(route_group_key, path_index),
                "finalized": path.finalized,
                "start_connection": _editor_path_connection_payload(path.start_connection, route_group_key),
                "end_connection": _editor_path_connection_payload(path.end_connection, route_group_key),
                "anchors": [
                    _editor_anchor_payload(
                        anchor,
                        transform=transform,
                        image_width=asset.width,
                        image_height=asset.height,
                    )
                    for anchor in path.anchors
                ],
            }
        )

    return {
        "format": "lw_zone_editor_route",
        "version": 1,
        "route_group_key": route_group_key,
        "zone_name": asset.zone_name,
        "zone_id": asset.zone_id,
        "map_id": asset.map_id,
        "world_map_area_id": asset.world_map_area_id,
        "sample_spacing_yards": sample_spacing_yards,
        "path_count": len(path_state.paths),
        "paths": path_payloads,
    }


def _deserialize_editor_path_state(payload: object) -> tuple[EditorPathState, str, float]:
    if not isinstance(payload, dict):
        raise ValueError("route payload must be a JSON object")

    route_group_key = str(payload.get("route_group_key") or "route_001")
    try:
        sample_spacing_yards = float(payload.get("sample_spacing_yards", DEFAULT_EDIT_SAMPLE_SPACING_YARDS))
    except (TypeError, ValueError):
        sample_spacing_yards = DEFAULT_EDIT_SAMPLE_SPACING_YARDS

    raw_paths = payload.get("paths")
    if not isinstance(raw_paths, list):
        raise ValueError("route payload is missing a paths array")

    parsed_paths: list[EditorPath] = []
    active_path_index: int | None = None
    for path_index, raw_path in enumerate(raw_paths):
        if not isinstance(raw_path, dict):
            continue
        raw_anchors = raw_path.get("anchors", [])
        if not isinstance(raw_anchors, list):
            raw_anchors = []
        anchors = [anchor for anchor in (_editor_anchor_from_payload(entry) for entry in raw_anchors) if anchor is not None]
        path = EditorPath(
            anchors=anchors,
            finalized=bool(raw_path.get("finalized", len(anchors) >= 2)),
            start_connection=_editor_connection_from_payload(raw_path.get("start_connection")),
            end_connection=_editor_connection_from_payload(raw_path.get("end_connection")),
        )
        parsed_paths.append(path)
        if not path.finalized:
            active_path_index = path_index

    return EditorPathState(paths=parsed_paths, active_path_index=active_path_index), route_group_key, max(
        1.0, sample_spacing_yards
    )


def _point_projection_payload(
    image_x: float,
    image_y: float,
    transform: ZoneCoordinateTransform,
    image_width: int,
    image_height: int,
) -> dict[str, float]:
    zone_x, zone_y = transform.image_to_zone(image_x, image_y, image_width, image_height)
    world_x, world_y = transform.image_to_world(image_x, image_y, image_width, image_height)
    return {
        "image_x": round(image_x, 2),
        "image_y": round(image_y, 2),
        "zone_x": round(zone_x, 4),
        "zone_y": round(zone_y, 4),
        "world_x": round(world_x, 4),
        "world_y": round(world_y, 4),
    }


def _distance_between_points(ax: float, ay: float, bx: float, by: float) -> float:
    return ((ax - bx) ** 2 + (ay - by) ** 2) ** 0.5


def _clamp(value: float, minimum: float, maximum: float) -> float:
    return max(minimum, min(maximum, value))


def _turn_angle_radians(
    previous_point: tuple[float, float],
    current_point: tuple[float, float],
    next_point: tuple[float, float],
) -> float:
    previous_vector = (current_point[0] - previous_point[0], current_point[1] - previous_point[1])
    next_vector = (next_point[0] - current_point[0], next_point[1] - current_point[1])
    previous_length = _distance_between_points(0.0, 0.0, previous_vector[0], previous_vector[1])
    next_length = _distance_between_points(0.0, 0.0, next_vector[0], next_vector[1])
    if previous_length <= 1e-6 or next_length <= 1e-6:
        return 0.0

    cosine = (
        (previous_vector[0] * next_vector[0]) + (previous_vector[1] * next_vector[1])
    ) / (previous_length * next_length)
    cosine = _clamp(cosine, -1.0, 1.0)
    return __import__("math").acos(cosine)


def _adaptive_spacing_yards(
    world_points: Sequence[tuple[float, float]],
    point_index: int,
    base_spacing_yards: float,
) -> float:
    if len(world_points) < 3 or point_index <= 0 or point_index >= len(world_points) - 1:
        return base_spacing_yards

    turn_angle = _turn_angle_radians(world_points[point_index - 1], world_points[point_index], world_points[point_index + 1])
    if turn_angle >= 1.2:  # ~69 degrees
        factor = ADAPTIVE_SPACING_MIN_FACTOR
    elif turn_angle >= 0.7:  # ~40 degrees
        factor = 0.75
    elif turn_angle >= 0.3:  # ~17 degrees
        factor = 1.0
    else:
        factor = ADAPTIVE_SPACING_MAX_FACTOR
    return max(1.0, base_spacing_yards * factor)


def _points_nearly_equal(first: tuple[object, object], second: tuple[float, float], epsilon: float = 0.01) -> bool:
    try:
        first_x = float(first[0])
        first_y = float(first[1])
    except (TypeError, ValueError):
        return False
    return _distance_between_points(first_x, first_y, second[0], second[1]) <= epsilon


def _project_point_onto_segment(
    point_x: float,
    point_y: float,
    start_x: float,
    start_y: float,
    end_x: float,
    end_y: float,
) -> tuple[float, float, float, float]:
    segment_x = end_x - start_x
    segment_y = end_y - start_y
    segment_length_squared = (segment_x * segment_x) + (segment_y * segment_y)
    if segment_length_squared <= 1e-6:
        distance = _distance_between_points(point_x, point_y, start_x, start_y)
        return distance, start_x, start_y, 0.0

    ratio = ((point_x - start_x) * segment_x + (point_y - start_y) * segment_y) / segment_length_squared
    ratio = _clamp(ratio, 0.0, 1.0)
    projected_x = start_x + segment_x * ratio
    projected_y = start_y + segment_y * ratio
    distance = _distance_between_points(point_x, point_y, projected_x, projected_y)
    return distance, projected_x, projected_y, ratio


def _move_anchor_with_handles(anchor: EditorAnchor, image_x: float, image_y: float) -> None:
    delta_x = image_x - anchor.image_x
    delta_y = image_y - anchor.image_y
    anchor.image_x = image_x
    anchor.image_y = image_y
    if anchor.handle_in_x is not None and anchor.handle_in_y is not None:
        anchor.handle_in_x += delta_x
        anchor.handle_in_y += delta_y
    if anchor.handle_out_x is not None and anchor.handle_out_y is not None:
        anchor.handle_out_x += delta_x
        anchor.handle_out_y += delta_y


def _anchor_has_connected_path_references(path_state: EditorPathState, path_index: int, anchor_index: int) -> bool:
    if not (0 <= path_index < len(path_state.paths)):
        return False
    path = path_state.paths[path_index]
    if not (0 <= anchor_index < len(path.anchors)):
        return False

    if anchor_index == 0 and path.start_connection is not None:
        return True
    if anchor_index == len(path.anchors) - 1 and path.end_connection is not None:
        return True

    target = EditorPathConnection(path_index=path_index, anchor_index=anchor_index)
    for other_path in path_state.paths:
        if other_path.start_connection == target or other_path.end_connection == target:
            return True
    return False


def _shift_anchor_references_after_insert(path_state: EditorPathState, path_index: int, inserted_anchor_index: int) -> None:
    for path in path_state.paths:
        if path.start_connection is not None and path.start_connection.path_index == path_index and path.start_connection.anchor_index >= inserted_anchor_index:
            path.start_connection = EditorPathConnection(path_index=path_index, anchor_index=path.start_connection.anchor_index + 1)
        if path.end_connection is not None and path.end_connection.path_index == path_index and path.end_connection.anchor_index >= inserted_anchor_index:
            path.end_connection = EditorPathConnection(path_index=path_index, anchor_index=path.end_connection.anchor_index + 1)


def _shift_anchor_references_after_delete(path_state: EditorPathState, path_index: int, deleted_anchor_index: int) -> None:
    for path in path_state.paths:
        if path.start_connection is not None and path.start_connection.path_index == path_index and path.start_connection.anchor_index > deleted_anchor_index:
            path.start_connection = EditorPathConnection(path_index=path_index, anchor_index=path.start_connection.anchor_index - 1)
        if path.end_connection is not None and path.end_connection.path_index == path_index and path.end_connection.anchor_index > deleted_anchor_index:
            path.end_connection = EditorPathConnection(path_index=path_index, anchor_index=path.end_connection.anchor_index - 1)


def _shift_path_references_after_remove(path_state: EditorPathState, removed_path_index: int) -> None:
    for path in path_state.paths:
        if path.start_connection is not None and path.start_connection.path_index > removed_path_index:
            path.start_connection = EditorPathConnection(path_index=path.start_connection.path_index - 1, anchor_index=path.start_connection.anchor_index)
        if path.end_connection is not None and path.end_connection.path_index > removed_path_index:
            path.end_connection = EditorPathConnection(path_index=path.end_connection.path_index - 1, anchor_index=path.end_connection.anchor_index)


def _insert_anchor_into_path(path_state: EditorPathState, path_index: int, insert_after_anchor_index: int, image_x: float, image_y: float) -> bool:
    if not (0 <= path_index < len(path_state.paths)):
        return False
    path = path_state.paths[path_index]
    if not (0 <= insert_after_anchor_index < len(path.anchors) - 1):
        return False

    next_anchor_index = insert_after_anchor_index + 1
    previous_anchor = path.anchors[insert_after_anchor_index]
    next_anchor = path.anchors[next_anchor_index]
    new_anchor = EditorAnchor(image_x=image_x, image_y=image_y)

    previous_out_x, previous_out_y, new_in_x, new_in_y = _default_curve_handles_between_points(
        previous_anchor.image_x,
        previous_anchor.image_y,
        image_x,
        image_y,
    )
    new_out_x, new_out_y, next_in_x, next_in_y = _default_curve_handles_between_points(
        image_x,
        image_y,
        next_anchor.image_x,
        next_anchor.image_y,
    )

    previous_anchor.handle_out_x = previous_out_x
    previous_anchor.handle_out_y = previous_out_y
    new_anchor.handle_in_x = new_in_x
    new_anchor.handle_in_y = new_in_y
    new_anchor.handle_out_x = new_out_x
    new_anchor.handle_out_y = new_out_y
    next_anchor.handle_in_x = next_in_x
    next_anchor.handle_in_y = next_in_y

    path.anchors.insert(next_anchor_index, new_anchor)
    _shift_anchor_references_after_insert(path_state, path_index, next_anchor_index)
    return True


def _delete_anchor_from_path(path_state: EditorPathState, path_index: int, anchor_index: int) -> bool:
    if not (0 <= path_index < len(path_state.paths)):
        return False
    path = path_state.paths[path_index]
    if not (0 <= anchor_index < len(path.anchors)):
        return False
    if _anchor_has_connected_path_references(path_state, path_index, anchor_index):
        return False

    path.anchors.pop(anchor_index)
    _shift_anchor_references_after_delete(path_state, path_index, anchor_index)

    if not path.anchors:
        path_state.paths.pop(path_index)
        _shift_path_references_after_remove(path_state, path_index)
        if path_state.active_path_index is not None:
            if path_state.active_path_index == path_index:
                path_state.active_path_index = None
            elif path_state.active_path_index > path_index:
                path_state.active_path_index -= 1
        return True

    if len(path.anchors) == 1:
        only_anchor = path.anchors[0]
        only_anchor.handle_in_x = None
        only_anchor.handle_in_y = None
        only_anchor.handle_out_x = None
        only_anchor.handle_out_y = None
        return True

    if anchor_index == 0:
        first_anchor = path.anchors[0]
        first_anchor.handle_in_x = None
        first_anchor.handle_in_y = None
    elif anchor_index >= len(path.anchors):
        last_anchor = path.anchors[-1]
        last_anchor.handle_out_x = None
        last_anchor.handle_out_y = None
    else:
        previous_anchor = path.anchors[anchor_index - 1]
        next_anchor = path.anchors[anchor_index]
        previous_out_x, previous_out_y, next_in_x, next_in_y = _default_curve_handles_between_points(
            previous_anchor.image_x,
            previous_anchor.image_y,
            next_anchor.image_x,
            next_anchor.image_y,
        )
        previous_anchor.handle_out_x = previous_out_x
        previous_anchor.handle_out_y = previous_out_y
        next_anchor.handle_in_x = next_in_x
        next_anchor.handle_in_y = next_in_y
    return True


def _mark_complete_paths_finalized(path_state: EditorPathState) -> None:
    for path in path_state.paths:
        if len(path.anchors) >= 2:
            path.finalized = True
    path_state.active_path_index = None


def _iter_connected_anchor_indices(
    path_state: EditorPathState,
    path_index: int,
    anchor_index: int,
) -> set[tuple[int, int]]:
    connected: set[tuple[int, int]] = set()
    pending = [(path_index, anchor_index)]

    while pending:
        current_path_index, current_anchor_index = pending.pop()
        current_key = (current_path_index, current_anchor_index)
        if current_key in connected:
            continue
        connected.add(current_key)

        if not (0 <= current_path_index < len(path_state.paths)):
            continue
        current_path = path_state.paths[current_path_index]

        if current_anchor_index == 0 and current_path.start_connection is not None:
            pending.append((current_path.start_connection.path_index, current_path.start_connection.anchor_index))
        if current_path.anchors and current_anchor_index == len(current_path.anchors) - 1 and current_path.end_connection is not None:
            pending.append((current_path.end_connection.path_index, current_path.end_connection.anchor_index))

        for other_path_index, other_path in enumerate(path_state.paths):
            if other_path.start_connection == EditorPathConnection(path_index=current_path_index, anchor_index=current_anchor_index):
                if other_path.anchors:
                    pending.append((other_path_index, 0))
            if other_path.end_connection == EditorPathConnection(path_index=current_path_index, anchor_index=current_anchor_index):
                if other_path.anchors:
                    pending.append((other_path_index, len(other_path.anchors) - 1))

    return connected


def _propagate_connected_anchor_position(
    path_state: EditorPathState,
    path_index: int,
    anchor_index: int,
) -> None:
    if not (0 <= path_index < len(path_state.paths)):
        return
    path = path_state.paths[path_index]
    if not (0 <= anchor_index < len(path.anchors)):
        return

    source_anchor = path.anchors[anchor_index]
    connected_indices = _iter_connected_anchor_indices(path_state, path_index, anchor_index)
    for connected_path_index, connected_anchor_index in connected_indices:
        if connected_path_index == path_index and connected_anchor_index == anchor_index:
            continue
        connected_path = path_state.paths[connected_path_index]
        if 0 <= connected_anchor_index < len(connected_path.anchors):
            _move_anchor_with_handles(
                connected_path.anchors[connected_anchor_index],
                source_anchor.image_x,
                source_anchor.image_y,
            )


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Open the standalone LivingWorld explored-zone viewer.")
    parser.add_argument(
        "--composite-dir",
        default=str(COMPOSITE_MAPS_DIR),
        help="Directory containing explored zone composites named 'ZoneName - ZoneId.png'.",
    )
    parser.add_argument("--zone", help="Optional initial zone-name substring or zone id to preselect.")
    parser.add_argument(
        "--smoke-test",
        action="store_true",
        help="Instantiate the viewer, load the initial zone, then exit immediately for validation.",
    )
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    assets = discover_zone_composites(args.composite_dir)
    if not assets:
        print("[lw-zone-editor] no explored composites found in:", args.composite_dir)
        return 1

    app = ZoneViewerApp(assets, initial_zone=args.zone)
    if args.smoke_test:
        app.update_idletasks()
        app.update()
        print(f"[lw-zone-editor] discovered zones: {len(assets)}")
        if app.current_asset:
            print(f"[lw-zone-editor] loaded zone: {app.current_asset.label}")
        app.destroy()
        return 0

    app.mainloop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
