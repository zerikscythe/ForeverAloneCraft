from __future__ import annotations

import tempfile
import unittest
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from lw_zone_editor.marker_cache import (
    MarkerRecord,
    _annotate_quests_with_objective_area_data,
    _build_objective_area_index,
    _build_quest_payload_from_row,
    _build_quest_route_graph,
    _quest_classification_tags,
    _resource_kind_from_name,
    load_marker_cache,
    marker_icon_crop_box,
    write_marker_cache,
)


class MarkerCacheTests(unittest.TestCase):
    def test_marker_icon_crop_box_uses_requested_quest_sprite_cell(self) -> None:
        crop = marker_icon_crop_box("quest_giver", 128, 32)
        self.assertEqual(crop, (16, 16, 32, 32))

    def test_marker_cache_round_trip(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            cache_path = Path(tmp) / "markers.json"
            payload = {
                "version": 1,
                "markers": [
                    {
                        "uid": "gameobject:1:mailbox",
                        "kind": "mailbox",
                        "label": "Mailbox",
                        "object_type": "gameobject",
                        "map_id": 0,
                        "world_x": -9455.99,
                        "world_y": 45.82,
                        "world_z": 56.44,
                        "entry": 142075,
                        "guid": 26784,
                        "icon_relpath": "interface/minimap/tracking/mailbox.png",
                        "metadata": {},
                    }
                ],
            }

            write_marker_cache(payload, cache_path)
            markers, loaded = load_marker_cache(cache_path)

            self.assertEqual(loaded["version"], 1)
            self.assertEqual(len(markers), 1)
            self.assertIsInstance(markers[0], MarkerRecord)
            self.assertEqual(markers[0].kind, "mailbox")

    def test_build_quest_payload_parses_requirements_rewards_and_related_quests(self) -> None:
        row = [
            "123",  # creature entry
            "456",  # quest id
            "A Troubling Discovery",
            "18",
            "14",
            "77",  # Alliance races
            "Read the report.",
            "Bring the report to the commander.",
            "At the tower.",
            "Quest complete.",
            "Scout the tower",
            "Recover the report",
            "",
            "",
            # target requirements (4 x id/count/name)
            "789",
            "10",
            "Defias Bandit",
            "-9001",
            "1",
            "Sealed Crate",
            "0",
            "0",
            "",
            "0",
            "0",
            "",
            # item requirements (6 x id/count/name)
            "1001",
            "4",
            "Iron Ore",
            "0",
            "0",
            "",
            "0",
            "0",
            "",
            "0",
            "0",
            "",
            "0",
            "0",
            "",
            "0",
            "0",
            "",
            # reward money
            "5000",
            # fixed rewards (4 x id/count/name)
            "2001",
            "1",
            "Militia Dagger",
            "0",
            "0",
            "",
            "0",
            "0",
            "",
            "0",
            "0",
            "",
            # choice rewards (6 x id/count/name)
            "3001",
            "2",
            "Healing Potion",
            "3002",
            "1",
            "Sturdy Boots",
            "0",
            "0",
            "",
            "0",
            "0",
            "",
            "0",
            "0",
            "",
            "0",
            "0",
            "",
            # related quests
            "455",
            "Investigate Echo Ridge",
            "457",
            "Return to the Marshal",
            "458",
            "Report to Gryan",
            "459",
            "An Urgent Warning",
        ]

        payload = _build_quest_payload_from_row(row)

        self.assertEqual(payload["quest_id"], 456)
        self.assertEqual(payload["faction"], "Alliance")
        self.assertEqual(payload["target_requirements"][0]["name"], "Defias Bandit")
        self.assertEqual(payload["target_requirements"][1]["target_kind"], "object")
        self.assertIn("0/4 Iron Ore", payload["requirement_lines"])
        self.assertEqual(payload["fixed_rewards"][0]["name"], "Militia Dagger")
        self.assertEqual(len(payload["choice_rewards"]), 2)
        self.assertIn("50s", payload["reward_lines"])
        self.assertEqual(payload["prerequisite_quests"][0]["quest_id"], 455)
        self.assertEqual(payload["followup_quests"][0]["quest_id"], 457)
        self.assertEqual(payload["followup_quests"][1]["relation"], "reward_next")
        self.assertEqual(payload["breadcrumb_for_quests"][0]["quest_id"], 459)

    def test_objective_area_index_and_route_graph_are_derived_from_target_requirements(self) -> None:
        quest_index = {
            100: [
                {
                    "quest_id": 200,
                    "title": "Kill Kobolds",
                    "quest_level": 7,
                    "target_requirements": [
                        {
                            "target_id": 300,
                            "quantity": 10,
                            "name": "Kobold Vermin",
                            "target_kind": "creature",
                        }
                    ],
                    "followup_quests": [{"quest_id": 201, "title": "More Kobolds", "relation": "next"}],
                    "prerequisite_quests": [],
                    "breadcrumb_for_quests": [],
                }
            ]
        }
        objective_area_index = _build_objective_area_index(
            {
                300: [
                    {
                        "map_id": 0,
                        "world_x": -9450.0,
                        "world_y": 100.0,
                        "world_z": 55.0,
                        "wander_distance": 12.0,
                        "movement_type": 0,
                        "phase_mask": 1,
                    },
                    {
                        "map_id": 0,
                        "world_x": -9440.0,
                        "world_y": 110.0,
                        "world_z": 56.0,
                        "wander_distance": 8.0,
                        "movement_type": 1,
                        "phase_mask": 1,
                    },
                ]
            }
        )

        _annotate_quests_with_objective_area_data(quest_index, objective_area_index)
        routes = _build_quest_route_graph(quest_index)

        quest = quest_index[100][0]
        requirement = quest["target_requirements"][0]
        self.assertEqual(requirement["objective_overlay_id"], "creature:300")
        self.assertTrue(requirement["objective_overlay_available"])
        self.assertEqual(quest["branch_metadata"]["branch_mode"], "branching")
        self.assertEqual(quest["branch_candidates"][0]["branch_kind"], "objective_area")
        self.assertEqual(objective_area_index["creature:300"]["target_kind"], "creature")
        self.assertGreaterEqual(objective_area_index["creature:300"]["areas"][0]["radius"], 35.0)
        self.assertEqual(routes[0]["objective_overlay_ids"], ["creature:300"])
        self.assertEqual(routes[0]["followup_quest_ids"], [201])

    def test_quest_classification_tags_include_event_and_daily_labels(self) -> None:
        tags = _quest_classification_tags(
            is_daily=True,
            is_weekly=False,
            is_monthly=False,
            is_event=True,
            quest_sort_id=0,
            quest_type=82,
        )

        self.assertEqual(tags, ["Event", "Daily"])

    def test_quest_classification_tags_mark_darkmoon_sort_as_event(self) -> None:
        tags = _quest_classification_tags(
            is_daily=False,
            is_weekly=False,
            is_monthly=False,
            is_event=False,
            quest_sort_id=-364,
            quest_type=0,
        )

        self.assertEqual(tags, ["Event"])

    def test_resource_kind_from_name_classifies_herb_and_ore_nodes(self) -> None:
        self.assertEqual(_resource_kind_from_name("Silverleaf"), "herb")
        self.assertEqual(_resource_kind_from_name("Copper Vein"), "ore")
        self.assertEqual(_resource_kind_from_name("Rich Saronite Deposit"), "ore")
        self.assertIsNone(_resource_kind_from_name("Ordinary Barrel"))


if __name__ == "__main__":
    unittest.main()