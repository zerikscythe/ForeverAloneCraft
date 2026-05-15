#!/usr/bin/env python3

from __future__ import annotations

from pathlib import Path
import sys
import traceback


SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from lw_zone_editor.zone_viewer import main


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception:
        traceback.print_exc()
        if sys.stdin is None or not sys.stdin.isatty():
            input("\n[lw-zone-editor] Press Enter to close...")
        raise