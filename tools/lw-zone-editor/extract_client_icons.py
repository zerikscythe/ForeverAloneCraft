#!/usr/bin/env python3
"""Extract spell/item icon assets from a WoW client using MPQExtractor."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from lw_zone_editor.client_assets import (
    ICON_PATTERNS,
    convert_blp_tree,
    detect_data_dir,
    detect_locale,
    discover_archives,
    extract_client_icon_assets,
    resolve_extractor_path,
    write_extraction_report,
)
from lw_zone_editor.paths import MANIFESTS_DIR, PNG_ICONS_DIR, RAW_ICON_BLP_DIR


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Rip client Interface\\\\Icons assets from a WoW client MPQ set into the zone-editor workspace."
    )
    parser.add_argument("--client-dir", required=True, help="Path to the WoW client root or its Data directory.")
    parser.add_argument("--locale", help="Optional locale override, e.g. enUS.")
    parser.add_argument("--extractor", help="Optional path to a built MPQExtractor binary.")
    parser.add_argument("--raw-dir", default=str(RAW_ICON_BLP_DIR), help="Directory for raw extracted BLP icon files.")
    parser.add_argument("--png-dir", default=str(PNG_ICONS_DIR), help="Directory for converted PNG icon files.")
    parser.add_argument("--report", default=str(MANIFESTS_DIR / "client_icon_extract_report.json"), help="Output JSON report path.")
    parser.add_argument("--pattern", action="append", dest="patterns", help="Additional or replacement extraction patterns. Can be specified more than once.")
    parser.add_argument("--skip-convert", action="store_true", help="Skip BLP to PNG conversion.")
    parser.add_argument("--dry-run", action="store_true", help="Print the extraction plan without running MPQ extraction.")
    return parser


def main() -> int:
    args = build_parser().parse_args()

    data_dir = detect_data_dir(args.client_dir)
    locale = args.locale or detect_locale(data_dir)
    archives = discover_archives(args.client_dir, locale)
    patterns = tuple(args.patterns) if args.patterns else ICON_PATTERNS

    if not archives:
        print("[lw-zone-editor] no MPQ archives found under:", data_dir)
        return 1

    extractor_path = None if args.dry_run else resolve_extractor_path(args.extractor)
    if args.dry_run and args.extractor:
        extractor_path = args.extractor

    extraction = extract_client_icon_assets(
        extractor_path=extractor_path or "MPQExtractor",
        archive_paths=archives,
        raw_output_dir=args.raw_dir,
        patterns=patterns,
        dry_run=args.dry_run,
    )

    conversion = None
    if not args.skip_convert and not args.dry_run:
        conversion = convert_blp_tree(args.raw_dir, args.png_dir)

    payload = {
        "client_dir": args.client_dir,
        "data_dir": str(data_dir),
        "locale": locale,
        "extractor_path": str(extractor_path) if extractor_path else None,
        "extraction": extraction,
        "conversion": conversion,
    }

    if not args.dry_run:
        report_path = write_extraction_report(Path(args.report).name, payload)
        print("[lw-zone-editor] report:", report_path)

    print("[lw-zone-editor] locale:", locale or "(none detected)")
    print("[lw-zone-editor] archives:", len(archives))
    print("[lw-zone-editor] patterns:", ", ".join(patterns))
    print("[lw-zone-editor] raw output:", args.raw_dir)
    if not args.skip_convert:
        print("[lw-zone-editor] png output:", args.png_dir)
    if args.dry_run:
        print("[lw-zone-editor] dry-run only; no extraction executed")
    elif conversion is not None:
        print("[lw-zone-editor] converted png count:", conversion["converted_count"])
        if conversion["failed_files"]:
            print("[lw-zone-editor] failed conversions:", len(conversion["failed_files"]))

    return 0


if __name__ == "__main__":
    sys.exit(main())
