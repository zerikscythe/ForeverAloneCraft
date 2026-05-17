from __future__ import annotations

from pathlib import Path
import sys
import unittest


APP_ROOT = Path(__file__).resolve().parents[1]
if str(APP_ROOT) not in sys.path:
    sys.path.insert(0, str(APP_ROOT))

from export_quest_hubs import ZoneBounds, _build_zone_payload, _is_runtime_eligible_quest, _resolve_zone  # noqa: E402


class ExportQuestHubsTest(unittest.TestCase):
    def test_resolve_zone_uses_world_map_area_bounds(self) -> None:
        bounds = [
            ZoneBounds(
                zone_id=40,
                zone_name="Westfall",
                map_id=0,
                left=3016.6665,
                right=-483.3333,
                top=-9400.0,
                bottom=-11733.3330,
            )
        ]
        zone = _resolve_zone(bounds, 0, -10600.0, 1000.0)
        self.assertIsNotNone(zone)
        self.assertEqual(40, zone.zone_id)

    def test_build_zone_payload_clusters_followup_hubs(self) -> None:
        route_by_quest_id = {
            100: [{"quest_id": 100, "giver_entry": 1}],
            101: [{"quest_id": 101, "giver_entry": 2}],
            200: [{"quest_id": 200, "giver_entry": 5}],
        }
        giver_entries_by_quest_id = {
            100: {1},
            101: {2},
            200: {5},
        }
        zone_rows = [
            {
                "zone_id": 40,
                "zone_name": "Westfall",
                "map_id": 0,
                "entry": 1,
                "guid": 1001,
                "label": "Farmer One",
                "world_x": -10600.0,
                "world_y": 1000.0,
                "world_z": 30.0,
                "route_by_quest_id": route_by_quest_id,
                "giver_entries_by_quest_id": giver_entries_by_quest_id,
                "quests": [
                    {
                        "quest_id": 100,
                        "title": "A Task",
                        "quest_level": 12,
                        "min_level": 10,
                        "faction": "Alliance",
                        "classification_tags": [],
                        "objective_overlay_ids": ["creature:500"],
                        "target_requirements": [
                            {
                                "target_id": 500,
                                "name": "Riverpaw Bandit",
                                "objective_overlay_id": "creature:500",
                            }
                        ],
                        "item_requirements": [],
                        "followup_quests": [{"quest_id": 200, "title": "Next Task"}],
                    }
                ],
            },
            {
                "zone_id": 40,
                "zone_name": "Westfall",
                "map_id": 0,
                "entry": 2,
                "guid": 1002,
                "label": "Farmer Two",
                "world_x": -10560.0,
                "world_y": 1010.0,
                "world_z": 31.0,
                "route_by_quest_id": route_by_quest_id,
                "giver_entries_by_quest_id": giver_entries_by_quest_id,
                "quests": [
                    {
                        "quest_id": 101,
                        "title": "Another Task",
                        "quest_level": 13,
                        "min_level": 10,
                        "faction": "Alliance",
                        "classification_tags": [],
                        "objective_overlay_ids": ["creature:500"],
                        "target_requirements": [
                            {
                                "target_id": 500,
                                "name": "Riverpaw Bandit",
                                "objective_overlay_id": "creature:500",
                            }
                        ],
                        "item_requirements": [],
                        "followup_quests": [{"quest_id": 200, "title": "Next Task"}],
                    }
                ],
            },
            {
                "zone_id": 40,
                "zone_name": "Westfall",
                "map_id": 0,
                "entry": 5,
                "guid": 1003,
                "label": "Captain Followup",
                "world_x": -10200.0,
                "world_y": 1400.0,
                "world_z": 32.0,
                "route_by_quest_id": route_by_quest_id,
                "giver_entries_by_quest_id": giver_entries_by_quest_id,
                "quests": [
                    {
                        "quest_id": 200,
                        "title": "Next Task",
                        "quest_level": 14,
                        "min_level": 12,
                        "faction": "Alliance",
                        "classification_tags": [],
                        "objective_overlay_ids": ["creature:600"],
                        "target_requirements": [
                            {
                                "target_id": 600,
                                "name": "Defias Smuggler",
                                "objective_overlay_id": "creature:600",
                            }
                        ],
                        "item_requirements": [],
                        "followup_quests": [],
                    }
                ],
            },
        ]

        objective_area_index = {
            "creature:500": {
                "overlay_id": "creature:500",
                "target_id": 500,
                "target_kind": "creature",
                "target_name": "Riverpaw Bandit",
                "areas": [
                    {
                        "area_id": "map:0:cluster:1",
                        "map_id": 0,
                        "center_x": -10540.0,
                        "center_y": 1080.0,
                        "center_z": 32.0,
                        "radius": 85.0,
                    }
                ],
            },
            "creature:600": {
                "overlay_id": "creature:600",
                "target_id": 600,
                "target_kind": "creature",
                "target_name": "Defias Smuggler",
                "areas": [
                    {
                        "area_id": "map:0:cluster:2",
                        "map_id": 0,
                        "center_x": -10160.0,
                        "center_y": 1410.0,
                        "center_z": 35.0,
                        "radius": 70.0,
                    }
                ],
            },
        }
        bounds = [
            ZoneBounds(
                zone_id=40,
                zone_name="Westfall",
                map_id=0,
                left=3016.6665,
                right=-483.3333,
                top=-9400.0,
                bottom=-11733.3330,
            )
        ]

        payload = _build_zone_payload("Westfall", 40, zone_rows, cluster_radius=90.0, objective_area_index=objective_area_index, bounds=bounds)

        self.assertEqual(2, payload["hubCount"])
        first_hub = payload["hubs"][0]
        self.assertEqual(2, first_hub["totalQuests"])
        self.assertEqual([1, 2], first_hub["questGivers"])
        self.assertEqual(1, len(first_hub["taskAreas"]))
        self.assertEqual(2, first_hub["taskAreas"][0]["weight"])
        self.assertEqual([500], first_hub["taskAreas"][0]["targetEntries"])
        self.assertEqual(1, len(first_hub["nextHubs"]))
        self.assertEqual(2, first_hub["nextHubs"][0]["weight"])
        self.assertEqual(40, first_hub["nextHubs"][0]["zoneId"])

    def test_build_zone_payload_skips_scattered_duplicate_single_quest_markers(self) -> None:
        bounds = [
            ZoneBounds(
                zone_id=40,
                zone_name="Westfall",
                map_id=0,
                left=3016.6665,
                right=-483.3333,
                top=-9400.0,
                bottom=-11733.3330,
            )
        ]
        zone_rows = [
            {
                "zone_id": 40,
                "zone_name": "Westfall",
                "map_id": 0,
                "entry": 620,
                "guid": 1000 + index,
                "label": "Chicken",
                "world_x": world_x,
                "world_y": world_y,
                "world_z": 40.0,
                "route_by_quest_id": {},
                "giver_entries_by_quest_id": {},
                "quests": [
                    {
                        "quest_id": 3861,
                        "title": "CLUCK!",
                        "quest_level": 1,
                        "min_level": 1,
                        "faction": "Both",
                        "classification_tags": [],
                        "objective_overlay_ids": [],
                        "target_requirements": [],
                        "item_requirements": [],
                        "followup_quests": [],
                    }
                ],
            }
            for index, (world_x, world_y) in enumerate(
                [
                    (-10731.8, 1678.6),
                    (-11023.7, 1429.8),
                    (-10314.4, 1417.7),
                ],
                start=1,
            )
        ]

        payload = _build_zone_payload(
            "Westfall",
            40,
            zone_rows,
            cluster_radius=60.0,
            objective_area_index={},
            bounds=bounds,
        )

        self.assertEqual(0, payload["hubCount"])
        self.assertEqual([], payload["hubs"])

    def test_runtime_eligible_quest_filters_event_and_rotating_quests(self) -> None:
        self.assertTrue(_is_runtime_eligible_quest({}))
        self.assertFalse(_is_runtime_eligible_quest({"is_event_quest": True}))
        self.assertFalse(_is_runtime_eligible_quest({"is_daily_quest": True}))
        self.assertFalse(_is_runtime_eligible_quest({"is_weekly_quest": True}))
        self.assertFalse(_is_runtime_eligible_quest({"is_monthly_quest": True}))


if __name__ == "__main__":
    unittest.main()
