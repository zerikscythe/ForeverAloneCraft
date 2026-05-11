import tkinter as tk
from tkinter import ttk, messagebox
from mysql.connector import Error as MySQLError

from .db import db


class TaskPointsTab(ttk.Frame):
    def __init__(self, parent, **kw):
        super().__init__(parent, **kw)
        self._rows = []
        self._sel = None
        self._build()

    def _build(self):
        pane = ttk.PanedWindow(self, orient=tk.HORIZONTAL)
        pane.pack(fill=tk.BOTH, expand=True)

        left = ttk.Frame(pane, width=280)
        pane.add(left, weight=0)
        ttk.Label(left, text="Task Points").pack(anchor="w", padx=4, pady=4)
        self._lb = tk.Listbox(left, exportselection=False)
        self._lb.pack(fill=tk.BOTH, expand=True, padx=4)
        self._lb.bind("<<ListboxSelect>>", self._on_select)
        btn = ttk.Frame(left)
        btn.pack(fill=tk.X, padx=4, pady=4)
        ttk.Button(btn, text="+ New", command=self._new).pack(side=tk.LEFT, padx=2)
        ttk.Button(btn, text="Delete", command=self._delete).pack(side=tk.LEFT, padx=2)

        right = ttk.Frame(pane)
        pane.add(right, weight=1)

        self.v_point_id = tk.StringVar()
        self.v_point_key = tk.StringVar()
        self.v_zone_id = tk.StringVar()
        self.v_map_id = tk.StringVar()
        self.v_point_type = tk.StringVar()
        self.v_point_name = tk.StringVar()
        self.v_x = tk.StringVar()
        self.v_y = tk.StringVar()
        self.v_z = tk.StringVar()

        fields = [
            ("Point ID", self.v_point_id),
            ("Point Key", self.v_point_key),
            ("Zone ID", self.v_zone_id),
            ("Map ID", self.v_map_id),
            ("Point Type", self.v_point_type),
            ("Point Name", self.v_point_name),
            ("X", self.v_x),
            ("Y", self.v_y),
            ("Z", self.v_z),
        ]
        for i, (label, var) in enumerate(fields):
            ttk.Label(right, text=label).grid(row=i, column=0, sticky="w", padx=4, pady=2)
            state = "readonly" if label == "Point ID" else "normal"
            ttk.Entry(right, textvariable=var, width=40, state=state).grid(row=i, column=1, sticky="ew", padx=4, pady=2)
        right.columnconfigure(1, weight=1)
        ttk.Button(right, text="Save Point", command=self._save).grid(row=len(fields), column=1, sticky="w", padx=4, pady=8)

    def refresh(self):
        if not db.ok():
            return
        self._rows = db.load_task_points()
        self._lb.delete(0, tk.END)
        for row in self._rows:
            self._lb.insert(tk.END, f"{row['point_key']} ({row['point_type']})")

    def _on_select(self, _=None):
        sel = self._lb.curselection()
        if not sel:
            return
        self._sel = self._rows[sel[0]]
        self.v_point_id.set(self._sel.get("point_id", ""))
        self.v_point_key.set(self._sel.get("point_key", ""))
        self.v_zone_id.set(self._sel.get("zone_id", ""))
        self.v_map_id.set(self._sel.get("map_id", ""))
        self.v_point_type.set(self._sel.get("point_type", ""))
        self.v_point_name.set(self._sel.get("point_name", ""))
        self.v_x.set(self._sel.get("x", ""))
        self.v_y.set(self._sel.get("y", ""))
        self.v_z.set(self._sel.get("z", ""))

    def _new(self):
        self._sel = None
        for var in (self.v_point_id, self.v_point_key, self.v_zone_id, self.v_map_id,
                    self.v_point_type, self.v_point_name, self.v_x, self.v_y, self.v_z):
            var.set("")

    def _save(self):
        try:
            row = {
                "point_id": self.v_point_id.get().strip() or None,
                "point_key": self.v_point_key.get(),
                "zone_id": self.v_zone_id.get(),
                "map_id": self.v_map_id.get(),
                "point_type": self.v_point_type.get(),
                "point_name": self.v_point_name.get(),
                "x": self.v_x.get(),
                "y": self.v_y.get(),
                "z": self.v_z.get(),
            }
            db.upsert_task_point(row)
            self.refresh()
        except (MySQLError, ValueError) as e:
            messagebox.showerror("DB error", str(e))

    def _delete(self):
        if not self.v_point_id.get():
            return
        if not messagebox.askyesno("Confirm", "Delete selected task point?"):
            return
        try:
            db.delete_task_point(int(self.v_point_id.get()))
            self._new()
            self.refresh()
        except MySQLError as e:
            messagebox.showerror("DB error", str(e))


class ZoneAnchorsTab(ttk.Frame):
    def __init__(self, parent, **kw):
        super().__init__(parent, **kw)
        self._rows = []
        self._sel = None
        self._build()

    def _build(self):
        pane = ttk.PanedWindow(self, orient=tk.HORIZONTAL)
        pane.pack(fill=tk.BOTH, expand=True)

        left = ttk.Frame(pane, width=320)
        pane.add(left, weight=0)
        ttk.Label(left, text="Zone Anchors").pack(anchor="w", padx=4, pady=4)
        self._lb = tk.Listbox(left, exportselection=False)
        self._lb.pack(fill=tk.BOTH, expand=True, padx=4)
        self._lb.bind("<<ListboxSelect>>", self._on_select)
        btn = ttk.Frame(left)
        btn.pack(fill=tk.X, padx=4, pady=4)
        ttk.Button(btn, text="+ New", command=self._new).pack(side=tk.LEFT, padx=2)
        ttk.Button(btn, text="Delete", command=self._delete).pack(side=tk.LEFT, padx=2)

        right = ttk.Frame(pane)
        pane.add(right, weight=1)

        self.v_anchor_id = tk.StringVar()
        self.v_zone_id = tk.StringVar()
        self.v_point_key = tk.StringVar()
        self.v_anchor_role = tk.StringVar()
        self.v_required_faction = tk.StringVar()
        self.v_min_level = tk.StringVar()
        self.v_max_level = tk.StringVar()
        self.v_weight = tk.StringVar()
        self.v_notes = tk.StringVar()

        fields = [
            ("Anchor ID", self.v_anchor_id, "readonly"),
            ("Zone ID", self.v_zone_id, "normal"),
            ("Point Key", self.v_point_key, "normal"),
            ("Anchor Role", self.v_anchor_role, "normal"),
            ("Required Faction", self.v_required_faction, "normal"),
            ("Min Level", self.v_min_level, "normal"),
            ("Max Level", self.v_max_level, "normal"),
            ("Weight", self.v_weight, "normal"),
            ("Notes", self.v_notes, "normal"),
        ]
        for i, (label, var, state) in enumerate(fields):
            ttk.Label(right, text=label).grid(row=i, column=0, sticky="w", padx=4, pady=2)
            ttk.Entry(right, textvariable=var, width=44, state=state).grid(row=i, column=1, sticky="ew", padx=4, pady=2)
        right.columnconfigure(1, weight=1)
        ttk.Button(right, text="Save Anchor", command=self._save).grid(row=len(fields), column=1, sticky="w", padx=4, pady=8)

    def refresh(self):
        if not db.ok():
            return
        self._rows = db.load_zone_anchors()
        self._lb.delete(0, tk.END)
        for row in self._rows:
            self._lb.insert(tk.END, f"zone={row['zone_id']} role={row['anchor_role']} point={row['point_key']}")

    def _on_select(self, _=None):
        sel = self._lb.curselection()
        if not sel:
            return
        self._sel = self._rows[sel[0]]
        self.v_anchor_id.set(self._sel.get("anchor_id", ""))
        self.v_zone_id.set(self._sel.get("zone_id", ""))
        self.v_point_key.set(self._sel.get("point_key", ""))
        self.v_anchor_role.set(self._sel.get("anchor_role", ""))
        self.v_required_faction.set(self._sel.get("required_faction", ""))
        self.v_min_level.set(self._sel.get("min_level", ""))
        self.v_max_level.set(self._sel.get("max_level", ""))
        self.v_weight.set(self._sel.get("weight", ""))
        self.v_notes.set(self._sel.get("notes", "") or "")

    def _new(self):
        self._sel = None
        for var in (self.v_anchor_id, self.v_zone_id, self.v_point_key, self.v_anchor_role,
                    self.v_required_faction, self.v_min_level, self.v_max_level, self.v_weight, self.v_notes):
            var.set("")

    def _save(self):
        try:
            db.upsert_zone_anchor({
                "anchor_id": self.v_anchor_id.get().strip() or None,
                "zone_id": self.v_zone_id.get(),
                "point_key": self.v_point_key.get(),
                "anchor_role": self.v_anchor_role.get(),
                "required_faction": self.v_required_faction.get(),
                "min_level": self.v_min_level.get(),
                "max_level": self.v_max_level.get(),
                "weight": self.v_weight.get(),
                "notes": self.v_notes.get(),
            })
            self.refresh()
        except (MySQLError, ValueError) as e:
            messagebox.showerror("DB error", str(e))

    def _delete(self):
        if not self.v_anchor_id.get():
            return
        if not messagebox.askyesno("Confirm", "Delete selected zone anchor?"):
            return
        try:
            db.delete_zone_anchor(int(self.v_anchor_id.get()))
            self._new()
            self.refresh()
        except MySQLError as e:
            messagebox.showerror("DB error", str(e))


class ZoneContentTab(ttk.Frame):
    def __init__(self, parent, **kw):
        super().__init__(parent, **kw)
        self._rows = []
        self._sel = None
        self._build()

    def _build(self):
        pane = ttk.PanedWindow(self, orient=tk.HORIZONTAL)
        pane.pack(fill=tk.BOTH, expand=True)

        left = ttk.Frame(pane, width=340)
        pane.add(left, weight=0)
        ttk.Label(left, text="Zone Content").pack(anchor="w", padx=4, pady=4)
        self._lb = tk.Listbox(left, exportselection=False)
        self._lb.pack(fill=tk.BOTH, expand=True, padx=4)
        self._lb.bind("<<ListboxSelect>>", self._on_select)
        btn = ttk.Frame(left)
        btn.pack(fill=tk.X, padx=4, pady=4)
        ttk.Button(btn, text="+ New", command=self._new).pack(side=tk.LEFT, padx=2)
        ttk.Button(btn, text="Delete", command=self._delete).pack(side=tk.LEFT, padx=2)

        right = ttk.Frame(pane)
        pane.add(right, weight=1)

        self.v_content_id = tk.StringVar()
        self.v_zone_id = tk.StringVar()
        self.v_content_kind = tk.StringVar()
        self.v_subject_id = tk.StringVar()
        self.v_subject_key = tk.StringVar()
        self.v_display_name = tk.StringVar()
        self.v_required_faction = tk.StringVar()
        self.v_min_level = tk.StringVar()
        self.v_max_level = tk.StringVar()
        self.v_min_skill = tk.StringVar()
        self.v_max_skill = tk.StringVar()
        self.v_weight = tk.StringVar()
        self.v_anchor_point_key = tk.StringVar()
        self.v_return_anchor_role = tk.StringVar()
        self.v_notes = tk.StringVar()

        fields = [
            ("Content ID", self.v_content_id, "readonly"),
            ("Zone ID", self.v_zone_id, "normal"),
            ("Content Kind", self.v_content_kind, "normal"),
            ("Subject ID", self.v_subject_id, "normal"),
            ("Subject Key", self.v_subject_key, "normal"),
            ("Display Name", self.v_display_name, "normal"),
            ("Required Faction", self.v_required_faction, "normal"),
            ("Min Level", self.v_min_level, "normal"),
            ("Max Level", self.v_max_level, "normal"),
            ("Min Skill", self.v_min_skill, "normal"),
            ("Max Skill", self.v_max_skill, "normal"),
            ("Weight", self.v_weight, "normal"),
            ("Anchor Point Key", self.v_anchor_point_key, "normal"),
            ("Return Anchor Role", self.v_return_anchor_role, "normal"),
            ("Notes", self.v_notes, "normal"),
        ]
        for i, (label, var, state) in enumerate(fields):
            ttk.Label(right, text=label).grid(row=i, column=0, sticky="w", padx=4, pady=2)
            ttk.Entry(right, textvariable=var, width=44, state=state).grid(row=i, column=1, sticky="ew", padx=4, pady=2)
        right.columnconfigure(1, weight=1)
        ttk.Button(right, text="Save Content", command=self._save).grid(row=len(fields), column=1, sticky="w", padx=4, pady=8)

    def refresh(self):
        if not db.ok():
            return
        self._rows = db.load_zone_content()
        self._lb.delete(0, tk.END)
        for row in self._rows:
            self._lb.insert(tk.END, f"{row['content_kind']} zone={row['zone_id']} :: {row['display_name']}")

    def _on_select(self, _=None):
        sel = self._lb.curselection()
        if not sel:
            return
        self._sel = self._rows[sel[0]]
        self.v_content_id.set(self._sel.get("content_id", ""))
        self.v_zone_id.set(self._sel.get("zone_id", ""))
        self.v_content_kind.set(self._sel.get("content_kind", ""))
        self.v_subject_id.set(self._sel.get("subject_id", "") or "")
        self.v_subject_key.set(self._sel.get("subject_key", "") or "")
        self.v_display_name.set(self._sel.get("display_name", ""))
        self.v_required_faction.set(self._sel.get("required_faction", ""))
        self.v_min_level.set(self._sel.get("min_level", ""))
        self.v_max_level.set(self._sel.get("max_level", ""))
        self.v_min_skill.set(self._sel.get("min_skill", ""))
        self.v_max_skill.set(self._sel.get("max_skill", ""))
        self.v_weight.set(self._sel.get("weight", ""))
        self.v_anchor_point_key.set(self._sel.get("anchor_point_key", "") or "")
        self.v_return_anchor_role.set(self._sel.get("return_anchor_role", "") or "")
        self.v_notes.set(self._sel.get("notes", "") or "")

    def _new(self):
        self._sel = None
        for var in (
            self.v_content_id, self.v_zone_id, self.v_content_kind, self.v_subject_id,
            self.v_subject_key, self.v_display_name, self.v_required_faction, self.v_min_level,
            self.v_max_level, self.v_min_skill, self.v_max_skill, self.v_weight,
            self.v_anchor_point_key, self.v_return_anchor_role, self.v_notes):
            var.set("")

    def _save(self):
        try:
            db.upsert_zone_content({
                "content_id": self.v_content_id.get().strip() or None,
                "zone_id": self.v_zone_id.get(),
                "content_kind": self.v_content_kind.get(),
                "subject_id": self.v_subject_id.get(),
                "subject_key": self.v_subject_key.get(),
                "display_name": self.v_display_name.get(),
                "required_faction": self.v_required_faction.get(),
                "min_level": self.v_min_level.get(),
                "max_level": self.v_max_level.get(),
                "min_skill": self.v_min_skill.get(),
                "max_skill": self.v_max_skill.get(),
                "weight": self.v_weight.get(),
                "anchor_point_key": self.v_anchor_point_key.get(),
                "return_anchor_role": self.v_return_anchor_role.get(),
                "notes": self.v_notes.get(),
            })
            self.refresh()
        except (MySQLError, ValueError) as e:
            messagebox.showerror("DB error", str(e))

    def _delete(self):
        if not self.v_content_id.get():
            return
        if not messagebox.askyesno("Confirm", "Delete selected zone content row?"):
            return
        try:
            db.delete_zone_content(int(self.v_content_id.get()))
            self._new()
            self.refresh()
        except MySQLError as e:
            messagebox.showerror("DB error", str(e))


class TaskTemplatesTab(ttk.Frame):
    def __init__(self, parent, **kw):
        super().__init__(parent, **kw)
        self._templates = []
        self._steps = []
        self._sel_template = None
        self._build()

    def _build(self):
        pane = ttk.PanedWindow(self, orient=tk.HORIZONTAL)
        pane.pack(fill=tk.BOTH, expand=True)

        left = ttk.Frame(pane, width=320)
        pane.add(left, weight=0)
        ttk.Label(left, text="Task Templates").pack(anchor="w", padx=4, pady=4)
        self._lb = tk.Listbox(left, exportselection=False)
        self._lb.pack(fill=tk.BOTH, expand=True, padx=4)
        self._lb.bind("<<ListboxSelect>>", self._on_template_select)
        btn = ttk.Frame(left)
        btn.pack(fill=tk.X, padx=4, pady=4)
        ttk.Button(btn, text="+ New", command=self._new_template).pack(side=tk.LEFT, padx=2)
        ttk.Button(btn, text="Delete", command=self._delete_template).pack(side=tk.LEFT, padx=2)

        right = ttk.Frame(pane)
        pane.add(right, weight=1)
        top = ttk.Frame(right)
        top.pack(fill=tk.X, padx=4, pady=4)

        self.v_template_id = tk.StringVar()
        self.v_template_key = tk.StringVar()
        self.v_display_name = tk.StringVar()
        self.v_task_family = tk.StringVar()
        self.v_required_faction = tk.StringVar()
        self.v_min_level = tk.StringVar()
        self.v_max_level = tk.StringVar()
        self.v_requires_herbalism = tk.IntVar(value=0)
        self.v_requires_mining = tk.IntVar(value=0)
        self.v_requires_fishing = tk.IntVar(value=0)
        self.v_weight = tk.StringVar()
        self.v_is_enabled = tk.IntVar(value=1)

        fields = [
            ("Template ID", self.v_template_id, "readonly"),
            ("Template Key", self.v_template_key, "normal"),
            ("Display Name", self.v_display_name, "normal"),
            ("Task Family", self.v_task_family, "normal"),
            ("Required Faction", self.v_required_faction, "normal"),
            ("Min Level", self.v_min_level, "normal"),
            ("Max Level", self.v_max_level, "normal"),
            ("Weight", self.v_weight, "normal"),
        ]
        for i, (label, var, state) in enumerate(fields):
            ttk.Label(top, text=label).grid(row=i, column=0, sticky="w", padx=4, pady=2)
            ttk.Entry(top, textvariable=var, width=36, state=state).grid(row=i, column=1, sticky="w", padx=4, pady=2)
        ttk.Checkbutton(top, text="Requires Herbalism", variable=self.v_requires_herbalism).grid(row=0, column=2, sticky="w", padx=8)
        ttk.Checkbutton(top, text="Requires Mining", variable=self.v_requires_mining).grid(row=1, column=2, sticky="w", padx=8)
        ttk.Checkbutton(top, text="Requires Fishing", variable=self.v_requires_fishing).grid(row=2, column=2, sticky="w", padx=8)
        ttk.Checkbutton(top, text="Enabled", variable=self.v_is_enabled).grid(row=3, column=2, sticky="w", padx=8)
        ttk.Button(top, text="Save Template", command=self._save_template).grid(row=8, column=1, sticky="w", padx=4, pady=8)

        ttk.Separator(right, orient="horizontal").pack(fill=tk.X, padx=4, pady=4)

        steps_wrap = ttk.Frame(right)
        steps_wrap.pack(fill=tk.BOTH, expand=True, padx=4, pady=4)
        ttk.Label(steps_wrap, text="Template Steps").pack(anchor="w")
        cols = ("order", "type", "resolver", "subject", "zone", "point", "cycles", "dur", "label")
        self._tv = ttk.Treeview(steps_wrap, columns=cols, show="headings", height=10)
        for col, text, width in [("order", "#", 40), ("type", "Type", 90), ("resolver", "Resolver", 100), ("subject", "Subject", 90), ("zone", "Zone", 60), ("point", "Point Key", 130), ("cycles", "Cycles", 55), ("dur", "Duration", 120), ("label", "Label", 220)]:
            self._tv.heading(col, text=text)
            self._tv.column(col, width=width, anchor="w")
        self._tv.pack(fill=tk.BOTH, expand=True)
        self._tv.bind("<<TreeviewSelect>>", self._on_step_tree_select)

        form = ttk.Frame(steps_wrap)
        form.pack(fill=tk.X, pady=6)
        self.v_step_order = tk.StringVar()
        self.v_step_type = tk.StringVar()
        self.v_step_zone = tk.StringVar()
        self.v_step_point = tk.StringVar()
        self.v_step_resolver_kind = tk.StringVar()
        self.v_step_subject_kind = tk.StringVar()
        self.v_step_subject_id = tk.StringVar()
        self.v_step_subject_key = tk.StringVar()
        self.v_step_return_anchor_role = tk.StringVar()
        self.v_step_cycle_count = tk.StringVar(value="1")
        self.v_step_dmin = tk.StringVar()
        self.v_step_dmax = tk.StringVar()
        self.v_step_label = tk.StringVar()
        step_fields = [
            ("Order", self.v_step_order), ("Type", self.v_step_type), ("Zone", self.v_step_zone),
            ("Point Key", self.v_step_point), ("Resolver", self.v_step_resolver_kind), ("Subject", self.v_step_subject_kind),
            ("Subject ID", self.v_step_subject_id), ("Subject Key", self.v_step_subject_key), ("Return Role", self.v_step_return_anchor_role),
            ("Cycles", self.v_step_cycle_count), ("Min Sec", self.v_step_dmin), ("Max Sec", self.v_step_dmax), ("Label", self.v_step_label)
        ]
        for i, (label, var) in enumerate(step_fields):
            row = i // 4
            col = (i % 4) * 2
            ttk.Label(form, text=label).grid(row=row, column=col, sticky="w", padx=4, pady=2)
            ttk.Entry(form, textvariable=var, width=18 if label != "Label" else 28).grid(row=row, column=col + 1, sticky="w", padx=4, pady=2)
        ttk.Button(form, text="Save Step", command=self._save_step).grid(row=3, column=1, sticky="w", padx=4, pady=6)
        ttk.Button(form, text="Delete Step", command=self._delete_step).grid(row=3, column=3, sticky="w", padx=4, pady=6)

    def refresh(self):
        if not db.ok():
            return
        self._templates = db.load_task_templates()
        self._lb.delete(0, tk.END)
        for row in self._templates:
            self._lb.insert(tk.END, f"{row['template_key']} ({row['task_family']})")
        self._tv.delete(*self._tv.get_children())

    def _load_step_form(self, step: dict):
        self.v_step_order.set(step.get("step_order", ""))
        self.v_step_type.set(step.get("step_type", "") or "")
        self.v_step_zone.set(step.get("target_zone_id", "") or "")
        self.v_step_point.set(step.get("target_point_key", "") or "")
        self.v_step_resolver_kind.set(step.get("resolver_kind", "") or "")
        self.v_step_subject_kind.set(step.get("subject_kind", "") or "")
        self.v_step_subject_id.set(step.get("subject_id", "") or "")
        self.v_step_subject_key.set(step.get("subject_key", "") or "")
        self.v_step_return_anchor_role.set(step.get("return_anchor_role", "") or "")
        self.v_step_cycle_count.set(step.get("cycle_count", 1) or 1)
        self.v_step_dmin.set(step.get("duration_min_sec", "") or "")
        self.v_step_dmax.set(step.get("duration_max_sec", "") or "")
        self.v_step_label.set(step.get("label", "") or "")

    def _clear_step_form(self):
        for var in (
            self.v_step_order, self.v_step_type, self.v_step_zone, self.v_step_point,
            self.v_step_resolver_kind, self.v_step_subject_kind, self.v_step_subject_id,
            self.v_step_subject_key, self.v_step_return_anchor_role, self.v_step_dmin,
            self.v_step_dmax, self.v_step_label):
            var.set("")
        self.v_step_cycle_count.set("1")

    def _on_template_select(self, _=None):
        sel = self._lb.curselection()
        if not sel:
            return
        self._sel_template = self._templates[sel[0]]
        t = self._sel_template
        self.v_template_id.set(t.get("template_id", ""))
        self.v_template_key.set(t.get("template_key", ""))
        self.v_display_name.set(t.get("display_name", ""))
        self.v_task_family.set(t.get("task_family", ""))
        self.v_required_faction.set(t.get("required_faction", ""))
        self.v_min_level.set(t.get("min_level", ""))
        self.v_max_level.set(t.get("max_level", ""))
        self.v_requires_herbalism.set(int(t.get("requires_herbalism", 0)))
        self.v_requires_mining.set(int(t.get("requires_mining", 0)))
        self.v_requires_fishing.set(int(t.get("requires_fishing", 0)))
        self.v_weight.set(t.get("weight", ""))
        self.v_is_enabled.set(int(t.get("is_enabled", 1)))
        self._steps = db.load_task_template_steps(int(t["template_id"]))
        self._tv.delete(*self._tv.get_children())
        for step in self._steps:
            self._tv.insert("", "end", iid=str(step["step_order"]), values=(
                step["step_order"], step["step_type"], step.get("resolver_kind") or "", step.get("subject_kind") or "",
                step["target_zone_id"], step.get("target_point_key") or "", step.get("cycle_count") or 1,
                f"{step['duration_min_sec']}-{step['duration_max_sec']}", step["label"]))
        if self._steps:
            self._tv.selection_set(str(self._steps[0]["step_order"]))
            self._load_step_form(self._steps[0])
        else:
            self._clear_step_form()

    def _on_step_tree_select(self, _=None):
        sel = self._tv.selection()
        if not sel:
            return
        try:
            step_order = int(sel[0])
        except (TypeError, ValueError):
            return
        for step in self._steps:
            if int(step.get("step_order", 0)) == step_order:
                self._load_step_form(step)
                return

    def _new_template(self):
        self._sel_template = None
        for var in (self.v_template_id, self.v_template_key, self.v_display_name, self.v_task_family,
                    self.v_required_faction, self.v_min_level, self.v_max_level, self.v_weight):
            var.set("")
        self.v_requires_herbalism.set(0)
        self.v_requires_mining.set(0)
        self.v_requires_fishing.set(0)
        self.v_is_enabled.set(1)
        self._tv.delete(*self._tv.get_children())
        self._clear_step_form()

    def _save_template(self):
        try:
            row = {
                "template_id": self.v_template_id.get().strip() or None,
                "template_key": self.v_template_key.get(),
                "display_name": self.v_display_name.get(),
                "task_family": self.v_task_family.get(),
                "required_faction": self.v_required_faction.get(),
                "min_level": self.v_min_level.get(),
                "max_level": self.v_max_level.get(),
                "requires_herbalism": self.v_requires_herbalism.get(),
                "requires_mining": self.v_requires_mining.get(),
                "requires_fishing": self.v_requires_fishing.get(),
                "weight": self.v_weight.get(),
                "is_enabled": self.v_is_enabled.get(),
            }
            db.upsert_task_template(row)
            self.refresh()
        except (MySQLError, ValueError) as e:
            messagebox.showerror("DB error", str(e))

    def _delete_template(self):
        if not self.v_template_id.get():
            return
        if not messagebox.askyesno("Confirm", "Delete selected template and all its steps?"):
            return
        try:
            db.delete_task_template(int(self.v_template_id.get()))
            self._new_template()
            self.refresh()
        except MySQLError as e:
            messagebox.showerror("DB error", str(e))

    def _save_step(self):
        if not self.v_template_id.get():
            messagebox.showwarning("No template", "Save/select a template first.")
            return
        try:
            row = {
                "template_id": self.v_template_id.get(),
                "step_order": self.v_step_order.get(),
                "step_type": self.v_step_type.get(),
                "target_zone_id": self.v_step_zone.get(),
                "target_point_key": self.v_step_point.get(),
                "resolver_kind": self.v_step_resolver_kind.get(),
                "subject_kind": self.v_step_subject_kind.get(),
                "subject_id": self.v_step_subject_id.get(),
                "subject_key": self.v_step_subject_key.get(),
                "return_anchor_role": self.v_step_return_anchor_role.get(),
                "cycle_count": self.v_step_cycle_count.get(),
                "duration_min_sec": self.v_step_dmin.get(),
                "duration_max_sec": self.v_step_dmax.get(),
                "label": self.v_step_label.get(),
            }
            db.upsert_task_template_step(row)
            self._on_template_select()
        except (MySQLError, ValueError) as e:
            messagebox.showerror("DB error", str(e))

    def _delete_step(self):
        sel = self._tv.selection()
        if not sel or not self.v_template_id.get():
            return
        if not messagebox.askyesno("Confirm", "Delete selected step?"):
            return
        try:
            db.delete_task_template_step(int(self.v_template_id.get()), int(sel[0]))
            self._on_template_select()
        except MySQLError as e:
            messagebox.showerror("DB error", str(e))


class TaxiRoutesTab(ttk.Frame):
    def __init__(self, parent, **kw):
        super().__init__(parent, **kw)
        self._rows = []
        self._build()

    def _build(self):
        pane = ttk.PanedWindow(self, orient=tk.HORIZONTAL)
        pane.pack(fill=tk.BOTH, expand=True)

        left = ttk.Frame(pane, width=300)
        pane.add(left, weight=0)
        ttk.Label(left, text="Transit Routes").pack(anchor="w", padx=4, pady=4)
        self._lb = tk.Listbox(left, exportselection=False)
        self._lb.pack(fill=tk.BOTH, expand=True, padx=4)
        self._lb.bind("<<ListboxSelect>>", self._on_select)
        btn = ttk.Frame(left)
        btn.pack(fill=tk.X, padx=4, pady=4)
        ttk.Button(btn, text="+ New", command=self._new).pack(side=tk.LEFT, padx=2)
        ttk.Button(btn, text="Delete", command=self._delete).pack(side=tk.LEFT, padx=2)

        right = ttk.Frame(pane)
        pane.add(right, weight=1)

        self.v_route_id = tk.StringVar()
        self.v_route_key = tk.StringVar()
        self.v_source = tk.StringVar()
        self.v_dest = tk.StringVar()
        self.v_transit_type = tk.StringVar()
        self.v_faction = tk.StringVar()
        self.v_min_level = tk.StringVar()
        self.v_max_level = tk.StringVar()
        self.v_duration = tk.StringVar()
        self.v_display = tk.StringVar()

        fields = [
            ("Route ID", self.v_route_id, "readonly"),
            ("Route Key", self.v_route_key, "normal"),
            ("Source Point Key", self.v_source, "normal"),
            ("Destination Point Key", self.v_dest, "normal"),
            ("Transit Type", self.v_transit_type, "normal"),
            ("Required Faction", self.v_faction, "normal"),
            ("Min Level", self.v_min_level, "normal"),
            ("Max Level", self.v_max_level, "normal"),
            ("Duration Sec", self.v_duration, "normal"),
            ("Display Name", self.v_display, "normal"),
        ]
        for i, (label, var, state) in enumerate(fields):
            ttk.Label(right, text=label).grid(row=i, column=0, sticky="w", padx=4, pady=2)
            ttk.Entry(right, textvariable=var, width=40, state=state).grid(row=i, column=1, sticky="ew", padx=4, pady=2)
        right.columnconfigure(1, weight=1)
        ttk.Button(right, text="Save Route", command=self._save).grid(row=len(fields), column=1, sticky="w", padx=4, pady=8)

    def refresh(self):
        if not db.ok():
            return
        self._rows = db.load_transit_routes()
        self._lb.delete(0, tk.END)
        for row in self._rows:
            self._lb.insert(tk.END, f"{row['route_key']} [{row['transit_type']}] ({row['source_point_key']} -> {row['dest_point_key']})")

    def _on_select(self, _=None):
        sel = self._lb.curselection()
        if not sel:
            return
        row = self._rows[sel[0]]
        self.v_route_id.set(row.get("route_id", ""))
        self.v_route_key.set(row.get("route_key", ""))
        self.v_source.set(row.get("source_point_key", ""))
        self.v_dest.set(row.get("dest_point_key", ""))
        self.v_transit_type.set(row.get("transit_type", ""))
        self.v_faction.set(row.get("required_faction", ""))
        self.v_min_level.set(row.get("min_level", ""))
        self.v_max_level.set(row.get("max_level", ""))
        self.v_duration.set(row.get("duration_sec", ""))
        self.v_display.set(row.get("display_name", ""))

    def _new(self):
        for var in (self.v_route_id, self.v_route_key, self.v_source, self.v_dest, self.v_transit_type, self.v_faction, self.v_min_level, self.v_max_level, self.v_duration, self.v_display):
            var.set("")
        self.v_transit_type.set("taxi")

    def _save(self):
        try:
            db.upsert_transit_route({
                "route_id": self.v_route_id.get().strip() or None,
                "route_key": self.v_route_key.get(),
                "source_point_key": self.v_source.get(),
                "dest_point_key": self.v_dest.get(),
                "transit_type": self.v_transit_type.get(),
                "required_faction": self.v_faction.get(),
                "min_level": self.v_min_level.get(),
                "max_level": self.v_max_level.get(),
                "duration_sec": self.v_duration.get(),
                "display_name": self.v_display.get(),
            })
            self.refresh()
        except (MySQLError, ValueError) as e:
            messagebox.showerror("DB error", str(e))

    def _delete(self):
        if not self.v_route_id.get():
            return
        if not messagebox.askyesno("Confirm", "Delete selected transit route?"):
            return
        try:
            db.delete_transit_route(int(self.v_route_id.get()))
            self._new()
            self.refresh()
        except MySQLError as e:
            messagebox.showerror("DB error", str(e))


class PlaylistsTab(ttk.Frame):
    def __init__(self, parent, **kw):
        super().__init__(parent, **kw)
        self._playlists = []
        self._entries = []
        self._build()

    def _build(self):
        pane = ttk.PanedWindow(self, orient=tk.HORIZONTAL)
        pane.pack(fill=tk.BOTH, expand=True)

        left = ttk.Frame(pane, width=320)
        pane.add(left, weight=0)
        ttk.Label(left, text="Playlists").pack(anchor="w", padx=4, pady=4)
        self._lb = tk.Listbox(left, exportselection=False)
        self._lb.pack(fill=tk.BOTH, expand=True, padx=4)
        self._lb.bind("<<ListboxSelect>>", self._on_playlist_select)
        btn = ttk.Frame(left)
        btn.pack(fill=tk.X, padx=4, pady=4)
        ttk.Button(btn, text="+ New", command=self._new_playlist).pack(side=tk.LEFT, padx=2)
        ttk.Button(btn, text="Delete", command=self._delete_playlist).pack(side=tk.LEFT, padx=2)

        right = ttk.Frame(pane)
        pane.add(right, weight=1)
        top = ttk.Frame(right)
        top.pack(fill=tk.X, padx=4, pady=4)

        self.v_playlist_id = tk.StringVar()
        self.v_playlist_key = tk.StringVar()
        self.v_playlist_name = tk.StringVar()
        self.v_playlist_family = tk.StringVar()
        self.v_playlist_faction = tk.StringVar()
        self.v_playlist_min = tk.StringVar()
        self.v_playlist_max = tk.StringVar()
        self.v_playlist_weight = tk.StringVar()
        self.v_playlist_enabled = tk.IntVar(value=1)
        self.v_playlist_herb = tk.IntVar(value=0)
        self.v_playlist_mine = tk.IntVar(value=0)
        self.v_playlist_fish = tk.IntVar(value=0)

        fields = [
            ("Playlist ID", self.v_playlist_id, "readonly"),
            ("Playlist Key", self.v_playlist_key, "normal"),
            ("Display Name", self.v_playlist_name, "normal"),
            ("Task Family", self.v_playlist_family, "normal"),
            ("Required Faction", self.v_playlist_faction, "normal"),
            ("Min Level", self.v_playlist_min, "normal"),
            ("Max Level", self.v_playlist_max, "normal"),
            ("Weight", self.v_playlist_weight, "normal"),
        ]
        for i, (label, var, state) in enumerate(fields):
            ttk.Label(top, text=label).grid(row=i, column=0, sticky="w", padx=4, pady=2)
            ttk.Entry(top, textvariable=var, width=36, state=state).grid(row=i, column=1, sticky="w", padx=4, pady=2)
        ttk.Checkbutton(top, text="Requires Herbalism", variable=self.v_playlist_herb).grid(row=0, column=2, sticky="w", padx=8)
        ttk.Checkbutton(top, text="Requires Mining", variable=self.v_playlist_mine).grid(row=1, column=2, sticky="w", padx=8)
        ttk.Checkbutton(top, text="Requires Fishing", variable=self.v_playlist_fish).grid(row=2, column=2, sticky="w", padx=8)
        ttk.Checkbutton(top, text="Enabled", variable=self.v_playlist_enabled).grid(row=3, column=2, sticky="w", padx=8)
        ttk.Button(top, text="Save Playlist", command=self._save_playlist).grid(row=8, column=1, sticky="w", padx=4, pady=8)

        ttk.Separator(right, orient="horizontal").pack(fill=tk.X, padx=4, pady=4)

        entries_wrap = ttk.Frame(right)
        entries_wrap.pack(fill=tk.BOTH, expand=True, padx=4, pady=4)
        ttk.Label(entries_wrap, text="Playlist Entries").pack(anchor="w")
        cols = ("order", "template", "repeat", "note")
        self._tv = ttk.Treeview(entries_wrap, columns=cols, show="headings", height=10)
        for col, text, width in [("order", "#", 40), ("template", "Template", 260), ("repeat", "Repeat", 70), ("note", "Note", 260)]:
            self._tv.heading(col, text=text)
            self._tv.column(col, width=width, anchor="w")
        self._tv.pack(fill=tk.BOTH, expand=True)

        form = ttk.Frame(entries_wrap)
        form.pack(fill=tk.X, pady=6)
        self.v_entry_order = tk.StringVar()
        self.v_entry_template_id = tk.StringVar()
        self.v_entry_repeat = tk.StringVar()
        self.v_entry_note = tk.StringVar()
        entry_fields = [
            ("Order", self.v_entry_order),
            ("Task Template ID", self.v_entry_template_id),
            ("Repeat Count", self.v_entry_repeat),
            ("Note", self.v_entry_note),
        ]
        for i, (label, var) in enumerate(entry_fields):
            ttk.Label(form, text=label).grid(row=0 if i < 3 else 1, column=(i % 3) * 2, sticky="w", padx=4, pady=2)
            ttk.Entry(form, textvariable=var, width=22 if label != "Note" else 36).grid(row=0 if i < 3 else 1, column=(i % 3) * 2 + 1, sticky="w", padx=4, pady=2)
        ttk.Button(form, text="Save Entry", command=self._save_entry).grid(row=2, column=1, sticky="w", padx=4, pady=6)
        ttk.Button(form, text="Delete Entry", command=self._delete_entry).grid(row=2, column=3, sticky="w", padx=4, pady=6)

    def refresh(self):
        if not db.ok():
            return
        self._playlists = db.load_playlists()
        self._lb.delete(0, tk.END)
        for row in self._playlists:
            self._lb.insert(tk.END, f"{row['playlist_key']} ({row['task_family']})")
        self._tv.delete(*self._tv.get_children())

    def _on_playlist_select(self, _=None):
        sel = self._lb.curselection()
        if not sel:
            return
        p = self._playlists[sel[0]]
        self.v_playlist_id.set(p.get("playlist_id", ""))
        self.v_playlist_key.set(p.get("playlist_key", ""))
        self.v_playlist_name.set(p.get("display_name", ""))
        self.v_playlist_family.set(p.get("task_family", ""))
        self.v_playlist_faction.set(p.get("required_faction", ""))
        self.v_playlist_min.set(p.get("min_level", ""))
        self.v_playlist_max.set(p.get("max_level", ""))
        self.v_playlist_weight.set(p.get("weight", ""))
        self.v_playlist_enabled.set(int(p.get("is_enabled", 1)))
        self.v_playlist_herb.set(int(p.get("requires_herbalism", 0)))
        self.v_playlist_mine.set(int(p.get("requires_mining", 0)))
        self.v_playlist_fish.set(int(p.get("requires_fishing", 0)))
        self._entries = db.load_playlist_entries(int(p["playlist_id"]))
        self._tv.delete(*self._tv.get_children())
        for entry in self._entries:
            template_label = entry.get("template_key") or f"template_id={entry['task_template_id']}"
            if entry.get("template_display_name"):
                template_label += f" / {entry['template_display_name']}"
            self._tv.insert("", "end", iid=str(entry["entry_order"]), values=(
                entry["entry_order"], template_label, entry["repeat_count"], entry.get("note") or ""))

    def _new_playlist(self):
        for var in (self.v_playlist_id, self.v_playlist_key, self.v_playlist_name, self.v_playlist_family,
                    self.v_playlist_faction, self.v_playlist_min, self.v_playlist_max, self.v_playlist_weight):
            var.set("")
        self.v_playlist_enabled.set(1)
        self.v_playlist_herb.set(0)
        self.v_playlist_mine.set(0)
        self.v_playlist_fish.set(0)
        self._tv.delete(*self._tv.get_children())

    def _save_playlist(self):
        try:
            db.upsert_playlist({
                "playlist_id": self.v_playlist_id.get().strip() or None,
                "playlist_key": self.v_playlist_key.get(),
                "display_name": self.v_playlist_name.get(),
                "task_family": self.v_playlist_family.get(),
                "required_faction": self.v_playlist_faction.get(),
                "min_level": self.v_playlist_min.get(),
                "max_level": self.v_playlist_max.get(),
                "requires_herbalism": self.v_playlist_herb.get(),
                "requires_mining": self.v_playlist_mine.get(),
                "requires_fishing": self.v_playlist_fish.get(),
                "weight": self.v_playlist_weight.get(),
                "is_enabled": self.v_playlist_enabled.get(),
            })
            self.refresh()
        except (MySQLError, ValueError) as e:
            messagebox.showerror("DB error", str(e))

    def _delete_playlist(self):
        if not self.v_playlist_id.get():
            return
        if not messagebox.askyesno("Confirm", "Delete selected playlist and all its entries?"):
            return
        try:
            db.delete_playlist(int(self.v_playlist_id.get()))
            self._new_playlist()
            self.refresh()
        except MySQLError as e:
            messagebox.showerror("DB error", str(e))

    def _save_entry(self):
        if not self.v_playlist_id.get():
            messagebox.showwarning("No playlist", "Save/select a playlist first.")
            return
        try:
            db.upsert_playlist_entry({
                "playlist_id": self.v_playlist_id.get(),
                "entry_order": self.v_entry_order.get(),
                "task_template_id": self.v_entry_template_id.get(),
                "repeat_count": self.v_entry_repeat.get(),
                "note": self.v_entry_note.get(),
            })
            self._on_playlist_select()
        except (MySQLError, ValueError) as e:
            messagebox.showerror("DB error", str(e))

    def _delete_entry(self):
        sel = self._tv.selection()
        if not sel or not self.v_playlist_id.get():
            return
        if not messagebox.askyesno("Confirm", "Delete selected playlist entry?"):
            return
        try:
            db.delete_playlist_entry(int(self.v_playlist_id.get()), int(sel[0]))
            self._on_playlist_select()
        except MySQLError as e:
            messagebox.showerror("DB error", str(e))