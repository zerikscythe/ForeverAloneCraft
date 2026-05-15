#!/usr/bin/env python3
"""Stage pilot map assets for the separate LivingWorld zone editor app."""

from __future__ import annotations

import argparse
import sys

from lw_zone_editor.paths import (
    COMPOSITE_MAPS_DIR,
    DEFAULT_CATALOG_PATH,
    MANIFESTS_DIR,
    STAGED_ASSETS_DIR,
)
from lw_zone_editor.staging import stage_catalog


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Stage pilot zone images into canonical LivingWorld zone-editor assets."
    )
    parser.add_argument("--catalog", default=str(DEFAULT_CATALOG_PATH), help="Path to the pilot catalog JSON file.")
    parser.add_argument(
        "--source-dir",
        default=str(COMPOSITE_MAPS_DIR),
        help="Directory containing stitched zone image files ready for staging.",
    )
    parser.add_argument("--staged-dir", default=str(STAGED_ASSETS_DIR), help="Directory where canonical staged assets should be written.")
    parser.add_argument(
        "--manifest",
        default=str(MANIFESTS_DIR / "pilot_manifest.json"),
        help="Output manifest JSON path.",
    )
    parser.add_argument(
        "--report",
        default=str(MANIFESTS_DIR / "pilot_validation_report.md"),
        help="Output validation report markdown path.",
    )
    parser.add_argument("--dry-run", action="store_true", help="Do not copy files or write outputs; only print what would happen.")
    parser.add_argument("--strict", action="store_true", help="Return non-zero if any pilot asset is missing.")
    return parser


def main() -> int:
    args = build_parser().parse_args()
    result = stage_catalog(
        catalog_path=args.catalog,
        source_dir=args.source_dir,
        staged_dir=args.staged_dir,
        manifest_path=args.manifest,
        report_path=args.report,
        dry_run=args.dry_run,
    )

    coverage = result["coverage"]
    print("[lw-zone-editor] catalog:", result["catalog_key"])
    print(
        "[lw-zone-editor] assets: total={total} staged={staged} missing={missing}".format(
            total=coverage["total_assets"],
            staged=coverage["staged_assets"],
            missing=coverage["missing_assets"],
        )
    )

    if not args.dry_run:
        print("[lw-zone-editor] manifest:", args.manifest)
        print("[lw-zone-editor] report:", args.report)

    if args.strict and coverage["missing_assets"]:
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
