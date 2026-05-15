from __future__ import annotations

from configparser import ConfigParser
from dataclasses import dataclass
from pathlib import Path

from .paths import APP_ROOT, DATA_DIR


@dataclass(frozen=True, slots=True)
class RouteSamplingSettings:
    base_spacing_yards: float
    min_spacing_yards: float
    max_spacing_yards: float

    @property
    def min_factor(self) -> float:
        return self.min_spacing_yards / self.base_spacing_yards

    @property
    def max_factor(self) -> float:
        return self.max_spacing_yards / self.base_spacing_yards


@dataclass(frozen=True, slots=True)
class RouteStorageSettings:
    route_data_root: Path
    editor_routes_subdir: str
    exported_routes_subdir: str

    @property
    def editor_routes_dir(self) -> Path:
        return self.route_data_root / self.editor_routes_subdir

    @property
    def exported_routes_dir(self) -> Path:
        return self.route_data_root / self.exported_routes_subdir


@dataclass(frozen=True, slots=True)
class DatabaseSettings:
    host: str
    port: int
    user: str
    password: str
    database: str
    mysql_binary: str


DEFAULT_ROUTE_SAMPLING_SETTINGS = RouteSamplingSettings(
    base_spacing_yards=25.0,
    min_spacing_yards=5.0,
    max_spacing_yards=50.0,
)

DEFAULT_ROUTE_STORAGE_SETTINGS = RouteStorageSettings(
    route_data_root=DATA_DIR,
    editor_routes_subdir="editor_routes",
    exported_routes_subdir="exported_routes",
)

DEFAULT_DATABASE_SETTINGS = DatabaseSettings(
    host="",
    port=3306,
    user="",
    password="",
    database="acore_world",
    mysql_binary="mysql",
)

CONFIG_PATH = APP_ROOT / "config.ini"


def _load_parser(config_path: Path) -> ConfigParser:
    parser = ConfigParser()
    try:
        parser.read(config_path, encoding="utf-8")
    except OSError:
        pass
    return parser


def _parse_positive_float(parser: ConfigParser, section: str, option: str, fallback: float) -> float:
    try:
        value = parser.getfloat(section, option, fallback=fallback)
    except ValueError:
        return fallback
    return value if value > 0.0 else fallback


def _parse_positive_int(parser: ConfigParser, section: str, option: str, fallback: int) -> int:
    try:
        value = parser.getint(section, option, fallback=fallback)
    except ValueError:
        return fallback
    return value if value > 0 else fallback


def _parse_string(parser: ConfigParser, section: str, option: str, fallback: str) -> str:
    try:
        value = parser.get(section, option, fallback=fallback)
    except ValueError:
        return fallback
    return value.strip() or fallback


def _parse_optional_string(parser: ConfigParser, section: str, option: str, fallback: str = "") -> str:
    try:
        value = parser.get(section, option, fallback=fallback)
    except ValueError:
        return fallback
    return value.strip()


def _resolve_directory(path_text: str, *, base_dir: Path) -> Path:
    path = Path(path_text)
    if not path.is_absolute():
        path = (base_dir / path).resolve()
    return path


def load_route_sampling_settings(
    config_path: Path = CONFIG_PATH,
    defaults: RouteSamplingSettings = DEFAULT_ROUTE_SAMPLING_SETTINGS,
) -> RouteSamplingSettings:
    parser = _load_parser(config_path)

    section = "route_sampling"
    base_spacing_yards = _parse_positive_float(parser, section, "base_spacing_yards", defaults.base_spacing_yards)
    min_spacing_yards = _parse_positive_float(parser, section, "min_spacing_yards", defaults.min_spacing_yards)
    max_spacing_yards = _parse_positive_float(parser, section, "max_spacing_yards", defaults.max_spacing_yards)

    min_spacing_yards = max(1.0, min(min_spacing_yards, base_spacing_yards))
    max_spacing_yards = max(base_spacing_yards, max_spacing_yards)
    return RouteSamplingSettings(
        base_spacing_yards=base_spacing_yards,
        min_spacing_yards=min_spacing_yards,
        max_spacing_yards=max_spacing_yards,
    )


def load_route_storage_settings(
    config_path: Path = CONFIG_PATH,
    defaults: RouteStorageSettings = DEFAULT_ROUTE_STORAGE_SETTINGS,
) -> RouteStorageSettings:
    parser = _load_parser(config_path)
    section = "route_storage"
    root_text = _parse_string(parser, section, "route_data_root", str(defaults.route_data_root))
    editor_subdir = _parse_string(parser, section, "editor_routes_subdir", defaults.editor_routes_subdir)
    exported_subdir = _parse_string(parser, section, "exported_routes_subdir", defaults.exported_routes_subdir)
    return RouteStorageSettings(
        route_data_root=_resolve_directory(root_text, base_dir=APP_ROOT),
        editor_routes_subdir=editor_subdir,
        exported_routes_subdir=exported_subdir,
    )


def load_database_settings(
    config_path: Path = CONFIG_PATH,
    defaults: DatabaseSettings = DEFAULT_DATABASE_SETTINGS,
) -> DatabaseSettings:
    parser = _load_parser(config_path)
    section = "database"
    return DatabaseSettings(
        host=_parse_optional_string(parser, section, "host", defaults.host),
        port=_parse_positive_int(parser, section, "port", defaults.port),
        user=_parse_optional_string(parser, section, "user", defaults.user),
        password=_parse_optional_string(parser, section, "password", defaults.password),
        database=_parse_string(parser, section, "database", defaults.database),
        mysql_binary=_parse_string(parser, section, "mysql_binary", defaults.mysql_binary),
    )


ROUTE_SAMPLING_SETTINGS = load_route_sampling_settings()
ROUTE_STORAGE_SETTINGS = load_route_storage_settings()
DATABASE_SETTINGS = load_database_settings()
