"""
helpers.py -- Utility and display helper functions.
"""
import tkinter as tk
from tkinter import ttk
from datetime import datetime
from .constants import (
    WOW_CLASSES, ITEM_QUALITY_LABEL_TO_ID, ITEM_QUALITY_COLORS,
    ITEM_MOD_NAMES, INVENTORY_TYPE_NAMES, ITEM_TYPE_NAMES,
    SLOT_ENCHANT_CATEGORY, SLOT_SPELL_NAME_KEYWORDS, ENCHANT_CATEGORY_KEYWORDS,
    SOCKET_COLOR_NAMES,
    ITEM_ENCHANTMENT_SLOT_COUNT, ITEM_ENCHANTMENT_OFFSET_COUNT,
)

# ═══════════════════════════════════════════════════════════════════════════
#  SHARED HELPER: labeled row builder
# ═══════════════════════════════════════════════════════════════════════════

def lbl(parent, text, row, col, **kw):
    ttk.Label(parent, text=text).grid(row=row, column=col, sticky="w",
                                      padx=4, pady=2, **kw)


# ═══════════════════════════════════════════════════════════════════════════
#  SORTABLE TREEVIEW
# ═══════════════════════════════════════════════════════════════════════════

def make_tv_sortable(tv: ttk.Treeview, numeric_cols: set | None = None):
    """
    Attach click-to-sort to every column header of *tv*.

    Click once  → sort ascending   (▲)
    Click again → sort descending  (▼)

    numeric_cols: set of column ids to force numeric sort.
    Columns not in the set try numeric first, fall back to case-insensitive
    string sort automatically.
    """
    _state: dict[str, bool] = {}   # col_id → currently_ascending

    # Store original header text so arrows don't accumulate
    _orig_text: dict[str, str] = {}

    def _sort(col: str) -> None:
        asc = not _state.get(col, True)   # toggle
        _state[col] = asc

        # Lazy-store original heading text (strip any old arrow first)
        for c in tv["columns"]:
            if c not in _orig_text:
                raw = tv.heading(c, "text")
                _orig_text[c] = raw.rstrip(" ▲▼")

        # Collect (sort_key, iid) pairs
        rows = []
        for iid in tv.get_children(""):
            val = tv.set(iid, col)
            rows.append((val, iid))

        # Determine sort key — numeric or string
        force_num = numeric_cols and col in numeric_cols
        if force_num:
            def key_fn(pair):
                try:
                    return float(pair[0]) if pair[0] not in ("", "—") else float("-inf")
                except (ValueError, TypeError):
                    return float("-inf")
        else:
            def key_fn(pair):
                try:
                    return (0, float(pair[0]) if pair[0] not in ("", "—") else float("-inf"))
                except (ValueError, TypeError):
                    return (1, pair[0].lower())

        rows.sort(key=key_fn, reverse=not asc)

        for idx, (_, iid) in enumerate(rows):
            tv.move(iid, "", idx)

        # Update headers: clear arrow from all, set on active column
        for c in tv["columns"]:
            base = _orig_text.get(c, tv.heading(c, "text").rstrip(" ▲▼"))
            arrow = (" ▲" if asc else " ▼") if c == col else ""
            tv.heading(c, text=base + arrow)

    for col in tv["columns"]:
        tv.heading(col, command=lambda c=col: _sort(c))

def entry_w(parent, var, row, col, width=10, **kw):
    widget_kwargs = {}
    for key in ("state",):
        if key in kw:
            widget_kwargs[key] = kw.pop(key)
    e = ttk.Entry(parent, textvariable=var, width=width, **widget_kwargs)
    e.grid(row=row, column=col, sticky="w", padx=4, pady=2, **kw)
    return e

def combo_w(parent, var, values, row, col, width=14, **kw):
    c = ttk.Combobox(parent, textvariable=var, values=values,
                     state="readonly", width=width)
    c.grid(row=row, column=col, sticky="w", padx=4, pady=2, **kw)
    return c

def check_w(parent, var, text, row, col, **kw):
    cb = ttk.Checkbutton(parent, text=text, variable=var)
    cb.grid(row=row, column=col, sticky="w", padx=4, pady=2, **kw)
    return cb

def money_text(copper) -> str:
    try:
        total = int(copper or 0)
    except (TypeError, ValueError):
        total = 0
    gold = total // 10000
    silver = (total % 10000) // 100
    copper_only = total % 100
    return f"{gold}g {silver}s {copper_only}c"

def unix_text(value) -> str:
    try:
        ts = int(value or 0)
    except (TypeError, ValueError):
        return ""
    if ts <= 0:
        return ""
    try:
        return datetime.fromtimestamp(ts).strftime("%Y-%m-%d %H:%M:%S")
    except Exception:
        return str(ts)

def _is_noise_item_name(name: str) -> bool:
    lowered = (name or "").strip().lower()
    return (
        not lowered or
        lowered.startswith("zzold") or
        lowered.startswith("deprecated") or
        lowered.startswith("test") or
        lowered.startswith("old")
    )

def item_quality_filter_id(label: str):
    return ITEM_QUALITY_LABEL_TO_ID.get((label or "").strip(), None)

def slot_enchant_category(slot_id: int) -> str | None:
    return SLOT_ENCHANT_CATEGORY.get(int(slot_id))

def filter_enchant_rows_for_slot(slot_id: int, rows: list) -> list:
    category = slot_enchant_category(slot_id)

    # Strip noise entries regardless of slot.
    cleaned = []
    for row in rows:
        name = (row.get("Name_Lang_enUS") or "").strip()
        if not name:
            continue
        lowered = name.lower()
        if any(word in lowered for word in ("test", "deprecated", "qa ", "zzold")):
            continue
        cleaned.append(row)

    if not category:
        return cleaned

    spell_keywords = SLOT_SPELL_NAME_KEYWORDS.get(category, [])
    stat_keywords  = ENCHANT_CATEGORY_KEYWORDS.get(category, [])

    # Pass 1 — filter by SpellName when available (most accurate).
    if spell_keywords:
        by_spell = []
        for row in cleaned:
            spell = (row.get("SpellName") or "").strip().lower()
            if spell and any(kw in spell for kw in spell_keywords):
                by_spell.append(row)
        if by_spell:
            return by_spell

    # Pass 2 — fall back to stat text keyword matching.
    if stat_keywords:
        by_stat = []
        for row in cleaned:
            stat = (row.get("Name_Lang_enUS") or "").strip().lower()
            if any(kw in stat for kw in stat_keywords):
                by_stat.append(row)
        if by_stat:
            return by_stat

    return cleaned

def parse_display_id(text: str):
    s = (text or "").strip()
    if not s:
        return None
    if s.endswith("]") and "[" in s:
        try:
            return int(s.rsplit("[", 1)[-1].rstrip("]").strip())
        except ValueError:
            pass
    if s.startswith("#"):
        try:
            return int(s[1:].strip())
        except ValueError:
            pass
    if s.isdigit():
        return int(s)
    return None


def format_item_tooltip(row: dict, enchant_cache: list | None = None,
                        spell_name_fn=None) -> str:
    """
    Build a WoW-style item tooltip string from an item_template row
    returned by DBCtx.item_tooltip_data().

    spell_name_fn(spell_id) -> str  optional callable for resolving built-in
    spell names (pass db.spell_name to get human-readable Equip/Use lines).
    """
    if not row:
        return ""

    lines = []

    _DAMAGE_TYPES = {1: "Holy", 2: "Fire", 3: "Nature", 4: "Frost", 5: "Shadow", 6: "Arcane"}
    _TRIGGER_LABELS = {0: "Use", 1: "Equip", 2: "Chance on Hit", 4: "Use", 6: "Equip"}

    # ── Name ──────────────────────────────────────────────────────────────
    name    = (row.get("name") or "").strip()
    quality = int(row.get("Quality") or 0)
    q_label = ITEM_QUALITY_COLORS.get(quality, "")
    lines.append(f"{name}  [{q_label}]" if q_label else name)

    # ── Item level ─────────────────────────────────────────────────────────
    ilvl = int(row.get("ItemLevel") or 0)
    if ilvl:
        lines.append(f"Item Level {ilvl}")

    # ── Type / slot line ───────────────────────────────────────────────────
    item_class    = int(row.get("item_class") or 0)
    item_subclass = int(row.get("item_subclass") or 0)
    inv_type      = int(row.get("InventoryType") or 0)
    type_label    = ITEM_TYPE_NAMES.get((item_class, item_subclass), "")
    slot_label    = INVENTORY_TYPE_NAMES.get(inv_type, "")
    if type_label and slot_label:
        lines.append(f"{type_label:<22}{slot_label}")
    elif type_label or slot_label:
        lines.append(type_label or slot_label)

    # ── Weapon damage ──────────────────────────────────────────────────────
    delay    = int(row.get("delay") or 0)
    dmg_min1 = float(row.get("dmg_min1") or 0)
    dmg_max1 = float(row.get("dmg_max1") or 0)
    dmg_min2 = float(row.get("dmg_min2") or 0)
    dmg_max2 = float(row.get("dmg_max2") or 0)
    if delay and dmg_max1:
        speed_s = delay / 1000.0
        dps1    = (dmg_min1 + dmg_max1) / 2.0 / speed_s
        dtype1  = _DAMAGE_TYPES.get(int(row.get("dmg_type1") or 0), "")
        dmg_label1 = f"{dmg_min1:.0f} - {dmg_max1:.0f}"
        if dtype1:
            dmg_label1 += f" {dtype1}"
        lines.append(f"{dmg_label1} Damage")
        if dmg_max2:
            dtype2 = _DAMAGE_TYPES.get(int(row.get("dmg_type2") or 0), "")
            dmg_label2 = f"+{dmg_min2:.0f} - {dmg_max2:.0f}"
            if dtype2:
                dmg_label2 += f" {dtype2}"
            lines.append(f"{dmg_label2} Damage")
        lines.append(f"Speed {speed_s:.2f}  ({dps1:.1f} damage per second)")

    # ── Armor / block ──────────────────────────────────────────────────────
    armor = int(row.get("armor") or 0)
    block = int(row.get("block") or 0)
    if armor:
        lines.append(f"{armor} Armor")
    if block:
        lines.append(f"{block} Block")

    # ── Stats ──────────────────────────────────────────────────────────────
    for n in range(1, 11):
        stype = int(row.get(f"stat_type{n}") or 0)
        sval  = int(row.get(f"stat_value{n}") or 0)
        if stype and sval:
            stat_name = ITEM_MOD_NAMES.get(stype, f"Stat({stype})")
            lines.append(f"+{sval} {stat_name}")

    # ── Sockets ────────────────────────────────────────────────────────────
    socket_names = {1: "Meta", 2: "Red", 4: "Yellow", 8: "Blue", 16: "Prismatic"}
    for n in (1, 2, 3):
        color_mask = int(row.get(f"socketColor_{n}") or 0)
        if color_mask:
            lines.append(f"[{socket_names.get(color_mask, 'Socket')} Socket]")
    socket_bonus = int(row.get("socketBonus") or 0)
    if socket_bonus and enchant_cache:
        bonus_entry = next((e for e in enchant_cache if int(e.get("ID", 0)) == socket_bonus), None)
        if bonus_entry:
            effects = bonus_entry.get("Effects") or []
            bonus_text = "  •  ".join(effects) if effects else bonus_entry.get("Name_Lang_enUS", "")
            if bonus_text:
                lines.append(f"Socket Bonus: {bonus_text}")
    elif socket_bonus:
        lines.append(f"Socket Bonus: Enchant {socket_bonus}")

    # ── Built-in spell effects (Equip / Use / Chance on Hit) ───────────────
    for n in range(1, 6):
        spell_id  = int(row.get(f"spellid_{n}") or 0)
        trigger   = int(row.get(f"spelltrigger_{n}") or 0)
        if not spell_id:
            continue
        label = _TRIGGER_LABELS.get(trigger, "Equip")
        sname = spell_name_fn(spell_id) if spell_name_fn else ""
        if sname:
            lines.append(f"{label}: {sname}")

    # ── Requirements ───────────────────────────────────────────────────────
    req_level = int(row.get("RequiredLevel") or 0)
    if req_level:
        lines.append(f"Requires Level {req_level}")

    allowable = int(row.get("AllowableClass") or -1)
    if allowable != -1 and allowable != 0:
        class_names = [cname for cid, cname in WOW_CLASSES.items()
                       if allowable & (1 << (cid - 1))]
        if class_names:
            lines.append("Classes: " + ", ".join(class_names))

    return "\n".join(lines)


def format_gem_tooltip(gem_row: dict, enchant_cache: list) -> str:
    """
    Build a tooltip for a gem item.
    gem_row must have 'name', 'gem_color', 'Enchant_Id' (from search_gem_items result).
    """
    if not gem_row:
        return ""

    name = (gem_row.get("name") or "").strip()
    color_mask = int(gem_row.get("gem_color") or 0)
    enchant_id = int(gem_row.get("Enchant_Id") or 0)

    socket_names = {1: "Meta", 2: "Red", 4: "Yellow", 8: "Blue", 16: "Prismatic"}
    color_label = socket_names.get(color_mask, "")

    lines = [name]
    if color_label:
        lines.append(f"Fits: {color_label} Socket")

    if enchant_id and enchant_cache:
        entry = next((e for e in enchant_cache if int(e.get("ID", 0)) == enchant_id), None)
        if entry:
            effects = entry.get("Effects") or []
            if effects:
                lines.append("Bonus:  " + "  •  ".join(effects))
            elif entry.get("Name_Lang_enUS"):
                lines.append(f"Bonus: {entry['Name_Lang_enUS']}")

    return "\n".join(lines)


def item_display_text(item_id, name: str = "") -> str:
    if not item_id:
        return ""
    try:
        iid = int(item_id)
    except (TypeError, ValueError):
        return str(item_id)
    return f"{name or f'Item {iid}'} [{iid}]"

def enchant_display_text(enchant_id, name: str = "", spell_name: str = "") -> str:
    if not enchant_id:
        return ""
    try:
        eid = int(enchant_id)
    except (TypeError, ValueError):
        return str(enchant_id)
    stat = name or f"Enchant {eid}"
    if spell_name and spell_name.lower() != stat.lower():
        return f"{spell_name}  [{stat}]  [{eid}]"
    return f"{stat} [{eid}]"

def socket_color_text(mask) -> str:
    try:
        value = int(mask or 0)
    except (TypeError, ValueError):
        value = 0
    if value in SOCKET_COLOR_NAMES:
        return SOCKET_COLOR_NAMES[value]
    parts = [name for bit, name in SOCKET_COLOR_NAMES.items() if bit and (value & bit)]
    return "/".join(parts) if parts else str(value)

def parse_item_enchantments(text: str) -> list[int]:
    values = []
    for part in (text or "").split():
        try:
            values.append(int(part))
        except ValueError:
            values.append(0)
    target_len = ITEM_ENCHANTMENT_SLOT_COUNT * ITEM_ENCHANTMENT_OFFSET_COUNT
    if len(values) < target_len:
        values.extend([0] * (target_len - len(values)))
    return values[:target_len]

def build_item_enchantments(values: list[int]) -> str:
    target_len = ITEM_ENCHANTMENT_SLOT_COUNT * ITEM_ENCHANTMENT_OFFSET_COUNT
    padded = [int(v or 0) for v in list(values[:target_len])]
    if len(padded) < target_len:
        padded.extend([0] * (target_len - len(padded)))
    return " ".join(str(v) for v in padded)

def enchant_value_index(slot: int, offset: int = 0) -> int:
    return slot * ITEM_ENCHANTMENT_OFFSET_COUNT + offset

def get_item_enchant_id(values: list[int], slot: int) -> int:
    idx = enchant_value_index(slot, 0)
    return int(values[idx] or 0) if idx < len(values) else 0

def set_item_enchant_id(values: list[int], slot: int, enchant_id: int):
    base = enchant_value_index(slot, 0)
    if base + 2 >= len(values):
        return
    values[base] = int(enchant_id or 0)
    values[base + 1] = 0
    values[base + 2] = 0


