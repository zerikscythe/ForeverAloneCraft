#!/usr/bin/env python3

from __future__ import annotations

import argparse
import sys

from lw_zone_editor.marker_cache import DEFAULT_MARKER_CACHE_PATH, build_marker_cache, write_marker_cache
from lw_zone_editor.settings import DATABASE_SETTINGS


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Build a cached marker JSON from AzerothCore world data.")
    parser.add_argument("--host", default=DATABASE_SETTINGS.host, help="MySQL host for the AzerothCore world database.")
    parser.add_argument("--port", type=int, default=DATABASE_SETTINGS.port, help="MySQL port.")
    parser.add_argument("--user", default=DATABASE_SETTINGS.user, help="MySQL username.")
    parser.add_argument("--password", default=DATABASE_SETTINGS.password, help="MySQL password.")
    parser.add_argument("--database", default=DATABASE_SETTINGS.database, help="World database name.")
    parser.add_argument("--mysql-binary", default=DATABASE_SETTINGS.mysql_binary, help="Optional mysql CLI binary path.")
    parser.add_argument("--output", default=str(DEFAULT_MARKER_CACHE_PATH), help="Output marker cache JSON path.")
    return parser


def main() -> int:
    args = build_parser().parse_args()
    missing = [name for name in ("host", "user", "password") if not getattr(args, name)]
    if missing:
        raise SystemExit(
            f"Missing database settings: {', '.join(missing)}. Set them in config.ini or pass CLI flags."
        )
    payload = build_marker_cache(
        host=args.host,
        port=args.port,
        user=args.user,
        password=args.password,
        database=args.database,
        mysql_binary=args.mysql_binary,
    )
    output_path = write_marker_cache(payload, args.output)
    print("[lw-zone-editor] marker cache:", output_path)
    print("[lw-zone-editor] marker counts:", payload.get("marker_counts", {}))
    return 0


if __name__ == "__main__":
    sys.exit(main())
