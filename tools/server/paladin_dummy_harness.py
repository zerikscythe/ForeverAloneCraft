#!/usr/bin/env python3
"""Spawn a level-80 Paladin world bot in Stormwind and force it onto a training dummy.

This is a cheap combat-validation harness for doctrine work:
- ensure one chosen ledger identity is a Paladin spec at the requested level
- force-spawn that identity near the Stormwind Cathedral training dummies
- enable a small debug AI hook that acquires the nearest training dummy by entry
- collect activity/combat trace rows so we can inspect spell choices quickly
"""

from __future__ import annotations

import argparse
import configparser
import csv
import datetime as dt
import io
import json
import pathlib
import re
import shutil
import subprocess
import time


REPO_ROOT = pathlib.Path(__file__).resolve().parents[2]
BUILD_ROOT = REPO_ROOT / "out" / "build-vs2022" / "bin" / "Debug"
WORLD_EXE = BUILD_ROOT / "worldserver.exe"
MODULE_CONF = BUILD_ROOT / "configs" / "modules" / "mod-living-world.conf"
DBCONFIG = REPO_ROOT / "tools" / "lw-editor" / "config.ini"
MYSQL_EXE = pathlib.Path(shutil.which("mysql") or r"D:\mysql-8.0.46-winx64\bin\mysql.exe")
ENCHANTMENT_JSON = REPO_ROOT / "tools" / "lw-editor" / "data" / "enchantment_data.json"

ITEM_ENCHANTMENT_SLOT_COUNT = 12
ITEM_ENCHANTMENT_OFFSET_COUNT = 3
PERM_ENCHANTMENT_SLOT = 0
SOCK_ENCHANTMENT_SLOT = 2

REFERENCE_LOADOUTS = {
    "ret_toc": {
        "display_name": "Paladin Retribution ToC Reference",
        "notes": "Passive summary excludes on-use/proc behavior like Hyperspeed and trinket procs.",
        "items": [
            {"slot": 0, "entry": 48609, "name": "Turalyon's Helm of Triumph"},
            {"slot": 1, "entry": 46040, "name": "Strength of the Heavens"},
            {"slot": 2, "entry": 47697, "name": "Pauldrons of Trembling Rage", "enchant": "Greater Inscription of the Axe"},
            {"slot": 14, "entry": 47320, "name": "Might of the Nerub", "enchant": "Flexweave Underlay"},
            {"slot": 4, "entry": 47589, "name": "Titanium Razorplate", "enchant": "Enchant Chest - Powerful Stats"},
            {"slot": 8, "entry": 47576, "name": "Crusader's Dragonscale Bracers", "enchant": "Enchant Bracers - Greater Assault"},
            {"slot": 9, "entry": 48608, "name": "Turalyon's Gauntlets of Triumph", "enchant": "Hyperspeed Accelerators"},
            {"slot": 5, "entry": 46095, "name": "Soul-Devouring Cinch"},
            {"slot": 6, "entry": 49903, "name": "Legplates of Painful Death", "enchant": "Icescale Leg Armor"},
            {"slot": 7, "entry": 49895, "name": "Footpads of Impending Death", "enchant": "Nitro Boosts"},
            {"slot": 10, "entry": 45534, "name": "Seal of the Betrayed King"},
            {"slot": 11, "entry": 47729, "name": "Bloodshed Band"},
            {"slot": 12, "entry": 42987, "name": "Darkmoon Card: Greatness"},
            {"slot": 13, "entry": 47303, "name": "Death's Choice"},
            {"slot": 15, "entry": 47285, "name": "Dual-blade Butcher"},
            {"slot": 17, "entry": 47661, "name": "Libram of Valiance"},
        ],
    }
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--name", default="PallyDummy80", help="Ledger identity name to create/update")
    parser.add_argument("--spec", default="paladin_ret", choices=["paladin_ret", "paladin_prot", "paladin_holy"])
    parser.add_argument("--level", type=int, default=80)
    parser.add_argument("--seconds", type=int, default=75)
    parser.add_argument("--startup-timeout", type=int, default=120)
    parser.add_argument("--zone-id", type=int, default=1519)
    parser.add_argument("--map-id", type=int, default=0)
    parser.add_argument("--spawn-x", type=float, default=-8772.5)
    parser.add_argument("--spawn-y", type=float, default=347.0)
    parser.add_argument("--spawn-z", type=float, default=101.1)
    parser.add_argument("--dummy-entry", type=int, default=31144, help="Grandmaster's Training Dummy by default")
    parser.add_argument("--dummy-radius", type=float, default=30.0)
    parser.add_argument("--force-spawn-count", type=int, default=1)
    parser.add_argument("--reference-loadout", choices=["none", "ret_toc"], default="ret_toc")
    return parser.parse_args()


def ensure_paths() -> None:
    if not WORLD_EXE.exists():
        raise FileNotFoundError(f"worldserver.exe not found: {WORLD_EXE}")
    if not MODULE_CONF.exists():
        raise FileNotFoundError(f"Module config not found: {MODULE_CONF}")
    if not MYSQL_EXE.exists():
        raise FileNotFoundError(f"mysql.exe not found: {MYSQL_EXE}")


def load_db_settings() -> dict[str, str | int]:
    parser = configparser.ConfigParser()
    if not parser.read(DBCONFIG):
        raise FileNotFoundError(f"Could not read DB config: {DBCONFIG}")

    return {
        "host": parser.get("database", "host"),
        "port": parser.getint("database", "port"),
        "user": parser.get("database", "user"),
        "password": parser.get("database", "password"),
        "characters_db": "acore_characters",
    }


def mysql_base_command(settings: dict[str, str | int], database: str) -> list[str]:
    return [
        str(MYSQL_EXE),
        "-h", str(settings["host"]),
        "-P", str(settings["port"]),
        "-u", str(settings["user"]),
        f"-p{settings['password']}",
        "--batch",
        "--raw",
        "--skip-column-names",
        "-D", database,
    ]


def run_mysql_query(
    settings: dict[str, str | int],
    database: str,
    sql: str,
) -> list[tuple[str, ...]]:
    command = mysql_base_command(settings, database) + ["-e", sql]
    result = subprocess.run(command, capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError(
            f"mysql query failed for {database}:\n{result.stdout}{result.stderr}"
        )

    output = result.stdout.strip()
    if not output:
        return []

    reader = csv.reader(io.StringIO(output), delimiter="\t")
    return [tuple(row) for row in reader]


def query_scalar(settings: dict[str, str | int], database: str, sql: str) -> int:
    rows = run_mysql_query(settings, database, sql)
    if not rows or not rows[0]:
        return 0
    return int(rows[0][0])


def build_item_enchantments(values: list[int]) -> str:
    target_len = ITEM_ENCHANTMENT_SLOT_COUNT * ITEM_ENCHANTMENT_OFFSET_COUNT
    padded = [int(v or 0) for v in list(values[:target_len])]
    if len(padded) < target_len:
        padded.extend([0] * (target_len - len(padded)))
    return " ".join(str(v) for v in padded)


def set_item_enchant_id(values: list[int], slot: int, enchant_id: int) -> None:
    base = slot * ITEM_ENCHANTMENT_OFFSET_COUNT
    if base + 2 >= len(values):
        return
    values[base] = int(enchant_id or 0)
    values[base + 1] = 0
    values[base + 2] = 0


def sql_quote(value: object) -> str:
    if value is None:
        return "NULL"
    if isinstance(value, bool):
        return "1" if value else "0"
    if isinstance(value, (int, float)):
        return str(value)
    text = str(value)
    return "'" + text.replace("\\", "\\\\").replace("'", "\\'") + "'"


def rewrite_module_config(original_text: str, updates: dict[str, str]) -> str:
    lines = original_text.splitlines()
    seen: set[str] = set()
    rewritten: list[str] = []
    for line in lines:
        stripped = line.strip()
        if not stripped or stripped.startswith("#") or "=" not in line:
            rewritten.append(line)
            continue
        key = line.split("=", 1)[0].strip()
        if key in updates:
            rewritten.append(f"{key} = {updates[key]}")
            seen.add(key)
        else:
            rewritten.append(line)

    for key, value in updates.items():
        if key not in seen:
            rewritten.append(f"{key} = {value}")

    return "\n".join(rewritten) + "\n"


def ensure_identity(settings: dict[str, str | int], args: argparse.Namespace) -> int:
    rows = run_mysql_query(
        settings,
        str(settings["characters_db"]),
        f"SELECT id FROM living_world_bot_identity WHERE name = {sql_quote(args.name)} LIMIT 1",
    )

    if rows:
        identity_id = int(rows[0][0])
        run_mysql_query(
            settings,
            str(settings["characters_db"]),
            (
                "UPDATE living_world_bot_identity SET "
                f"race_id=1, class_id=2, spec_key={sql_quote(args.spec)}, loadout_key='', "
                f"faction=1, display_id=49, gender=0, level={args.level}, gear_tier=3, "
                "population_role='world', reserve_city_zone_id=NULL, home_zone_id=1519, "
                "home_anchor_point_key='stormwind_inn', home_bind_point_key='stormwind_inn', "
                "is_available=1, session_count=0, total_world_online_ms=0, "
                "world_online_ms_since_level=0, post_max_world_online_ms=0, active_world_session_ms=0, "
                "runtime_state='', runtime_detail='', last_session_source_kind='', last_session_source_key='', "
                "last_task_family='', last_task_target_zone=NULL, gear_refresh_pending=1, last_gear_refresh_band=0, "
                "active_world_session_start=NULL, is_retired=0, successor_spawned=0, retired_at=NULL, "
                "last_seen_zone=1519, last_seen_at=NULL "
                f"WHERE id={identity_id}"
            ),
        )
        return identity_id

    run_mysql_query(
        settings,
        str(settings["characters_db"]),
        (
            "INSERT INTO living_world_bot_identity "
            "(name, race_id, class_id, spec_key, loadout_key, faction, display_id, gender, level, gear_tier, "
            "population_role, home_zone_id, home_anchor_point_key, home_bind_point_key, is_available, is_retired, last_seen_zone) VALUES "
            f"({sql_quote(args.name)}, 1, 2, {sql_quote(args.spec)}, '', 1, 49, 0, {args.level}, 3, "
            "'world', 1519, 'stormwind_inn', 'stormwind_inn', 1, 0, 1519)"
        ),
    )
    rows = run_mysql_query(
        settings,
        str(settings["characters_db"]),
        f"SELECT id FROM living_world_bot_identity WHERE name = {sql_quote(args.name)} LIMIT 1",
    )
    if not rows:
        raise RuntimeError("Failed to create harness identity")
    return int(rows[0][0])


def load_enchantment_cache() -> list[dict[str, object]]:
    if not ENCHANTMENT_JSON.exists():
        raise FileNotFoundError(f"Missing enchantment cache: {ENCHANTMENT_JSON}")
    return json.loads(ENCHANTMENT_JSON.read_text(encoding="utf-8"))


def resolve_enchant_id(cache: list[dict[str, object]], text: str) -> int:
    needle = text.strip().lower()
    if not needle:
        return 0

    exact_spell = []
    exact_name = []
    fuzzy = []
    for row in cache:
        spell_name = str(row.get("SpellName", "") or "")
        name = str(row.get("Name_Lang_enUS", "") or "")
        effects = " ; ".join(row.get("Effects", []) or [])

        if spell_name.lower() == needle:
            exact_spell.append(row)
        if name.lower() == needle:
            exact_name.append(row)

        hay = f"{name} | {spell_name} | {effects}".lower()
        if needle in hay:
            fuzzy.append(row)

    for bucket in (exact_spell, exact_name, fuzzy):
        if bucket:
            return int(bucket[0]["ID"])

    raise KeyError(f"Could not resolve enchant '{text}' from {ENCHANTMENT_JSON}")


def compute_refresh_band(level: int) -> int:
    if level <= 0:
        return 0
    return 1 + ((min(level, 80) - 1) // 5)


def ensure_assigned_gear_schema(settings: dict[str, str | int]) -> None:
    rows = run_mysql_query(
        settings,
        str(settings["characters_db"]),
        "SHOW COLUMNS FROM living_world_bot_assigned_gear LIKE 'enchantments'",
    )
    if rows:
        return
    run_mysql_query(
        settings,
        str(settings["characters_db"]),
        (
            "ALTER TABLE living_world_bot_assigned_gear "
            "ADD COLUMN enchantments VARCHAR(512) NOT NULL DEFAULT '' AFTER quality"
        ),
    )


def load_item_stats(
    settings: dict[str, str | int],
    item_entry: int,
) -> dict[str, int]:
    rows = run_mysql_query(
        settings,
        "acore_world",
        (
            "SELECT armor, block, holy_res, fire_res, nature_res, frost_res, shadow_res, arcane_res, "
            "stat_type1, stat_value1, stat_type2, stat_value2, stat_type3, stat_value3, "
            "stat_type4, stat_value4, stat_type5, stat_value5, stat_type6, stat_value6, "
            "stat_type7, stat_value7, stat_type8, stat_value8, stat_type9, stat_value9, "
            f"stat_type10, stat_value10 FROM item_template WHERE entry = {item_entry} LIMIT 1"
        ),
    )
    if not rows:
        raise RuntimeError(f"Missing item_template row for item {item_entry}")

    cols = rows[0]
    keys = [
        "armor", "block", "holy_res", "fire_res", "nature_res", "frost_res", "shadow_res", "arcane_res",
        "stat_type1", "stat_value1", "stat_type2", "stat_value2", "stat_type3", "stat_value3",
        "stat_type4", "stat_value4", "stat_type5", "stat_value5", "stat_type6", "stat_value6",
        "stat_type7", "stat_value7", "stat_type8", "stat_value8", "stat_type9", "stat_value9",
        "stat_type10", "stat_value10",
    ]
    return {key: int(value or 0) for key, value in zip(keys, cols)}


def empty_summary() -> dict[str, int]:
    return {
        "str": 0, "agi": 0, "sta": 0, "int": 0, "spi": 0,
        "hp": 0, "mana": 0, "armor": 0,
        "res_holy": 0, "res_fire": 0, "res_nature": 0, "res_frost": 0, "res_shadow": 0, "res_arcane": 0,
        "ap": 0, "rap": 0,
        "def": 0, "dodge": 0, "parry": 0, "block": 0, "block_value": 0,
        "hit_melee": 0, "hit_ranged": 0, "hit_spell": 0,
        "crit_melee": 0, "crit_ranged": 0, "crit_spell": 0,
        "haste_melee": 0, "haste_ranged": 0, "haste_spell": 0,
        "expertise": 0, "armor_pen": 0,
        "hit_taken": 0, "crit_taken": 0, "resilience": 0,
        "spell_power": 0, "healing": 0, "mp5": 0, "hp5": 0, "spell_pen": 0,
    }


def accumulate_item_stat(summary: dict[str, int], stat_type: int, stat_value: int) -> None:
    if stat_value == 0:
        return
    mapping = {
        0: ("mana",),
        1: ("hp",),
        3: ("agi",),
        4: ("str",),
        5: ("int",),
        6: ("spi",),
        7: ("sta",),
        12: ("def",),
        13: ("dodge",),
        14: ("parry",),
        15: ("block",),
        16: ("hit_melee",),
        17: ("hit_ranged",),
        18: ("hit_spell",),
        19: ("crit_melee",),
        20: ("crit_ranged",),
        21: ("crit_spell",),
        22: ("hit_taken",),
        23: ("hit_taken",),
        24: ("hit_taken",),
        25: ("crit_taken",),
        26: ("crit_taken",),
        27: ("crit_taken",),
        28: ("haste_melee",),
        29: ("haste_ranged",),
        30: ("haste_spell",),
        31: ("hit_melee", "hit_ranged", "hit_spell"),
        32: ("crit_melee", "crit_ranged", "crit_spell"),
        33: ("hit_taken",),
        34: ("crit_taken",),
        35: ("resilience",),
        36: ("haste_melee", "haste_ranged", "haste_spell"),
        37: ("expertise",),
        38: ("ap",),
        39: ("rap",),
        41: ("healing",),
        42: ("spell_power",),
        43: ("mp5",),
        44: ("armor_pen",),
        45: ("spell_power", "healing"),
        46: ("hp5",),
        47: ("spell_pen",),
        48: ("block_value",),
    }
    for key in mapping.get(stat_type, ()):
        summary[key] += stat_value


def accumulate_item_row(summary: dict[str, int], row: dict[str, int]) -> None:
    summary["armor"] += row["armor"]
    summary["block_value"] += row["block"]
    summary["res_holy"] += row["holy_res"]
    summary["res_fire"] += row["fire_res"]
    summary["res_nature"] += row["nature_res"]
    summary["res_frost"] += row["frost_res"]
    summary["res_shadow"] += row["shadow_res"]
    summary["res_arcane"] += row["arcane_res"]
    for i in range(1, 11):
        accumulate_item_stat(summary, row[f"stat_type{i}"], row[f"stat_value{i}"])


def extract_enchant_bonus_texts(enchant_row: dict[str, object]) -> list[str]:
    effects = [str(v).strip() for v in (enchant_row.get("Effects") or []) if str(v).strip()]
    numeric_effects = [
        effect for effect in effects
        if effect.startswith("+") or re.search(r"\b\d+\b", effect)
    ]
    if numeric_effects:
        return numeric_effects

    name = str(enchant_row.get("Name_Lang_enUS", "") or "").strip()
    if name.startswith("+") or re.search(r"\b\d+\b", name):
        return [name]
    return []


def accumulate_enchant_text(summary: dict[str, int], text: str) -> None:
    text = text.strip()
    if not text or text.lower().startswith("use:") or text.lower().startswith("proc:") or text.lower().startswith("equip:"):
        return

    all_stats = re.match(r"^\+(\d+)\s+All Stats$", text, re.IGNORECASE)
    if all_stats:
        amount = int(all_stats.group(1))
        for key in ("str", "agi", "sta", "int", "spi"):
            summary[key] += amount
        return

    patterns = [
        (r"^\+(\d+)\s+Attack Power$", ("ap",)),
        (r"^\+(\d+)\s+Agility$", ("agi",)),
        (r"^\+(\d+)\s+Strength$", ("str",)),
        (r"^\+(\d+)\s+Stamina$", ("sta",)),
        (r"^\+(\d+)\s+Intellect$", ("int",)),
        (r"^\+(\d+)\s+Spirit$", ("spi",)),
        (r"^\+(\d+)\s+(?:Crit Rating|Critical Strike Rating)$", ("crit_melee", "crit_ranged", "crit_spell")),
        (r"^\+(\d+)\s+Hit Rating$", ("hit_melee", "hit_ranged", "hit_spell")),
        (r"^\+(\d+)\s+Haste Rating$", ("haste_melee", "haste_ranged", "haste_spell")),
        (r"^\+(\d+)\s+Expertise Rating$", ("expertise",)),
        (r"^\+(\d+)\s+Armor Penetration Rating$", ("armor_pen",)),
        (r"^\+(\d+)\s+Spell Power$", ("spell_power", "healing")),
        (r"^\+(\d+)\s+Mana per 5 sec\.$", ("mp5",)),
        (r"^\+(\d+)\s+Mana per 5 sec$", ("mp5",)),
    ]
    for pattern, keys in patterns:
        match = re.match(pattern, text, re.IGNORECASE)
        if not match:
            continue
        amount = int(match.group(1))
        for key in keys:
            summary[key] += amount
        return


def format_summary(summary: dict[str, int]) -> str:
    ordered = [
        "str", "agi", "sta", "int", "spi", "hp", "mana", "armor",
        "ap", "rap", "hit_melee", "crit_melee", "haste_melee",
        "expertise", "armor_pen", "spell_power", "healing", "mp5", "hp5",
    ]
    return ", ".join(f"{key}={summary[key]}" for key in ordered if summary.get(key))


def seed_reference_loadout(
    settings: dict[str, str | int],
    identity_id: int,
    level: int,
    reference_loadout_key: str,
) -> dict[str, object]:
    if reference_loadout_key == "none":
        return {"key": "none", "display_name": "None", "notes": "Generator will choose gear."}

    ensure_assigned_gear_schema(settings)
    spec = REFERENCE_LOADOUTS[reference_loadout_key]
    enchant_cache = load_enchantment_cache()
    summary = empty_summary()
    seeded_rows: list[dict[str, object]] = []

    run_mysql_query(
        settings,
        str(settings["characters_db"]),
        f"DELETE FROM living_world_bot_assigned_gear WHERE identity_id = {identity_id}",
    )

    for item in spec["items"]:
        enchant_values = [0] * (ITEM_ENCHANTMENT_SLOT_COUNT * ITEM_ENCHANTMENT_OFFSET_COUNT)
        enchant_id = 0
        enchant_row: dict[str, object] | None = None
        if item.get("enchant"):
            enchant_name = str(item["enchant"])
            enchant_id = resolve_enchant_id(enchant_cache, enchant_name)
            enchant_row = next(
                (row for row in enchant_cache if int(row.get("ID", 0)) == enchant_id),
                None,
            )
            set_item_enchant_id(enchant_values, PERM_ENCHANTMENT_SLOT, enchant_id)

        enchantments_text = build_item_enchantments(enchant_values)
        run_mysql_query(
            settings,
            str(settings["characters_db"]),
            (
                "INSERT INTO living_world_bot_assigned_gear "
                "(identity_id, slot_id, item_id, item_level, quality, enchantments) "
                f"VALUES ({identity_id}, {int(item['slot'])}, {int(item['entry'])}, 0, 4, {sql_quote(enchantments_text)})"
            ),
        )

        item_row = load_item_stats(settings, int(item["entry"]))
        accumulate_item_row(summary, item_row)
        if enchant_row:
            for text in extract_enchant_bonus_texts(enchant_row):
                accumulate_enchant_text(summary, text)

        seeded_rows.append({
            "slot": int(item["slot"]),
            "item_entry": int(item["entry"]),
            "item_name": str(item["name"]),
            "enchant_id": enchant_id,
            "enchant_name": str(item.get("enchant", "")),
            "enchantments": enchantments_text,
        })

    refresh_band = compute_refresh_band(level)
    run_mysql_query(
        settings,
        str(settings["characters_db"]),
        (
            "UPDATE living_world_bot_identity SET "
            f"gear_refresh_pending = 0, last_gear_refresh_band = {refresh_band} "
            f"WHERE id = {identity_id}"
        ),
    )

    return {
        "key": reference_loadout_key,
        "display_name": spec["display_name"],
        "notes": spec["notes"],
        "rows": seeded_rows,
        "expected_summary": summary,
    }


def wait_for_identity_activity(
    settings: dict[str, str | int],
    baseline_id: int,
    identity_id: int,
    timeout_seconds: int,
) -> None:
    deadline = time.time() + timeout_seconds
    sql = (
        "SELECT COUNT(*) FROM living_world_bot_activity_log "
        f"WHERE id > {baseline_id} AND bot_guid = {identity_id}"
    )
    while time.time() < deadline:
        if query_scalar(settings, str(settings["characters_db"]), sql) > 0:
            return
        time.sleep(1)
    raise TimeoutError("Timed out waiting for harness bot activity rows")


def stop_worldserver(process: subprocess.Popen[bytes]) -> None:
    if process.poll() is not None:
        return
    process.terminate()
    try:
        process.wait(timeout=20)
        return
    except subprocess.TimeoutExpired:
        pass
    process.kill()
    process.wait(timeout=15)


def build_report(
    settings: dict[str, str | int],
    baseline_id: int,
    identity_id: int,
    name: str,
    stdout_path: pathlib.Path,
    stderr_path: pathlib.Path,
    report_path: pathlib.Path,
    seeded_loadout: dict[str, object],
) -> None:
    rows = run_mysql_query(
        settings,
        str(settings["characters_db"]),
        (
            "SELECT event_type, COUNT(*) FROM living_world_bot_activity_log "
            f"WHERE id > {baseline_id} AND bot_guid = {identity_id} "
            "GROUP BY event_type ORDER BY COUNT(*) DESC, event_type ASC"
        ),
    )
    recent = run_mysql_query(
        settings,
        str(settings["characters_db"]),
        (
            "SELECT id, bot_name, event_type, zone_id, detail "
            "FROM living_world_bot_activity_log "
            f"WHERE id > {baseline_id} AND bot_guid = {identity_id} "
            "ORDER BY id DESC LIMIT 120"
        ),
    )
    identity_rows = run_mysql_query(
        settings,
        str(settings["characters_db"]),
        (
            "SELECT id, name, level, spec_key, runtime_state, runtime_detail, session_count, active_world_session_ms "
            "FROM living_world_bot_identity "
            f"WHERE id = {identity_id} LIMIT 1"
        ),
    )
    build_prepared = run_mysql_query(
        settings,
        str(settings["characters_db"]),
        (
            "SELECT detail FROM living_world_bot_activity_log "
            f"WHERE id > {baseline_id} AND bot_guid = {identity_id} AND event_type = 'build_prepared' "
            "ORDER BY id DESC LIMIT 1"
        ),
    )

    with report_path.open("w", encoding="utf-8") as handle:
        handle.write("=== CONFIG ===\n")
        handle.write(f"identity_id={identity_id}\n")
        handle.write(f"name={name}\n")
        handle.write(f"reference_loadout={seeded_loadout.get('key', 'none')}\n")
        handle.write(f"stdout_log={stdout_path.name}\n")
        handle.write(f"stderr_log={stderr_path.name}\n\n")

        handle.write("=== SEEDED_LOADOUT ===\n")
        handle.write(f"display_name={seeded_loadout.get('display_name', 'None')}\n")
        handle.write(f"notes={seeded_loadout.get('notes', '')}\n")
        expected_summary = seeded_loadout.get("expected_summary")
        if isinstance(expected_summary, dict):
            handle.write(f"expected_passive_summary={format_summary(expected_summary)}\n")
        for row in seeded_loadout.get("rows", []) or []:
            handle.write(
                f"slot={row['slot']} item={row['item_name']} [{row['item_entry']}] "
                f"enchant={row.get('enchant_name','')} [{row.get('enchant_id', 0)}]\n"
            )
        handle.write("\n")

        handle.write("=== IDENTITY ===\n")
        for row in identity_rows:
            handle.write("\t".join(str(value) for value in row) + "\n")
        handle.write("\n")

        handle.write("=== BUILD_PREPARED ===\n")
        for row in build_prepared:
            handle.write("\t".join(str(value) for value in row) + "\n")
        handle.write("\n")

        handle.write("=== EVENT_COUNTS ===\n")
        for row in rows:
            handle.write("\t".join(str(value) for value in row) + "\n")
        handle.write("\n")

        handle.write("=== RECENT_ROWS ===\n")
        for row in recent:
            handle.write("\t".join(str(value) for value in row) + "\n")


def main() -> int:
    args = parse_args()
    ensure_paths()
    settings = load_db_settings()
    identity_id = ensure_identity(settings, args)
    seeded_loadout = seed_reference_loadout(settings, identity_id, args.level, args.reference_loadout)

    timestamp = dt.datetime.now().strftime("%Y%m%d-%H%M%S")
    prefix = REPO_ROOT / f"paladin-dummy-harness-{timestamp}"
    stdout_path = prefix.with_suffix(".stdout.log")
    stderr_path = prefix.with_suffix(".stderr.log")
    report_path = prefix.with_suffix(".report.txt")

    updates = {
        "LivingWorld.AmbientPopulation": str(args.force_spawn_count),
        "LivingWorld.AmbientPopulationTickMs": "10000",
        "LivingWorld.AmbientForceSpawnCount": str(args.force_spawn_count),
        "LivingWorld.AmbientForceSpawnMapId": str(args.map_id),
        "LivingWorld.AmbientForceSpawnX": str(args.spawn_x),
        "LivingWorld.AmbientForceSpawnY": str(args.spawn_y),
        "LivingWorld.AmbientForceSpawnZ": str(args.spawn_z),
        "LivingWorld.DebugSyntheticInterestEnabled": "1",
        "LivingWorld.DebugSyntheticInterestMapId": str(args.map_id),
        "LivingWorld.DebugSyntheticInterestZoneId": str(args.zone_id),
        "LivingWorld.DebugSyntheticInterestSwitchMapId": "0",
        "LivingWorld.DebugSyntheticInterestSwitchZoneId": "0",
        "LivingWorld.DebugSyntheticInterestSwitchMs": "0",
        "LivingWorld.DebugSyntheticInterestClearMs": "0",
        "LivingWorld.DebugHotZoneCooldownMs": "10000",
        "LivingWorld.DebugForceIdentityIds": f"\"{identity_id}\"",
        "LivingWorld.DebugForceSessionZoneId": str(args.zone_id),
        "LivingWorld.DebugForceSessionComposeAttempts": "64",
        "LivingWorld.DebugForceCombatTargetIdentityId": str(identity_id),
        "LivingWorld.DebugForceCombatTargetEntry": str(args.dummy_entry),
        "LivingWorld.DebugForceCombatTargetSearchRadius": str(args.dummy_radius),
    }

    baseline_id = query_scalar(
        settings,
        str(settings["characters_db"]),
        "SELECT COALESCE(MAX(id), 0) FROM living_world_bot_activity_log",
    )

    original_config = MODULE_CONF.read_text(encoding="utf-8")
    process: subprocess.Popen[bytes] | None = None
    try:
        MODULE_CONF.write_text(rewrite_module_config(original_config, updates), encoding="utf-8")

        with stdout_path.open("wb") as stdout_handle, stderr_path.open("wb") as stderr_handle:
            process = subprocess.Popen(
                [str(WORLD_EXE)],
                cwd=str(BUILD_ROOT),
                stdout=stdout_handle,
                stderr=stderr_handle,
            )

            wait_for_identity_activity(settings, baseline_id, identity_id, args.startup_timeout)
            time.sleep(args.seconds)
    finally:
        if process is not None:
            stop_worldserver(process)
        MODULE_CONF.write_text(original_config, encoding="utf-8")

    build_report(settings, baseline_id, identity_id, args.name, stdout_path, stderr_path, report_path, seeded_loadout)
    print(report_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
