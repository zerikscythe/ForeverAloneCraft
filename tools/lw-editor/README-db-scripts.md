# DB Query Scripts — How-To

Quick reference for running ad-hoc Python scripts against the ForeverAloneCraft
databases (the same pattern used throughout this session for schema inspection
and data verification).

---

## Prerequisites

Install `pymysql` once:

```
pip install pymysql
```

---

## Connection Details

| Setting  | Value           |
|----------|-----------------|
| Host     | `192.168.0.93`  |
| Port     | `3306`          |
| User     | `acore`         |
| Password | `acore`         |

Available databases:

| Database           | Contains                          |
|--------------------|-----------------------------------|
| `acore_world`      | Static world data, living-world profiles, hazard tables |
| `acore_characters` | Per-character runtime data, clones, quest status, items |
| `acore_auth`       | Account pool, bot account pool    |

---

## Minimal Script Template

```python
import pymysql

conn = pymysql.connect(
    host='192.168.0.93',
    port=3306,
    user='acore',
    password='acore',
    database='acore_world',   # change as needed
    autocommit=True           # set False if you need transaction control
)
cur = conn.cursor()

cur.execute("SELECT * FROM living_world_bot_combat_default_profile LIMIT 5")
for row in cur.fetchall():
    print(row)

conn.close()
```

Save as `tools/lw-editor/tmp_query.py` and run:

```
python tools/lw-editor/tmp_query.py
```

> **Convention:** use `tmp_query.py` for throwaway scripts. It is in `.gitignore`
> and will not accidentally be committed.

---

## Read-Only Query (safe to run any time)

```python
import pymysql

conn = pymysql.connect(host='192.168.0.93', port=3306,
                       user='acore', password='acore',
                       database='acore_world')
cur = conn.cursor()
cur.execute("""
    SELECT p.default_profile_id, p.spec_key, p.role_key, p.class_key,
           COUNT(e.entry_id) AS entries
    FROM   living_world_bot_combat_default_profile p
    LEFT   JOIN living_world_bot_combat_default_entry e
           ON e.default_profile_id = p.default_profile_id
    GROUP  BY p.default_profile_id
    ORDER  BY p.default_profile_id
""")
for row in cur.fetchall():
    print(row)
conn.close()
```

---

## Write Script with Transaction Control

Use `autocommit=False` and an explicit commit/rollback so you can inspect
`ROW_COUNT()` before committing.

```python
import pymysql

conn = pymysql.connect(host='192.168.0.93', port=3306,
                       user='acore', password='acore',
                       database='acore_world',
                       autocommit=False)
cur = conn.cursor()

try:
    cur.execute("""
        INSERT INTO living_world_hazard_auras (spell_id, severity, notes)
        VALUES (12345, 1.0, 'example fire patch')
        ON DUPLICATE KEY UPDATE severity = VALUES(severity)
    """)
    cur.execute("SELECT ROW_COUNT()")
    print("affected rows:", cur.fetchone()[0])

    conn.commit()
    print("Committed.")
except Exception as e:
    conn.rollback()
    print("ERROR - rolled back:", e)
finally:
    conn.close()
```

---

## Applying a Full SQL Migration File

```python
import pymysql

SQL_FILE = r"\\livingroom-pc\src\azerothcore-wotlk\data\sql\updates\pending_db_world\rev_living_world_007_class_specific_healer_profiles.sql"

conn = pymysql.connect(host='192.168.0.93', port=3306,
                       user='acore', password='acore',
                       database='acore_world',
                       autocommit=False)
cur = conn.cursor()

with open(SQL_FILE, encoding='utf-8') as f:
    raw = f.read()

# Split on ';' and skip blank / comment-only chunks
statements = []
for chunk in raw.split(';'):
    lines = [l for l in chunk.splitlines()
             if l.strip() and not l.strip().startswith('--')]
    if lines:
        statements.append(chunk.strip())

print(f"Found {len(statements)} statements")

errors = []
for i, stmt in enumerate(statements):
    try:
        cur.execute(stmt)
    except Exception as e:
        errors.append((i, stmt[:120], str(e)))

if errors:
    conn.rollback()
    print("ERRORS — rolled back:")
    for idx, s, err in errors:
        print(f"  [{idx}] {s!r} => {err}")
else:
    conn.commit()
    print("All statements committed.")

conn.close()
```

> **Note:** `PREPARE / EXECUTE / DEALLOCATE` statements used in some migrations
> for conditional DDL cannot be split on `;` naively. Apply those migrations
> via the MySQL CLI or a tool like DBeaver if the script runner hits a parse
> error on `PREPARE`.

---

## Useful One-Liners

```python
# List all living_world tables
cur.execute("""SELECT table_name FROM information_schema.tables
               WHERE table_schema = 'acore_world' AND table_name LIKE 'living_world%'
               ORDER BY table_name""")

# Show indexes on a table
cur.execute("SHOW INDEX FROM living_world_bot_combat_default_profile")

# Check MySQL version
cur.execute("SELECT VERSION()")

# Count rows across profile tables
cur.execute("""
    SELECT 'profiles' AS t, COUNT(*) FROM living_world_bot_combat_default_profile
    UNION ALL
    SELECT 'entries',       COUNT(*) FROM living_world_bot_combat_default_entry
    UNION ALL
    SELECT 'actions',       COUNT(*) FROM living_world_bot_combat_default_action
    UNION ALL
    SELECT 'conditions',    COUNT(*) FROM living_world_bot_combat_default_condition
""")
```

---

## Tips

- `autocommit=True` is the safest default for exploratory read/write scripts —
  no dangling transactions if the script crashes mid-run.
- Use `autocommit=False` when you need atomic multi-statement writes or want to
  inspect results before committing.
- `ROW_COUNT()` after an INSERT/UPDATE/DELETE tells you how many rows were
  actually changed (0 = hit `ON DUPLICATE KEY UPDATE` with no change, 1 = new
  insert, 2 = updated duplicate row).
- For DDL (`ALTER TABLE`, `CREATE TABLE`, `DROP INDEX`) MySQL auto-commits
  implicitly even inside a transaction — those cannot be rolled back.