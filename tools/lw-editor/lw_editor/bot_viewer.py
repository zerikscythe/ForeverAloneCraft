"""
bot_viewer.py -- Dedicated realtime world-bot viewer.
"""
import configparser
import pathlib
import re
import tkinter as tk
from tkinter import ttk, messagebox

from mysql.connector import Error as MySQLError

from .db import db


CONFIG_FILE = pathlib.Path(__file__).resolve().parent.parent / "config.ini"


def _parse_session_meta(detail: str | None) -> dict:
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


def _bot_status(row: dict) -> str:
    if int(row.get("is_retired") or 0) != 0:
        return "retired"
    if int(row.get("is_available") or 0) == 0:
        return "active"
    return "available"


def _fmt_dt(value) -> str:
    return "" if value is None else str(value)


def _fmt_pos(row: dict) -> str:
    x = row.get("latest_pos_x")
    y = row.get("latest_pos_y")
    z = row.get("latest_pos_z")
    if x is None or y is None or z is None:
        return ""
    return f"({float(x):.1f}, {float(y):.1f}, {float(z):.1f})"


def _parse_int_filter(text: str) -> int | None:
    raw = (text or "").strip()
    if not raw:
        return None
    try:
        return int(raw)
    except ValueError:
        return None


def _effective_zone(row: dict):
    if row.get("latest_zone_id") is not None:
        return row.get("latest_zone_id")
    return row.get("last_seen_zone")


def _matches_filters(row: dict, map_filter: int | None, zone_filter: int | None) -> bool:
    if map_filter is not None:
        if int(row.get("latest_map_id") or -1) != map_filter:
            return False
    if zone_filter is not None:
        if int(_effective_zone(row) or -1) != zone_filter:
            return False
    return True


def _bot_badge(row: dict) -> str:
    status = _bot_status(row)
    if status == "retired":
        return "RETIRED"
    if status == "available":
        return "AVAILABLE"

    event_type = (row.get("latest_event_type") or "").strip().lower()
    if event_type in {"travel_start", "travel_arrive", "travel_taxi_start", "travel_taxi_arrive", "travel_teleport"}:
        return "TRAVEL"
    if event_type in {"activity_start", "activity_tick", "activity_complete", "task_start", "task_complete"}:
        return "TASK"
    if event_type in {"session_start", "ai_assigned"}:
        return "SESSION"
    if event_type in {"session_complete", "despawn"}:
        return "COMPLETE"
    if event_type == "status_change":
        return "STATUS"
    return "ACTIVE"


def _badge_tag(row: dict) -> str:
    badge = _bot_badge(row)
    if badge == "RETIRED":
        return "retired"
    if badge == "AVAILABLE":
        return "available"
    if badge == "TRAVEL":
        return "travel"
    if badge == "TASK":
        return "task"
    if badge == "SESSION":
        return "session"
    if badge == "COMPLETE":
        return "complete"
    if badge == "STATUS":
        return "status"
    return "active"


class BotViewerApp(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("LivingWorld Bot Viewer")
        self.geometry("1500x860")
        self.minsize(1150, 700)
        self._rows_by_id: dict[int, dict] = {}
        self._selected_bot_id: int | None = None
        self._refresh_job = None

        self.v_status = tk.StringVar(value="Not connected")
        self.v_summary = tk.StringVar(value="Bots: 0")
        self.v_active_only = tk.BooleanVar(value=False)
        self.v_auto_refresh = tk.BooleanVar(value=True)
        self.v_refresh_secs = tk.StringVar(value="2")
        self.v_map_filter = tk.StringVar(value="")
        self.v_zone_filter = tk.StringVar(value="")

        self._build_topbar()
        self._build_body()
        self.after(150, self._connect_from_config)

    def _build_topbar(self):
        bar = ttk.Frame(self, padding=(8, 6))
        bar.pack(side=tk.TOP, fill=tk.X)

        ttk.Button(bar, text="Reconnect", command=self._connect_from_config).pack(side=tk.LEFT, padx=(0, 6))
        ttk.Button(bar, text="Refresh Now", command=self.refresh).pack(side=tk.LEFT, padx=(0, 10))
        ttk.Checkbutton(bar, text="Active only", variable=self.v_active_only, command=self.refresh).pack(side=tk.LEFT, padx=(0, 10))
        ttk.Checkbutton(bar, text="Auto refresh", variable=self.v_auto_refresh, command=self._reschedule_refresh).pack(side=tk.LEFT, padx=(0, 6))
        ttk.Label(bar, text="Every (sec):").pack(side=tk.LEFT)
        spin = ttk.Spinbox(bar, from_=1, to=60, width=4, textvariable=self.v_refresh_secs, command=self._reschedule_refresh)
        spin.pack(side=tk.LEFT, padx=(4, 12))

        ttk.Label(bar, text="Map:").pack(side=tk.LEFT)
        ent_map = ttk.Entry(bar, width=6, textvariable=self.v_map_filter)
        ent_map.pack(side=tk.LEFT, padx=(4, 8))
        ent_map.bind("<Return>", lambda _e: self.refresh())

        ttk.Label(bar, text="Zone:").pack(side=tk.LEFT)
        ent_zone = ttk.Entry(bar, width=7, textvariable=self.v_zone_filter)
        ent_zone.pack(side=tk.LEFT, padx=(4, 8))
        ent_zone.bind("<Return>", lambda _e: self.refresh())

        ttk.Button(bar, text="Clear Filters", command=self._clear_filters).pack(side=tk.LEFT, padx=(0, 10))

        ttk.Label(bar, textvariable=self.v_summary).pack(side=tk.LEFT, padx=(8, 0))
        ttk.Label(bar, textvariable=self.v_status, foreground="#1a4f8b").pack(side=tk.RIGHT)

    def _build_body(self):
        pane = ttk.PanedWindow(self, orient=tk.HORIZONTAL)
        pane.pack(fill=tk.BOTH, expand=True, padx=8, pady=(0, 8))

        left = ttk.Frame(pane)
        pane.add(left, weight=3)

        cols = ("id", "name", "status", "level", "faction", "spec", "source_kind", "source_key", "event", "zone", "updated")
        self.tv_bots = ttk.Treeview(left, columns=cols, show="headings")
        headings = {
            "id": "ID", "name": "Name", "status": "Status", "level": "Lvl", "faction": "Fac",
            "spec": "Spec", "source_kind": "Source", "source_key": "Routine/Key",
            "event": "Latest Event", "zone": "Zone", "updated": "Updated"
        }
        widths = {
            "id": 55, "name": 140, "status": 80, "level": 50, "faction": 45,
            "spec": 140, "source_kind": 95, "source_key": 220,
            "event": 120, "zone": 70, "updated": 165,
        }
        for col in cols:
            self.tv_bots.heading(col, text=headings[col])
            self.tv_bots.column(col, width=widths[col], anchor="w")
        self.tv_bots.tag_configure("retired", background="#f3f3f3", foreground="#777777")
        self.tv_bots.tag_configure("available", background="#eef8ee", foreground="#1f6b1f")
        self.tv_bots.tag_configure("active", background="#fff7df", foreground="#6b4f00")
        self.tv_bots.tag_configure("travel", background="#e6f0ff", foreground="#114a9f")
        self.tv_bots.tag_configure("task", background="#ede7ff", foreground="#5a2ca0")
        self.tv_bots.tag_configure("session", background="#fff0e6", foreground="#9a4b00")
        self.tv_bots.tag_configure("complete", background="#e8f8f0", foreground="#0b6b47")
        self.tv_bots.tag_configure("status", background="#f6f0ff", foreground="#5c3d99")
        self.tv_bots.pack(fill=tk.BOTH, expand=True)
        self.tv_bots.bind("<<TreeviewSelect>>", self._on_select_bot)

        right = ttk.Frame(pane)
        pane.add(right, weight=2)

        ttk.Label(right, text="Selected Bot Details").pack(anchor="w", pady=(0, 4))
        self.txt_detail = tk.Text(right, height=16, wrap="word")
        self.txt_detail.pack(fill=tk.X, expand=False)

        ttk.Label(right, text="Recent Activity").pack(anchor="w", pady=(8, 4))
        act_cols = ("time", "event", "map", "zone", "pos", "detail")
        self.tv_activity = ttk.Treeview(right, columns=act_cols, show="headings", height=18)
        act_headings = {
            "time": "Time", "event": "Event", "map": "Map", "zone": "Zone", "pos": "Position", "detail": "Detail"
        }
        act_widths = {"time": 165, "event": 110, "map": 55, "zone": 60, "pos": 170, "detail": 500}
        for col in act_cols:
            self.tv_activity.heading(col, text=act_headings[col])
            self.tv_activity.column(col, width=act_widths[col], anchor="w")
        self.tv_activity.pack(fill=tk.BOTH, expand=True)

    def _connect_from_config(self):
        try:
            db.disconnect()
            cfg = configparser.ConfigParser()
            cfg.read(CONFIG_FILE)
            d = cfg["database"] if cfg.has_section("database") else {}
            s = cfg["ssh"] if cfg.has_section("ssh") else {}

            ssh_enabled = s.get("enabled", "0") == "1"
            host = d.get("host", "127.0.0.1")
            port = int(d.get("port", "3306"))
            user = d.get("user", "acore")
            password = d.get("password", "acore")
            if ssh_enabled:
                user = s.get("db_user", user)
                password = s.get("db_password", password)

            db.connect(
                host=host,
                port=port,
                user=user,
                password=password,
                ssh_enabled=ssh_enabled,
                ssh_host=s.get("host", ""),
                ssh_port=int(s.get("port", "22") or 22),
                ssh_user=s.get("user", ""),
                ssh_password=s.get("password", ""),
                ssh_key_file=s.get("key_file", ""),
                db_host=s.get("db_host", "127.0.0.1"),
                db_port=int(s.get("db_port", "3306") or 3306),
            )
            self.v_status.set(f"Connected via {'SSH tunnel' if ssh_enabled else 'direct MySQL'}")
            self.refresh()
            self._reschedule_refresh()
        except (MySQLError, ValueError, ImportError) as e:
            self.v_status.set(f"Connect failed: {e}")
            messagebox.showerror("Bot Viewer", str(e))

    def _clear_filters(self):
        self.v_map_filter.set("")
        self.v_zone_filter.set("")
        self.refresh()

    def _reschedule_refresh(self):
        if self._refresh_job:
            self.after_cancel(self._refresh_job)
            self._refresh_job = None
        if not self.v_auto_refresh.get():
            return
        try:
            delay_ms = max(1, int(self.v_refresh_secs.get())) * 1000
        except ValueError:
            delay_ms = 2000
        self._refresh_job = self.after(delay_ms, self._auto_refresh_tick)

    def _auto_refresh_tick(self):
        self._refresh_job = None
        self.refresh()
        self._reschedule_refresh()

    def refresh(self):
        if not db.ok():
            return
        all_rows = db.load_world_bot_statuses(active_only=self.v_active_only.get())
        map_filter = _parse_int_filter(self.v_map_filter.get())
        zone_filter = _parse_int_filter(self.v_zone_filter.get())
        rows = [row for row in all_rows if _matches_filters(row, map_filter, zone_filter)]
        self._rows_by_id = {int(row["id"]): row for row in rows}

        active = sum(1 for row in rows if _bot_status(row) == "active")
        available = sum(1 for row in rows if _bot_status(row) == "available")
        retired = sum(1 for row in rows if _bot_status(row) == "retired")
        self.v_summary.set(
            f"Showing {len(rows)} / {len(all_rows)} bots   active={active}   available={available}   retired={retired}")

        current_selection = self._selected_bot_id
        self.tv_bots.delete(*self.tv_bots.get_children())
        for row in rows:
            session_meta = _parse_session_meta(row.get("session_start_detail"))
            bot_id = int(row["id"])
            self.tv_bots.insert("", "end", iid=str(bot_id), values=(
                bot_id,
                row.get("name", ""),
                _bot_badge(row),
                row.get("level", ""),
                row.get("faction", ""),
                row.get("spec_key", ""),
                session_meta.get("source_kind", ""),
                session_meta.get("source_key", ""),
                row.get("latest_event_type", ""),
                _effective_zone(row),
                _fmt_dt(row.get("latest_logged_at")),
            ), tags=(_badge_tag(row),))

        if current_selection and current_selection in self._rows_by_id:
            self.tv_bots.selection_set(str(current_selection))
            self.tv_bots.focus(str(current_selection))
            self._show_bot_details(current_selection)
        elif rows:
            first_id = int(rows[0]["id"])
            self.tv_bots.selection_set(str(first_id))
            self.tv_bots.focus(str(first_id))
            self._show_bot_details(first_id)
        else:
            self._selected_bot_id = None
            self.txt_detail.delete("1.0", tk.END)
            self.tv_activity.delete(*self.tv_activity.get_children())

    def _on_select_bot(self, _event=None):
        sel = self.tv_bots.selection()
        if not sel:
            return
        self._show_bot_details(int(sel[0]))

    def _show_bot_details(self, bot_id: int):
        row = self._rows_by_id.get(bot_id)
        if not row:
            return
        self._selected_bot_id = bot_id
        session_meta = _parse_session_meta(row.get("session_start_detail"))

        lines = [
            f"ID: {row.get('id')}",
            f"Name: {row.get('name', '')}",
            f"Status: {_bot_status(row)}",
            f"Badge: {_bot_badge(row)}",
            f"Faction: {row.get('faction', '')}   Level: {row.get('level', '')}",
            f"Race/Class: {row.get('race_id', '')}/{row.get('class_id', '')}",
            f"Spec: {row.get('spec_key', '')}",
            f"Sessions: {row.get('session_count', 0)}",
            f"Active Session Ms: {row.get('active_world_session_ms', 0)}",
            f"Active Session Start: {_fmt_dt(row.get('active_world_session_start'))}",
            f"Last Seen Zone: {row.get('last_seen_zone', '')}",
            f"Last Seen At: {_fmt_dt(row.get('last_seen_at'))}",
            "",
            f"Current/Last Session Source Kind: {session_meta.get('source_kind', '')}",
            f"Current/Last Session Source Key: {session_meta.get('source_key', '')}",
            f"Current/Last Session Name: {session_meta.get('session', '')}",
            f"Tasks/Steps: {session_meta.get('tasks', '')}/{session_meta.get('steps', '')}",
            f"Session Start Logged At: {_fmt_dt(row.get('session_start_logged_at'))}",
            "",
            f"Latest Event: {row.get('latest_event_type', '')}",
            f"Latest Event Time: {_fmt_dt(row.get('latest_logged_at'))}",
            f"Latest Zone/Map: {row.get('latest_zone_id', '')}/{row.get('latest_map_id', '')}",
            f"Latest Position: {_fmt_pos(row)}",
            f"Latest Detail: {row.get('latest_detail', '')}",
        ]
        self.txt_detail.delete("1.0", tk.END)
        self.txt_detail.insert("1.0", "\n".join(lines))
        self._refresh_activity(bot_id)

    def _refresh_activity(self, bot_id: int):
        rows = db.load_world_bot_activity_log(bot_id, limit=200)
        self.tv_activity.delete(*self.tv_activity.get_children())
        for row in rows:
            pos = ""
            if row.get("pos_x") is not None and row.get("pos_y") is not None and row.get("pos_z") is not None:
                pos = f"({float(row['pos_x']):.1f}, {float(row['pos_y']):.1f}, {float(row['pos_z']):.1f})"
            self.tv_activity.insert("", "end", values=(
                _fmt_dt(row.get("logged_at")),
                row.get("event_type", ""),
                row.get("map_id", ""),
                row.get("zone_id", ""),
                pos,
                row.get("detail", ""),
            ))

    def on_close(self):
        if self._refresh_job:
            self.after_cancel(self._refresh_job)
            self._refresh_job = None
        db.disconnect()
        self.destroy()


def main():
    app = BotViewerApp()
    app.protocol("WM_DELETE_WINDOW", app.on_close)
    app.mainloop()


if __name__ == "__main__":
    main()