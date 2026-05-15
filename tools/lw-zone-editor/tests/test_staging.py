from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from lw_zone_editor.catalog import canonical_filename, slugify
from lw_zone_editor.image_info import read_image_size
from lw_zone_editor.staging import stage_catalog


MINIMAL_PNG_1X1 = (
    b"\x89PNG\r\n\x1a\n"
    b"\x00\x00\x00\rIHDR"
    b"\x00\x00\x00\x01\x00\x00\x00\x01\x08\x02\x00\x00\x00"
    b"\x90wS\xde"
    b"\x00\x00\x00\x0cIDAT\x08\x99c``\x00\x00\x00\x04\x00\x01"
    b"\x0b\xe7\x02\x9d"
    b"\x00\x00\x00\x00IEND\xaeB`\x82"
)


class ZoneEditorStagingTests(unittest.TestCase):
    def test_slugify_and_canonical_filename(self) -> None:
        self.assertEqual(slugify("Stormwind City"), "stormwind_city")
        self.assertEqual(
            canonical_filename(0, 1519, "Stormwind City", "png"),
            "map_000__zone_1519__stormwind_city.png",
        )

    def test_read_png_size(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            image = Path(tmp) / "test.png"
            image.write_bytes(MINIMAL_PNG_1X1)
            self.assertEqual(read_image_size(image), (1, 1))

    def test_stage_catalog_copies_and_reports_asset(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source_dir = root / "source"
            staged_dir = root / "staged"
            manifest_path = root / "manifest.json"
            report_path = root / "report.md"
            catalog_path = root / "catalog.json"

            source_dir.mkdir(parents=True)
            (source_dir / "stormwind_city.png").write_bytes(MINIMAL_PNG_1X1)

            catalog_path.write_text(
                json.dumps(
                    {
                        "catalog_key": "test_catalog",
                        "assets": [
                            {
                                "asset_key": "stormwind_city",
                                "display_name": "Stormwind City",
                                "map_id": 0,
                                "zone_id": 1519,
                                "source_candidates": ["stormwind_city.png"],
                            }
                        ],
                    }
                ),
                encoding="utf-8",
            )

            manifest = stage_catalog(
                catalog_path=catalog_path,
                source_dir=source_dir,
                staged_dir=staged_dir,
                manifest_path=manifest_path,
                report_path=report_path,
                dry_run=False,
            )

            self.assertEqual(manifest["coverage"]["staged_assets"], 1)
            self.assertEqual(manifest["coverage"]["missing_assets"], 0)
            self.assertTrue(manifest_path.exists())
            self.assertTrue(report_path.exists())

            asset = manifest["assets"][0]
            self.assertEqual(asset["pixel_width"], 1)
            self.assertEqual(asset["pixel_height"], 1)
            self.assertTrue(Path(asset["staged_image_path"]).exists())


if __name__ == "__main__":
    unittest.main()
