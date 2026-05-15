from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from lw_zone_editor.settings import (  # noqa: E402
    DEFAULT_DATABASE_SETTINGS,
    DEFAULT_ROUTE_SAMPLING_SETTINGS,
    DEFAULT_ROUTE_STORAGE_SETTINGS,
    load_database_settings,
    load_route_sampling_settings,
    load_route_storage_settings,
)


class SettingsTests(unittest.TestCase):
    def test_load_route_sampling_settings_reads_ini_values(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            config_path = Path(temp_dir) / "config.ini"
            config_path.write_text(
                "[route_sampling]\n"
                "base_spacing_yards = 30\n"
                "min_spacing_yards = 6\n"
                "max_spacing_yards = 75\n",
                encoding="utf-8",
            )

            settings = load_route_sampling_settings(config_path)

            self.assertEqual(settings.base_spacing_yards, 30.0)
            self.assertEqual(settings.min_spacing_yards, 6.0)
            self.assertEqual(settings.max_spacing_yards, 75.0)

    def test_load_route_sampling_settings_clamps_invalid_ranges(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            config_path = Path(temp_dir) / "config.ini"
            config_path.write_text(
                "[route_sampling]\n"
                "base_spacing_yards = 25\n"
                "min_spacing_yards = 100\n"
                "max_spacing_yards = 10\n",
                encoding="utf-8",
            )

            settings = load_route_sampling_settings(config_path)

            self.assertEqual(settings.base_spacing_yards, 25.0)
            self.assertEqual(settings.min_spacing_yards, 25.0)
            self.assertEqual(settings.max_spacing_yards, 25.0)

    def test_load_route_sampling_settings_falls_back_on_invalid_numbers(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            config_path = Path(temp_dir) / "config.ini"
            config_path.write_text(
                "[route_sampling]\n"
                "base_spacing_yards = nope\n"
                "min_spacing_yards = -3\n"
                "max_spacing_yards = nope\n",
                encoding="utf-8",
            )

            settings = load_route_sampling_settings(config_path)

            self.assertEqual(settings, DEFAULT_ROUTE_SAMPLING_SETTINGS)

    def test_load_route_storage_settings_resolves_relative_root_from_app_dir(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            config_path = Path(temp_dir) / "config.ini"
            config_path.write_text(
                "[route_storage]\n"
                "route_data_root = ../Server/data/worldbot_routes\n"
                "editor_routes_subdir = authored\n"
                "exported_routes_subdir = active\n",
                encoding="utf-8",
            )

            settings = load_route_storage_settings(config_path)

            self.assertEqual(settings.editor_routes_subdir, "authored")
            self.assertEqual(settings.exported_routes_subdir, "active")
            self.assertEqual(settings.route_data_root.name, "worldbot_routes")
            self.assertEqual(settings.editor_routes_dir.name, "authored")
            self.assertEqual(settings.exported_routes_dir.name, "active")

    def test_load_database_settings_reads_ini_values(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            config_path = Path(temp_dir) / "config.ini"
            config_path.write_text(
                "[database]\n"
                "host = 127.0.0.1\n"
                "port = 3307\n"
                "user = editor\n"
                "password = secret\n"
                "database = acore_world_test\n"
                "mysql_binary = C:/mysql/bin/mysql.exe\n",
                encoding="utf-8",
            )

            settings = load_database_settings(config_path)

            self.assertEqual(settings.host, "127.0.0.1")
            self.assertEqual(settings.port, 3307)
            self.assertEqual(settings.user, "editor")
            self.assertEqual(settings.password, "secret")
            self.assertEqual(settings.database, "acore_world_test")
            self.assertEqual(settings.mysql_binary, "C:/mysql/bin/mysql.exe")

    def test_load_database_settings_falls_back_cleanly(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            config_path = Path(temp_dir) / "config.ini"
            config_path.write_text(
                "[database]\n"
                "port = nope\n"
                "database = \n",
                encoding="utf-8",
            )

            settings = load_database_settings(config_path)

            self.assertEqual(settings, DEFAULT_DATABASE_SETTINGS)


if __name__ == "__main__":
    unittest.main()
