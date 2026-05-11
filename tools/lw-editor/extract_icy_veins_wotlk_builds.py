#!/usr/bin/env python3
"""
Icy Veins WotLK Build Extractor
──────────────────────────────────────────────────────────────────────────────
Fetches WotLK Classic PvE/PvP guide pages from Icy Veins, extracts embedded
talent-calculator hashes, and normalizes those loadouts against the locally
trusted DBC-derived talent dump in tools/lw-editor/data/talent_data.json.

Important design rule:
    - Icy Veins is used only for build selection / allocation shape.
    - Local DBC-derived data is used for authoritative talent IDs and spell IDs.

Outputs:
    - data/icy_veins_wotlk_builds.json

Usage:
    python extract_icy_veins_wotlk_builds.py
"""

from __future__ import annotations

import json
import pathlib
import sys
import urllib.parse
import urllib.request
import xml.etree.ElementTree as ET
from collections import Counter
from html.parser import HTMLParser
from typing import Any


SCRIPT_DIR = pathlib.Path(__file__).resolve().parent
OUTPUT_DIR = SCRIPT_DIR / "data"
LOCAL_TALENT_DATA_FILE = OUTPUT_DIR / "talent_data.json"
OUTPUT_FILE = OUTPUT_DIR / "icy_veins_wotlk_builds.json"

SITEMAP_URL = "https://www.icy-veins.com/sitemap.xml"
ICY_JSON_BASE = "https://static.icy-veins.com/json/wotlk-talent-calculator/"

# Matches the WotLK calculator's JS exactly.
URL_ID_CHARACTERS = [str(i) for i in range(10)] + list(
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ-._~[]()"
    "ｦｧｨｩｪｫｬｭｮｯｰｱｲｳｴｵｶｷｸｹｺｻｼｽｾｿﾀﾁﾂﾃﾄﾅﾆﾇﾈﾉﾊﾋﾌﾍﾎﾏﾐﾑﾒﾓﾔﾕﾖﾗﾘﾙﾚﾛﾜﾝﾞﾟ"
)


def fetch_text(url: str) -> str:
    request = urllib.request.Request(
        url,
        headers={
            "User-Agent": (
                "Mozilla/5.0 (compatible; LWEditorBuildExtractor/1.0; +https://example.invalid)"
            )
        },
    )
    with urllib.request.urlopen(request, timeout=30) as response:
        charset = response.headers.get_content_charset() or "utf-8"
        return response.read().decode(charset, errors="ignore")


class TalentCalculatorParser(HTMLParser):
    def __init__(self) -> None:
        super().__init__()
        self.calculators: list[dict[str, str]] = []

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        if tag.lower() != "div":
            return

        attr_map = {k: (v or "") for k, v in attrs}
        if "data-talentcalculator" not in attr_map:
            return

        self.calculators.append(attr_map)


def load_local_talent_data() -> dict[str, Any]:
    if not LOCAL_TALENT_DATA_FILE.exists():
        raise FileNotFoundError(
            f"Local talent data not found: {LOCAL_TALENT_DATA_FILE}. "
            "Run tools/lw-editor/extract_dbc_data.py first."
        )

    return json.loads(LOCAL_TALENT_DATA_FILE.read_text(encoding="utf-8"))


def fetch_sitemap_urls() -> list[str]:
    xml_text = fetch_text(SITEMAP_URL)
    root = ET.fromstring(xml_text)
    ns = {"sm": "http://www.sitemaps.org/schemas/sitemap/0.9"}
    return [loc.text for loc in root.findall("sm:url/sm:loc", ns) if loc.text]


def classify_guide_urls(urls: list[str]) -> tuple[list[str], list[str], list[str]]:
    pve = [u for u in urls if "/wotlk-classic/" in u and u.endswith("-spec-builds-talents-glyphs")]
    pvp = [u for u in urls if "/wotlk-classic/" in u and u.endswith("-pvp-guide")]
    calcs = [u for u in urls if "/wotlk-classic/" in u and u.endswith("-talent-calculator")]
    return pve, pvp, calcs


def load_icy_class_json(class_slug: str) -> dict[str, Any]:
    return json.loads(fetch_text(f"{ICY_JSON_BASE}{class_slug}.json"))


def build_local_tree_order(local_talent_data: dict[str, Any], class_id: int) -> list[dict[str, Any]]:
    class_data = local_talent_data[str(class_id)]
    ordered_tree_indices = sorted(class_data.keys(), key=lambda x: int(x))
    trees: list[dict[str, Any]] = []
    for tree_index in ordered_tree_indices:
        tree = class_data[tree_index]
        trees.append(
            {
                "tree_index": int(tree_index),
                "tree_name": tree["tree_name"],
                "talents": tree["talents"],
            }
        )
    return trees


def build_urlid_mapping(
    icy_class_data: dict[str, Any],
    local_talent_data: dict[str, Any],
) -> tuple[dict[str, dict[str, Any]], list[str]]:
    class_id = int(icy_class_data["classId"])
    local_trees = build_local_tree_order(local_talent_data, class_id)
    icy_trees = icy_class_data.get("talentGroups", [])

    mapping: dict[str, dict[str, Any]] = {}
    warnings: list[str] = []
    url_index = 0

    for tree_pos, icy_tree in enumerate(icy_trees):
        if tree_pos >= len(local_trees):
            warnings.append(f"Missing local tree for class {class_id} tree_pos={tree_pos}")
            continue

        local_tree = local_trees[tree_pos]
        local_talents = local_tree["talents"]
        icy_talents = icy_tree.get("talents", [])
        icy_live_talents = [t for t in icy_talents if t]

        if len(local_talents) != len(icy_live_talents):
            warnings.append(
                f"Talent count mismatch for class {class_id} tree '{icy_tree.get('name', local_tree['tree_name'])}': "
                f"icy={len(icy_live_talents)} local={len(local_talents)}"
            )

        for talent_pos, icy_talent in enumerate(icy_live_talents):
            if url_index >= len(URL_ID_CHARACTERS):
                raise RuntimeError("Ran out of URL ID characters while building calculator mapping")

            url_id = URL_ID_CHARACTERS[url_index]
            url_index += 1

            local_talent: dict[str, Any] | None = None
            icy_name = icy_talent.get("name", "")

            # Prefer exact name match inside the same tree.
            for candidate in local_talents:
                if candidate.get("name") == icy_name:
                    local_talent = candidate
                    break

            # Fall back to same positional index if the local dump had a placeholder name.
            if local_talent is None and talent_pos < len(local_talents):
                local_talent = local_talents[talent_pos]
                warnings.append(
                    f"Fallback positional mapping for class {class_id} tree '{local_tree['tree_name']}' talent '{icy_name}'"
                )

            if local_talent is None:
                warnings.append(
                    f"Unable to map talent '{icy_name}' for class {class_id} tree '{local_tree['tree_name']}'"
                )
                continue

            mapping[url_id] = {
                "class_id": class_id,
                "tree_index": local_tree["tree_index"],
                "tree_name": local_tree["tree_name"],
                "talent_name": local_talent["name"],
                "talent_id": local_talent["talent_id"],
                "row": local_talent["row"],
                "col": local_talent["col"],
                "max_rank": local_talent["max_rank"],
                "rank_spell_ids": local_talent["rank_spell_ids"],
                "depends_on": local_talent.get("depends_on"),
                "depends_on_rank": local_talent.get("depends_on_rank", 0),
                "source_name": icy_name,
                "source_slug": icy_talent.get("slug", ""),
                "source_required_points": icy_talent.get("requiredPoints", 0),
            }

    return mapping, warnings


def decode_pointsurl(pointsurl: str, mapping: dict[str, dict[str, Any]]) -> dict[str, Any]:
    decoded = urllib.parse.unquote(pointsurl or "")
    if "#tc-" not in decoded:
        return {
            "raw": decoded,
            "glyph_hash": "",
            "talent_hash": "",
            "tree_totals": {},
            "talents": [],
            "warnings": ["Missing #tc- prefix"],
        }

    payload = decoded.split("#tc-", 1)[1]
    glyph_hash = ""
    if "|" in payload:
        payload, glyph_hash = payload.rsplit("|", 1)

    counts = Counter(payload)
    talents: list[dict[str, Any]] = []
    warnings: list[str] = []
    tree_totals: dict[str, int] = {}

    for url_id, points in counts.items():
        talent = mapping.get(url_id)
        if not talent:
            warnings.append(f"Unknown urlId '{url_id}' in payload")
            continue

        if points > int(talent["max_rank"]):
            warnings.append(
                f"Points overflow for {talent['talent_name']}: points={points} max_rank={talent['max_rank']}"
            )

        entry = dict(talent)
        entry["points"] = points
        talents.append(entry)
        tree_totals[entry["tree_name"]] = tree_totals.get(entry["tree_name"], 0) + points

    talents.sort(key=lambda t: (t["tree_index"], t["row"], t["col"], t["talent_name"]))

    return {
        "raw": decoded,
        "glyph_hash": glyph_hash,
        "talent_hash": payload,
        "tree_totals": tree_totals,
        "talents": talents,
        "warnings": warnings,
    }


def build_rotation_url(page_url: str, kind: str) -> str | None:
    if kind == "pve" and page_url.endswith("-spec-builds-talents-glyphs"):
        return page_url.replace("-spec-builds-talents-glyphs", "-rotation-cooldowns-abilities")
    return None


def process_page(
    page_url: str,
    kind: str,
    local_talent_data: dict[str, Any],
    icy_class_cache: dict[str, dict[str, Any]],
) -> dict[str, Any]:
    html = fetch_text(page_url)
    parser = TalentCalculatorParser()
    parser.feed(html)

    page_result: dict[str, Any] = {
        "url": page_url,
        "kind": kind,
        "rotation_url": build_rotation_url(page_url, kind),
        "calculators": [],
        "warnings": [],
    }

    for calc_index, calc_attrs in enumerate(parser.calculators):
        class_slug = calc_attrs.get("data-talentcalculator-class", "")
        pointsurl = calc_attrs.get("data-talentcalculator-pointsurl", "")
        if not class_slug or not pointsurl:
            continue

        if class_slug not in icy_class_cache:
            icy_class_cache[class_slug] = load_icy_class_json(class_slug)

        icy_class_data = icy_class_cache[class_slug]
        url_map, map_warnings = build_urlid_mapping(icy_class_data, local_talent_data)
        decoded = decode_pointsurl(pointsurl, url_map)

        page_result["calculators"].append(
            {
                "index": calc_index,
                "class_slug": class_slug,
                "class_name": icy_class_data.get("class", class_slug),
                "class_id": icy_class_data.get("classId"),
                "pointsurl": pointsurl,
                "normalized": decoded,
                "warnings": map_warnings,
            }
        )

    if not page_result["calculators"]:
        page_result["warnings"].append("No embedded talent calculators found")

    return page_result


def main() -> int:
    print("================================================================")
    print("Icy Veins WotLK PvE/PvP Build Extractor / Normalizer")
    print("================================================================")
    print()

    local_talent_data = load_local_talent_data()
    urls = fetch_sitemap_urls()
    pve_urls, pvp_urls, calc_urls = classify_guide_urls(urls)

    print(f"Found {len(pve_urls)} PvE build pages")
    print(f"Found {len(pvp_urls)} PvP guide pages")
    print(f"Found {len(calc_urls)} class calculator pages")
    print()

    icy_class_cache: dict[str, dict[str, Any]] = {}
    pages: list[dict[str, Any]] = []

    for url in pve_urls:
        print(f"[PvE] {url}")
        pages.append(process_page(url, "pve", local_talent_data, icy_class_cache))

    for url in pvp_urls:
        print(f"[PvP] {url}")
        pages.append(process_page(url, "pvp", local_talent_data, icy_class_cache))

    result = {
        "source": "Icy Veins WotLK Classic",
        "sitemap_url": SITEMAP_URL,
        "local_talent_data": str(LOCAL_TALENT_DATA_FILE.relative_to(SCRIPT_DIR)),
        "counts": {
            "pve_pages": len(pve_urls),
            "pvp_pages": len(pvp_urls),
            "calculator_pages": len(calc_urls),
            "pages_processed": len(pages),
        },
        "pages": pages,
    }

    OUTPUT_FILE.write_text(json.dumps(result, indent=2, ensure_ascii=False), encoding="utf-8")
    print()
    print(f"OK: Wrote normalized build data to {OUTPUT_FILE}")
    return 0


if __name__ == "__main__":
    sys.exit(main())