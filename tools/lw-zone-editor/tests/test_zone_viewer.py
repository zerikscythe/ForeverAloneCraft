from __future__ import annotations

import copy
import tempfile
import unittest
from pathlib import Path
import sys

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from lw_zone_editor.zone_viewer import (
    MAP_COMMON,
    MAP_INSTANCE,
    MAP_RAID,
    EditorAnchor,
    EditorPath,
    EditorPathConnection,
    EditorPathState,
    ZoneCompositeAsset,
    ZoneCoordinateTransform,
    TREE_SORT_ZONE,
    _category_root_asset,
    _build_route_editor_filename,
    _asset_floor_tree_label,
    _build_route_export_filename,
    _build_route_runtime_filename,
    _deserialize_editor_path_state,
    _default_curve_handles_between_points,
    _distance_between_points,
    _delete_anchor_from_path,
    _editor_path_connection_payload,
    _expansion_root_asset,
    _filter_visible_quests,
    _filter_event_quests,
    _format_requirement_tree_label,
    _move_anchor_with_handles,
    _parse_optional_level_filter,
    _propagate_connected_anchor_position,
    _quest_filter_level_value,
    _quest_marker_icon_kind,
    _quest_matches_level_filter,
    _route_export_group_key,
    _route_export_preference,
    _overlay_id_from_requirement,
    _objective_area_canvas_radii,
    _project_overlay_asset_into_base_image,
    _related_overlay_assets_for_edit,
    _insert_anchor_into_path,
    _mark_complete_paths_finalized,
    _sample_bezier_polyline,
    _sample_editor_path,
    _serialize_editor_path_state,
    _format_quest_tree_label,
    _format_related_quest_tree_label,
    classify_zone_group,
    compute_render_state,
    discover_zone_composites,
    group_category_assets,
    group_assets_for_tree,
)


class ZoneViewerTests(unittest.TestCase):
    def test_discover_zone_composites_only_returns_canonical_files(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)

            Image.new("RGB", (32, 16), (255, 0, 0)).save(root / "Elwynn Forest - 12.png", format="PNG")
            Image.new("RGB", (64, 32), (0, 255, 0)).save(root / "Stormwind City - 1519.png", format="PNG")
            Image.new("RGB", (16, 16), (0, 0, 255)).save(root / "elwynn_probe.png", format="PNG")
            Image.new("RGB", (16, 16), (255, 255, 0)).save(root / "stormwind_city.png", format="PNG")
            (root / "compare_elwynn.html").write_text("<html></html>", encoding="utf-8")

            assets = discover_zone_composites(root)

            self.assertEqual([asset.zone_name for asset in assets], ["Elwynn Forest", "Stormwind City"])
            self.assertEqual([asset.zone_id for asset in assets], [12, 1519])
            self.assertEqual((assets[0].width, assets[0].height), (32, 16))
            self.assertEqual((assets[1].width, assets[1].height), (64, 32))
            self.assertEqual([asset.source_zone_name for asset in assets], ["Elwynn Forest", "Stormwind City"])

    def test_classify_zone_group_uses_expansion_and_map_type(self) -> None:
        self.assertEqual(classify_zone_group(0, MAP_COMMON, 0), ("Vanilla", "Eastern Kingdoms"))
        self.assertEqual(classify_zone_group(1, MAP_COMMON, 0), ("Vanilla", "Kalimdor"))
        self.assertEqual(classify_zone_group(530, MAP_COMMON, 1), ("TBC", "Outland"))
        self.assertEqual(classify_zone_group(571, MAP_COMMON, 2), ("WotLK", "Northrend"))
        self.assertEqual(classify_zone_group(33, MAP_INSTANCE, 0), ("Vanilla", "Dungeons"))
        self.assertEqual(classify_zone_group(249, MAP_RAID, 0), ("Vanilla", "Raids"))

    def test_zone_coordinate_transform_matches_azerothcore_conversion_math(self) -> None:
        transform = ZoneCoordinateTransform(
            world_map_area_id=42,
            map_id=0,
            zone_id=12,
            world_y1=-1000.0,
            world_y2=1000.0,
            world_x1=2000.0,
            world_x2=1000.0,
        )

        world_x, world_y = transform.zone_to_world(25.0, 75.0)
        self.assertAlmostEqual(world_x, 1250.0)
        self.assertAlmostEqual(world_y, -500.0)

        zone_x, zone_y = transform.world_to_zone(world_x, world_y)
        self.assertAlmostEqual(zone_x, 25.0)
        self.assertAlmostEqual(zone_y, 75.0)

        image_zone_x, image_zone_y = transform.image_to_zone(256.0, 128.0, 512, 256)
        self.assertAlmostEqual(image_zone_x, 50.0)
        self.assertAlmostEqual(image_zone_y, 50.0)

        image_world_x, image_world_y = transform.image_to_world(256.0, 128.0, 512, 256)
        self.assertAlmostEqual(image_world_x, 1500.0)
        self.assertAlmostEqual(image_world_y, 0.0)

    def test_compute_render_state_maps_canvas_points_back_to_image_space(self) -> None:
        state = compute_render_state(
            image_width=1024,
            image_height=768,
            canvas_width=800,
            canvas_height=600,
            zoom_factor=2.0,
            viewport_x=100.0,
            viewport_y=50.0,
        )

        self.assertEqual((state.draw_width, state.draw_height), (800, 600))
        self.assertAlmostEqual(state.viewport_width, 512.0)
        self.assertAlmostEqual(state.viewport_height, 384.0)
        self.assertEqual(state.canvas_to_image(400, 300), (356.0, 242.0))

    def test_compute_render_state_clamps_zoomed_viewport_origin(self) -> None:
        state = compute_render_state(
            image_width=1024,
            image_height=768,
            canvas_width=800,
            canvas_height=600,
            zoom_factor=2.0,
            viewport_x=9999.0,
            viewport_y=9999.0,
        )

        self.assertAlmostEqual(state.viewport_x, 512.0)
        self.assertAlmostEqual(state.viewport_y, 384.0)

    def test_group_assets_for_tree_groups_by_expansion_then_category(self) -> None:
        assets = [
            ZoneCompositeAsset(source_zone_name="Elwynn Forest", zone_name="Elwynn Forest", zone_id=12, path=Path("a.png"), width=32, height=16, map_id=0, map_name="Azeroth", map_type=MAP_COMMON, expansion_id=0, expansion_label="Vanilla", category_label="Eastern Kingdoms"),
            ZoneCompositeAsset(source_zone_name="Westfall", zone_name="Westfall", zone_id=40, path=Path("b.png"), width=32, height=16, map_id=0, map_name="Azeroth", map_type=MAP_COMMON, expansion_id=0, expansion_label="Vanilla", category_label="Eastern Kingdoms"),
            ZoneCompositeAsset(source_zone_name="Ragefire Chasm", zone_name="Ragefire Chasm", zone_id=2437, path=Path("c.png"), width=32, height=16, map_id=389, map_name="Ragefire Chasm", map_type=MAP_INSTANCE, expansion_id=0, expansion_label="Vanilla", category_label="Dungeons"),
            ZoneCompositeAsset(source_zone_name="Nagrand", zone_name="Nagrand", zone_id=3518, path=Path("d.png"), width=32, height=16, map_id=530, map_name="Outland", map_type=MAP_COMMON, expansion_id=1, expansion_label="TBC", category_label="Outland"),
        ]

        grouped = group_assets_for_tree(assets)

        self.assertEqual(list(grouped.keys()), ["Vanilla", "TBC"])
        self.assertEqual(list(grouped["Vanilla"].keys()), ["Eastern Kingdoms", "Dungeons"])
        self.assertEqual([asset.zone_name for asset in grouped["Vanilla"]["Eastern Kingdoms"]], ["Elwynn Forest", "Westfall"])
        self.assertEqual([asset.zone_name for asset in grouped["Vanilla"]["Dungeons"]], ["Ragefire Chasm"])
        self.assertEqual([asset.zone_name for asset in grouped["TBC"]["Outland"]], ["Nagrand"])

    def test_group_assets_for_tree_can_sort_leaf_assets_by_zone_id(self) -> None:
        assets = [
            ZoneCompositeAsset(source_zone_name="Zul'Farrak", zone_name="Zul'Farrak", zone_id=978, path=Path("zf.png"), width=32, height=16, map_id=209, map_name="Zul'Farrak", map_type=MAP_INSTANCE, expansion_id=0, expansion_label="Vanilla", category_label="Dungeons"),
            ZoneCompositeAsset(source_zone_name="Westfall", zone_name="Westfall", zone_id=40, path=Path("westfall.png"), width=32, height=16, map_id=0, map_name="Azeroth", map_type=MAP_COMMON, expansion_id=0, expansion_label="Vanilla", category_label="Eastern Kingdoms"),
            ZoneCompositeAsset(source_zone_name="Elwynn Forest", zone_name="Elwynn Forest", zone_id=12, path=Path("elwynn.png"), width=32, height=16, map_id=0, map_name="Azeroth", map_type=MAP_COMMON, expansion_id=0, expansion_label="Vanilla", category_label="Eastern Kingdoms"),
        ]

        grouped = group_assets_for_tree(assets, sort_mode=TREE_SORT_ZONE)

        self.assertEqual([asset.zone_id for asset in grouped["Vanilla"]["Eastern Kingdoms"]], [12, 40])

    def test_category_root_asset_uses_exact_match_and_eastern_kingdoms_fallback(self) -> None:
        kalimdor = ZoneCompositeAsset(source_zone_name="Kalimdor", zone_name="Kalimdor", zone_id=1, path=Path("kalimdor.png"), width=32, height=16, map_id=1, map_name="Kalimdor", map_type=MAP_COMMON, expansion_id=0, expansion_label="Vanilla", category_label="Kalimdor")
        azeroth = ZoneCompositeAsset(source_zone_name="Azeroth", zone_name="Azeroth", zone_id=0, path=Path("azeroth.png"), width=32, height=16, map_id=0, map_name="Azeroth", map_type=MAP_COMMON, expansion_id=0, expansion_label="Vanilla", category_label="Eastern Kingdoms")

        self.assertEqual(_category_root_asset("Kalimdor", [kalimdor]), kalimdor)
        self.assertEqual(_category_root_asset("Eastern Kingdoms", [azeroth]), azeroth)

    def test_expansion_root_asset_uses_azeroth_when_world_is_missing(self) -> None:
        azeroth = ZoneCompositeAsset(source_zone_name="Azeroth", zone_name="Azeroth", zone_id=0, path=Path("azeroth.png"), width=32, height=16, map_id=0, map_name="Azeroth", map_type=MAP_COMMON, expansion_id=0, expansion_label="Vanilla", category_label="Eastern Kingdoms")

        self.assertEqual(_expansion_root_asset("Vanilla", [azeroth]), azeroth)
        self.assertIsNone(_expansion_root_asset("TBC", [azeroth]))

    def test_group_category_assets_nests_multilevel_dungeons_under_parent_node(self) -> None:
        assets = [
            ZoneCompositeAsset(source_zone_name="Utgardekeep 1", zone_name="Utgardekeep", zone_id=0, path=Path("u1.png"), width=32, height=16, map_id=574, map_name="Utgarde Keep", map_type=MAP_INSTANCE, expansion_id=2, expansion_label="WotLK", category_label="Dungeons"),
            ZoneCompositeAsset(source_zone_name="Utgardekeep 2", zone_name="Utgardekeep", zone_id=0, path=Path("u2.png"), width=32, height=16, map_id=574, map_name="Utgarde Keep", map_type=MAP_INSTANCE, expansion_id=2, expansion_label="WotLK", category_label="Dungeons"),
            ZoneCompositeAsset(source_zone_name="Ragefire Chasm", zone_name="Ragefire Chasm", zone_id=2437, path=Path("rfc.png"), width=32, height=16, map_id=389, map_name="Ragefire Chasm", map_type=MAP_INSTANCE, expansion_id=0, expansion_label="Vanilla", category_label="Dungeons"),
        ]

        leaf_assets, grouped_assets = group_category_assets(assets)

        self.assertEqual([asset.zone_name for asset in leaf_assets], ["Ragefire Chasm"])
        self.assertEqual([group.label for group in grouped_assets], ["Utgardekeep"])
        self.assertEqual([child.label for child in grouped_assets[0].children], ["Level 1", "Level 2"])

    def test_group_category_assets_groups_floor_variants_under_parent_node(self) -> None:
        assets = [
            ZoneCompositeAsset(source_zone_name="Elwynn Forest", zone_name="Elwynn Forest", zone_id=12, path=Path("surface.png"), width=32, height=16, map_id=0, map_name="Azeroth", map_type=MAP_COMMON, expansion_id=0, expansion_label="Vanilla", category_label="Eastern Kingdoms", floor=0),
            ZoneCompositeAsset(source_zone_name="Elwynn Forest Mine", zone_name="Elwynn Forest", zone_id=12, path=Path("mine.png"), width=32, height=16, map_id=0, map_name="Azeroth", map_type=MAP_COMMON, expansion_id=0, expansion_label="Vanilla", category_label="Eastern Kingdoms", floor=1),
            ZoneCompositeAsset(source_zone_name="Westfall", zone_name="Westfall", zone_id=40, path=Path("westfall.png"), width=32, height=16, map_id=0, map_name="Azeroth", map_type=MAP_COMMON, expansion_id=0, expansion_label="Vanilla", category_label="Eastern Kingdoms", floor=0),
        ]

        leaf_assets, grouped_assets = group_category_assets(assets)

        self.assertEqual([asset.zone_name for asset in leaf_assets], ["Westfall"])
        self.assertEqual([group.label for group in grouped_assets], ["Elwynn Forest"])
        self.assertEqual([child.label for child in grouped_assets[0].children], ["Surface", "Floor 1"])

    def test_asset_floor_tree_label_uses_surface_for_zero_floor(self) -> None:
        surface = ZoneCompositeAsset(source_zone_name="A", zone_name="A", zone_id=1, path=Path("a.png"), width=1, height=1, floor=0)
        floor = ZoneCompositeAsset(source_zone_name="A", zone_name="A", zone_id=1, path=Path("b.png"), width=1, height=1, floor=2)

        self.assertEqual(_asset_floor_tree_label(surface), "Surface")
        self.assertEqual(_asset_floor_tree_label(floor), "Floor 2")

    def test_related_overlay_assets_only_returns_same_zone_floor_variants(self) -> None:
        transform = ZoneCoordinateTransform(world_map_area_id=1, map_id=0, zone_id=12, world_y1=0.0, world_y2=100.0, world_x1=0.0, world_x2=100.0)
        current = ZoneCompositeAsset(source_zone_name="Base", zone_name="Elwynn Forest", zone_id=12, path=Path("base.png"), width=100, height=100, map_id=0, coordinate_transform=transform)
        related = ZoneCompositeAsset(source_zone_name="Cave", zone_name="Elwynn Forest", zone_id=12, path=Path("cave.png"), width=50, height=50, map_id=0, floor=1, coordinate_transform=transform)
        other_zone = ZoneCompositeAsset(source_zone_name="Other", zone_name="Westfall", zone_id=40, path=Path("other.png"), width=50, height=50, map_id=0, floor=1, coordinate_transform=transform)
        missing_transform = ZoneCompositeAsset(source_zone_name="Missing", zone_name="Elwynn Forest", zone_id=12, path=Path("missing.png"), width=50, height=50, map_id=0, floor=2)

        related_assets = _related_overlay_assets_for_edit([current, other_zone, missing_transform, related], current)

        self.assertEqual(related_assets, [related])

    def test_project_overlay_asset_into_base_image_uses_world_coordinates(self) -> None:
        base_transform = ZoneCoordinateTransform(world_map_area_id=1, map_id=0, zone_id=12, world_y1=0.0, world_y2=100.0, world_x1=0.0, world_x2=100.0)
        overlay_transform = ZoneCoordinateTransform(world_map_area_id=2, map_id=0, zone_id=12, world_y1=20.0, world_y2=60.0, world_x1=25.0, world_x2=75.0)
        base = ZoneCompositeAsset(source_zone_name="Base", zone_name="Elwynn Forest", zone_id=12, path=Path("base.png"), width=100, height=100, map_id=0, coordinate_transform=base_transform)
        overlay = ZoneCompositeAsset(source_zone_name="Cave", zone_name="Elwynn Forest", zone_id=12, path=Path("cave.png"), width=50, height=40, map_id=0, floor=1, coordinate_transform=overlay_transform)

        projected = _project_overlay_asset_into_base_image(overlay, base)

        self.assertEqual(projected, (20, 25, 40, 50, False, False))

    def test_format_quest_tree_label_includes_title_and_level_requirement(self) -> None:
        label = _format_quest_tree_label(
            {
                "quest_id": 123,
                "title": "The Fargodeep Mine",
                "quest_level": 10,
                "min_level": 8,
            }
        )

        self.assertEqual(label, "[123] The Fargodeep Mine (Level 10 | Req 8)")

    def test_format_related_quest_tree_label_includes_relation(self) -> None:
        label = _format_related_quest_tree_label(
            {
                "quest_id": 124,
                "title": "Return to Goldshire",
                "relation": "reward_next",
            }
        )

        self.assertEqual(label, "Reward next: [124] Return to Goldshire")

    def test_format_requirement_tree_label_uses_creature_and_collect_variants(self) -> None:
        creature_label = _format_requirement_tree_label(
            {"quantity": 10, "name": "Defias Bandit", "target_kind": "creature"}
        )
        item_label = _format_requirement_tree_label(
            {"quantity": 4, "name": "Iron Ore", "target_kind": "item"}
        )

        self.assertEqual(creature_label, "Kill: 0/10 Defias Bandit")
        self.assertEqual(item_label, "Collect: 0/4 Iron Ore")

    def test_overlay_id_from_requirement_returns_none_when_missing(self) -> None:
        self.assertIsNone(_overlay_id_from_requirement({"name": "Iron Ore"}))
        self.assertEqual(_overlay_id_from_requirement({"objective_overlay_id": "creature:123"}), "creature:123")

    def test_objective_area_canvas_radii_returns_positive_values(self) -> None:
        transform = ZoneCoordinateTransform(
            world_map_area_id=42,
            map_id=0,
            zone_id=12,
            world_y1=-1000.0,
            world_y2=1000.0,
            world_x1=2000.0,
            world_x2=1000.0,
        )
        render_state = compute_render_state(
            image_width=1024,
            image_height=768,
            canvas_width=800,
            canvas_height=600,
            zoom_factor=1.0,
            viewport_x=0.0,
            viewport_y=0.0,
        )

        radius_x, radius_y = _objective_area_canvas_radii(
            transform=transform,
            render_state=render_state,
            area={"center_x": 1500.0, "center_y": 0.0, "radius": 75.0},
            image_width=1024,
            image_height=768,
        )

        self.assertGreater(radius_x, 0.0)
        self.assertGreater(radius_y, 0.0)

    def test_filter_event_quests_hides_only_event_entries(self) -> None:
        quests = [
            {"quest_id": 1, "title": "Normal Quest", "is_event_quest": False},
            {"quest_id": 2, "title": "Holiday Quest", "is_event_quest": True},
        ]

        filtered = _filter_event_quests(quests, include_event_quests=False)

        self.assertEqual([quest["quest_id"] for quest in filtered], [1])

    def test_filter_visible_quests_applies_event_and_level_filters_together(self) -> None:
        quests = [
            {"quest_id": 1, "title": "Holiday Quest", "is_event_quest": True, "quest_level": 10, "min_level": 8},
            {"quest_id": 2, "title": "Low Quest", "is_event_quest": False, "quest_level": 6, "min_level": 4},
            {"quest_id": 3, "title": "Right Quest", "is_event_quest": False, "quest_level": 15, "min_level": 12},
        ]

        filtered = _filter_visible_quests(quests, include_event_quests=False, min_level=10, max_level=20)

        self.assertEqual([quest["quest_id"] for quest in filtered], [3])

    def test_quest_filter_level_value_falls_back_to_min_level_for_negative_quest_levels(self) -> None:
        self.assertEqual(_quest_filter_level_value({"quest_level": -1, "min_level": 6}), 6)

    def test_quest_matches_level_filter_uses_effective_level(self) -> None:
        quest = {"quest_level": -1, "min_level": 6}

        self.assertTrue(_quest_matches_level_filter(quest, min_level=1, max_level=10))
        self.assertFalse(_quest_matches_level_filter(quest, min_level=7, max_level=10))

    def test_parse_optional_level_filter_returns_none_for_blank_or_invalid_input(self) -> None:
        self.assertIsNone(_parse_optional_level_filter(""))
        self.assertIsNone(_parse_optional_level_filter("abc"))
        self.assertEqual(_parse_optional_level_filter(" 12 "), 12)

    def test_build_route_export_filename_uses_map_zone_and_route_key(self) -> None:
        filename = _build_route_export_filename(
            zone_name="Hillsbrad Foothills",
            zone_id=267,
            map_id=0,
            route_group_key="route_001",
        )

        self.assertEqual(
            filename,
            "map_000__zone_267__hillsbrad_foothills__route_001.json",
        )

    def test_build_route_editor_and_runtime_filenames_use_stable_map_zone_names(self) -> None:
        editor_filename = _build_route_editor_filename(
            zone_name="Elwynn Forest",
            zone_id=12,
            map_id=0,
        )
        runtime_filename = _build_route_runtime_filename(
            zone_name="Elwynn Forest",
            zone_id=12,
            map_id=0,
        )

        self.assertEqual(editor_filename, "map_000__zone_12__elwynn_forest__editor.json")
        self.assertEqual(runtime_filename, "map_000__zone_12__elwynn_forest__routes.json")

    def test_serialize_and_deserialize_editor_path_state_preserves_branches_and_handles(self) -> None:
        transform = ZoneCoordinateTransform(
            world_map_area_id=30,
            map_id=0,
            zone_id=12,
            world_y1=0.0,
            world_y2=100.0,
            world_x1=100.0,
            world_x2=0.0,
        )
        asset = ZoneCompositeAsset(
            source_zone_name="Elwynn Forest",
            zone_name="Elwynn Forest",
            zone_id=12,
            path=Path("elwynn.png"),
            width=1000,
            height=800,
            map_id=0,
            world_map_area_id=30,
            coordinate_transform=transform,
        )
        path_state = EditorPathState(
            paths=[
                EditorPath(
                    anchors=[
                        EditorAnchor(image_x=100.0, image_y=200.0, handle_out_x=120.0, handle_out_y=230.0),
                        EditorAnchor(image_x=160.0, image_y=260.0, handle_in_x=140.0, handle_in_y=240.0),
                    ],
                    finalized=True,
                ),
                EditorPath(
                    anchors=[
                        EditorAnchor(image_x=160.0, image_y=260.0),
                        EditorAnchor(image_x=220.0, image_y=320.0),
                    ],
                    finalized=False,
                    start_connection=EditorPathConnection(path_index=0, anchor_index=1),
                ),
            ],
            active_path_index=1,
        )

        payload = _serialize_editor_path_state(
            path_state,
            route_group_key="ElwynnForest",
            sample_spacing_yards=5.0,
            asset=asset,
        )
        loaded_state, loaded_key, loaded_spacing = _deserialize_editor_path_state(payload)

        self.assertEqual(loaded_key, "ElwynnForest")
        self.assertEqual(loaded_spacing, 5.0)
        self.assertEqual(len(loaded_state.paths), 2)
        self.assertTrue(loaded_state.paths[0].finalized)
        self.assertFalse(loaded_state.paths[1].finalized)
        self.assertEqual(loaded_state.active_path_index, 1)
        self.assertEqual(loaded_state.paths[1].start_connection, EditorPathConnection(path_index=0, anchor_index=1))
        self.assertAlmostEqual(loaded_state.paths[0].anchors[0].handle_out_x or 0.0, 120.0)
        self.assertAlmostEqual(loaded_state.paths[0].anchors[1].handle_in_y or 0.0, 240.0)

    def test_serialize_and_deserialize_editor_path_state_preserves_self_loop_connection(self) -> None:
        transform = ZoneCoordinateTransform(
            world_map_area_id=30,
            map_id=0,
            zone_id=12,
            world_y1=0.0,
            world_y2=100.0,
            world_x1=100.0,
            world_x2=0.0,
        )
        asset = ZoneCompositeAsset(
            source_zone_name="Elwynn Forest",
            zone_name="Elwynn Forest",
            zone_id=12,
            path=Path("elwynn.png"),
            width=1000,
            height=800,
            map_id=0,
            world_map_area_id=30,
            coordinate_transform=transform,
        )
        path_state = EditorPathState(
            paths=[
                EditorPath(
                    anchors=[
                        EditorAnchor(image_x=100.0, image_y=200.0),
                        EditorAnchor(image_x=160.0, image_y=260.0),
                        EditorAnchor(image_x=100.0, image_y=200.0),
                    ],
                    finalized=True,
                    end_connection=EditorPathConnection(path_index=0, anchor_index=0),
                ),
            ]
        )

        payload = _serialize_editor_path_state(
            path_state,
            route_group_key="ElwynnForest",
            sample_spacing_yards=5.0,
            asset=asset,
        )
        loaded_state, loaded_key, loaded_spacing = _deserialize_editor_path_state(payload)

        self.assertEqual(loaded_key, "ElwynnForest")
        self.assertEqual(loaded_spacing, 5.0)
        self.assertEqual(len(loaded_state.paths), 1)
        self.assertTrue(loaded_state.paths[0].finalized)
        self.assertEqual(loaded_state.paths[0].end_connection, EditorPathConnection(path_index=0, anchor_index=0))

    def test_editor_path_connection_payload_includes_target_path_metadata(self) -> None:
        payload = _editor_path_connection_payload(
            EditorPathConnection(path_index=2, anchor_index=4),
            "route_001",
        )

        self.assertEqual(
            payload,
            {
                "path_index": 2,
                "path_key": "route_001_03",
                "anchor_index": 4,
                "anchor_label": "P3-A5",
            },
        )

    def test_default_curve_handles_split_segment_into_thirds(self) -> None:
        out_x, out_y, in_x, in_y = _default_curve_handles_between_points(0.0, 0.0, 9.0, 0.0)

        self.assertAlmostEqual(out_x, 3.0)
        self.assertAlmostEqual(out_y, 0.0)
        self.assertAlmostEqual(in_x, 6.0)
        self.assertAlmostEqual(in_y, 0.0)

    def test_sample_bezier_polyline_includes_endpoints(self) -> None:
        anchors = [
            EditorAnchor(image_x=0.0, image_y=0.0, handle_out_x=3.0, handle_out_y=0.0),
            EditorAnchor(image_x=9.0, image_y=0.0, handle_in_x=6.0, handle_in_y=0.0),
        ]

        points = _sample_bezier_polyline(anchors, subdivisions=6)

        self.assertEqual(points[0], (0.0, 0.0))
        self.assertEqual(points[-1], (9.0, 0.0))
        self.assertGreater(len(points), 2)

    def test_sample_editor_path_generates_world_space_points_at_spacing(self) -> None:
        transform = ZoneCoordinateTransform(
            world_map_area_id=1,
            map_id=0,
            zone_id=12,
            world_y1=0.0,
            world_y2=100.0,
            world_x1=0.0,
            world_x2=100.0,
        )
        anchors = [
            EditorAnchor(image_x=0.0, image_y=0.0, handle_out_x=33.0, handle_out_y=0.0),
            EditorAnchor(image_x=100.0, image_y=0.0, handle_in_x=66.0, handle_in_y=0.0),
        ]

        sampled = _sample_editor_path(
            anchors,
            transform=transform,
            image_width=100,
            image_height=100,
            map_id=0,
            spacing_yards=20.0,
            subdivisions=10,
        )

        self.assertGreaterEqual(len(sampled), 4)
        self.assertAlmostEqual(float(sampled[0]["world_x"]), 0.0)
        self.assertAlmostEqual(float(sampled[-1]["world_x"]), 0.0)
        self.assertAlmostEqual(float(sampled[-1]["world_y"]), 100.0)

    def test_sample_editor_path_uses_fewer_points_on_straights_than_curves(self) -> None:
        transform = ZoneCoordinateTransform(
            world_map_area_id=1,
            map_id=0,
            zone_id=12,
            world_y1=0.0,
            world_y2=100.0,
            world_x1=0.0,
            world_x2=100.0,
        )
        straight_anchors = [
            EditorAnchor(image_x=0.0, image_y=0.0, handle_out_x=33.0, handle_out_y=0.0),
            EditorAnchor(image_x=100.0, image_y=0.0, handle_in_x=66.0, handle_in_y=0.0),
        ]
        curved_anchors = [
            EditorAnchor(image_x=0.0, image_y=0.0, handle_out_x=40.0, handle_out_y=0.0),
            EditorAnchor(image_x=50.0, image_y=50.0, handle_in_x=10.0, handle_in_y=50.0, handle_out_x=90.0, handle_out_y=50.0),
            EditorAnchor(image_x=100.0, image_y=0.0, handle_in_x=60.0, handle_in_y=0.0),
        ]

        straight_sampled = _sample_editor_path(
            straight_anchors,
            transform=transform,
            image_width=100,
            image_height=100,
            map_id=0,
            spacing_yards=10.0,
            subdivisions=24,
        )
        curved_sampled = _sample_editor_path(
            curved_anchors,
            transform=transform,
            image_width=100,
            image_height=100,
            map_id=0,
            spacing_yards=10.0,
            subdivisions=24,
        )

        self.assertLess(len(straight_sampled), len(curved_sampled))
        self.assertAlmostEqual(float(curved_sampled[0]["world_x"]), 0.0)
        self.assertAlmostEqual(float(curved_sampled[-1]["world_y"]), 100.0)

    def test_distance_between_points_returns_expected_length(self) -> None:
        self.assertAlmostEqual(_distance_between_points(0.0, 0.0, 3.0, 4.0), 5.0)

    def test_route_export_preference_prefers_canonical_runtime_file_over_legacy_export(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            base_path = Path(temp_dir)
            canonical = base_path / "map_000__zone_12__elwynn_forest__routes.json"
            legacy = base_path / "map_000__zone_12__elwynn_forest__elwynnforest.json"
            canonical.write_text("{}", encoding="utf-8")
            legacy.write_text("{}", encoding="utf-8")

            self.assertEqual(_route_export_group_key(canonical), _route_export_group_key(legacy))
            self.assertGreater(_route_export_preference(canonical), _route_export_preference(legacy))

    def test_insert_anchor_into_path_shifts_downstream_connection_indices(self) -> None:
        path_state = EditorPathState(
            paths=[
                EditorPath(
                    anchors=[
                        EditorAnchor(image_x=10.0, image_y=10.0),
                        EditorAnchor(image_x=40.0, image_y=40.0),
                        EditorAnchor(image_x=70.0, image_y=70.0),
                    ],
                    finalized=True,
                ),
                EditorPath(
                    anchors=[
                        EditorAnchor(image_x=70.0, image_y=70.0),
                        EditorAnchor(image_x=90.0, image_y=90.0),
                    ],
                    finalized=True,
                    start_connection=EditorPathConnection(path_index=0, anchor_index=2),
                ),
            ]
        )

        inserted = _insert_anchor_into_path(path_state, 0, 0, 25.0, 25.0)

        self.assertTrue(inserted)
        self.assertEqual(len(path_state.paths[0].anchors), 4)
        self.assertEqual(path_state.paths[0].anchors[1].image_x, 25.0)
        self.assertEqual(path_state.paths[1].start_connection, EditorPathConnection(path_index=0, anchor_index=3))

    def test_delete_anchor_from_path_reconnects_neighbors_and_shifts_indices(self) -> None:
        path_state = EditorPathState(
            paths=[
                EditorPath(
                    anchors=[
                        EditorAnchor(image_x=10.0, image_y=10.0),
                        EditorAnchor(image_x=40.0, image_y=40.0),
                        EditorAnchor(image_x=70.0, image_y=70.0),
                    ],
                    finalized=True,
                ),
                EditorPath(
                    anchors=[
                        EditorAnchor(image_x=70.0, image_y=70.0),
                        EditorAnchor(image_x=90.0, image_y=90.0),
                    ],
                    finalized=True,
                    start_connection=EditorPathConnection(path_index=0, anchor_index=2),
                ),
            ]
        )

        deleted = _delete_anchor_from_path(path_state, 0, 1)

        self.assertTrue(deleted)
        self.assertEqual(len(path_state.paths[0].anchors), 2)
        self.assertEqual(path_state.paths[1].start_connection, EditorPathConnection(path_index=0, anchor_index=1))
        self.assertIsNotNone(path_state.paths[0].anchors[0].handle_out_x)
        self.assertIsNotNone(path_state.paths[0].anchors[1].handle_in_x)

    def test_delete_anchor_from_path_rejects_connected_anchor(self) -> None:
        path_state = EditorPathState(
            paths=[
                EditorPath(
                    anchors=[
                        EditorAnchor(image_x=10.0, image_y=10.0),
                        EditorAnchor(image_x=40.0, image_y=40.0),
                    ],
                    finalized=True,
                ),
                EditorPath(
                    anchors=[
                        EditorAnchor(image_x=40.0, image_y=40.0),
                        EditorAnchor(image_x=90.0, image_y=90.0),
                    ],
                    finalized=True,
                    start_connection=EditorPathConnection(path_index=0, anchor_index=1),
                ),
            ]
        )

        deleted = _delete_anchor_from_path(path_state, 0, 1)

        self.assertFalse(deleted)
        self.assertEqual(len(path_state.paths[0].anchors), 2)

    def test_editor_path_state_deepcopy_preserves_pre_edit_snapshot(self) -> None:
        original = EditorPathState(
            paths=[
                EditorPath(
                    anchors=[
                        EditorAnchor(image_x=10.0, image_y=10.0),
                        EditorAnchor(image_x=20.0, image_y=20.0),
                    ],
                    finalized=True,
                ),
            ],
            active_path_index=0,
        )

        snapshot = copy.deepcopy(original)
        original.paths[0].anchors.append(EditorAnchor(image_x=30.0, image_y=30.0))
        original.paths[0].finalized = False

        self.assertEqual(len(snapshot.paths[0].anchors), 2)
        self.assertTrue(snapshot.paths[0].finalized)

    def test_insert_and_delete_helpers_support_round_trip_editing(self) -> None:
        path_state = EditorPathState(
            paths=[
                EditorPath(
                    anchors=[
                        EditorAnchor(image_x=10.0, image_y=10.0),
                        EditorAnchor(image_x=40.0, image_y=40.0),
                        EditorAnchor(image_x=70.0, image_y=70.0),
                    ],
                    finalized=True,
                ),
            ]
        )

        inserted = _insert_anchor_into_path(path_state, 0, 0, 25.0, 25.0)
        deleted = _delete_anchor_from_path(path_state, 0, 1)

        self.assertTrue(inserted)
        self.assertTrue(deleted)
        self.assertEqual([(anchor.image_x, anchor.image_y) for anchor in path_state.paths[0].anchors], [(10.0, 10.0), (40.0, 40.0), (70.0, 70.0)])

    def test_mark_complete_paths_finalized_marks_saved_quality_paths_done(self) -> None:
        path_state = EditorPathState(
            paths=[
                EditorPath(
                    anchors=[
                        EditorAnchor(image_x=10.0, image_y=10.0),
                        EditorAnchor(image_x=20.0, image_y=20.0),
                    ],
                    finalized=False,
                ),
                EditorPath(
                    anchors=[EditorAnchor(image_x=30.0, image_y=30.0)],
                    finalized=False,
                ),
            ],
            active_path_index=0,
        )

        _mark_complete_paths_finalized(path_state)

        self.assertTrue(path_state.paths[0].finalized)
        self.assertFalse(path_state.paths[1].finalized)
        self.assertIsNone(path_state.active_path_index)

    def test_quest_marker_icon_kind_switches_to_daily_variant_when_visible_daily_exists(self) -> None:
        kind = _quest_marker_icon_kind(
            "quest_giver",
            [
                {"quest_id": 10, "is_daily_quest": False},
                {"quest_id": 11, "is_daily_quest": True},
            ],
        )

        self.assertEqual(kind, "quest_giver_daily")

    def test_propagate_connected_anchor_position_keeps_joined_branch_endpoints_glued(self) -> None:
        path_state = EditorPathState(
            paths=[
                EditorPath(
                    anchors=[
                        EditorAnchor(image_x=10.0, image_y=10.0),
                        EditorAnchor(image_x=20.0, image_y=20.0, handle_in_x=18.0, handle_in_y=18.0),
                    ],
                    finalized=True,
                ),
                EditorPath(
                    anchors=[
                        EditorAnchor(image_x=20.0, image_y=20.0, handle_out_x=24.0, handle_out_y=24.0),
                        EditorAnchor(image_x=30.0, image_y=30.0),
                    ],
                    finalized=True,
                    start_connection=EditorPathConnection(path_index=0, anchor_index=1),
                ),
                EditorPath(
                    anchors=[
                        EditorAnchor(image_x=40.0, image_y=40.0),
                        EditorAnchor(image_x=20.0, image_y=20.0, handle_in_x=21.0, handle_in_y=21.0),
                    ],
                    finalized=True,
                    end_connection=EditorPathConnection(path_index=0, anchor_index=1),
                ),
            ]
        )

        _move_anchor_with_handles(path_state.paths[0].anchors[1], 50.0, 60.0)
        _propagate_connected_anchor_position(path_state, 0, 1)

        self.assertEqual((path_state.paths[1].anchors[0].image_x, path_state.paths[1].anchors[0].image_y), (50.0, 60.0))
        self.assertEqual((path_state.paths[2].anchors[1].image_x, path_state.paths[2].anchors[1].image_y), (50.0, 60.0))
        self.assertEqual((path_state.paths[1].anchors[0].handle_out_x, path_state.paths[1].anchors[0].handle_out_y), (54.0, 64.0))
        self.assertEqual((path_state.paths[2].anchors[1].handle_in_x, path_state.paths[2].anchors[1].handle_in_y), (51.0, 61.0))

    def test_propagate_connected_anchor_position_keeps_self_loop_endpoint_glued(self) -> None:
        path_state = EditorPathState(
            paths=[
                EditorPath(
                    anchors=[
                        EditorAnchor(image_x=10.0, image_y=10.0, handle_out_x=12.0, handle_out_y=12.0),
                        EditorAnchor(image_x=20.0, image_y=20.0),
                        EditorAnchor(image_x=10.0, image_y=10.0, handle_in_x=9.0, handle_in_y=9.0),
                    ],
                    finalized=True,
                    end_connection=EditorPathConnection(path_index=0, anchor_index=0),
                ),
            ]
        )

        _move_anchor_with_handles(path_state.paths[0].anchors[0], 40.0, 50.0)
        _propagate_connected_anchor_position(path_state, 0, 0)

        self.assertEqual((path_state.paths[0].anchors[2].image_x, path_state.paths[0].anchors[2].image_y), (40.0, 50.0))
        self.assertEqual((path_state.paths[0].anchors[2].handle_in_x, path_state.paths[0].anchors[2].handle_in_y), (39.0, 49.0))


if __name__ == "__main__":
    unittest.main()
