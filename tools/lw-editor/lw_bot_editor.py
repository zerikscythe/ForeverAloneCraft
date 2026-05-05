#!/usr/bin/env python3
"""
lw_bot_editor.py — entry point shim.

All source code now lives in the lw_editor/ package.
Run this file directly to launch the editor:
    python lw_bot_editor.py
"""
import warnings
warnings.filterwarnings('ignore', category=DeprecationWarning, module='paramiko')
warnings.filterwarnings('ignore', message='.*TripleDES.*')
warnings.filterwarnings('ignore', message='.*Blowfish.*')

from lw_editor import main  # noqa: E402

if __name__ == "__main__":
    main()
