from __future__ import annotations

import json
import re
import shutil
import struct
import subprocess
from pathlib import Path
from typing import Sequence

from PIL import Image
import PIL.BlpImagePlugin  # noqa: F401 - ensures BLP support is registered

from .paths import (
    APP_ROOT,
    COMPOSITE_MAPS_DIR,
    MANIFESTS_DIR,
    PNG_MAPS_DIR,
    PNG_ICONS_DIR,
    RAW_BLP_DIR,
    RAW_ICON_BLP_DIR,
)

REPO_ROOT = APP_ROOT.parents[1]
DBC_DIR = REPO_ROOT / "var" / "extractors" / "dbc"
WORLD_MAP_AREA_DBC_PATH = DBC_DIR / "WorldMapArea.dbc"
WORLD_MAP_OVERLAY_DBC_PATH = DBC_DIR / "WorldMapOverlay.dbc"
AREA_TABLE_DBC_PATH = DBC_DIR / "AreaTable.dbc"
WORLD_MAP_AREA_RECORD_FORMAT = "<4I4f3I"
WORLD_MAP_OVERLAY_RECORD_FORMAT = "<17I"
INVALID_WINDOWS_FILENAME_CHARS = re.compile(r'[<>:"/\\|?*]')

KNOWN_LOCALES: tuple[str, ...] = (
    "enGB",
    "enUS",
    "deDE",
    "esES",
    "frFR",
    "koKR",
    "zhCN",
    "zhTW",
    "enCN",
    "enTW",
    "esMX",
    "ruRU",
)

WORLD_MAP_PATTERNS: tuple[str, ...] = (
    r"Interface\WorldMap\*",
    r"World\Minimaps\*",
    r"Interface\Minimap\*",
)

ICON_PATTERNS: tuple[str, ...] = (
    r"Interface\Icons\*",
)

PILOT_ZONE_SPECS: tuple[dict[str, str | int], ...] = (
    {
        "asset_key": "stormwind_city",
        "zone_dir": "stormwind",
        "tile_prefix": "stormwind",
        "output_name": "stormwind_city.png",
        "columns": 4,
    },
    {
        "asset_key": "westfall",
        "zone_dir": "westfall",
        "tile_prefix": "westfall",
        "output_name": "westfall.png",
        "columns": 4,
    },
    {
        "asset_key": "elwynn_forest",
        "zone_dir": "elwynn",
        "tile_prefix": "elwynn",
        "output_name": "elwynn_forest.png",
        "columns": 4,
    },
)


def load_world_map_area_records(dbc_path: str | Path = WORLD_MAP_AREA_DBC_PATH) -> list[dict]:
    _, field_count, record_size, records, strings = _read_wdbc_file(dbc_path)
    expected_size = struct.calcsize(WORLD_MAP_AREA_RECORD_FORMAT)
    if field_count < 11 or record_size != expected_size:
        raise ValueError(
            f"Unexpected WorldMapArea.dbc layout: fields={field_count} record_size={record_size}"
        )

    parsed: list[dict] = []
    for index in range(len(records) // record_size):
        row = struct.unpack_from(WORLD_MAP_AREA_RECORD_FORMAT, records, index * record_size)
        parsed.append(
            {
                "world_map_area_id": row[0],
                "map_id": row[1],
                "zone_id": row[2],
                "internal_name": _get_wdbc_string(strings, row[3]).strip(),
                "left": row[4],
                "right": row[5],
                "top": row[6],
                "bottom": row[7],
                "display_map_id": row[8],
                "floor": row[9],
                "parent_world_map_area_id": row[10],
            }
        )
    return parsed


def load_area_table_names(dbc_path: str | Path = AREA_TABLE_DBC_PATH) -> dict[int, str]:
    _, field_count, record_size, records, strings = _read_wdbc_file(dbc_path)
    if field_count < 12:
        raise ValueError(f"Unexpected AreaTable.dbc layout: fields={field_count}")

    area_names: dict[int, str] = {}
    for index in range(len(records) // record_size):
        row = struct.unpack_from(f"<{field_count}I", records, index * record_size)
        area_id = int(row[0])
        name = _get_wdbc_string(strings, row[11]).strip()
        if area_id and name:
            area_names[area_id] = name
    return area_names


def load_world_map_overlay_records(dbc_path: str | Path = WORLD_MAP_OVERLAY_DBC_PATH) -> list[dict]:
    _, field_count, record_size, records, strings = _read_wdbc_file(dbc_path)
    expected_size = struct.calcsize(WORLD_MAP_OVERLAY_RECORD_FORMAT)
    if field_count < 17 or record_size != expected_size:
        raise ValueError(
            f"Unexpected WorldMapOverlay.dbc layout: fields={field_count} record_size={record_size}"
        )

    parsed: list[dict] = []
    for index in range(len(records) // record_size):
        row = struct.unpack_from(WORLD_MAP_OVERLAY_RECORD_FORMAT, records, index * record_size)
        parsed.append(
            {
                "id": row[0],
                "world_map_area_id": row[1],
                "area_ids": [value for value in row[2:6] if value],
                "internal_name": _get_wdbc_string(strings, row[8]).strip(),
                "texture_width": row[9],
                "texture_height": row[10],
                "offset_x": row[11],
                "offset_y": row[12],
                "hit_rect": list(row[13:17]),
            }
        )
    return parsed


def discover_all_zone_stitch_specs(
    png_input_dir: str | Path = PNG_MAPS_DIR,
    world_map_area_dbc_path: str | Path = WORLD_MAP_AREA_DBC_PATH,
    area_table_dbc_path: str | Path = AREA_TABLE_DBC_PATH,
) -> dict:
    png_root = Path(png_input_dir)
    worldmap_root = png_root / "interface" / "worldmap"

    area_names = load_area_table_names(area_table_dbc_path)
    records_by_key: dict[str, list[dict]] = {}
    records_by_folder_key: dict[str, list[dict]] = {}
    for record in load_world_map_area_records(world_map_area_dbc_path):
        key = _normalize_zone_key(record["internal_name"])
        if key:
            records_by_key.setdefault(key, []).append(record)
            records_by_folder_key.setdefault(_worldmap_folder_key(str(record["internal_name"])), []).append(record)

    matched: list[dict] = []
    unmatched: list[dict] = []
    duplicate_keys: list[dict] = []

    if not worldmap_root.is_dir():
        return {
            "png_input_dir": str(png_root),
            "worldmap_dir": str(worldmap_root),
            "matched": matched,
            "unmatched": unmatched,
            "duplicate_keys": duplicate_keys,
        }

    for zone_dir in sorted(worldmap_root.iterdir(), key=lambda path: path.name.lower()):
        if not zone_dir.is_dir():
            continue

        folder_key = _normalize_zone_key(zone_dir.name)
        folder_records = list(records_by_key.get(folder_key, []))
        for record in records_by_folder_key.get(folder_key, []):
            if record not in folder_records:
                folder_records.append(record)

        specs_for_folder = _discover_folder_specs(zone_dir, folder_records, area_names)
        if specs_for_folder:
            matched.extend(specs_for_folder)
            continue

        detected_prefixes = _discover_folder_prefixes(zone_dir)
        if detected_prefixes:
            unmatched.append(
                {
                    "zone_dir": str(zone_dir),
                    "folder_name": zone_dir.name,
                    "tile_prefixes": detected_prefixes,
                }
            )

    return {
        "png_input_dir": str(png_root),
        "worldmap_dir": str(worldmap_root),
        "matched": matched,
        "unmatched": unmatched,
        "duplicate_keys": duplicate_keys,
    }


def stitch_all_zone_composites(
    png_input_dir: str | Path = PNG_MAPS_DIR,
    composite_output_dir: str | Path = COMPOSITE_MAPS_DIR,
    world_map_area_dbc_path: str | Path = WORLD_MAP_AREA_DBC_PATH,
    area_table_dbc_path: str | Path = AREA_TABLE_DBC_PATH,
    world_map_overlay_dbc_path: str | Path = WORLD_MAP_OVERLAY_DBC_PATH,
) -> dict:
    composite_root = Path(composite_output_dir)
    composite_root.mkdir(parents=True, exist_ok=True)

    discovery = discover_all_zone_stitch_specs(
        png_input_dir=png_input_dir,
        world_map_area_dbc_path=world_map_area_dbc_path,
        area_table_dbc_path=area_table_dbc_path,
    )
    overlays_by_world_map_area_id: dict[int, list[dict]] = {}
    for record in load_world_map_overlay_records(world_map_overlay_dbc_path):
        overlays_by_world_map_area_id.setdefault(int(record["world_map_area_id"]), []).append(record)

    stitched: list[dict] = []
    missing: list[dict] = []
    for spec in discovery["matched"]:
        output_path = composite_root / str(spec["output_name"])
        result = stitch_zone_with_overlays(
            zone_dir=spec["zone_dir"],
            tile_prefix=str(spec["tile_prefix"]),
            output_path=output_path,
            overlay_records=overlays_by_world_map_area_id.get(int(spec["world_map_area_id"]), []),
            columns=4,
        )
        result.update(
            {
                "folder_name": spec["folder_name"],
                "world_map_area_id": spec["world_map_area_id"],
                "map_id": spec["map_id"],
                "zone_id": spec["zone_id"],
                "zone_name": spec["zone_name"],
                "internal_name": spec["internal_name"],
            }
        )
        if result["tile_count"] > 0:
            stitched.append(result)
        else:
            missing.append(result)

    return {
        "png_input_dir": discovery["png_input_dir"],
        "composite_output_dir": str(composite_root),
        "stitched": stitched,
        "missing": missing,
        "unmatched": discovery["unmatched"],
        "duplicate_keys": discovery["duplicate_keys"],
    }


def format_zone_output_filename(zone_name: str, zone_id: int) -> str:
    safe_name = INVALID_WINDOWS_FILENAME_CHARS.sub("-", zone_name).strip().rstrip(".")
    return f"{safe_name} - {zone_id}.png"


def detect_data_dir(client_dir: str | Path) -> Path:
    root = Path(client_dir)
    data_dir = root / "Data"
    return data_dir if data_dir.is_dir() else root


def detect_locale(data_dir: str | Path) -> str | None:
    data_root = Path(data_dir)
    for locale in KNOWN_LOCALES:
        if (data_root / locale).is_dir():
            return locale
    return None


def discover_archives(client_dir: str | Path, locale: str | None = None) -> list[Path]:
    data_dir = detect_data_dir(client_dir)
    detected_locale = locale or detect_locale(data_dir)

    ordered: list[Path] = []

    base_names = [
        "common.MPQ",
        "common-2.MPQ",
        "expansion.MPQ",
        "lichking.MPQ",
    ]
    for name in base_names:
        candidate = data_dir / name
        if candidate.is_file():
            ordered.append(candidate)

    ordered.extend(_sorted_matching(data_dir, "patch*.MPQ"))

    if detected_locale:
        locale_dir = data_dir / detected_locale
        locale_names = [
            f"locale-{detected_locale}.MPQ",
            f"expansion-locale-{detected_locale}.MPQ",
            f"lichking-locale-{detected_locale}.MPQ",
        ]
        for name in locale_names:
            candidate = locale_dir / name
            if candidate.is_file():
                ordered.append(candidate)

        ordered.extend(_sorted_matching(locale_dir, f"patch-{detected_locale}*.MPQ"))

    deduped: list[Path] = []
    seen: set[str] = set()
    for item in ordered:
        key = str(item.resolve())
        if key not in seen:
            seen.add(key)
            deduped.append(item)
    return deduped


def resolve_extractor_path(explicit: str | Path | None = None) -> Path:
    candidates: list[Path] = []
    if explicit:
        candidates.append(Path(explicit))

    which = shutil.which("MPQExtractor") or shutil.which("MPQExtractor.exe")
    if which:
        candidates.append(Path(which))

    candidates.extend(
        [
            REPO_ROOT / "tools" / "MPQExtractor" / "build" / "bin" / "MPQExtractor.exe",
            REPO_ROOT / "tools" / "MPQExtractor" / "build" / "bin" / "MPQExtractor",
            REPO_ROOT / "tools" / "third_party" / "MPQExtractor" / "build" / "bin" / "MPQExtractor.exe",
            REPO_ROOT / "tools" / "third_party" / "MPQExtractor" / "build" / "bin" / "MPQExtractor",
        ]
    )

    for candidate in candidates:
        if candidate.is_file():
            return candidate

    raise FileNotFoundError(
        "Could not find MPQExtractor. Build or provide the Kanma/MPQExtractor binary first."
    )


def build_extract_command(
    extractor_path: str | Path,
    archive_path: str | Path,
    output_dir: str | Path,
    pattern: str,
) -> list[str]:
    return [
        str(extractor_path),
        "-e",
        pattern,
        "-f",
        "-c",
        "-o",
        str(output_dir),
        str(archive_path),
    ]


def extract_client_archive_assets(
    extractor_path: str | Path,
    archive_paths: Sequence[str | Path],
    raw_output_dir: str | Path,
    patterns: Sequence[str],
    dry_run: bool = False,
) -> dict:
    raw_dir = Path(raw_output_dir)
    raw_dir.mkdir(parents=True, exist_ok=True)

    commands: list[list[str]] = []
    for archive in archive_paths:
        for pattern in patterns:
            commands.append(build_extract_command(extractor_path, archive, raw_dir, pattern))

    executed = 0
    if not dry_run:
        for command in commands:
            subprocess.run(command, check=True)
            executed += 1

    return {
        "archives": [str(Path(p)) for p in archive_paths],
        "patterns": list(patterns),
        "command_count": len(commands),
        "executed_command_count": executed,
        "commands": commands,
        "raw_output_dir": str(raw_dir),
        "dry_run": dry_run,
    }


def extract_client_map_assets(
    extractor_path: str | Path,
    archive_paths: Sequence[str | Path],
    raw_output_dir: str | Path = RAW_BLP_DIR,
    patterns: Sequence[str] = WORLD_MAP_PATTERNS,
    dry_run: bool = False,
) -> dict:
    return extract_client_archive_assets(
        extractor_path=extractor_path,
        archive_paths=archive_paths,
        raw_output_dir=raw_output_dir,
        patterns=patterns,
        dry_run=dry_run,
    )


def extract_client_icon_assets(
    extractor_path: str | Path,
    archive_paths: Sequence[str | Path],
    raw_output_dir: str | Path = RAW_ICON_BLP_DIR,
    patterns: Sequence[str] = ICON_PATTERNS,
    dry_run: bool = False,
) -> dict:
    return extract_client_archive_assets(
        extractor_path=extractor_path,
        archive_paths=archive_paths,
        raw_output_dir=raw_output_dir,
        patterns=patterns,
        dry_run=dry_run,
    )


def convert_blp_tree(raw_input_dir: str | Path = RAW_BLP_DIR, png_output_dir: str | Path = PNG_MAPS_DIR) -> dict:
    raw_dir = Path(raw_input_dir)
    png_dir = Path(png_output_dir)
    png_dir.mkdir(parents=True, exist_ok=True)

    converted = 0
    failed: list[str] = []

    for blp_path in raw_dir.rglob("*.blp"):
        relative = blp_path.relative_to(raw_dir)
        png_path = png_dir / relative.with_suffix(".png")
        png_path.parent.mkdir(parents=True, exist_ok=True)

        try:
            with Image.open(blp_path) as image:
                image.save(png_path, format="PNG")
            converted += 1
        except Exception:
            failed.append(str(blp_path))

    return {
        "raw_input_dir": str(raw_dir),
        "png_output_dir": str(png_dir),
        "converted_count": converted,
        "failed_files": failed,
    }


def stitch_named_pilot_zones(
    png_input_dir: str | Path = PNG_MAPS_DIR,
    composite_output_dir: str | Path = COMPOSITE_MAPS_DIR,
) -> dict:
    png_root = Path(png_input_dir)
    composite_root = Path(composite_output_dir)
    composite_root.mkdir(parents=True, exist_ok=True)

    stitched: list[dict] = []
    missing: list[dict] = []

    for spec in PILOT_ZONE_SPECS:
        zone_dir = png_root / "interface" / "worldmap" / str(spec["zone_dir"])
        output_path = composite_root / str(spec["output_name"])
        result = stitch_zone_tiles(
            zone_dir=zone_dir,
            tile_prefix=str(spec["tile_prefix"]),
            output_path=output_path,
            columns=int(spec["columns"]),
        )
        result["asset_key"] = spec["asset_key"]
        if result["tile_count"] > 0:
            stitched.append(result)
        else:
            missing.append(result)

    return {
        "png_input_dir": str(png_root),
        "composite_output_dir": str(composite_root),
        "stitched": stitched,
        "missing": missing,
    }


def stitch_zone_tiles(
    zone_dir: str | Path,
    tile_prefix: str,
    output_path: str | Path,
    columns: int = 4,
) -> dict:
    canvas, result, _, _ = _build_zone_base_canvas(zone_dir=zone_dir, tile_prefix=tile_prefix, columns=columns)
    destination = Path(output_path)
    result["output_path"] = str(destination)
    if canvas is None:
        return result

    canvas = _crop_trailing_black_padding(canvas)
    destination.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(destination, format="PNG")
    result["width"] = canvas.size[0]
    result["height"] = canvas.size[1]
    return result


def stitch_zone_with_overlays(
    zone_dir: str | Path,
    tile_prefix: str,
    output_path: str | Path,
    overlay_records: Sequence[dict] | None = None,
    columns: int = 4,
) -> dict:
    canvas, result, zone_root, tile_size = _build_zone_base_canvas(zone_dir=zone_dir, tile_prefix=tile_prefix, columns=columns)
    destination = Path(output_path)
    result["output_path"] = str(destination)
    if canvas is None:
        return result

    applied_overlays = 0
    for overlay_record in overlay_records or ():
        overlay_image = _build_overlay_image(zone_root, overlay_record, tile_size)
        if overlay_image is None:
            continue

        mask = Image.new("L", overlay_image.size)
        rgb_overlay = overlay_image.convert("RGB")
        mask.putdata([255 if pixel != (0, 0, 0) else 0 for pixel in rgb_overlay.getdata()])
        canvas.paste(
            rgb_overlay,
            (int(overlay_record["offset_x"]), int(overlay_record["offset_y"])),
            mask,
        )
        applied_overlays += 1

    canvas = _crop_trailing_black_padding(canvas)
    destination.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(destination, format="PNG")
    result["width"] = canvas.size[0]
    result["height"] = canvas.size[1]
    result["overlay_count"] = applied_overlays
    return result


def write_extraction_report(report_name: str, payload: dict) -> Path:
    MANIFESTS_DIR.mkdir(parents=True, exist_ok=True)
    path = MANIFESTS_DIR / report_name
    with path.open("w", encoding="utf-8") as handle:
        json.dump(payload, handle, indent=2, ensure_ascii=False)
    return path


def _sorted_matching(root: Path, pattern: str) -> list[Path]:
    return sorted(root.glob(pattern), key=lambda path: path.name.lower())


def _build_zone_base_canvas(
    zone_dir: str | Path,
    tile_prefix: str,
    columns: int,
) -> tuple[Image.Image | None, dict, Path, int]:
    zone_root = Path(zone_dir)
    tile_paths = _find_numbered_tile_paths(zone_root, tile_prefix)

    result = {
        "zone_dir": str(zone_root),
        "tile_prefix": tile_prefix,
        "output_path": "",
        "tile_count": len(tile_paths),
        "columns": columns,
        "rows": 0,
        "width": 0,
        "height": 0,
    }

    if not tile_paths:
        return None, result, zone_root, 0

    with Image.open(tile_paths[0]) as first_image:
        tile_width, tile_height = first_image.size

    rows = (len(tile_paths) + columns - 1) // columns
    canvas = Image.new("RGB", (tile_width * columns, tile_height * rows))

    for index, path in enumerate(tile_paths):
        col = index % columns
        row = index // columns
        with Image.open(path) as image:
            canvas.paste(image.convert("RGB"), (col * tile_width, row * tile_height))

    result.update(
        {
            "rows": rows,
            "width": canvas.size[0],
            "height": canvas.size[1],
        }
    )
    return canvas, result, zone_root, tile_width


def _crop_trailing_black_padding(canvas: Image.Image) -> Image.Image:
    bbox = canvas.getbbox()
    if bbox is None:
        return canvas

    _, _, right, bottom = bbox
    if right >= canvas.size[0] and bottom >= canvas.size[1]:
        return canvas

    return canvas.crop((0, 0, right, bottom))


def _read_wdbc_file(path: str | Path) -> tuple[int, int, int, bytes, bytes]:
    dbc_path = Path(path)
    with dbc_path.open("rb") as handle:
        magic, record_count, field_count, record_size, string_size = struct.unpack("<4s4I", handle.read(20))
        if magic != b"WDBC":
            raise ValueError(f"Unsupported DBC magic for {dbc_path}: {magic!r}")
        records = handle.read(record_count * record_size)
        strings = handle.read(string_size)
    return record_count, field_count, record_size, records, strings


def _get_wdbc_string(strings: bytes, offset: int) -> str:
    if offset <= 0 or offset >= len(strings):
        return ""
    end = strings.find(b"\x00", offset)
    if end == -1:
        end = len(strings)
    return strings[offset:end].decode("utf-8", errors="ignore")


def _normalize_zone_key(text: str) -> str:
    return re.sub(r"[^a-z0-9]+", "", text.lower())


def _worldmap_folder_key(text: str) -> str:
    return re.sub(r"\d+$", "", _normalize_zone_key(text))


def _overlay_prefix(text: str) -> str:
    return re.sub(r"[^a-z0-9]+", "", text.lower())


def _humanize_world_map_name(name: str) -> str:
    humanized = name.replace("_", " ")
    humanized = re.sub(r"(?<=[a-z])(?=[A-Z])", " ", humanized)
    humanized = re.sub(r"(?<=[A-Za-z])(?=[0-9])", " ", humanized)
    humanized = re.sub(r"(?<=[0-9])(?=[A-Za-z])", " ", humanized)
    return re.sub(r"\s+", " ", humanized).strip()


def _select_preferred_world_map_area_record(records: Sequence[dict]) -> dict:
    return sorted(
        records,
        key=lambda record: (
            int(record["floor"]) != 0,
            int(record["zone_id"]) == 0,
            int(record["display_map_id"]) != 0xFFFFFFFF,
            int(record["world_map_area_id"]),
        ),
    )[0]


def _build_overlay_image(zone_root: Path, overlay_record: dict, tile_size: int) -> Image.Image | None:
    prefix = _overlay_prefix(str(overlay_record["internal_name"]))
    if not prefix:
        return None

    tile_paths = _find_numbered_tile_paths(zone_root, prefix)
    if not tile_paths:
        return None

    width = int(overlay_record["texture_width"])
    height = int(overlay_record["texture_height"])
    cols = max(1, (width + tile_size - 1) // tile_size)
    rows = max(1, (height + tile_size - 1) // tile_size)
    expected_tiles = cols * rows
    if len(tile_paths) < expected_tiles:
        return None

    canvas = Image.new("RGB", (cols * tile_size, rows * tile_size))
    for index, path in enumerate(tile_paths[:expected_tiles]):
        col = index % cols
        row = index // cols
        with Image.open(path) as image:
            canvas.paste(image.convert("RGB"), (col * tile_size, row * tile_size))

    return canvas.crop((0, 0, width, height))


def _find_numbered_tile_paths(zone_root: Path, tile_prefix: str) -> list[Path]:
    matches: list[tuple[int, Path]] = []
    prefix = tile_prefix.lower()
    exact_match: Path | None = None

    for path in zone_root.glob("*.png"):
        stem = path.stem.lower()
        if stem == prefix:
            exact_match = path
            continue
        if not stem.startswith(prefix):
            continue
        suffix = stem[len(prefix):]
        if suffix.isdigit():
            matches.append((int(suffix), path))
            continue
        if suffix.startswith("_") and suffix[1:].isdigit():
            matches.append((int(suffix[1:]), path))
            continue

    matches.sort(key=lambda item: item[0])
    if matches:
        return [path for _, path in matches]
    return [exact_match] if exact_match is not None else []


def _discover_folder_prefixes(zone_root: Path) -> list[str]:
    plain_digit_counts: dict[str, int] = {}
    stems: list[str] = []
    for path in zone_root.glob("*.png"):
        stem = path.stem.lower()
        if stem.endswith("highlight"):
            continue
        stems.append(stem)
        match = re.match(r"([a-z0-9]+?)(\d+)$", stem)
        if match:
            base = match.group(1)
            plain_digit_counts[base] = plain_digit_counts.get(base, 0) + 1

    prefixes: set[str] = set()
    for stem in stems:
        match = re.match(r"([a-z0-9]+?)(\d+)(?:_(\d+))?$", stem)
        if not match:
            prefixes.add(stem)
            continue
        if match.group(3):
            prefixes.add(match.group(1) + match.group(2))
        else:
            base = match.group(1)
            prefixes.add(base if plain_digit_counts.get(base, 0) > 1 else stem)
    return sorted(prefixes)


def _discover_folder_specs(zone_dir: Path, folder_records: Sequence[dict], area_names: dict[int, str]) -> list[dict]:
    discovered: list[dict] = []
    used_output_names: set[str] = set()
    used_prefixes: set[str] = set()
    folder_prefix = _normalize_zone_key(zone_dir.name)
    discovered_prefixes = _discover_folder_prefixes(zone_dir)

    for record in sorted(folder_records, key=lambda item: int(item["world_map_area_id"])):
        tile_prefix = _normalize_zone_key(str(record["internal_name"]))
        tile_paths = _find_numbered_tile_paths(zone_dir, tile_prefix)
        if not tile_paths:
            continue
        used_prefixes.add(tile_prefix)

        zone_id = int(record["zone_id"])
        display_name = area_names.get(zone_id) or _humanize_world_map_name(str(record["internal_name"]))
        output_name = format_zone_output_filename(display_name, zone_id)
        if output_name in used_output_names:
            display_name = _humanize_world_map_name(str(record["internal_name"]))
            output_name = format_zone_output_filename(display_name, zone_id)
        used_output_names.add(output_name)

        discovered.append(
            {
                "zone_dir": str(zone_dir),
                "folder_name": zone_dir.name,
                "tile_prefix": tile_prefix,
                "tile_count": len(tile_paths),
                "world_map_area_id": int(record["world_map_area_id"]),
                "map_id": int(record["map_id"]),
                "zone_id": zone_id,
                "zone_name": display_name,
                "internal_name": str(record["internal_name"]),
                "output_name": output_name,
            }
        )

    if len(discovered_prefixes) <= 1:
        return discovered

    inferred_map_id = int(folder_records[0]["map_id"]) if folder_records else 0
    for prefix in discovered_prefixes:
        if prefix in used_prefixes:
            continue
        if not prefix.startswith(folder_prefix):
            continue
        if prefix == folder_prefix:
            continue

        tile_paths = _find_numbered_tile_paths(zone_dir, prefix)
        if not tile_paths:
            continue

        display_name = _humanize_world_map_name(prefix).title()
        output_name = format_zone_output_filename(display_name, 0)
        if output_name in used_output_names:
            continue
        used_output_names.add(output_name)

        discovered.append(
            {
                "zone_dir": str(zone_dir),
                "folder_name": zone_dir.name,
                "tile_prefix": prefix,
                "tile_count": len(tile_paths),
                "world_map_area_id": 0,
                "map_id": inferred_map_id,
                "zone_id": 0,
                "zone_name": display_name,
                "internal_name": prefix,
                "output_name": output_name,
            }
        )

    return discovered
