from __future__ import annotations

import struct
import tempfile
import unittest
from pathlib import Path

from PIL import Image
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from lw_zone_editor.client_assets import (
    ICON_PATTERNS,
    WORLD_MAP_PATTERNS,
    build_extract_command,
    detect_data_dir,
    detect_locale,
    discover_archives,
    extract_client_icon_assets,
    format_zone_output_filename,
    stitch_all_zone_composites,
    stitch_zone_tiles,
)


class ClientAssetTests(unittest.TestCase):
    def test_default_extraction_patterns_include_interface_minimap_assets(self) -> None:
        self.assertIn(r"Interface\WorldMap\*", WORLD_MAP_PATTERNS)
        self.assertIn(r"World\Minimaps\*", WORLD_MAP_PATTERNS)
        self.assertIn(r"Interface\Minimap\*", WORLD_MAP_PATTERNS)

    def test_default_icon_extraction_patterns_include_interface_icons(self) -> None:
        self.assertEqual(ICON_PATTERNS, (r"Interface\Icons\*",))

    def test_detect_data_dir_prefers_client_data_subdir(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            data = root / "Data"
            data.mkdir()
            self.assertEqual(detect_data_dir(root), data)

    def test_detect_locale_finds_known_locale_folder(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            data = Path(tmp)
            (data / "enUS").mkdir()
            self.assertEqual(detect_locale(data), "enUS")

    def test_discover_archives_orders_base_then_patches_then_locale(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            data = root / "Data"
            locale = data / "enUS"
            locale.mkdir(parents=True)

            for name in ["common.MPQ", "common-2.MPQ", "expansion.MPQ", "patch.MPQ", "patch-2.MPQ"]:
                (data / name).write_text("x", encoding="utf-8")
            for name in ["locale-enUS.MPQ", "patch-enUS.MPQ"]:
                (locale / name).write_text("x", encoding="utf-8")

            archives = [path.name for path in discover_archives(root)]
            self.assertEqual(
                archives,
                [
                    "common.MPQ",
                    "common-2.MPQ",
                    "expansion.MPQ",
                    "patch-2.MPQ",
                    "patch.MPQ",
                    "locale-enUS.MPQ",
                    "patch-enUS.MPQ",
                ],
            )

    def test_build_extract_command(self) -> None:
        cmd = build_extract_command("MPQExtractor.exe", "common.MPQ", "out", r"Interface\WorldMap\*")
        self.assertEqual(
            cmd,
            [
                "MPQExtractor.exe",
                "-e",
                r"Interface\WorldMap\*",
                "-f",
                "-c",
                "-o",
                "out",
                "common.MPQ",
            ],
        )

    def test_extract_client_icon_assets_uses_icon_pattern_by_default(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            raw_dir = Path(tmp) / "raw_icons"
            result = extract_client_icon_assets(
                extractor_path="MPQExtractor.exe",
                archive_paths=["common.MPQ"],
                raw_output_dir=raw_dir,
                dry_run=True,
            )

            self.assertEqual(result["patterns"], [r"Interface\Icons\*"])
            self.assertEqual(result["command_count"], 1)
            self.assertEqual(result["executed_command_count"], 0)
            self.assertEqual(result["raw_output_dir"], str(raw_dir))

    def test_stitch_zone_tiles_builds_expected_composite(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            zone = root / "stormwind"
            zone.mkdir()

            colors = [
                (255, 0, 0, 255),
                (0, 255, 0, 255),
                (0, 0, 255, 255),
                (255, 255, 0, 255),
            ]
            for index, color in enumerate(colors, start=1):
                image = Image.new("RGBA", (8, 8), color)
                image.save(zone / f"stormwind{index}.png", format="PNG")

            output = root / "stormwind_city.png"
            result = stitch_zone_tiles(zone, "stormwind", output, columns=2)

            self.assertEqual(result["tile_count"], 4)
            self.assertEqual(result["rows"], 2)
            self.assertEqual(result["width"], 16)
            self.assertEqual(result["height"], 16)
            self.assertTrue(output.exists())

    def test_stitch_zone_tiles_crops_trailing_black_padding_from_right_and_bottom(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            zone = root / "elwynn"
            zone.mkdir()

            top_left = Image.new("RGBA", (8, 8), (255, 0, 0, 255))
            top_right = Image.new("RGBA", (8, 8), (0, 0, 0, 255))
            for x in range(4):
                for y in range(8):
                    top_right.putpixel((x, y), (0, 255, 0, 255))

            bottom_left = Image.new("RGBA", (8, 8), (0, 0, 0, 255))
            for x in range(8):
                for y in range(4):
                    bottom_left.putpixel((x, y), (0, 0, 255, 255))

            bottom_right = Image.new("RGBA", (8, 8), (0, 0, 0, 255))
            for x in range(4):
                for y in range(4):
                    bottom_right.putpixel((x, y), (255, 255, 0, 255))

            top_left.save(zone / "elwynn1.png", format="PNG")
            top_right.save(zone / "elwynn2.png", format="PNG")
            bottom_left.save(zone / "elwynn3.png", format="PNG")
            bottom_right.save(zone / "elwynn4.png", format="PNG")

            output = root / "Elwynn Forest - 12.png"
            result = stitch_zone_tiles(zone, "elwynn", output, columns=2)

            self.assertEqual(result["width"], 12)
            self.assertEqual(result["height"], 12)
            with Image.open(output) as image:
                self.assertEqual(image.size, (12, 12))
                self.assertEqual(image.getpixel((0, 0)), (255, 0, 0))
                self.assertEqual(image.getpixel((11, 0)), (0, 255, 0))
                self.assertEqual(image.getpixel((0, 11)), (0, 0, 255))
                self.assertEqual(image.getpixel((11, 11)), (255, 255, 0))

    def test_format_zone_output_filename_uses_requested_convention(self) -> None:
        self.assertEqual(format_zone_output_filename("Stormwind City", 1519), "Stormwind City - 1519.png")

    def test_stitch_all_zone_composites_uses_dbc_mapping_and_area_names(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            png_root = root / "png"
            worldmap_root = png_root / "interface" / "worldmap"
            composite_root = root / "composite"

            stormwind = worldmap_root / "stormwind"
            elwynn = worldmap_root / "elwynn"
            unknown = worldmap_root / "unknownzone"
            for folder in [stormwind, elwynn, unknown]:
                folder.mkdir(parents=True)

            for index in range(1, 5):
                Image.new("RGBA", (8, 8), (255, 0, 0, 255)).save(stormwind / f"stormwind{index}.png", format="PNG")
            for index in range(1, 5):
                Image.new("RGBA", (8, 8), (0, 255, 0, 255)).save(elwynn / f"elwynn{index}.png", format="PNG")
            Image.new("RGBA", (8, 8), (255, 0, 255, 255)).save(elwynn / "goldshire1.png", format="PNG")
            Image.new("RGBA", (8, 8), (0, 0, 255, 255)).save(unknown / "unknownzone1.png", format="PNG")

            world_map_area_dbc = root / "WorldMapArea.dbc"
            world_map_overlay_dbc = root / "WorldMapOverlay.dbc"
            area_table_dbc = root / "AreaTable.dbc"
            self._write_world_map_area_dbc(
                world_map_area_dbc,
                [
                    {"world_map_area_id": 1000, "map_id": 0, "zone_id": 1519, "internal_name": "Stormwind"},
                    {"world_map_area_id": 1001, "map_id": 0, "zone_id": 12, "internal_name": "Elwynn"},
                ],
            )
            self._write_world_map_overlay_dbc(
                world_map_overlay_dbc,
                [
                    {
                        "id": 2000,
                        "world_map_area_id": 1001,
                        "area_ids": [87, 0, 0, 0],
                        "internal_name": "GOLDSHIRE",
                        "texture_width": 8,
                        "texture_height": 8,
                        "offset_x": 8,
                        "offset_y": 0,
                        "hit_rect": [8, 0, 16, 8],
                    }
                ],
            )
            self._write_area_table_dbc(
                area_table_dbc,
                [
                    {"area_id": 1519, "name": "Stormwind City"},
                    {"area_id": 12, "name": "Elwynn Forest"},
                    {"area_id": 87, "name": "Goldshire"},
                ],
            )

            result = stitch_all_zone_composites(
                png_input_dir=png_root,
                composite_output_dir=composite_root,
                world_map_area_dbc_path=world_map_area_dbc,
                world_map_overlay_dbc_path=world_map_overlay_dbc,
                area_table_dbc_path=area_table_dbc,
            )

            self.assertEqual(len(result["stitched"]), 2)
            self.assertEqual(len(result["unmatched"]), 1)
            output_names = sorted(Path(item["output_path"]).name for item in result["stitched"])
            self.assertEqual(output_names, ["Elwynn Forest - 12.png", "Stormwind City - 1519.png"])
            self.assertEqual(result["unmatched"][0]["folder_name"], "unknownzone")
            self.assertTrue((composite_root / "Stormwind City - 1519.png").exists())
            self.assertTrue((composite_root / "Elwynn Forest - 12.png").exists())

            with Image.open(composite_root / "Elwynn Forest - 12.png") as image:
                composite = image.convert("RGBA")
                self.assertEqual(composite.getpixel((1, 1)), (0, 255, 0, 255))
                self.assertEqual(composite.getpixel((9, 1)), (255, 0, 255, 255))

    def test_stitch_all_zone_composites_discovers_multi_submap_prefixes_in_one_folder(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            png_root = root / "png"
            worldmap_root = png_root / "interface" / "worldmap"
            composite_root = root / "composite"
            utgarde = worldmap_root / "utgardekeep"
            utgarde.mkdir(parents=True)

            for index in range(1, 5):
                Image.new("RGBA", (8, 8), (255, 0, 0, 255)).save(utgarde / f"utgardekeep1_{index}.png", format="PNG")
            Image.new("RGBA", (8, 8), (0, 255, 0, 255)).save(utgarde / "utgardekeep2.png", format="PNG")

            world_map_area_dbc = root / "WorldMapArea.dbc"
            world_map_overlay_dbc = root / "WorldMapOverlay.dbc"
            area_table_dbc = root / "AreaTable.dbc"
            self._write_world_map_area_dbc(
                world_map_area_dbc,
                [
                    {"world_map_area_id": 3000, "map_id": 574, "zone_id": 0, "internal_name": "Utgardekeep1"},
                    {"world_map_area_id": 3001, "map_id": 574, "zone_id": 0, "internal_name": "Utgardekeep2"},
                ],
            )
            self._write_world_map_overlay_dbc(world_map_overlay_dbc, [])
            self._write_area_table_dbc(area_table_dbc, [])

            result = stitch_all_zone_composites(
                png_input_dir=png_root,
                composite_output_dir=composite_root,
                world_map_area_dbc_path=world_map_area_dbc,
                world_map_overlay_dbc_path=world_map_overlay_dbc,
                area_table_dbc_path=area_table_dbc,
            )

            output_names = sorted(Path(item["output_path"]).name for item in result["stitched"])
            self.assertEqual(output_names, ["Utgardekeep 1 - 0.png", "Utgardekeep 2 - 0.png"])
            self.assertEqual(len(result["unmatched"]), 0)
            self.assertTrue((composite_root / "Utgardekeep 1 - 0.png").exists())
            self.assertTrue((composite_root / "Utgardekeep 2 - 0.png").exists())

    def test_stitch_all_zone_composites_falls_back_to_synthetic_prefix_specs_without_dbc_match(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            png_root = root / "png"
            worldmap_root = png_root / "interface" / "worldmap"
            composite_root = root / "composite"
            zone = worldmap_root / "hallsoflightning"
            zone.mkdir(parents=True)

            for index in range(1, 5):
                Image.new("RGBA", (8, 8), (255, 0, 0, 255)).save(zone / f"hallsoflightning1_{index}.png", format="PNG")
            Image.new("RGBA", (8, 8), (0, 255, 0, 255)).save(zone / "hallsoflightning2.png", format="PNG")

            world_map_area_dbc = root / "WorldMapArea.dbc"
            world_map_overlay_dbc = root / "WorldMapOverlay.dbc"
            area_table_dbc = root / "AreaTable.dbc"
            self._write_world_map_area_dbc(world_map_area_dbc, [])
            self._write_world_map_overlay_dbc(world_map_overlay_dbc, [])
            self._write_area_table_dbc(area_table_dbc, [])

            result = stitch_all_zone_composites(
                png_input_dir=png_root,
                composite_output_dir=composite_root,
                world_map_area_dbc_path=world_map_area_dbc,
                world_map_overlay_dbc_path=world_map_overlay_dbc,
                area_table_dbc_path=area_table_dbc,
            )

            output_names = sorted(Path(item["output_path"]).name for item in result["stitched"])
            self.assertEqual(output_names, ["Hallsoflightning 1 - 0.png", "Hallsoflightning 2 - 0.png"])
            self.assertEqual(len(result["unmatched"]), 0)

    def _write_world_map_area_dbc(self, path: Path, rows: list[dict]) -> None:
        string_offsets = {"": 0}
        string_blob = bytearray(b"\x00")

        def add_string(value: str) -> int:
            if value not in string_offsets:
                string_offsets[value] = len(string_blob)
                string_blob.extend(value.encode("utf-8"))
                string_blob.append(0)
            return string_offsets[value]

        record_bytes = bytearray()
        for row in rows:
            record_bytes.extend(
                struct.pack(
                    "<4I4f3I",
                    row["world_map_area_id"],
                    row["map_id"],
                    row["zone_id"],
                    add_string(row["internal_name"]),
                    0.0,
                    1.0,
                    0.0,
                    1.0,
                    0xFFFFFFFF,
                    0,
                    0,
                )
            )

        with path.open("wb") as handle:
            handle.write(struct.pack("<4s4I", b"WDBC", len(rows), 11, struct.calcsize("<4I4f3I"), len(string_blob)))
            handle.write(record_bytes)
            handle.write(string_blob)

    def _write_area_table_dbc(self, path: Path, rows: list[dict]) -> None:
        string_offsets = {"": 0}
        string_blob = bytearray(b"\x00")

        def add_string(value: str) -> int:
            if value not in string_offsets:
                string_offsets[value] = len(string_blob)
                string_blob.extend(value.encode("utf-8"))
                string_blob.append(0)
            return string_offsets[value]

        field_count = 36
        record_bytes = bytearray()
        for row in rows:
            values = [0] * field_count
            values[0] = row["area_id"]
            values[11] = add_string(row["name"])
            record_bytes.extend(struct.pack(f"<{field_count}I", *values))

        with path.open("wb") as handle:
            handle.write(struct.pack("<4s4I", b"WDBC", len(rows), field_count, field_count * 4, len(string_blob)))
            handle.write(record_bytes)
            handle.write(string_blob)

    def _write_world_map_overlay_dbc(self, path: Path, rows: list[dict]) -> None:
        string_offsets = {"": 0}
        string_blob = bytearray(b"\x00")

        def add_string(value: str) -> int:
            if value not in string_offsets:
                string_offsets[value] = len(string_blob)
                string_blob.extend(value.encode("utf-8"))
                string_blob.append(0)
            return string_offsets[value]

        record_bytes = bytearray()
        for row in rows:
            area_ids = list(row["area_ids"])
            area_ids.extend([0] * (4 - len(area_ids)))
            hit_rect = list(row.get("hit_rect", [0, 0, 0, 0]))
            record_bytes.extend(
                struct.pack(
                    "<17I",
                    row["id"],
                    row["world_map_area_id"],
                    area_ids[0],
                    area_ids[1],
                    area_ids[2],
                    area_ids[3],
                    0,
                    0,
                    add_string(row["internal_name"]),
                    row["texture_width"],
                    row["texture_height"],
                    row["offset_x"],
                    row["offset_y"],
                    hit_rect[0],
                    hit_rect[1],
                    hit_rect[2],
                    hit_rect[3],
                )
            )

        with path.open("wb") as handle:
            handle.write(struct.pack("<4s4I", b"WDBC", len(rows), 17, struct.calcsize("<17I"), len(string_blob)))
            handle.write(record_bytes)
            handle.write(string_blob)


if __name__ == "__main__":
    unittest.main()
