from __future__ import annotations

from pathlib import Path

PACKAGE_DIR = Path(__file__).resolve().parent
APP_ROOT = PACKAGE_DIR.parent
DATA_DIR = APP_ROOT / "data"
EXTRACTED_MAPS_DIR = DATA_DIR / "extracted_maps"
EXTRACTED_ICONS_DIR = DATA_DIR / "extracted_icons"
# Backward-compatible alias from the initial scaffold. Raw map ingestion should
# now use `data/extracted_maps/` explicitly.
SOURCE_ASSETS_DIR = EXTRACTED_MAPS_DIR
RAW_BLP_DIR = EXTRACTED_MAPS_DIR / "raw_blp"
PNG_MAPS_DIR = EXTRACTED_MAPS_DIR / "png"
COMPOSITE_MAPS_DIR = EXTRACTED_MAPS_DIR / "composite"
RAW_ICON_BLP_DIR = EXTRACTED_ICONS_DIR / "raw_blp"
PNG_ICONS_DIR = EXTRACTED_ICONS_DIR / "png"
STAGED_ASSETS_DIR = DATA_DIR / "staged_assets"
MANIFESTS_DIR = DATA_DIR / "manifests"
MARKER_CACHE_DIR = DATA_DIR / "marker_cache"
EDITOR_ROUTES_DIR = DATA_DIR / "editor_routes"
EXPORTED_ROUTES_DIR = DATA_DIR / "exported_routes"
DEFAULT_CATALOG_PATH = DATA_DIR / "pilot_catalog.json"
