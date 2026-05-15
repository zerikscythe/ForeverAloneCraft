from __future__ import annotations

import json
import re
from pathlib import Path
from typing import Any


def load_catalog(path: str | Path) -> dict[str, Any]:
    catalog_path = Path(path)
    with catalog_path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def slugify(value: str) -> str:
    slug = re.sub(r"[^a-z0-9]+", "_", value.lower()).strip("_")
    return slug or "asset"


def canonical_filename(map_id: int, zone_id: int | None, display_name: str, extension: str) -> str:
    safe_ext = extension.lower().lstrip(".")
    zone_part = f"zone_{int(zone_id):04d}" if zone_id is not None else "zone_none"
    return f"map_{int(map_id):03d}__{zone_part}__{slugify(display_name)}.{safe_ext}"
