"""
seed_world_bot_resume.py
------------------------
Seed or clear world-bot quest resume metadata in `living_world_bot_identity`.

This is a debug/prototyping helper so we can exercise quest resume behavior
before bots have naturally completed questing sessions.

Usage:
    python tools/seed_world_bot_resume.py --name RouteHarnessL20 --zone-id 40
    python tools/seed_world_bot_resume.py --identity-id 123 --zone-id 12 --show
    python tools/seed_world_bot_resume.py --name RouteHarnessL20 --clear

The script reads DB settings from `tools/lw-editor/config.ini`.
"""

from __future__ import annotations

import argparse
from pathlib import Path
import sys


TOOLS_ROOT = Path(__file__).resolve().parent
if str(TOOLS_ROOT) not in sys.path:
    sys.path.insert(0, str(TOOLS_ROOT))

from seed_bot_identities import connect_characters_db, load_config  # noqa: E402


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Seed or clear world-bot quest resume metadata."
    )
    target = parser.add_mutually_exclusive_group(required=True)
    target.add_argument("--identity-id", type=int, help="living_world_bot_identity.id")
    target.add_argument("--name", help="living_world_bot_identity.name")
    parser.add_argument("--zone-id", type=int, help="Quest zone to resume in")
    parser.add_argument(
        "--task-family",
        default="questing",
        help="Task family to seed, default: questing",
    )
    parser.add_argument(
        "--source-kind",
        default="task_template",
        help="Last session source kind, default: task_template",
    )
    parser.add_argument(
        "--source-key",
        default="quest_resume_seed",
        help="Last session source key, default: quest_resume_seed",
    )
    parser.add_argument(
        "--set-last-seen",
        action="store_true",
        help="Also set last_seen_zone to the provided zone id",
    )
    parser.add_argument(
        "--clear",
        action="store_true",
        help="Clear previously seeded resume metadata instead of setting it",
    )
    parser.add_argument(
        "--show",
        action="store_true",
        help="Print the row after the update",
    )
    return parser


def resolve_target_clause(args: argparse.Namespace) -> tuple[str, tuple[object, ...]]:
    if args.identity_id is not None:
        return "id = %s", (args.identity_id,)
    return "name = %s", (args.name,)


def ensure_resume_columns(cur) -> None:
    cur.execute("SHOW COLUMNS FROM living_world_bot_identity")
    existing = {row[0] for row in cur.fetchall()}
    wanted = [
        ("last_session_source_kind", "ALTER TABLE living_world_bot_identity ADD COLUMN last_session_source_kind VARCHAR(64) NOT NULL DEFAULT '' AFTER runtime_detail"),
        ("last_session_source_key", "ALTER TABLE living_world_bot_identity ADD COLUMN last_session_source_key VARCHAR(128) NOT NULL DEFAULT '' AFTER last_session_source_kind"),
        ("last_task_family", "ALTER TABLE living_world_bot_identity ADD COLUMN last_task_family VARCHAR(32) NOT NULL DEFAULT '' AFTER last_session_source_key"),
        ("last_task_target_zone", "ALTER TABLE living_world_bot_identity ADD COLUMN last_task_target_zone INT UNSIGNED NULL AFTER last_task_family"),
    ]
    for column_name, ddl in wanted:
        if column_name in existing:
            continue
        cur.execute(ddl)
        print(f"[resume-seed] added missing column: {column_name}")


def show_row(cur, where_sql: str, where_params: tuple[object, ...]) -> None:
    cur.execute(
        f"""
        SELECT id, name, level, last_seen_zone, last_session_source_kind,
               last_session_source_key, last_task_family, last_task_target_zone,
               runtime_state, runtime_detail
        FROM living_world_bot_identity
        WHERE {where_sql}
        LIMIT 1
        """,
        where_params,
    )
    row = cur.fetchone()
    if not row:
        print("[resume-seed] no matching bot found")
        return

    columns = [
        "id",
        "name",
        "level",
        "last_seen_zone",
        "last_session_source_kind",
        "last_session_source_key",
        "last_task_family",
        "last_task_target_zone",
        "runtime_state",
        "runtime_detail",
    ]
    print("[resume-seed] current bot state:")
    for key, value in zip(columns, row):
        print(f"  {key}: {value}")


def main() -> int:
    args = build_parser().parse_args()
    if not args.clear and not args.zone_id:
        raise SystemExit("--zone-id is required unless --clear is used")

    cfg = load_config()
    tunnel = None
    conn = None
    try:
        tunnel, conn = connect_characters_db(cfg)
        where_sql, where_params = resolve_target_clause(args)

        with conn.cursor() as cur:
            ensure_resume_columns(cur)
            if args.clear:
                cur.execute(
                    f"""
                    UPDATE living_world_bot_identity
                    SET last_session_source_kind = '',
                        last_session_source_key = '',
                        last_task_family = '',
                        last_task_target_zone = NULL
                    WHERE {where_sql}
                    LIMIT 1
                    """,
                    where_params,
                )
                print(f"[resume-seed] cleared resume metadata for {cur.rowcount} row(s)")
            else:
                if args.set_last_seen:
                    cur.execute(
                        f"""
                        UPDATE living_world_bot_identity
                        SET last_seen_zone = %s,
                            last_session_source_kind = %s,
                            last_session_source_key = %s,
                            last_task_family = %s,
                            last_task_target_zone = %s
                        WHERE {where_sql}
                        LIMIT 1
                        """,
                        (
                            args.zone_id,
                            args.source_kind,
                            args.source_key,
                            args.task_family,
                            args.zone_id,
                            *where_params,
                        ),
                    )
                else:
                    cur.execute(
                        f"""
                        UPDATE living_world_bot_identity
                        SET last_session_source_kind = %s,
                            last_session_source_key = %s,
                            last_task_family = %s,
                            last_task_target_zone = %s
                        WHERE {where_sql}
                        LIMIT 1
                        """,
                        (
                            args.source_kind,
                            args.source_key,
                            args.task_family,
                            args.zone_id,
                            *where_params,
                        ),
                    )
                print(
                    f"[resume-seed] seeded resume metadata for {cur.rowcount} row(s) "
                    f"zone={args.zone_id} task_family={args.task_family}"
                )

            show_row(cur, where_sql, where_params)

        return 0
    finally:
        if conn is not None:
            conn.close()
        if tunnel is not None:
            tunnel.stop()


if __name__ == "__main__":
    raise SystemExit(main())
