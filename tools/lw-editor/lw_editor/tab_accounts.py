"""
tab_accounts.py -- AccountsTab.
"""
import tkinter as tk
from tkinter import ttk, messagebox, simpledialog
from mysql.connector import Error as MySQLError
from .constants import WOW_CLASSES
from .db import db
from .helpers import lbl, entry_w, unix_text

# ═══════════════════════════════════════════════════════════════════════════

class AccountsTab(ttk.Frame):

    def __init__(self, parent, **kw):
        super().__init__(parent, **kw)
        self._rows = []
        self._build()

    def _build(self):
        top = ttk.Frame(self)
        top.pack(fill=tk.X, padx=8, pady=6)
        ttk.Button(top, text="🔄 Refresh",       command=self.refresh).pack(side=tk.LEFT, padx=4)
        ttk.Button(top, text="+ Create account", command=self._create).pack(side=tk.LEFT, padx=4)
        ttk.Button(top, text="Rename",           command=self._rename).pack(side=tk.LEFT, padx=4)
        ttk.Button(top, text="Set password",     command=self._password).pack(side=tk.LEFT, padx=4)
        ttk.Button(top, text="Delete",           command=self._delete).pack(side=tk.LEFT, padx=4)
        ttk.Button(top, text="✓ Pool On",        command=self._enable).pack(side=tk.LEFT, padx=4)
        ttk.Button(top, text="✗ Pool Off",       command=self._disable).pack(side=tk.LEFT, padx=4)

        cols = ("id", "username", "chars", "pool", "enabled", "online")
        self._tv = ttk.Treeview(self, columns=cols, show="headings",
                                selectmode="browse")
        self._tv.heading("id",       text="Account ID")
        self._tv.heading("username", text="Username")
        self._tv.heading("chars",    text="Chars")
        self._tv.heading("pool",     text="Bot Pool")
        self._tv.heading("enabled",  text="Pool Enabled")
        self._tv.heading("online",   text="Online")
        self._tv.column("id",       width=80,  anchor="center")
        self._tv.column("username", width=170)
        self._tv.column("chars",    width=50,  anchor="center")
        self._tv.column("pool",     width=65,  anchor="center")
        self._tv.column("enabled",  width=60,  anchor="center")
        self._tv.column("online",   width=50,  anchor="center")
        self._tv.pack(fill=tk.BOTH, expand=True, padx=8, pady=4)

        # Info bar
        self._info = ttk.Label(self, text="", foreground="#555")
        self._info.pack(anchor="w", padx=8, pady=2)

    def refresh(self):
        if not db.ok():
            return
        try:
            self._rows = db.load_all_accounts()
            self._tv.delete(*self._tv.get_children())
            for r in self._rows:
                en = "✓" if r.get("is_enabled") else "✗"
                in_pool = "✓" if r.get("in_pool") else "✗"
                online = "✓" if r.get("online") else "✗"
                self._tv.insert("", "end",
                                iid=str(r["account_id"]),
                                tags=("enabled",) if r.get("in_pool") and r.get("is_enabled") else ("disabled",),
                                values=(r["account_id"],
                                        r.get("username", ""),
                                        r.get("char_count", 0),
                                        in_pool,
                                        en,
                                        online))
            self._tv.tag_configure("enabled",  foreground="#1a7f1a")
            self._tv.tag_configure("disabled", foreground="#999")
            total   = len(self._rows)
            enabled = sum(1 for r in self._rows if r.get("in_pool") and r.get("is_enabled"))
            in_pool = sum(1 for r in self._rows if r.get("in_pool"))
            self._info.configure(
                text=f"{total} total accounts  |  {in_pool} in bot pool  |  {enabled} pool-enabled")
        except MySQLError as e:
            messagebox.showerror("DB error", str(e))

    def _selected_id(self):
        sel = self._tv.selection()
        return int(sel[0]) if sel else None

    def _enable(self):
        aid = self._selected_id()
        if aid is None:
            return
        db.set_account_enabled(aid, True)
        self.refresh()

    def _disable(self):
        aid = self._selected_id()
        if aid is None:
            return
        db.set_account_enabled(aid, False)
        self.refresh()

    def _create(self):
        if not db.ok():
            return
        username = simpledialog.askstring("New account", "Username:")
        if not username:
            return
        password = simpledialog.askstring("New account",
                                          f"Password for {username.upper()}:", show="*")
        if not password:
            return
        email = simpledialog.askstring("New account", "Email (optional):") or ""
        add_to_pool = messagebox.askyesno("Bot pool", "Add this account to the bot account pool?")
        try:
            aid = db.create_account(username, password, email=email, add_to_pool=add_to_pool)
            messagebox.showinfo("Created",
                f"Account '{username.upper()}' created with ID {aid}.")
            self.refresh()
        except (MySQLError, ValueError) as e:
            messagebox.showerror("DB error", str(e))

    def _rename(self):
        aid = self._selected_id()
        if aid is None:
            return
        row = next((r for r in self._rows if int(r["account_id"]) == aid), None)
        if not row:
            return
        username = simpledialog.askstring("Rename account", "New username:", initialvalue=row.get("username", ""))
        if not username:
            return
        password = simpledialog.askstring("Rename account",
                                          f"New password for {username.upper()}:", show="*")
        if not password:
            return
        try:
            db.rename_account(aid, username, password)
            self.refresh()
        except (MySQLError, ValueError) as e:
            messagebox.showerror("Rename error", str(e))

    def _password(self):
        aid = self._selected_id()
        if aid is None:
            return
        password = simpledialog.askstring("Set password", f"New password for account {aid}:", show="*")
        if not password:
            return
        try:
            db.change_account_password(aid, password)
            messagebox.showinfo("Updated", f"Password updated for account {aid}.")
        except (MySQLError, ValueError) as e:
            messagebox.showerror("Password error", str(e))

    def _delete(self):
        aid = self._selected_id()
        if aid is None:
            return
        if not messagebox.askyesno("Confirm delete", f"Delete account {aid}? This only works if it has no characters."):
            return
        try:
            db.delete_account(aid)
            self.refresh()
        except (MySQLError, ValueError) as e:
            messagebox.showerror("Delete error", str(e))


