"""
lw_editor — LivingWorld Bot Editor package.

Directory layout (relative to this file's parent = tools/lw-editor/):
  lw_editor/          ← this package
  data/               ← extracted DBC JSON caches
  config.ini          ← user connection settings
"""
import pathlib

# Absolute paths used throughout the package so that every module resolves
# files correctly regardless of the working directory.
EDITOR_DIR  = pathlib.Path(__file__).resolve().parent.parent   # tools/lw-editor/
DATA_DIR    = EDITOR_DIR / "data"
DBC_DIR     = EDITOR_DIR.parent.parent / "var" / "extractors" / "dbc"
CONFIG_FILE = str(EDITOR_DIR / "config.ini")

from .app import main  # noqa: E402  (imported for convenience)
