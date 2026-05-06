"""
app.py -- App (main window) and entry point.
"""
import os
import tkinter as tk
from tkinter import ttk, messagebox, filedialog
import configparser
from mysql.connector import Error as MySQLError
from .constants import VERSION
from .db import db, SSH_TUNNEL_AVAILABLE
from lw_editor import CONFIG_FILE
from .tab_bots import BotProfilesTab, DefaultProfilesTab
from .tab_character import CharacterEditorTab
from .tab_accounts import AccountsTab
from .tab_diagnostics import DiagnosticsTab
from .tab_hazard_config import HazardConfigTab

# ═══════════════════════════════════════════════════════════════════════════

class App(tk.Tk):

    def __init__(self):
        super().__init__()
        self.title(f"LivingWorld Bot Editor  v{VERSION}")
        self.geometry("1280x800")
        self.minsize(1000, 640)
        self._preferred_game_account_id = None
        self._auto_connect_enabled = False
        self._build_connection_bar()
        self._build_notebook()
        self._load_saved_config()
        if self._auto_connect_enabled:
            self.after(150, self._connect)

    def _load_saved_config(self):
        cfg = configparser.ConfigParser()
        if os.path.exists(CONFIG_FILE):
            cfg.read(CONFIG_FILE)
        d = cfg["database"] if cfg.has_section("database") else {}
        self.v_host.set(d.get("host", "127.0.0.1"))
        self.v_port.set(d.get("port", "3306"))
        self.v_user.set(d.get("user", "acore"))
        self.v_pass.set(d.get("password", "acore"))

        # SSH tunnel config
        s = cfg["ssh"] if cfg.has_section("ssh") else {}
        self.v_ssh_enabled.set(s.get("enabled", "0") == "1")
        self.v_ssh_host.set(s.get("host", ""))
        self.v_ssh_port.set(s.get("port", "22"))
        self.v_ssh_user.set(s.get("user", ""))
        self.v_ssh_pass.set(s.get("password", ""))
        self.v_ssh_key.set(s.get("key_file", ""))
        self.v_db_host.set(s.get("db_host", "127.0.0.1"))
        self.v_db_port.set(s.get("db_port", "3306"))
        # Load MySQL credentials from SSH section
        self.v_db_user.set(s.get("db_user", d.get("user", "acore")))
        self.v_db_pass.set(s.get("db_password", d.get("password", "acore")))
        pref = cfg["preferences"] if cfg.has_section("preferences") else {}
        pref_id = pref.get("preferred_game_account_id", "").strip()
        self._preferred_game_account_id = int(pref_id) if pref_id.isdigit() else None
        self._auto_connect_enabled = pref.get("auto_connect_on_startup", "0") == "1"
        self.v_ssh_collapsed.set(pref.get("ssh_collapsed", "0") == "1")
        self._toggle_ssh_fields()

    def _save_config(self):
        cfg = configparser.ConfigParser()
        cfg["database"] = {
            "host": self.v_host.get(), "port": self.v_port.get(),
            "user": self.v_user.get(), "password": self.v_pass.get(),
        }
        cfg["ssh"] = {
            "enabled": "1" if self.v_ssh_enabled.get() else "0",
            "host": self.v_ssh_host.get(),
            "port": self.v_ssh_port.get(),
            "user": self.v_ssh_user.get(),
            "password": self.v_ssh_pass.get(),
            "key_file": self.v_ssh_key.get(),
            "db_host": self.v_db_host.get(),
            "db_port": self.v_db_port.get(),
            "db_user": self.v_db_user.get(),
            "db_password": self.v_db_pass.get(),
        }
        cfg["preferences"] = {
            "preferred_game_account_id": str(self._preferred_game_account_id or ""),
            "auto_connect_on_startup": "1" if self._auto_connect_enabled else "0",
            "ssh_collapsed": "1" if self.v_ssh_collapsed.get() else "0",
        }
        with open(CONFIG_FILE, "w") as f:
            cfg.write(f)

    def get_preferred_game_account(self) -> int | None:
        return self._preferred_game_account_id

    def set_preferred_game_account(self, account_id: int | None):
        self._preferred_game_account_id = account_id
        self._save_config()

    def _build_connection_bar(self):
        bar = ttk.Frame(self, padding=(6, 4))
        bar.pack(side=tk.TOP, fill=tk.X)

        # ── Single collapsible Connection group ────────────────────────────
        conn_frame = ttk.LabelFrame(bar, text="Connection", padding=(4, 2))
        conn_frame.pack(fill=tk.X)
        self._conn_frame = conn_frame
        self.v_ssh_collapsed = tk.BooleanVar(value=False)

        # Header row — collapse button lives here
        hdr_row = ttk.Frame(conn_frame)
        hdr_row.pack(fill=tk.X, pady=(0, 2))

        self._conn_collapse_btn = ttk.Button(
            hdr_row, text="[V]", width=4,
            command=self._toggle_ssh_collapsed)
        self._conn_collapse_btn.pack(side=tk.RIGHT, padx=(4, 4))

        # ── Body frame (everything that collapses) ─────────────────────────
        self._conn_body = ttk.Frame(conn_frame)
        self._conn_body.pack(fill=tk.X)

        # Direct MySQL row
        direct_row = ttk.Frame(self._conn_body)
        direct_row.pack(fill=tk.X, pady=(0, 2))
        self._direct_row = direct_row

        self.v_host   = tk.StringVar()
        self.v_port   = tk.StringVar()
        self.v_user   = tk.StringVar()
        self.v_pass   = tk.StringVar()
        self.v_status = tk.StringVar(value="Not connected")

        ttk.Label(direct_row, text="MySQL:", foreground="green").pack(side=tk.LEFT, padx=(4, 4))
        self._direct_widgets = []
        for label, var, w, show in [
            ("Host:", self.v_host, 16, ""),
            ("Port:", self.v_port, 6,  ""),
            ("User:", self.v_user, 10, ""),
            ("Pass:", self.v_pass, 10, "*"),
        ]:
            lbl = ttk.Label(direct_row, text=label)
            lbl.pack(side=tk.LEFT, padx=(4, 1))
            ent = ttk.Entry(direct_row, textvariable=var, width=w, show=show)
            ent.pack(side=tk.LEFT, padx=(0, 6))
            self._direct_widgets.extend([lbl, ent])

        ttk.Button(direct_row, text="Connect", command=self._connect).pack(side=tk.LEFT, padx=4)
        self._status_lbl = ttk.Label(direct_row, textvariable=self.v_status, foreground="red")
        self._status_lbl.pack(side=tk.LEFT, padx=8)

        # SSH checkbox row
        check_row = ttk.Frame(self._conn_body)
        check_row.pack(fill=tk.X, pady=(0, 2))
        self._ssh_check_row = check_row

        self.v_ssh_enabled = tk.BooleanVar(value=False)
        ssh_check = ttk.Checkbutton(check_row, text="Enable SSH Tunnel",
                                    variable=self.v_ssh_enabled,
                                    command=self._toggle_ssh_fields)
        ssh_check.pack(side=tk.LEFT, padx=(4, 8))

        if not SSH_TUNNEL_AVAILABLE:
            ssh_check.configure(state="disabled")
            ttk.Label(check_row, text="⚠ Package not installed: pip install sshtunnel",
                      foreground="orange").pack(side=tk.LEFT)
        else:
            ttk.Label(check_row,
                      text="→ When enabled, connects through a jump host to reach the database",
                      foreground="gray").pack(side=tk.LEFT)

        # SSH credentials row
        ssh_row1 = ttk.Frame(self._conn_body)
        ssh_row1.pack(fill=tk.X, pady=1)
        self._ssh_row1 = ssh_row1

        self.v_ssh_host = tk.StringVar()
        self.v_ssh_port = tk.StringVar()
        self.v_ssh_user = tk.StringVar()
        self.v_ssh_pass = tk.StringVar()
        self.v_ssh_key  = tk.StringVar()

        self._ssh_widgets = []
        ttk.Label(ssh_row1, text="SSH:", foreground="blue").pack(side=tk.LEFT, padx=(4, 4))
        for label, var, w, show in [
            ("Host:", self.v_ssh_host, 18, ""),
            ("Port:", self.v_ssh_port, 5,  ""),
            ("User:", self.v_ssh_user, 10, ""),
            ("Pass:", self.v_ssh_pass, 10, "*"),
        ]:
            lbl = ttk.Label(ssh_row1, text=label)
            lbl.pack(side=tk.LEFT, padx=(4, 1))
            ent = ttk.Entry(ssh_row1, textvariable=var, width=w, show=show)
            ent.pack(side=tk.LEFT, padx=(0, 4))
            self._ssh_widgets.extend([lbl, ent])

        # SSH key file row
        ssh_row2 = ttk.Frame(self._conn_body)
        ssh_row2.pack(fill=tk.X, pady=1)
        self._ssh_row2 = ssh_row2

        ttk.Label(ssh_row2, text="SSH:", foreground="blue").pack(side=tk.LEFT, padx=(4, 4))
        lbl_key = ttk.Label(ssh_row2, text="Key File:")
        lbl_key.pack(side=tk.LEFT, padx=(4, 1))
        ent_key = ttk.Entry(ssh_row2, textvariable=self.v_ssh_key, width=40)
        ent_key.pack(side=tk.LEFT, padx=(0, 2))
        btn_browse = ttk.Button(ssh_row2, text="Browse...", command=self._browse_ssh_key)
        btn_browse.pack(side=tk.LEFT, padx=2)
        lbl_hint = ttk.Label(ssh_row2, text="(leave blank to use password)", foreground="gray")
        lbl_hint.pack(side=tk.LEFT, padx=(4, 0))
        self._ssh_widgets.extend([lbl_key, ent_key, btn_browse, lbl_hint])

        # Remote MySQL row
        ssh_row3 = ttk.Frame(self._conn_body)
        ssh_row3.pack(fill=tk.X, pady=1)
        self._ssh_row3 = ssh_row3

        self.v_db_host = tk.StringVar()
        self.v_db_port = tk.StringVar()
        self.v_db_user = tk.StringVar()
        self.v_db_pass = tk.StringVar()

        ttk.Label(ssh_row3, text="MySQL:", foreground="green").pack(side=tk.LEFT, padx=(4, 4))
        for label, var, w, show in [
            ("Host:", self.v_db_host, 18, ""),
            ("Port:", self.v_db_port, 5,  ""),
            ("User:", self.v_db_user, 10, ""),
            ("Pass:", self.v_db_pass, 10, "*"),
        ]:
            lbl = ttk.Label(ssh_row3, text=label)
            lbl.pack(side=tk.LEFT, padx=(4, 1))
            ent = ttk.Entry(ssh_row3, textvariable=var, width=w, show=show)
            ent.pack(side=tk.LEFT, padx=(0, 4))
            self._ssh_widgets.extend([lbl, ent])

        lbl_mysql_hint = ttk.Label(ssh_row3, text="(MySQL server as seen from SSH host)",
                                    foreground="gray")
        lbl_mysql_hint.pack(side=tk.LEFT, padx=(4, 0))
        self._ssh_widgets.append(lbl_mysql_hint)

    def _toggle_ssh_collapsed(self):
        self.v_ssh_collapsed.set(not self.v_ssh_collapsed.get())
        self._apply_ssh_collapsed_state()
        self._save_config()

    def _apply_ssh_collapsed_state(self):
        collapsed = self.v_ssh_collapsed.get()
        self._conn_collapse_btn.configure(text="[>]" if collapsed else "[V]")
        if collapsed:
            self._conn_body.pack_forget()
        else:
            if not self._conn_body.winfo_manager():
                self._conn_body.pack(fill=tk.X)

    def _toggle_ssh_fields(self):
        """Enable/disable SSH fields and direct connection based on mode."""
        ssh_enabled = self.v_ssh_enabled.get()

        # Enable/disable SSH fields
        ssh_state = "normal" if ssh_enabled else "disabled"
        for w in self._ssh_widgets:
            w.configure(state=ssh_state)

        # Show/hide direct connection fields
        direct_state = "disabled" if ssh_enabled else "normal"
        for w in self._direct_widgets:
            w.configure(state=direct_state)

        # Auto-populate direct connection from SSH MySQL fields when enabling SSH
        if ssh_enabled:
            # When using SSH tunnel, direct connection always goes to localhost
            self.v_host.set("127.0.0.1")
            self.v_port.set("3306")
            # Copy MySQL credentials from SSH section to direct section
            if self.v_db_user.get():
                self.v_user.set(self.v_db_user.get())
            if self.v_db_pass.get():
                self.v_pass.set(self.v_db_pass.get())

        self._apply_ssh_collapsed_state()

    def _browse_ssh_key(self):
        """Browse for SSH private key file."""
        filename = filedialog.askopenfilename(
            title="Select SSH Private Key",
            filetypes=[("All files", "*"), ("PEM files", "*.pem"), ("Key files", "id_rsa")]
        )
        if filename:
            self.v_ssh_key.set(filename)

    def _build_notebook(self):
        self.nb = ttk.Notebook(self)
        self.nb.pack(fill=tk.BOTH, expand=True, padx=6, pady=6)
        self.tab_hazard_config = HazardConfigTab(self.nb)
        self.tab_defaults = DefaultProfilesTab(self.nb)
        self.tab_profiles = BotProfilesTab(self.nb)
        self.tab_character_editor = CharacterEditorTab(self.nb)
        self.tab_accounts = AccountsTab(self.nb)
        self.tab_diagnostics = DiagnosticsTab(self.nb)
        self.nb.add(self.tab_hazard_config,    text="  🌐 Global Rules  ")
        self.nb.add(self.tab_defaults,         text="  Class Defaults  ")
        self.nb.add(self.tab_profiles,         text="  Bot Profiles  ")
        self.nb.add(self.tab_character_editor, text="  Character Editor  ")
        self.nb.add(self.tab_accounts,         text="  Accounts  ")
        self.nb.add(self.tab_diagnostics,      text="  🔧 Diagnostics  ")

    def _connect(self):
        try:
            db.disconnect()

            # When SSH is enabled, use MySQL credentials from SSH section
            mysql_user = self.v_db_user.get() if self.v_ssh_enabled.get() else self.v_user.get()
            mysql_pass = self.v_db_pass.get() if self.v_ssh_enabled.get() else self.v_pass.get()

            db.connect(
                host=self.v_host.get(),
                port=int(self.v_port.get()),
                user=mysql_user,
                password=mysql_pass,
                ssh_enabled=self.v_ssh_enabled.get(),
                ssh_host=self.v_ssh_host.get(),
                ssh_port=int(self.v_ssh_port.get()) if self.v_ssh_port.get() else 22,
                ssh_user=self.v_ssh_user.get(),
                ssh_password=self.v_ssh_pass.get(),
                ssh_key_file=self.v_ssh_key.get(),
                db_host=self.v_db_host.get() if self.v_db_host.get() else "127.0.0.1",
                db_port=int(self.v_db_port.get()) if self.v_db_port.get() else 3306
            )
            self._auto_connect_enabled = True
            self._save_config()
            self.tab_character_editor.clear_session_caches()
            tunnel_info = " (via SSH tunnel)" if self.v_ssh_enabled.get() else ""
            self.v_status.set(f"● Connected{tunnel_info}")
            self._status_lbl.configure(foreground="#1a7f1a")
            self.tab_defaults.refresh()
            self.tab_profiles.refresh()
            self.tab_character_editor.refresh()
            self.tab_accounts.refresh()
            self.tab_hazard_config.refresh()
            self.tab_diagnostics.refresh()
        except (MySQLError, ValueError, ImportError) as e:
            self.v_status.set(f"✗ {e}")
            self._status_lbl.configure(foreground="red")

    def on_close(self):
        self.tab_character_editor.clear_session_caches()
        db.disconnect()
        self.destroy()


# ═══════════════════════════════════════════════════════════════════════════
#  ENTRY POINT
# ═══════════════════════════════════════════════════════════════════════════

if __name__ == "__main__":
    app = App()
    app.protocol("WM_DELETE_WINDOW", app.on_close)
    app.mainloop()


def main():
    app = App()
    app.protocol("WM_DELETE_WINDOW", app.on_close)
    app.mainloop()
