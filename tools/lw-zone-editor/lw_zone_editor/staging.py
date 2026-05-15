from __future__ import annotations

import json
import shutil
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

from .catalog import canonical_filename, load_catalog
from .image_info import read_image_size


def stage_catalog(
    catalog_path: str | Path,
    source_dir: str | Path,
    staged_dir: str | Path,
    manifest_path: str | Path,
    report_path: str | Path,
    dry_run: bool = False,
) -> dict[str, Any]:
    catalog = load_catalog(catalog_path)
    source_root = Path(source_dir)
    staged_root = Path(staged_dir)
    manifest_file = Path(manifest_path)
    report_file = Path(report_path)

    entries: list[dict[str, Any]] = []
    staged_count = 0
    missing_count = 0

    if not dry_run:
        staged_root.mkdir(parents=True, exist_ok=True)
        manifest_file.parent.mkdir(parents=True, exist_ok=True)
        report_file.parent.mkdir(parents=True, exist_ok=True)

    for asset in catalog.get("assets", []):
        entry = _stage_one_asset(asset, source_root, staged_root, dry_run=dry_run)
        entries.append(entry)
        if entry["status"] == "staged":
            staged_count += 1
        else:
            missing_count += 1

    manifest = {
        "catalog_key": catalog.get("catalog_key", "pilot_catalog"),
        "description": catalog.get("description", ""),
        "generated_at_utc": datetime.now(timezone.utc).isoformat(),
        "coverage": {
            "total_assets": len(entries),
            "staged_assets": staged_count,
            "missing_assets": missing_count,
        },
        "assets": entries,
    }

    report = _build_report(manifest)

    if not dry_run:
        with manifest_file.open("w", encoding="utf-8") as handle:
            json.dump(manifest, handle, indent=2, ensure_ascii=False)
        with report_file.open("w", encoding="utf-8") as handle:
            handle.write(report)

    return manifest


def _stage_one_asset(asset: dict[str, Any], source_root: Path, staged_root: Path, dry_run: bool) -> dict[str, Any]:
    source_path = _find_source_asset(source_root, asset)
    source_candidates = asset.get("source_candidates", [])

    result: dict[str, Any] = {
        "asset_key": asset["asset_key"],
        "display_name": asset["display_name"],
        "map_id": int(asset["map_id"]),
        "zone_id": int(asset["zone_id"]) if asset.get("zone_id") is not None else None,
        "source_kind": asset.get("source_kind", "zone_map"),
        "tags": list(asset.get("tags", [])),
        "validation_notes": asset.get("validation_notes", ""),
        "source_candidates": source_candidates,
        "status": "missing_source",
        "source_image_path": None,
        "staged_image_path": None,
        "pixel_width": None,
        "pixel_height": None,
    }

    if not source_path:
        return result

    extension = source_path.suffix.lower().lstrip(".")
    canonical_name = canonical_filename(result["map_id"], result["zone_id"], result["display_name"], extension)
    destination = staged_root / result["asset_key"] / canonical_name

    width, height = read_image_size(source_path)

    if not dry_run:
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source_path, destination)

    result.update(
        {
            "status": "staged",
            "source_image_path": str(source_path.as_posix()),
            "staged_image_path": str(destination.as_posix()),
            "pixel_width": width,
            "pixel_height": height,
        }
    )
    return result


def _find_source_asset(source_root: Path, asset: dict[str, Any]) -> Path | None:
    explicit = asset.get("source_file")
    if explicit:
        candidate = source_root / explicit
        if candidate.exists() and candidate.is_file():
            return candidate

    for name in asset.get("source_candidates", []):
        candidate = source_root / name
        if candidate.exists() and candidate.is_file():
            return candidate

    return None


def _build_report(manifest: dict[str, Any]) -> str:
    coverage = manifest["coverage"]
    lines = [
        "# Pilot Validation Report",
        "",
        f"- Catalog: `{manifest['catalog_key']}`",
        f"- Generated: `{manifest['generated_at_utc']}`",
        f"- Total assets: `{coverage['total_assets']}`",
        f"- Staged assets: `{coverage['staged_assets']}`",
        f"- Missing assets: `{coverage['missing_assets']}`",
        "",
        "## Asset Results",
        "",
        "| Asset Key | Display Name | Status | Size | Staged Path | Notes |",
        "|---|---|---|---|---|---|",
    ]

    for asset in manifest["assets"]:
        size = "-"
        if asset["pixel_width"] and asset["pixel_height"]:
            size = f"{asset['pixel_width']}x{asset['pixel_height']}"
        staged_path = asset["staged_image_path"] or "-"
        note = asset.get("validation_notes", "") or ""
        lines.append(
            f"| `{asset['asset_key']}` | {asset['display_name']} | `{asset['status']}` | {size} | `{staged_path}` | {note} |"
        )

    missing = [asset for asset in manifest["assets"] if asset["status"] != "staged"]
    if missing:
        lines.extend(
            [
                "",
                "## Missing Source Assets",
                "",
                "Drop matching files into `data/source_assets/` using one of the listed candidate names.",
                "",
            ]
        )
        for asset in missing:
            candidates = ", ".join(f"`{name}`" for name in asset.get("source_candidates", [])) or "(no candidates listed)"
            lines.append(f"- `{asset['asset_key']}` → {candidates}")

    return "\n".join(lines) + "\n"
