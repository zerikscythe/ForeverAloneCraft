"""
tab_bot_ledger.py -- Embedded world-bot identity ledger browser.

Shows the non-account bot catalog backed by living_world_bot_identity, with
human-readable names, sorting, filters, jump actions, and recent activity.
"""
from __future__ import annotations

import pathlib
import re
import subprocess
import sys
import tkinter as tk
from tkinter import ttk, messagebox

from mysql.connector import Error as MySQLError

from .constants import WOW_CLASSES, WOW_RACES
from .db import db


FACTION_LABELS = {
    1: "Alliance",
    2: "Horde",
}
STATUS_OPTIONS = ["All", "Active", "Available", "Retired"]
FACTION_OPTIONS = ["All", "Alliance", "Horde"]
PROFESSION_OPTIONS = ["All", "Any Profession", "No Profession", "Herbalism", "Mining", "Fishing"]
_CAMEL_WORD_RE = re.compile(r"(?<=[a-z0-9])(?=[A-Z])")


def _bot_status(row: dict) -> str:
    if int(row.get("is_retired") or 0) != 0:
        return "Retired"
    if int(row.get("is_available") or 0) == 0:
        return "Active"
    return "Available"


def _fmt_dt(value) -> str:
    return "" if value is None else str(value)


def _fmt_pos(row: dict) -> str:
    x = row.get("latest_pos_x")
    y = row.get("latest_pos_y")
    z = row.get("latest_pos_z")
    if x is None or y is None or z is None:
        return ""
    return f"({float(x):.1f}, {float(y):.1f}, {float(z):.1f})"


def _humanize_identifier(text: str | None) -> str:
    raw = (text or "").strip()
    if not raw:
        return ""
    raw = raw.replace("-", " ").replace("_", " ")
    raw = _CAMEL_WORD_RE.sub(" ", raw)
    words = []
    for word in raw.split():
        if word.upper() in {"AI", "BG", "PVP", "OOC"}:
            words.append(word.upper())
        elif word.isupper():
            words.append(word)
        else:
            words.append(word.capitalize())
    return " ".join(words)


def _lookup_label(lookup: dict[int, str], raw_id) -> str:
    try:
        key = int(raw_id)
    except (TypeError, ValueError):
        return ""
    return lookup.get(key, str(raw_id))


def _label_with_id(name: str, raw_id) -> str:
    if raw_id is None or raw_id == "":
        return name or ""
    if name:
        return f"{name} [{raw_id}]"
    return str(raw_id)


def _profession_slots(row: dict) -> list[str]:
    profs: list[str] = []
    if int(row.get("has_herbalism") or 0):
        profs.append("Herbalism")
    if int(row.get("has_mining") or 0):
        profs.append("Mining")
    if int(row.get("has_fishing") or 0):
        profs.append("Fishing")
    while len(profs) < 3:
        profs.append("")
    return profs[:3]


def _current_zone_id(row: dict):
    if row.get("latest_zone_id") is not None:
        return row.get("latest_zone_id")
    return row.get("last_seen_zone")


def _spec_label(row: dict) -> str:
    spec = (row.get("spec_key") or "").strip()
    if not spec:
        return ""
    class_name = WOW_CLASSES.get(int(row.get("class_id") or 0), "")
    lower = spec.lower()
    prefixes = [
        class_name.lower().replace(" ", "_"),
        class_name.lower().replace(" ", ""),
    ]
    for prefix in prefixes:
        if prefix and lower.startswith(prefix + "_"):
            spec = spec[len(prefix) + 1:]
            break
    return _humanize_identifier(spec)


def _event_label(event_type: str | None) -> str:
    return _humanize_identifier(event_type)


def _session_value_label(value: str | None) -> str:
    raw = (value or "").strip()
    if not raw:
        return ""
    human = _humanize_identifier(raw)
    if not human or human.lower() == raw.lower():
        return raw
    return f"{human} [{raw}]"


def _parse_session_meta(detail: str | None) -> dict[str, str]:
    text = detail or ""
    result = {
        "source_kind": "",
        "source_key": "",
        "session": "",
        "tasks": "",
        "steps": "",
    }
    for key in ("source_kind", "source_key", "session"):
        m = re.search(rf"{key}='([^']*)'", text)
        if m:
            result[key] = m.group(1)
    for key in ("tasks", "steps"):
        m = re.search(rf"{key}=(\d+)", text)
        if m:
            result[key] = m.group(1)
    return result


class BotLedgerTab(ttk.Frame):
    def __init__(self, parent, **kw):
        super().__init__(parent, **kw)
        self._rows_by_id: dict[int, dict] = {}
        self._selected_bot_id: int | None = None
        self._sort_column = "name"
        self._sort_reverse = False

        self.v_name_filter = tk.StringVar(value="")
        self.v_status_filter = tk.StringVar(value="All")
        self.v_faction_filter = tk.StringVar(value="All")
        self.v_race_filter = tk.StringVar(value="All")
        self.v_class_filter = tk.StringVar(value="All")
        self.v_spec_filter = tk.StringVar(value="All")
        self.v_profession_filter = tk.StringVar(value="All")
        self.v_source_filter = tk.StringVar(value="All")
        self.v_summary = tk.StringVar(value="Bots: 0")

        self._build()

    def _build(self):
        ttk.Label(self, text=(
            "Persistent ledger of non-account world bots. This is the historical catalog of creature-based bots, "
            "with readable names for race, class, spec, faction, map, and zone while still using backend IDs internally."
        ), wraplength=1080, justify=tk.LEFT).pack(anchor="w", padx=8, pady=(8, 4))

        filter_row = ttk.Frame(self)
        filter_row.pack(fill=tk.X, padx=8, pady=(0, 6))

        ttk.Label(filter_row, text="Name:").pack(side=tk.LEFT)
        ent = ttk.Entry(filter_row, width=22, textvariable=self.v_name_filter)
        ent.pack(side=tk.LEFT, padx=(4, 10))
        ent.bind("<Return>", lambda _e: self.refresh())

        ttk.Label(filter_row, text="Status:").pack(side=tk.LEFT)
        ttk.Combobox(filter_row, textvariable=self.v_status_filter, values=STATUS_OPTIONS,
                     state="readonly", width=10).pack(side=tk.LEFT, padx=(4, 10))
        self.v_status_filter.trace_add("write", lambda *_: self.refresh())

        ttk.Label(filter_row, text="Faction:").pack(side=tk.LEFT)
        ttk.Combobox(filter_row, textvariable=self.v_faction_filter, values=FACTION_OPTIONS,
                     state="readonly", width=10).pack(side=tk.LEFT, padx=(4, 10))
        self.v_faction_filter.trace_add("write", lambda *_: self.refresh())

        ttk.Label(filter_row, text="Race:").pack(side=tk.LEFT)
        self._race_cb = ttk.Combobox(filter_row, textvariable=self.v_race_filter, values=["All"],
                                     state="readonly", width=12)
        self._race_cb.pack(side=tk.LEFT, padx=(4, 10))
        self.v_race_filter.trace_add("write", lambda *_: self.refresh())

        ttk.Label(filter_row, text="Class:").pack(side=tk.LEFT)
        self._class_cb = ttk.Combobox(filter_row, textvariable=self.v_class_filter, values=["All"],
                                      state="readonly", width=12)
        self._class_cb.pack(side=tk.LEFT, padx=(4, 10))
        self.v_class_filter.trace_add("write", lambda *_: self.refresh())

        filter_row2 = ttk.Frame(self)
        filter_row2.pack(fill=tk.X, padx=8, pady=(0, 6))

        ttk.Label(filter_row2, text="Spec:").pack(side=tk.LEFT)
        self._spec_cb = ttk.Combobox(filter_row2, textvariable=self.v_spec_filter, values=["All"],
                                     state="readonly", width=20)
        self._spec_cb.pack(side=tk.LEFT, padx=(4, 10))
        self.v_spec_filter.trace_add("write", lambda *_: self.refresh())

        ttk.Label(filter_row2, text="Profession:").pack(side=tk.LEFT)
        ttk.Combobox(filter_row2, textvariable=self.v_profession_filter, values=PROFESSION_OPTIONS,
                     state="readonly", width=14).pack(side=tk.LEFT, padx=(4, 10))
        self.v_profession_filter.trace_add("write", lambda *_: self.refresh())

        ttk.Label(filter_row2, text="Source:").pack(side=tk.LEFT)
        self._source_cb = ttk.Combobox(filter_row2, textvariable=self.v_source_filter, values=["All"],
                                       state="readonly", width=16)
        self._source_cb.pack(side=tk.LEFT, padx=(4, 10))
        self.v_source_filter.trace_add("write", lambda *_: self.refresh())

        ttk.Button(filter_row2, text="Refresh", command=self.refresh).pack(side=tk.LEFT)
        ttk.Button(filter_row2, text="Clear Filters", command=self._clear_filters).pack(side=tk.LEFT, padx=(6, 0))
        ttk.Button(filter_row2, text="Open Realtime Viewer", command=self._open_standalone_viewer).pack(side=tk.LEFT, padx=(12, 0))
        ttk.Label(filter_row2, textvariable=self.v_summary).pack(side=tk.LEFT, padx=(12, 0))

        pane = ttk.PanedWindow(self, orient=tk.HORIZONTAL)
        pane.pack(fill=tk.BOTH, expand=True, padx=8, pady=(0, 8))

        left = ttk.Frame(pane)
        pane.add(left, weight=3)

        cols = (
            "id", "name", "race", "class", "spec", "level", "faction",
            "prof1", "prof2", "prof3", "status", "zone", "source", "event", "sessions",
        )
        self._tv = ttk.Treeview(left, columns=cols, show="headings")
        headings = {
            "id": "ID",
            "name": "Name",
            "race": "Race",
            "class": "Class",
            "spec": "Spec",
            "level": "Lvl",
            "faction": "Faction",
            "prof1": "Prof 1",
            "prof2": "Prof 2",
            "prof3": "Prof 3",
            "status": "Status",
            "zone": "Current / Last Zone",
            "source": "Source",
            "event": "Latest Event",
            "sessions": "Sessions",
        }
        widths = {
            "id": 55,
            "name": 140,
            "race": 100,
            "class": 100,
            "spec": 120,
            "level": 50,
            "faction": 80,
            "prof1": 90,
            "prof2": 90,
            "prof3": 90,
            "status": 80,
            "zone": 170,
            "source": 120,
            "event": 120,
            "sessions": 70,
        }
        for col in cols:
            self._tv.heading(col, text=headings[col], command=lambda c=col: self._sort_by(c))
            self._tv.column(col, width=widths[col], anchor="w")
        self._tv.tag_configure("retired", background="#f3f3f3", foreground="#777")
        self._tv.tag_configure("available", background="#eef8ee", foreground="#1f6b1f")
        self._tv.tag_configure("active", background="#fff7df", foreground="#6b4f00")
        self._tv.pack(fill=tk.BOTH, expand=True)
        self._tv.bind("<<TreeviewSelect>>", self._on_select)

        right = ttk.Frame(pane)
        pane.add(right, weight=2)

        tabs = ttk.Notebook(right)
        tabs.pack(fill=tk.BOTH, expand=True)

        state_tab = ttk.Frame(tabs)
        tabs.add(state_tab, text="  Identity & State  ")
        ttk.Label(state_tab, text="Identity + current state + historical ledger summary").pack(anchor="w", padx=4, pady=(4, 2))
        self._detail = tk.Text(state_tab, height=18, wrap="word")
        self._detail.pack(fill=tk.BOTH, expand=True, padx=4, pady=(0, 4))

        activity_tab = ttk.Frame(tabs)
        tabs.add(activity_tab, text="  Recent Activity  ")
        act_cols = ("time", "event", "map", "zone", "pos", "detail")
        self._activity = ttk.Treeview(activity_tab, columns=act_cols, show="headings", height=16)
        act_headings = {
            "time": "Time",
            "event": "Event",
            "map": "Map",
            "zone": "Zone",
            "pos": "Position",
            "detail": "Detail",
        }
        act_widths = {"time": 155, "event": 110, "map": 130, "zone": 160, "pos": 150, "detail": 320}
        for col in act_cols:
            self._activity.heading(col, text=act_headings[col])
            self._activity.column(col, width=act_widths[col], anchor="w")
        self._activity.pack(fill=tk.BOTH, expand=True, padx=4, pady=4)

    def _clear_filters(self):
        self.v_name_filter.set("")
        self.v_status_filter.set("All")
        self.v_faction_filter.set("All")
        self.v_race_filter.set("All")
        self.v_class_filter.set("All")
        self.v_spec_filter.set("All")
        self.v_profession_filter.set("All")
        self.v_source_filter.set("All")
        self.refresh()

    def _open_standalone_viewer(self):
        viewer_path = pathlib.Path(__file__).resolve().parent.parent / "lw_bot_viewer.py"
        try:
            subprocess.Popen([sys.executable, str(viewer_path)])
        except Exception as exc:
            messagebox.showerror("Open Viewer", str(exc))

    def _zone_label(self, zone_id, with_id: bool = False) -> str:
        name = db.area_name(zone_id)
        return _label_with_id(name, zone_id) if with_id else (name or (str(zone_id) if zone_id else ""))

    def _map_label(self, map_id, with_id: bool = False) -> str:
        name = db.map_name(map_id)
        return _label_with_id(name, map_id) if with_id else (name or (str(map_id) if map_id is not None else ""))

    def _race_label(self, row: dict, with_id: bool = False) -> str:
        raw = row.get("race_id")
        name = _lookup_label(WOW_RACES, raw)
        return _label_with_id(name, raw) if with_id else name

    def _class_label(self, row: dict, with_id: bool = False) -> str:
        raw = row.get("class_id")
        name = _lookup_label(WOW_CLASSES, raw)
        return _label_with_id(name, raw) if with_id else name

    def _faction_label(self, row: dict, with_id: bool = False) -> str:
        raw = row.get("faction")
        try:
            raw_int = int(raw)
        except (TypeError, ValueError):
            raw_int = 0
        name = FACTION_LABELS.get(raw_int, str(raw) if raw is not None else "")
        return _label_with_id(name, raw) if with_id else name

    def _make_view(self, row: dict) -> dict:
        prof1, prof2, prof3 = _profession_slots(row)
        zone_id = _current_zone_id(row)
        session_meta = _parse_session_meta(row.get("session_start_detail"))
        source_label = _humanize_identifier(session_meta.get("source_kind")) or "Legacy Activity"
        view = {
            "id": int(row.get("id") or 0),
            "name": row.get("name", "") or "",
            "race": self._race_label(row),
            "class": self._class_label(row),
            "spec": _spec_label(row),
            "level": int(row.get("level") or 0),
            "faction": self._faction_label(row),
            "prof1": prof1,
            "prof2": prof2,
            "prof3": prof3,
            "status": _bot_status(row),
            "zone": self._zone_label(zone_id),
            "source": source_label,
            "event": _event_label(row.get("latest_event_type", "")),
            "sessions": int(row.get("session_count") or 0),
            "_raw": row,
        }
        view["_sort"] = {
            "id": view["id"],
            "name": view["name"].lower(),
            "race": view["race"].lower(),
            "class": view["class"].lower(),
            "spec": view["spec"].lower(),
            "level": view["level"],
            "faction": view["faction"].lower(),
            "prof1": view["prof1"].lower(),
            "prof2": view["prof2"].lower(),
            "prof3": view["prof3"].lower(),
            "status": view["status"].lower(),
            "zone": view["zone"].lower(),
            "source": view["source"].lower(),
            "event": view["event"].lower(),
            "sessions": view["sessions"],
        }
        return view

    def _sync_dynamic_filters(self, rows: list[dict]):
        races = ["All"] + sorted({self._race_label(r) for r in rows if self._race_label(r)})
        classes = ["All"] + sorted({self._class_label(r) for r in rows if self._class_label(r)})
        specs = ["All"] + sorted({_spec_label(r) for r in rows if _spec_label(r)})
        sources = ["All"] + sorted({self._make_view(r)["source"] for r in rows if self._make_view(r)["source"]})

        self._race_cb.configure(values=races)
        self._class_cb.configure(values=classes)
        self._spec_cb.configure(values=specs)
        self._source_cb.configure(values=sources)

        for var, options in [
            (self.v_race_filter, races),
            (self.v_class_filter, classes),
            (self.v_spec_filter, specs),
            (self.v_source_filter, sources),
        ]:
            if var.get() not in options:
                var.set("All")

    def _apply_filters(self, rows: list[dict]) -> list[dict]:
        name_filter = self.v_name_filter.get().strip().lower()
        status_filter = self.v_status_filter.get()
        faction_filter = self.v_faction_filter.get()
        race_filter = self.v_race_filter.get()
        class_filter = self.v_class_filter.get()
        spec_filter = self.v_spec_filter.get()
        profession_filter = self.v_profession_filter.get()
        source_filter = self.v_source_filter.get()

        filtered = []
        for row in rows:
            view = self._make_view(row)
            if name_filter and name_filter not in str(row.get("name", "")).lower():
                continue
            if status_filter != "All" and _bot_status(row) != status_filter:
                continue
            if faction_filter != "All" and view["faction"] != faction_filter:
                continue
            if race_filter != "All" and view["race"] != race_filter:
                continue
            if class_filter != "All" and view["class"] != class_filter:
                continue
            if spec_filter != "All" and view["spec"] != spec_filter:
                continue
            if source_filter != "All" and view["source"] != source_filter:
                continue
            profs = {p for p in _profession_slots(row) if p}
            if profession_filter == "Any Profession" and not profs:
                continue
            if profession_filter == "No Profession" and profs:
                continue
            if profession_filter not in {"All", "Any Profession", "No Profession"} and profession_filter not in profs:
                continue
            filtered.append(row)
        return filtered

    def _sort_by(self, column: str):
        if self._sort_column == column:
            self._sort_reverse = not self._sort_reverse
        else:
            self._sort_column = column
            self._sort_reverse = False
        self.refresh()

    def refresh(self):
        if not db.ok():
            return
        try:
            all_rows = db.load_world_bot_statuses(active_only=False)
        except MySQLError as e:
            messagebox.showerror("DB error", str(e))
            return

        self._sync_dynamic_filters(all_rows)
        rows = self._apply_filters(all_rows)
        views = [self._make_view(r) for r in rows]
        views.sort(key=lambda v: v["_sort"].get(self._sort_column, ""), reverse=self._sort_reverse)

        self._rows_by_id = {int(v["id"]): v["_raw"] for v in views}
        active = sum(1 for r in rows if _bot_status(r) == "Active")
        available = sum(1 for r in rows if _bot_status(r) == "Available")
        retired = sum(1 for r in rows if _bot_status(r) == "Retired")
        self.v_summary.set(
            f"Showing {len(rows)} / {len(all_rows)}   active={active}   available={available}   retired={retired}")

        current_selection = self._selected_bot_id
        self._tv.delete(*self._tv.get_children())
        for view in views:
            status = view["status"]
            tag = status.lower()
            self._tv.insert("", "end", iid=str(view["id"]), values=(
                view["id"],
                view["name"],
                view["race"],
                view["class"],
                view["spec"],
                view["level"],
                view["faction"],
                view["prof1"],
                view["prof2"],
                view["prof3"],
                status,
                view["zone"],
                view["source"],
                view["event"],
                view["sessions"],
            ), tags=(tag,))

        if current_selection and current_selection in self._rows_by_id:
            self._tv.selection_set(str(current_selection))
            self._tv.focus(str(current_selection))
            self._show_details(current_selection)
        elif views:
            first_id = int(views[0]["id"])
            self._tv.selection_set(str(first_id))
            self._tv.focus(str(first_id))
            self._show_details(first_id)
        else:
            self._selected_bot_id = None
            self._detail.delete("1.0", tk.END)
            self._activity.delete(*self._activity.get_children())

    def _on_select(self, _event=None):
        sel = self._tv.selection()
        if not sel:
            return
        self._show_details(int(sel[0]))

    def _show_details(self, bot_id: int):
        row = self._rows_by_id.get(bot_id)
        if not row:
            return
        self._selected_bot_id = bot_id

        prof1, prof2, prof3 = _profession_slots(row)
        professions = ", ".join([p for p in (prof1, prof2, prof3) if p]) or "None"
        zone_id = _current_zone_id(row)
        session_meta = _parse_session_meta(row.get("session_start_detail"))
        lines = [
            "=== Identity ===",
            f"ID: {row.get('id')}",
            f"Name: {row.get('name', '')}",
            f"Race: {self._race_label(row, with_id=True)}",
            f"Class: {self._class_label(row, with_id=True)}",
            f"Spec: {_spec_label(row)}",
            f"Personality: {_humanize_identifier(row.get('personality_key', 'uninterested'))}",
            f"Faction: {self._faction_label(row, with_id=True)}",
            f"Level: {row.get('level', '')}",
            f"Gear Tier: {row.get('gear_tier', '')}",
            f"Professions: {professions}",
            "",
            "=== Current / Latest State ===",
            f"Status: {_bot_status(row)}",
            f"Current / Last Zone: {self._zone_label(zone_id, with_id=True)}",
            f"Latest Map: {self._map_label(row.get('latest_map_id'), with_id=True)}",
            f"Latest Event: {_event_label(row.get('latest_event_type', ''))}",
            f"Latest Event Time: {_fmt_dt(row.get('latest_logged_at'))}",
            f"Latest Position: {_fmt_pos(row)}",
            f"Latest Detail: {row.get('latest_detail', '')}",
            f"Current / Last Session Source: {_humanize_identifier(session_meta.get('source_kind'))}",
            f"Current / Last Session Key: {_session_value_label(session_meta.get('source_key'))}",
            f"Current / Last Session Name: {_session_value_label(session_meta.get('session'))}",
            f"Tasks / Steps: {session_meta.get('tasks', '')}/{session_meta.get('steps', '')}",
            "",
            "=== Historical Ledger ===",
            f"Session Count: {row.get('session_count', 0)}",
            f"Active Session Ms: {row.get('active_world_session_ms', 0)}",
            f"Active Session Start: {_fmt_dt(row.get('active_world_session_start'))}",
            f"Total World Online Ms: {row.get('total_world_online_ms', 0)}",
            f"World Online Since Level Ms: {row.get('world_online_ms_since_level', 0)}",
            f"Post-Max World Online Ms: {row.get('post_max_world_online_ms', 0)}",
            f"Last Seen Zone: {self._zone_label(row.get('last_seen_zone'), with_id=True)}",
            f"Last Seen At: {_fmt_dt(row.get('last_seen_at'))}",
            f"Retired At: {_fmt_dt(row.get('retired_at'))}",
        ]
        self._detail.delete("1.0", tk.END)
        self._detail.insert("1.0", "\n".join(lines))
        self._refresh_activity(bot_id)

    def _refresh_activity(self, bot_id: int):
        rows = db.load_world_bot_activity_log(bot_id, limit=100)
        self._activity.delete(*self._activity.get_children())
        for row in rows:
            pos = ""
            if row.get("pos_x") is not None and row.get("pos_y") is not None and row.get("pos_z") is not None:
                pos = f"({float(row['pos_x']):.1f}, {float(row['pos_y']):.1f}, {float(row['pos_z']):.1f})"
            self._activity.insert("", "end", values=(
                _fmt_dt(row.get("logged_at")),
                _event_label(row.get("event_type", "")),
                self._map_label(row.get("map_id")),
                self._zone_label(row.get("zone_id")),
                pos,
                row.get("detail", ""),
            ))