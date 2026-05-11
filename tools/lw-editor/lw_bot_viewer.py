#!/usr/bin/env python3
"""
lw_bot_viewer.py — entry point shim for the dedicated realtime bot viewer.

Run directly with:
    python lw_bot_viewer.py
"""
import warnings
warnings.filterwarnings('ignore', category=DeprecationWarning, module='paramiko')
warnings.filterwarnings('ignore', message='.*TripleDES.*')
warnings.filterwarnings('ignore', message='.*Blowfish.*')

from lw_editor.bot_viewer import main  # noqa: E402


if __name__ == "__main__":
    main()