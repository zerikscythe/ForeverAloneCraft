import os
import shutil
import subprocess
import sys

HOST = "192.168.0.93"
PORT = "3306"
USER = "acore"
PASSWORD = "acore"
DATABASE = "acore_characters"

SQL = "SELECT name FROM updates WHERE name LIKE '%living_world%' ORDER BY name;"


def main() -> int:
    mysql_exe = shutil.which("mysql")
    if not mysql_exe:
        print("MYSQL_NOT_FOUND")
        return 2

    env = os.environ.copy()
    env["MYSQL_PWD"] = PASSWORD

    cmd = [
        mysql_exe,
        "-h", HOST,
        "-P", PORT,
        "-u", USER,
        "-D", DATABASE,
        "-N",
        "-B",
        "-e", SQL,
    ]

    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=15,
            env=env,
        )
    except Exception as exc:
        print(f"RUN_ERROR: {exc}")
        return 3

    if result.returncode != 0:
        err = (result.stderr or result.stdout).strip()
        print(f"MYSQL_ERROR: {err}")
        return 4

    print("LIVING_WORLD_UPDATES_BEGIN")
    output = result.stdout.strip()
    if output:
        print(output)
    print("LIVING_WORLD_UPDATES_END")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
