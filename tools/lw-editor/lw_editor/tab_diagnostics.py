"""
tab_diagnostics.py -- Diagnostics & Cleanup tab for runtime / bot-pool health.

Covers the common "crashed server left stale state" scenarios:
  - bot pool accounts marked online=1 but no live session
  - bot pool accounts locked (is_available=0) by a runtime that is no longer
    actively running
  - runtimes stuck in Active state whose clones are no longer in the DB
  - source characters still carrying their parked placeholder name even though
    the clone is offline / gone
  - orphaned clone characters on bot accounts with no matching runtime

Fix actions the tool can safely perform **offline** (server must be stopped
or the affected characters must not be loaded):
  - Reset stale online flags
  - Release locked pool slots
  - Restore parked source names
  - Force-retire a stuck runtime (runs the safe additive DB syncs then
    marks the runtime Failed so the next request recreates it cleanly)
"""
from __future__ import annotations

import datetime
import tkinter as tk
from tkinter import ttk, messagebox
from typing import Any

from .db import db

# ---------------------------------------------------------------------------
# State enum mapping  (mirrors model/AccountAltRuntime.h)
# ---------------------------------------------------------------------------
_STATE_NAMES = {
    0: "PreparingClone",
    1: "Active",
    2: "SyncingBack",
    3: "Recovering",
    4: "Failed",
    5: "SyncingEquipment",
    6: "SyncingInventory",
    7: "SyncingBank",
}


def _state_name(v) -> str:
    try:
        return _STATE_NAMES.get(int(v), f"Unknown({v})")
    except (TypeError, ValueError):
        return str(v)


def _build_reserved_name(source_guid: int) -> str:
    """Python mirror of AccountAltRuntimeService::BuildReservedSourceName."""
    name = "Lw"
    value = source_guid
    pos = 0
    while len(name) < 12:
        name += chr(ord("a") + (value + pos * 7) % 26)
        value //= 26
        pos += 1
    return name


# ---------------------------------------------------------------------------
# Diagnostic queries
# ---------------------------------------------------------------------------

def _load_runtimes() -> list[dict]:
    """Load all runtimes, join source / clone names from characters."""
    sql = """
        SELECT
            r.runtime_id,
            r.source_account_id,
            r.source_character_guid,
            r.clone_character_guid,
            r.clone_account_id,
            r.state,
            r.source_character_name,
            r.reserved_source_character_name,
            r.clone_character_name,
            r.updated_at,
            sc.name   AS source_db_name,
            cc.name   AS clone_db_name
        FROM living_world_account_alt_runtime r
        LEFT JOIN characters sc ON sc.guid = r.source_character_guid
        LEFT JOIN characters cc ON cc.guid = r.clone_character_guid
        ORDER BY r.updated_at DESC
    """
    return db.q(db.chars, sql)


def _load_pool_accounts() -> list[dict]:
    """Load bot pool accounts with auth.account online flag."""
    sql = """
        SELECT
            p.account_id,
            p.account_name,
            p.is_enabled,
            p.is_available,
            p.reserved_for,
            p.assigned_source_account_id,
            p.assigned_source_character_guid,
            a.online   AS auth_online
        FROM living_world_bot_account_pool p
        LEFT JOIN account a ON a.id = p.account_id
        ORDER BY p.account_id
    """
    return db.q(db.auth, sql)


def _load_orphaned_clones(pool_account_ids: list[int]) -> list[dict]:
    """Characters on bot accounts that have NO matching runtime record."""
    if not pool_account_ids:
        return []
    placeholders = ",".join(["%s"] * len(pool_account_ids))
    sql = f"""
        SELECT c.guid, c.name, c.account, c.level
        FROM characters c
        LEFT JOIN living_world_account_alt_runtime r
              ON r.clone_character_guid = c.guid
        WHERE c.account IN ({placeholders})
          AND r.runtime_id IS NULL
    """
    return db.q(db.chars, sql, pool_account_ids)


# ---------------------------------------------------------------------------
# Fix operations (each returns (changed: int, log_lines: list[str]))
# ---------------------------------------------------------------------------

def fix_stale_online_flags(pool_rows: list[dict]) -> tuple[int, list[str]]:
    stale = [r for r in pool_rows if r.get("auth_online") == 1]
    log = []
    changed = 0
    for r in stale:
        db.run(db.auth,
               "UPDATE account SET online = 0 WHERE id = %s",
               (r["account_id"],))
        log.append(f"  Reset account {r['account_id']} ({r['account_name']}) online -> 0")
        changed += 1
    return changed, log


def fix_locked_pool_slots(pool_rows: list[dict], runtime_rows: list[dict]) -> tuple[int, list[str]]:
    """Release is_available=0 slots whose runtimes are no longer actively running."""
    log = []
    changed = 0
    # Build a set of clone_account_ids that belong to runtimes still considered active
    # (state = Active/PreparingClone/Recovering/Syncing*).  We only release accounts
    # that are locked AND whose runtime is not in one of those in-progress states, OR
    # have no runtime at all.
    active_states = {0, 1, 2, 3, 5, 6, 7}
    live_account_ids: set[int] = set()
    for rt in runtime_rows:
        if int(rt.get("state", 4)) in active_states:
            if rt.get("clone_account_id"):
                live_account_ids.add(int(rt["clone_account_id"]))

    for r in pool_rows:
        if r.get("is_available") == 1:
            continue  # already free
        acct_id = int(r["account_id"])
        if acct_id in live_account_ids:
            continue  # runtime is legitimately in progress
        db.run(db.auth,
               "UPDATE living_world_bot_account_pool "
               "SET is_available = 1, reserved_for = NULL WHERE account_id = %s",
               (acct_id,))
        log.append(f"  Released pool slot for account {acct_id} ({r['account_name']})")
        changed += 1
    return changed, log


def fix_restore_parked_name(runtime: dict) -> tuple[bool, str]:
    """
    Restore the source character's original visible name if:
      - the source character is currently carrying the placeholder name
      - the clone is offline (not in characters table or known offline)
    Returns (success, message).
    """
    source_guid = runtime.get("source_character_guid")
    source_original = runtime.get("source_character_name", "")
    reserved_name  = runtime.get("reserved_source_character_name", "")
    source_db_name = runtime.get("source_db_name", "")

    if not source_guid or not source_original or not reserved_name:
        return False, "Runtime is missing name lease fields — cannot restore."

    if source_db_name != reserved_name:
        return False, (
            f"Source char '{source_db_name}' is not carrying the parked name "
            f"'{reserved_name}' — no rename needed."
        )

    # Check the clone is genuinely offline
    clone_guid = runtime.get("clone_character_guid")
    if clone_guid:
        rows = db.q(db.chars,
                    "SELECT guid FROM characters WHERE guid = %s LIMIT 1",
                    (clone_guid,))
        if rows:
            # Clone still exists — might be live; refuse to rename
            return False, (
                f"Clone (guid {clone_guid}) still exists in DB. "
                "Dismiss the bot normally or delete the clone first."
            )

    # Restore
    db.run(db.chars,
           "UPDATE characters SET name = %s WHERE guid = %s",
           (source_original, source_guid))
    return True, (
        f"Renamed source guid {source_guid}: '{reserved_name}' -> '{source_original}'"
    )


def fix_force_retire_runtime(runtime: dict) -> tuple[bool, list[str]]:
    """
    Offline-safe partial sync + retire for a stuck runtime.
    Performs additive syncs (reputation, quests, achievements, spells, skills)
    that use INSERT IGNORE / GREATEST and are therefore safe without sanity
    checks.  Skips inventory/equipment — those need the in-game recovery path.
    Then marks the runtime state=4 (Failed) and releases the pool slot so the
    next .lwbot request recreates everything cleanly.

    The server's OnPlayerLogin startup recovery will handle the full sync
    (including progress + items) the next time the source character logs in.
    """
    log = []
    runtime_id  = runtime.get("runtime_id")
    source_guid = runtime.get("source_character_guid")
    clone_guid  = runtime.get("clone_character_guid")
    clone_acct  = runtime.get("clone_account_id")

    if not source_guid or not clone_guid:
        return False, ["Runtime is incomplete — cannot retire."]

    log.append(f"Retiring runtime {runtime_id}: "
               f"source={source_guid} clone={clone_guid}")

    # ── Additive syncs (INSERT IGNORE or GREATEST — safe offline) ──────────

    # Reputation: max of the two standings
    try:
        db.run(db.chars, """
            INSERT INTO character_reputation (guid, faction, standing, flags)
            SELECT %s, cr.faction,
                   GREATEST(cr.standing,
                            COALESCE((SELECT standing FROM character_reputation
                                      WHERE guid = %s AND faction = cr.faction), 0)),
                   cr.flags
            FROM character_reputation cr WHERE cr.guid = %s
            ON DUPLICATE KEY UPDATE
                standing = GREATEST(standing, VALUES(standing))
        """, (source_guid, source_guid, clone_guid))
        log.append("  Reputation synced (GREATEST merge).")
    except Exception as e:
        log.append(f"  Reputation sync skipped: {e}")

    # Quests: INSERT IGNORE for completed quests
    try:
        db.run(db.chars, """
            INSERT IGNORE INTO character_queststatus_rewarded (guid, quest)
            SELECT %s, quest FROM character_queststatus_rewarded WHERE guid = %s
        """, (source_guid, clone_guid))
        log.append("  Completed quests synced (INSERT IGNORE).")
    except Exception as e:
        log.append(f"  Quest sync skipped: {e}")

    # Achievements
    try:
        db.run(db.chars, """
            INSERT IGNORE INTO character_achievement (guid, achievement, date)
            SELECT %s, achievement, date
            FROM character_achievement WHERE guid = %s
        """, (source_guid, clone_guid))
        log.append("  Achievements synced (INSERT IGNORE).")
    except Exception as e:
        log.append(f"  Achievement sync skipped: {e}")

    # Spells
    try:
        db.run(db.chars, """
            INSERT IGNORE INTO character_spell (guid, spell, specMask)
            SELECT %s, spell, specMask FROM character_spell WHERE guid = %s
        """, (source_guid, clone_guid))
        log.append("  Spells synced (INSERT IGNORE).")
    except Exception as e:
        log.append(f"  Spell sync skipped: {e}")

    # Skills: GREATEST rank / step
    try:
        db.run(db.chars, """
            INSERT INTO character_skills (guid, skill, value, max)
            SELECT %s, skill,
                   GREATEST(value,
                            COALESCE((SELECT value FROM character_skills
                                      WHERE guid = %s AND skill = cs.skill), 0)),
                   GREATEST(max,
                            COALESCE((SELECT max FROM character_skills
                                      WHERE guid = %s AND skill = cs.skill), 0))
            FROM character_skills cs WHERE cs.guid = %s
            ON DUPLICATE KEY UPDATE
                value = GREATEST(value, VALUES(value)),
                max   = GREATEST(max,   VALUES(max))
        """, (source_guid, source_guid, source_guid, clone_guid))
        log.append("  Skills synced (GREATEST merge).")
    except Exception as e:
        log.append(f"  Skill sync skipped: {e}")

    # ── Restore parked name if needed ──────────────────────────────────────
    ok, msg = fix_restore_parked_name(runtime)
    log.append(f"  Name: {msg}")

    # ── Mark runtime Failed + release pool slot ────────────────────────────
    try:
        db.run(db.chars,
               "UPDATE living_world_account_alt_runtime SET state = 4 WHERE runtime_id = %s",
               (runtime_id,))
        log.append(f"  Runtime {runtime_id} marked Failed.")
    except Exception as e:
        log.append(f"  Could not update runtime state: {e}")

    if clone_acct:
        try:
            db.run(db.auth,
                   "UPDATE living_world_bot_account_pool "
                   "SET is_available = 1, reserved_for = NULL WHERE account_id = %s",
                   (clone_acct,))
            log.append(f"  Pool slot {clone_acct} released.")
        except Exception as e:
            log.append(f"  Could not release pool slot: {e}")

    return True, log


# ---------------------------------------------------------------------------
# DiagnosticsTab
# ---------------------------------------------------------------------------

class DiagnosticsTab(ttk.Frame):

    def __init__(self, parent, **kw):
        super().__init__(parent, **kw)
        self._runtime_rows: list[dict] = []
        self._pool_rows:    list[dict] = []
        self._orphan_rows:  list[dict] = []
        self._build()

    # ── Layout ──────────────────────────────────────────────────────────────

    def _build(self):
        # ── Top toolbar ────────────────────────────────────────────────────
        toolbar = ttk.Frame(self)
        toolbar.pack(fill=tk.X, padx=8, pady=(6, 2))

        ttk.Button(toolbar, text="Scan", command=self.refresh).pack(side=tk.LEFT, padx=4)

        ttk.Separator(toolbar, orient="vertical").pack(side=tk.LEFT, fill=tk.Y, padx=6)

        ttk.Button(toolbar, text="Fix: Stale Online Flags",
                   command=self._fix_online_flags).pack(side=tk.LEFT, padx=4)
        ttk.Button(toolbar, text="Fix: Locked Pool Slots",
                   command=self._fix_pool_slots).pack(side=tk.LEFT, padx=4)
        ttk.Button(toolbar, text="Restore Parked Name (selected)",
                   command=self._fix_restore_name).pack(side=tk.LEFT, padx=4)
        ttk.Button(toolbar, text="Force Retire Runtime (selected)",
                   command=self._fix_retire_runtime).pack(side=tk.LEFT, padx=4)

        self._status_lbl = ttk.Label(toolbar, text="Not scanned yet.", foreground="gray")
        self._status_lbl.pack(side=tk.LEFT, padx=12)

        # ── Main paned window ───────────────────────────────────────────────
        pw = ttk.PanedWindow(self, orient=tk.VERTICAL)
        pw.pack(fill=tk.BOTH, expand=True, padx=8, pady=4)

        tables_frame = ttk.Frame(pw)
        pw.add(tables_frame, weight=3)

        log_frame = ttk.LabelFrame(pw, text="Action Log")
        pw.add(log_frame, weight=1)

        # ── Horizontal split: runtimes | pool ──────────────────────────────
        hpw = ttk.PanedWindow(tables_frame, orient=tk.HORIZONTAL)
        hpw.pack(fill=tk.BOTH, expand=True)

        # -- Runtimes table --
        rt_frame = ttk.LabelFrame(hpw, text="Account Alt Runtimes")
        hpw.add(rt_frame, weight=3)

        rt_cols = ("id", "source", "clone", "state", "issues")
        self._rt_tv = ttk.Treeview(rt_frame, columns=rt_cols, show="headings",
                                   selectmode="browse")
        self._rt_tv.heading("id",     text="Runtime")
        self._rt_tv.heading("source", text="Source Char")
        self._rt_tv.heading("clone",  text="Clone Char")
        self._rt_tv.heading("state",  text="State")
        self._rt_tv.heading("issues", text="Issues")
        self._rt_tv.column("id",     width=70,  anchor="center")
        self._rt_tv.column("source", width=150)
        self._rt_tv.column("clone",  width=150)
        self._rt_tv.column("state",  width=120)
        self._rt_tv.column("issues", width=280)
        self._rt_tv.tag_configure("ok",      foreground="#1a7f1a")
        self._rt_tv.tag_configure("warn",    foreground="#b07000")
        self._rt_tv.tag_configure("error",   foreground="#c0000c")
        self._rt_tv.tag_configure("pending", foreground="#0055cc")

        rt_sb = ttk.Scrollbar(rt_frame, orient=tk.VERTICAL,
                              command=self._rt_tv.yview)
        self._rt_tv.configure(yscrollcommand=rt_sb.set)
        rt_sb.pack(side=tk.RIGHT, fill=tk.Y)
        self._rt_tv.pack(fill=tk.BOTH, expand=True)

        # -- Pool table --
        pool_frame = ttk.LabelFrame(hpw, text="Bot Pool Accounts")
        hpw.add(pool_frame, weight=2)

        pool_cols = ("id", "name", "enabled", "available", "online", "assigned")
        self._pool_tv = ttk.Treeview(pool_frame, columns=pool_cols, show="headings",
                                     selectmode="browse")
        self._pool_tv.heading("id",        text="Acct ID")
        self._pool_tv.heading("name",      text="Name")
        self._pool_tv.heading("enabled",   text="Pool On")
        self._pool_tv.heading("available", text="Available")
        self._pool_tv.heading("online",    text="DB Online")
        self._pool_tv.heading("assigned",  text="Assigned To Char")
        self._pool_tv.column("id",        width=60,  anchor="center")
        self._pool_tv.column("name",      width=120)
        self._pool_tv.column("enabled",   width=62,  anchor="center")
        self._pool_tv.column("available", width=70,  anchor="center")
        self._pool_tv.column("online",    width=70,  anchor="center")
        self._pool_tv.column("assigned",  width=120, anchor="center")
        self._pool_tv.tag_configure("ok",    foreground="#1a7f1a")
        self._pool_tv.tag_configure("warn",  foreground="#b07000")
        self._pool_tv.tag_configure("error", foreground="#c0000c")

        pool_sb = ttk.Scrollbar(pool_frame, orient=tk.VERTICAL,
                                command=self._pool_tv.yview)
        self._pool_tv.configure(yscrollcommand=pool_sb.set)
        pool_sb.pack(side=tk.RIGHT, fill=tk.Y)
        self._pool_tv.pack(fill=tk.BOTH, expand=True)

        # -- Log area --
        self._log = tk.Text(log_frame, height=8, state=tk.DISABLED,
                            font=("Consolas", 9), wrap=tk.WORD,
                            background="#1e1e1e", foreground="#d4d4d4")
        log_sb = ttk.Scrollbar(log_frame, orient=tk.VERTICAL, command=self._log.yview)
        self._log.configure(yscrollcommand=log_sb.set)
        log_sb.pack(side=tk.RIGHT, fill=tk.Y)
        self._log.pack(fill=tk.BOTH, expand=True)

        self._log.tag_configure("ok",    foreground="#6a9955")
        self._log.tag_configure("warn",  foreground="#ce9178")
        self._log.tag_configure("error", foreground="#f44747")
        self._log.tag_configure("head",  foreground="#569cd6")

    # ── Refresh / scan ──────────────────────────────────────────────────────

    def refresh(self):
        if not db.ok():
            self._log_write("Not connected to database.", "error")
            return

        self._rt_tv.delete(*self._rt_tv.get_children())
        self._pool_tv.delete(*self._pool_tv.get_children())

        try:
            self._runtime_rows = _load_runtimes()
            self._pool_rows    = _load_pool_accounts()
            pool_ids = [int(r["account_id"]) for r in self._pool_rows]
            self._orphan_rows  = _load_orphaned_clones(pool_ids)
        except Exception as e:
            self._log_write(f"Scan error: {e}", "error")
            return

        issues_total = 0

        # -- Populate runtimes --
        for rt in self._runtime_rows:
            issues = self._classify_runtime_issues(rt)
            tag = "error" if issues else "ok"
            if not issues and int(rt.get("state", 0)) in (2, 3, 5, 6, 7):
                tag = "pending"
            self._rt_tv.insert("", tk.END,
                iid=str(rt["runtime_id"]),
                values=(
                    rt["runtime_id"],
                    rt.get("source_db_name") or rt.get("source_character_name", "?"),
                    rt.get("clone_db_name")  or rt.get("clone_character_name", "—"),
                    _state_name(rt.get("state")),
                    "  ".join(issues) if issues else "OK",
                ),
                tags=(tag,))
            issues_total += len(issues)

        # -- Populate pool --
        for pr in self._pool_rows:
            p_issues = self._classify_pool_issues(pr)
            tag = "error" if any("stale" in i or "STALE" in i for i in p_issues) \
                  else ("warn" if p_issues else "ok")
            self._pool_tv.insert("", tk.END,
                iid=f"pool_{pr['account_id']}",
                values=(
                    pr["account_id"],
                    pr.get("account_name", "?"),
                    "Yes" if pr.get("is_enabled") else "No",
                    "Yes" if pr.get("is_available") else "No",
                    "STALE!" if pr.get("auth_online") == 1 else "No",
                    str(pr.get("assigned_source_character_guid") or "—"),
                ),
                tags=(tag,))
            issues_total += len(p_issues)

        # -- Orphans in log --
        if self._orphan_rows:
            self._log_write(
                f"\n[Orphaned clones — characters on bot accounts with no runtime]\n",
                "warn")
            for o in self._orphan_rows:
                self._log_write(
                    f"  guid={o['guid']}  name={o['name']}  "
                    f"account={o['account']}  level={o['level']}\n",
                    "warn")
            issues_total += len(self._orphan_rows)

        ts = datetime.datetime.now().strftime("%H:%M:%S")
        summary = (
            f"Scanned {len(self._runtime_rows)} runtime(s), "
            f"{len(self._pool_rows)} pool account(s) — "
            f"{issues_total} issue(s) found.  [{ts}]"
        )
        self._status_lbl.configure(
            text=summary,
            foreground="#c0000c" if issues_total else "#1a7f1a")
        if issues_total == 0:
            self._log_write(f"Scan complete. No issues found. [{ts}]\n", "ok")
        else:
            self._log_write(f"\nScan complete: {issues_total} issue(s). [{ts}]\n", "warn")

    # ── Issue classifiers ────────────────────────────────────────────────────

    @staticmethod
    def _classify_runtime_issues(rt: dict) -> list[str]:
        issues = []
        state = int(rt.get("state", 0))
        clone_guid = rt.get("clone_character_guid")
        clone_db   = rt.get("clone_db_name")
        source_db  = rt.get("source_db_name")
        reserved   = rt.get("reserved_source_character_name", "")
        original   = rt.get("source_character_name", "")

        if state == 4:
            issues.append("state=Failed")

        if clone_guid and not clone_db:
            issues.append("clone missing from DB")

        if reserved and source_db and source_db == reserved and state == 4:
            issues.append("source name still parked")

        if reserved and source_db and source_db == reserved and not clone_guid:
            issues.append("source name parked, no clone")

        if state == 1 and clone_guid and not clone_db:
            issues.append("Active but clone gone — likely crash")

        return issues

    @staticmethod
    def _classify_pool_issues(pr: dict) -> list[str]:
        issues = []
        if pr.get("auth_online") == 1:
            issues.append("STALE online=1")
        if not pr.get("is_available") and not pr.get("reserved_for"):
            issues.append("locked but no reservation")
        return issues

    # ── Fix actions ──────────────────────────────────────────────────────────

    def _fix_online_flags(self):
        if not db.ok():
            return
        if not self._pool_rows:
            messagebox.showinfo("Diagnostics", "Run Scan first.")
            return
        stale = [r for r in self._pool_rows if r.get("auth_online") == 1]
        if not stale:
            self._log_write("No stale online flags found.\n", "ok")
            return
        if not messagebox.askyesno(
                "Fix: Stale Online Flags",
                f"Reset online=0 for {len(stale)} bot account(s)?\n\n"
                + "\n".join(f"  {r['account_id']} ({r['account_name']})"
                            for r in stale)):
            return
        self._log_write("[Fix: Stale Online Flags]\n", "head")
        changed, lines = fix_stale_online_flags(self._pool_rows)
        for l in lines:
            self._log_write(l + "\n", "ok")
        self._log_write(f"Done — {changed} account(s) reset.\n", "ok")
        self.refresh()

    def _fix_pool_slots(self):
        if not db.ok():
            return
        if not self._pool_rows:
            messagebox.showinfo("Diagnostics", "Run Scan first.")
            return
        self._log_write("[Fix: Locked Pool Slots]\n", "head")
        changed, lines = fix_locked_pool_slots(self._pool_rows, self._runtime_rows)
        if not lines:
            self._log_write("No locked slots needed releasing.\n", "ok")
            return
        for l in lines:
            self._log_write(l + "\n", "ok")
        self._log_write(f"Done — {changed} slot(s) released.\n", "ok")
        self.refresh()

    def _fix_restore_name(self):
        if not db.ok():
            return
        sel = self._rt_tv.selection()
        if not sel:
            messagebox.showinfo("Diagnostics", "Select a runtime row first.")
            return
        rt = self._find_runtime(int(sel[0]))
        if rt is None:
            return
        if not messagebox.askyesno(
                "Restore Parked Name",
                f"Attempt to restore the original name for source guid "
                f"{rt.get('source_character_guid')}?\n\n"
                f"Original: {rt.get('source_character_name')}\n"
                f"Current:  {rt.get('source_db_name')}"):
            return
        self._log_write("[Fix: Restore Parked Name]\n", "head")
        ok, msg = fix_restore_parked_name(rt)
        self._log_write(msg + "\n", "ok" if ok else "error")
        if ok:
            self.refresh()

    def _fix_retire_runtime(self):
        if not db.ok():
            return
        sel = self._rt_tv.selection()
        if not sel:
            messagebox.showinfo("Diagnostics", "Select a runtime row first.")
            return
        rt = self._find_runtime(int(sel[0]))
        if rt is None:
            return

        warn = (
            "This will:\n"
            "  1. Run safe additive syncs (reputation, quests, achievements,\n"
            "     spells, skills) from the CLONE back to the SOURCE character.\n"
            "  2. Restore the source character's original name if it is parked.\n"
            "  3. Mark the runtime as Failed so the next .lwbot request\n"
            "     creates a fresh clone.\n"
            "  4. Release the bot pool account.\n\n"
            "Inventory and equipment sync are NOT performed here — those\n"
            "will be handled by the server's startup recovery when the\n"
            "source character next logs in.\n\n"
            f"Runtime {rt.get('runtime_id')} — "
            f"source='{rt.get('source_character_name')}' "
            f"clone='{rt.get('clone_character_name')}'\n\n"
            "Continue?"
        )
        if not messagebox.askyesno("Force Retire Runtime", warn):
            return

        self._log_write("[Fix: Force Retire Runtime]\n", "head")
        ok, lines = fix_force_retire_runtime(rt)
        for l in lines:
            self._log_write(l + "\n", "ok" if ok else "warn")
        self._log_write(
            f"{'Done.' if ok else 'Finished with errors.'} "
            f"Runtime {rt.get('runtime_id')}\n",
            "ok" if ok else "error")
        self.refresh()

    # ── Helpers ──────────────────────────────────────────────────────────────

    def _find_runtime(self, runtime_id: int) -> dict | None:
        for rt in self._runtime_rows:
            if rt.get("runtime_id") == runtime_id:
                return rt
        return None

    def _log_write(self, text: str, tag: str = ""):
        self._log.configure(state=tk.NORMAL)
        if tag:
            self._log.insert(tk.END, text, tag)
        else:
            self._log.insert(tk.END, text)
        self._log.see(tk.END)
        self._log.configure(state=tk.DISABLED)
