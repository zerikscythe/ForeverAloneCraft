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

import html
import json
import pathlib
import re
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

ROTATION_SECTION_KIND_MAP = {
    "rotation": "rotation",
    "important dps-enhancing spells": "cooldowns",
    "aoe damage and threat": "aoe",
    "tips and tricks for using aoe spells": "aoe_tips",
    "single target damage": "single_target",
    "other important arcane mage spells to have keybound": "keybound",
    "other important fire mage spells to have keybound": "keybound",
    "other important frost mage spells to have keybound": "keybound",
    "other important death knight spells to have keybound": "keybound",
    "other useful combat abilities": "utility",
}


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


def clean_html_text(raw_html: str) -> str:
    text = re.sub(r"(?is)<(script|style).*?>.*?</\1>", " ", raw_html)
    text = re.sub(r"(?is)<br\s*/?>", " ", text)
    text = re.sub(r"(?is)<[^>]+>", " ", text)
    text = html.unescape(text)
    return " ".join(text.split())


def extract_title_from_html(html_text: str) -> str:
    match = re.search(r"<title>(.*?)</title>", html_text, re.IGNORECASE | re.DOTALL)
    return clean_html_text(match.group(1)) if match else ""


def classify_rotation_heading(heading: str) -> str | None:
    lowered = heading.strip().lower()
    if lowered in ROTATION_SECTION_KIND_MAP:
        return ROTATION_SECTION_KIND_MAP[lowered]

    if "single target" in lowered:
        return "single_target"
    if "aoe" in lowered:
        return "aoe"
    if "rotation" in lowered:
        return "rotation"
    if "cooldown" in lowered or "dps-enhancing" in lowered:
        return "cooldowns"
    if "keybound" in lowered:
        return "keybound"
    if "combat abilities" in lowered:
        return "utility"

    return None


def extract_rotation_sections(
    html_text: str,
) -> list[dict[str, Any]]:
    heading_matches = list(
        re.finditer(r"<h([1-4])[^>]*>(.*?)</h\1>", html_text, re.IGNORECASE | re.DOTALL)
    )
    sections: list[dict[str, Any]] = []

    for index, heading_match in enumerate(heading_matches):
        heading = clean_html_text(heading_match.group(2))
        section_kind = classify_rotation_heading(heading)
        if not section_kind:
            continue

        body_start = heading_match.end()
        body_end = heading_matches[index + 1].start() if index + 1 < len(heading_matches) else len(html_text)
        body_html = html_text[body_start:body_end]

        text_blocks: list[dict[str, Any]] = []
        for block_match in re.finditer(r"<(p|li)\b[^>]*>(.*?)</\1>", body_html, re.IGNORECASE | re.DOTALL):
            source_type = block_match.group(1).lower()
            text = clean_html_text(block_match.group(2))
            if len(text) < 20:
                continue

            block = {
                "source_type": source_type,
                "text": text,
            }
            text_blocks.append(block)

        if not text_blocks:
            continue

        section_text = "\n".join(block["text"] for block in text_blocks)

        sections.append(
            {
                "heading": heading,
                "section_kind": section_kind,
                "section_text": section_text,
                "text_blocks": text_blocks,
            }
        )

    return sections


def fetch_rotation_article_analysis(
    rotation_url: str | None,
) -> dict[str, Any] | None:
    if not rotation_url:
        return None

    analysis: dict[str, Any] = {
        "url": rotation_url,
        "title": "",
        "status": "unavailable",
        "article_text": "",
        "sections": [],
        "warnings": [],
    }

    try:
        html_text = fetch_text(rotation_url)
    except Exception as exc:  # pragma: no cover - network failures are non-deterministic
        analysis["warnings"].append(f"Rotation page fetch failed: {exc}")
        return analysis

    analysis["title"] = extract_title_from_html(html_text)
    sections = extract_rotation_sections(html_text)
    analysis["sections"] = sections
    analysis["article_text"] = "\n\n".join(
        section["section_text"] for section in sections if section.get("section_text")
    )
    analysis["status"] = "ok" if sections else "no_relevant_sections"
    if not sections:
        analysis["warnings"].append("No relevant rotation sections found on rotation page")

    return analysis


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


def humanize_slug(slug: str) -> str:
    if not slug:
        return ""
    parts = [part for part in slug.split("-") if part]
    return " ".join(part.capitalize() for part in parts)


def extract_page_slug(page_url: str) -> str:
    return urllib.parse.urlparse(page_url).path.rsplit("/", 1)[-1]


def strip_page_suffix(page_slug: str, kind: str) -> str:
    if kind == "pve" and page_slug.endswith("-spec-builds-talents-glyphs"):
        return page_slug.removesuffix("-spec-builds-talents-glyphs")
    if kind == "pvp" and page_slug.endswith("-pvp-guide"):
        return page_slug.removesuffix("-pvp-guide")
    return page_slug


def infer_guide_metadata(
    page_url: str,
    kind: str,
    class_slug: str,
    class_name: str,
) -> dict[str, Any]:
    page_slug = extract_page_slug(page_url)
    guide_slug = strip_page_suffix(page_slug, kind)
    core_slug = guide_slug.removesuffix(f"-{kind}")
    class_token = f"-{class_slug}"
    class_pos = core_slug.find(class_token)

    spec_slug = core_slug.strip("-")
    role_slug = ""
    if class_pos != -1:
        spec_slug = core_slug[:class_pos].strip("-")
        role_slug = core_slug[class_pos + len(class_token) :].strip("-")

    guide_key_parts = [class_slug]
    if spec_slug:
        guide_key_parts.append(spec_slug)
    if role_slug:
        guide_key_parts.append(role_slug)
    guide_key_parts.append(kind)

    spec_name = humanize_slug(spec_slug)
    role_name = humanize_slug(role_slug)
    guide_name_parts = [part for part in [spec_name, class_name, role_name] if part]
    guide_name = " ".join(guide_name_parts).strip() or class_name

    return {
        "page_slug": page_slug,
        "guide_slug": guide_slug,
        "guide_key": "__".join(guide_key_parts),
        "spec_slug": spec_slug,
        "spec_name": spec_name,
        "role_slug": role_slug,
        "role_name": role_name,
        "guide_name": guide_name,
    }


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


def summarize_talent_distribution(
    local_talent_data: dict[str, Any],
    class_id: int,
    decoded: dict[str, Any],
) -> dict[str, Any]:
    tree_order = build_local_tree_order(local_talent_data, class_id)
    tree_distribution: list[dict[str, Any]] = []

    for tree in tree_order:
        points = int(decoded.get("tree_totals", {}).get(tree["tree_name"], 0))
        tree_distribution.append(
            {
                "tree_index": tree["tree_index"],
                "tree_name": tree["tree_name"],
                "points": points,
            }
        )

    primary_tree = max(
        tree_distribution,
        key=lambda item: (item["points"], -item["tree_index"]),
        default=None,
    )
    active_trees = [tree for tree in tree_distribution if tree["points"] > 0]

    return {
        "talent_points_spent": sum(tree["points"] for tree in tree_distribution),
        "tree_distribution": tree_distribution,
        "distribution_signature": "/".join(str(tree["points"]) for tree in tree_distribution),
        "primary_tree_name": primary_tree["tree_name"] if primary_tree else "",
        "primary_tree_index": primary_tree["tree_index"] if primary_tree else None,
        "active_trees": active_trees,
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

    guide_meta: dict[str, Any] | None = None
    class_trees: list[dict[str, Any]] = []

    for calc_index, calc_attrs in enumerate(parser.calculators):
        class_slug = calc_attrs.get("data-talentcalculator-class", "")
        pointsurl = calc_attrs.get("data-talentcalculator-pointsurl", "")
        if not class_slug or not pointsurl:
            continue

        if class_slug not in icy_class_cache:
            icy_class_cache[class_slug] = load_icy_class_json(class_slug)

        icy_class_data = icy_class_cache[class_slug]
        class_id = int(icy_class_data["classId"])
        url_map, map_warnings = build_urlid_mapping(icy_class_data, local_talent_data)
        decoded = decode_pointsurl(pointsurl, url_map)
        build_summary = summarize_talent_distribution(local_talent_data, class_id, decoded)

        if guide_meta is None:
            guide_meta = infer_guide_metadata(
                page_url,
                kind,
                class_slug,
                icy_class_data.get("class", class_slug),
            )
            class_trees = [
                {
                    "tree_index": tree["tree_index"],
                    "tree_name": tree["tree_name"],
                }
                for tree in build_local_tree_order(local_talent_data, class_id)
            ]

        page_result["calculators"].append(
            {
                "index": calc_index,
                "build_index": calc_index + 1,
                "class_slug": class_slug,
                "class_name": icy_class_data.get("class", class_slug),
                "class_id": class_id,
                "pointsurl": pointsurl,
                "build_key": (
                    f"{guide_meta['guide_key']}__build_{calc_index + 1}"
                    if guide_meta
                    else f"{class_slug}__{kind}__build_{calc_index + 1}"
                ),
                "build_name": (
                    f"Build {calc_index + 1} ({build_summary['distribution_signature']})"
                    if len(parser.calculators) > 1
                    else "Primary Build"
                ),
                "primary_tree_name": build_summary["primary_tree_name"],
                "primary_tree_index": build_summary["primary_tree_index"],
                "distribution_signature": build_summary["distribution_signature"],
                "talent_points_spent": build_summary["talent_points_spent"],
                "tree_distribution": build_summary["tree_distribution"],
                "active_trees": build_summary["active_trees"],
                "normalized": decoded,
                "warnings": map_warnings,
            }
        )

    if not page_result["calculators"]:
        page_result["warnings"].append("No embedded talent calculators found")
    else:
        page_result.update(guide_meta or {})
        first_calc = page_result["calculators"][0]
        page_result["class_slug"] = first_calc["class_slug"]
        page_result["class_name"] = first_calc["class_name"]
        page_result["class_id"] = first_calc["class_id"]
        page_result["talent_trees"] = class_trees
        page_result["build_count"] = len(page_result["calculators"])

        if kind == "pve" and page_result.get("rotation_url"):
            rotation_analysis = fetch_rotation_article_analysis(page_result["rotation_url"])
            if rotation_analysis:
                page_result["rotation_analysis"] = rotation_analysis
                page_result["warnings"].extend(rotation_analysis.get("warnings", []))
                for calc in page_result["calculators"]:
                    calc["rotation_status"] = rotation_analysis.get("status", "unavailable")
                    calc["rotation_article_text"] = rotation_analysis.get("article_text", "")
                    calc["rotation_sections"] = rotation_analysis.get("sections", [])

    return page_result


def build_class_index(pages: list[dict[str, Any]]) -> list[dict[str, Any]]:
    classes: dict[str, dict[str, Any]] = {}

    for page in pages:
        if not page.get("calculators"):
            continue

        class_slug = page["class_slug"]
        class_entry = classes.setdefault(
            class_slug,
            {
                "class_slug": class_slug,
                "class_name": page["class_name"],
                "class_id": page["class_id"],
                "talent_trees": page.get("talent_trees", []),
                "pve_specs": [],
                "pvp_specs": [],
            },
        )

        spec_entry = {
            "guide_key": page.get("guide_key"),
            "guide_name": page.get("guide_name"),
            "page_slug": page.get("page_slug"),
            "guide_slug": page.get("guide_slug"),
            "kind": page.get("kind"),
            "url": page.get("url"),
            "rotation_url": page.get("rotation_url"),
            "spec_slug": page.get("spec_slug"),
            "spec_name": page.get("spec_name"),
            "role_slug": page.get("role_slug"),
            "role_name": page.get("role_name"),
            "rotation_analysis": page.get("rotation_analysis"),
            "build_count": page.get("build_count", len(page.get("calculators", []))),
            "builds": page.get("calculators", []),
            "warnings": page.get("warnings", []),
        }

        key = "pve_specs" if page.get("kind") == "pve" else "pvp_specs"
        class_entry[key].append(spec_entry)

    for class_entry in classes.values():
        class_entry["pve_specs"].sort(
            key=lambda spec: (spec.get("spec_slug", ""), spec.get("role_slug", ""), spec.get("guide_key", ""))
        )
        class_entry["pvp_specs"].sort(
            key=lambda spec: (spec.get("spec_slug", ""), spec.get("role_slug", ""), spec.get("guide_key", ""))
        )

    return sorted(classes.values(), key=lambda entry: entry["class_name"])


def build_guide_index(pages: list[dict[str, Any]]) -> dict[str, dict[str, Any]]:
    guide_index: dict[str, dict[str, Any]] = {}

    for page in pages:
        guide_key = page.get("guide_key")
        if not guide_key:
            continue

        guide_index[guide_key] = {
            "class_slug": page.get("class_slug"),
            "class_name": page.get("class_name"),
            "kind": page.get("kind"),
            "spec_slug": page.get("spec_slug"),
            "spec_name": page.get("spec_name"),
            "role_slug": page.get("role_slug"),
            "role_name": page.get("role_name"),
            "url": page.get("url"),
            "rotation_url": page.get("rotation_url"),
            "rotation_status": page.get("rotation_analysis", {}).get("status"),
            "build_count": page.get("build_count", len(page.get("calculators", []))),
            "primary_trees": [calc.get("primary_tree_name", "") for calc in page.get("calculators", [])],
        }

    return dict(sorted(guide_index.items()))


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

    classes = build_class_index(pages)
    guide_index = build_guide_index(pages)

    result = {
        "source": "Icy Veins WotLK Classic",
        "sitemap_url": SITEMAP_URL,
        "local_talent_data": str(LOCAL_TALENT_DATA_FILE.relative_to(SCRIPT_DIR)),
        "counts": {
            "classes": len(classes),
            "pve_pages": len(pve_urls),
            "pvp_pages": len(pvp_urls),
            "calculator_pages": len(calc_urls),
            "pages_processed": len(pages),
            "guide_entries": len(guide_index),
            "build_variants": sum(len(page.get("calculators", [])) for page in pages),
            "rotation_pages_parsed": sum(1 for page in pages if page.get("rotation_analysis")),
            "rotation_pages_with_sections": sum(
                1
                for page in pages
                if page.get("rotation_analysis", {}).get("status") == "ok"
            ),
        },
        "classes": classes,
        "guide_index": guide_index,
        "pages": pages,
    }

    OUTPUT_FILE.write_text(json.dumps(result, indent=2, ensure_ascii=False), encoding="utf-8")
    print()
    print(f"OK: Wrote normalized build data to {OUTPUT_FILE}")
    return 0


if __name__ == "__main__":
    sys.exit(main())