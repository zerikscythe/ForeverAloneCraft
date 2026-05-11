"""
widgets.py -- Reusable UI widgets: ProfileHeaderFrame, DefaultProfilePicker.
"""
import tkinter as tk
from tkinter import ttk, messagebox, simpledialog
from mysql.connector import Error as MySQLError
from .constants import (
    WOW_CLASSES, CLASS_OPTS, CLASS_NAME_TO_ID, CLASS_SPEC_OPTS, ROLE_OPTS,
    CONSERVATION_MODES, CONSERVATION_INV, CONSERVATION_OPTS,
    AOE_MODES, AOE_INV, AOE_OPTS,
    _normalize_role,
)
from .db import db
from .helpers import lbl, entry_w, combo_w, check_w
# Import lazily to avoid circular dependency: rotation → widgets → rotation
def _class_from_spec(*texts):
    from .rotation import _class_from_spec as _impl
    return _impl(*texts)

# ═══════════════════════════════════════════════════════════════════════════

class ProfileHeaderFrame(ttk.LabelFrame):
    """Displays and edits the scalar fields of a combat profile."""

    def __init__(self, parent, is_default=True, on_class_change=None, **kw):
        super().__init__(parent, text="Profile Settings", padding=6, **kw)
        self.is_default = is_default
        self._on_class_change_cb = on_class_change
        self._build()
        self.clear()

    def _build(self):
        f = self
        # Row 0 – display name / class / spec / role
        if not self.is_default:
            lbl(f, "Name:",         0, 0)
            self.v_name     = tk.StringVar()
            entry_w(f, self.v_name, 0, 1, width=20)
            lbl(f, "Slot:",         0, 2)
            self.v_slot     = tk.StringVar()
            entry_w(f, self.v_slot, 0, 3, width=4)
        _hr = 0 if self.is_default else 1
        self.v_class = tk.StringVar()
        if self.is_default:
            # Class Defaults tab: operator picks the class explicitly.
            lbl(f, "Class:", _hr, 0)
            class_cb = ttk.Combobox(f, textvariable=self.v_class, values=CLASS_OPTS,
                                    state="readonly", width=14)
            class_cb.grid(row=_hr, column=1, sticky="w", padx=4, pady=2)
            class_cb.bind("<<ComboboxSelected>>", self._on_class_change)
        else:
            # Clone Profiles tab: class is already known from the character selection.
            # Show it as a read-only badge; no need for the operator to set it.
            lbl(f, "Class:", _hr, 0)
            self._class_badge = ttk.Label(f, textvariable=self.v_class,
                                          width=14, relief="sunken", anchor="w",
                                          foreground="#0055cc")
            self._class_badge.grid(row=_hr, column=1, sticky="w", padx=4, pady=2)
        lbl(f, "Spec:", _hr, 2)
        self.v_spec = tk.StringVar()
        self._spec_cb = ttk.Combobox(f, textvariable=self.v_spec, values=[],
                                     state="readonly", width=16)
        self._spec_cb.grid(row=_hr, column=3, sticky="w", padx=4, pady=2)
        lbl(f, "Role:", _hr, 4)
        self.v_role = tk.StringVar()
        combo_w(f, self.v_role, ROLE_OPTS, _hr, 5, width=10)

        if self.is_default:
            lbl(f, "Display name:", 1, 0)
            self.v_display = tk.StringVar()
            entry_w(f, self.v_display, 1, 1, width=28, columnspan=5)

        # Row 2 – conservation / mana
        r = 2
        lbl(f, "Conservation:", r, 0)
        self.v_conservation = tk.StringVar()
        combo_w(f, self.v_conservation, CONSERVATION_OPTS, r, 1)

        lbl(f, "Mana low %:", r, 2)
        self.v_mana_low = tk.StringVar()
        entry_w(f, self.v_mana_low, r, 3, width=5)

        lbl(f, "Mana high %:", r, 4)
        self.v_mana_high = tk.StringVar()
        entry_w(f, self.v_mana_high, r, 5, width=5)

        # Row 3 – down-rank / AoE
        r = 3
        self.v_downrank = tk.BooleanVar()
        check_w(f, self.v_downrank, "Down-rank", r, 0)

        lbl(f, "DR floor:", r, 1)
        self.v_dr_floor = tk.StringVar()
        entry_w(f, self.v_dr_floor, r, 2, width=4)

        lbl(f, "AoE mode:", r, 3)
        self.v_aoe_mode = tk.StringVar()
        combo_w(f, self.v_aoe_mode, AOE_OPTS, r, 4, width=10)

        lbl(f, "AoE min targets:", r, 5)
        self.v_aoe_min = tk.StringVar()
        entry_w(f, self.v_aoe_min, r, 6, width=4)

        lbl(f, "AoE radius:", r, 7)
        self.v_aoe_radius = tk.StringVar()
        entry_w(f, self.v_aoe_radius, r, 8, width=6)

    def set_class_from_character(self, class_id: int | None):
        """Clone Profiles tab calls this when the character selection changes.
        Updates the class badge and repopulates the spec combobox immediately,
        before any profile is loaded."""
        class_name = WOW_CLASSES.get(class_id, "") if class_id else ""
        self.v_class.set(class_name)
        self._apply_spec_values(class_name)
        self._emit_class_change()

    def _spec_values_for_class(self, class_name: str) -> list[str]:
        class_id = CLASS_NAME_TO_ID.get(class_name)
        return CLASS_SPEC_OPTS.get(class_id, [])

    def _apply_spec_values(self, class_name: str, preferred_spec: str = ""):
        values = self._spec_values_for_class(class_name)
        self._spec_cb.configure(values=values)
        if preferred_spec in values:
            self.v_spec.set(preferred_spec)
        elif values:
            self.v_spec.set(values[0] if self.v_spec.get() not in values else self.v_spec.get())
        else:
            self.v_spec.set(preferred_spec)

    def _emit_class_change(self):
        if self._on_class_change_cb:
            self._on_class_change_cb(CLASS_NAME_TO_ID.get(self.v_class.get()))

    def _on_class_change(self, _=None):
        self._apply_spec_values(self.v_class.get())
        self._emit_class_change()

    def clear(self):
        if self.is_default:
            self.v_class.set("")
        # For bot profiles, leave v_class alone — it reflects the selected character
        # and is reset by load() / set_class_from_character().
        self.v_spec.set("")
        self.v_role.set("")
        self.v_conservation.set(CONSERVATION_MODES[1])
        self.v_mana_low.set("55")
        self.v_mana_high.set("75")
        self.v_downrank.set(True)
        self.v_dr_floor.set("2")
        self.v_aoe_mode.set(AOE_MODES[0])
        self.v_aoe_min.set("2")
        self.v_aoe_radius.set("10.0")
        if self.is_default:
            self.v_display.set("")
        else:
            self.v_name.set("")
            self.v_slot.set("")

    def load(self, p: dict, forced_class_id: int | None = None):
        if self.is_default:
            raw_spec = p.get("spec_key", "") or ""
            raw_role = p.get("role_key", "") or ""
            # Prefer the stored class_key; fall back to heuristic for old rows
            stored_class = (p.get("class_key") or "").strip()
            class_id = (forced_class_id
                        or CLASS_NAME_TO_ID.get(stored_class)
                        or _class_from_spec(raw_spec, p.get("display_name", "") or ""))
        else:
            raw_spec = (p.get("spec_override_key") or
                        p.get("guessed_spec_key") or "")
            raw_role = (p.get("role_override_key") or
                        p.get("guessed_role_key") or "")
            class_id = forced_class_id or _class_from_spec(raw_spec, p.get("profile_name", "") or "")

        class_name = WOW_CLASSES.get(class_id, "")
        self.v_class.set(class_name)
        self._apply_spec_values(class_name, raw_spec)
        self.v_role.set(_normalize_role(raw_role))
        self.v_conservation.set(CONSERVATION_MODES.get(p.get("conservation_mode", 1), "Conservative"))
        self.v_mana_low.set(str(p.get("resource_low_water", 55)))
        self.v_mana_high.set(str(p.get("resource_high_water", 75)))
        self.v_downrank.set(bool(p.get("enable_down_rank", 1)))
        self.v_dr_floor.set(str(p.get("down_rank_floor", 2)))
        self.v_aoe_mode.set(AOE_MODES.get(p.get("default_aoe_mode", 0), "Centroid"))
        self.v_aoe_min.set(str(p.get("default_aoe_min_targets", 2)))
        self.v_aoe_radius.set(str(p.get("default_aoe_scan_radius", 10.0)))
        if self.is_default:
            self.v_display.set(p.get("display_name", "") or "")
        else:
            self.v_name.set(p.get("profile_name", "") or "")
            self.v_slot.set(str(p.get("slot", "")))
        self._emit_class_change()

    def collect(self, base: dict) -> dict:
        base["conservation_mode"]    = CONSERVATION_INV.get(self.v_conservation.get(), 1)
        base["resource_low_water"]       = int(self.v_mana_low.get() or 55)
        base["resource_high_water"]      = int(self.v_mana_high.get() or 75)
        base["enable_down_rank"]     = int(self.v_downrank.get())
        base["down_rank_floor"]      = int(self.v_dr_floor.get() or 2)
        base["default_aoe_mode"]     = AOE_INV.get(self.v_aoe_mode.get(), 0)
        base["default_aoe_min_targets"] = int(self.v_aoe_min.get() or 2)
        base["default_aoe_scan_radius"] = float(self.v_aoe_radius.get() or 10.0)
        if self.is_default:
            base["class_key"]   = self.v_class.get().strip() or None
            base["spec_key"]    = self.v_spec.get().strip()
            base["role_key"]    = _normalize_role(self.v_role.get().strip())
            base["display_name"] = self.v_display.get().strip()
        else:
            base["spec_override_key"] = self.v_spec.get().strip() or None
            base["role_override_key"] = _normalize_role(self.v_role.get().strip()) or None
            base["profile_name"] = self.v_name.get().strip()
            base["slot"]         = int(self.v_slot.get() or 0)
        return base


class DefaultProfilePicker:
    """Modal picker for choosing a default profile template or blank slate."""

    def __init__(self, parent, options: list[tuple[str, int | None]], title="New Bot Profile"):
        self._options = options or [("Blank slate", None)]
        self.selection = None
        self.cancelled = True

        self._choice = tk.StringVar(value=self._options[0][0])
        self._result = None
        self._win = tk.Toplevel(parent)
        self._win.title(title)
        self._win.transient(parent)
        self._win.resizable(False, False)
        self._win.protocol("WM_DELETE_WINDOW", self._cancel)

        body = ttk.Frame(self._win, padding=10)
        body.pack(fill=tk.BOTH, expand=True)
        ttk.Label(body, text="Create the new profile from:").grid(
            row=0, column=0, sticky="w", pady=(0, 4))
        combo = ttk.Combobox(
            body,
            textvariable=self._choice,
            values=[label for label, _value in self._options],
            state="readonly",
            width=40,
        )
        combo.grid(row=1, column=0, sticky="ew")
        combo.focus_set()

        btns = ttk.Frame(body)
        btns.grid(row=2, column=0, sticky="e", pady=(10, 0))
        ttk.Button(btns, text="OK", command=self._ok).pack(side=tk.LEFT, padx=(0, 6))
        ttk.Button(btns, text="Cancel", command=self._cancel).pack(side=tk.LEFT)

        body.columnconfigure(0, weight=1)
        self._win.bind("<Return>", lambda _e: self._ok())
        self._win.bind("<Escape>", lambda _e: self._cancel())
        self._win.update_idletasks()

        parent_x = parent.winfo_rootx()
        parent_y = parent.winfo_rooty()
        parent_w = parent.winfo_width()
        parent_h = parent.winfo_height()
        win_w = self._win.winfo_width()
        win_h = self._win.winfo_height()
        pos_x = parent_x + max((parent_w - win_w) // 2, 0)
        pos_y = parent_y + max((parent_h - win_h) // 2, 0)
        self._win.geometry(f"+{pos_x}+{pos_y}")

        self._win.grab_set()
        parent.wait_window(self._win)

    def _ok(self):
        selected = self._choice.get()
        self.cancelled = False
        for label, value in self._options:
            if label == selected:
                self.selection = value
                break
        self._close()

    def _cancel(self):
        self.cancelled = True
        self.selection = None
        self._close()

    def _close(self):
        if self._win and self._win.winfo_exists():
            self._win.grab_release()
            self._win.destroy()



