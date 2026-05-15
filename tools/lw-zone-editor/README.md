# LivingWorld Zone Editor

This is the **separate app home** for the future LivingWorld zone/travel editor.

It is intentionally **not** part of `tools/lw-editor/`.

## Current slice status

This directory begins **WorldBotTravelNetworkRoadmap -> Slice 1: Canonical map asset pilot**.

The first slice is a tooling/data foundation, and now includes the first pass of a **client asset extractor pipeline**.

Current deliverables in this directory:

- a standalone client map extractor entrypoint
- a standalone client icon extractor entrypoint
- a standalone explored-zone viewer app entrypoint
- MPQ archive discovery helpers for WoW 3.3.5a-style clients
- BLP-to-PNG conversion via Pillow
- a standalone asset-staging pipeline
- a pilot catalog for a small initial zone set
- canonical output naming rules
- manifest generation
- validation report generation
- focused unit coverage
- extracted minimap/interface icon support for future marker overlays

## Why this exists separately

The existing `lw-editor` is focused on LivingWorld DB/content editing.

The zone editor is a different tool with different responsibilities:

- client map extraction
- map image staging
- coordinate calibration
- overlay authoring
- route export/import

Keeping it separate prevents travel-network authoring concerns from being mixed into the main bot editor app.

## Folder layout

```text
tools/lw-zone-editor/
  README.md
  requirements.txt
  extract_client_maps.py
  extract_client_icons.py
  stage_map_assets.py
  data/
    pilot_catalog.json
    extracted_maps/
      raw_blp/
      png/
    extracted_icons/
      raw_blp/
      png/
    staged_assets/
    manifests/
  lw_zone_editor/
    __init__.py
    catalog.py
    client_assets.py
    image_info.py
    paths.py
    staging.py
  tests/
    test_client_assets.py
    test_staging.py
```

## MPQ / client extraction path

For WotLK-era clients, the data is stored in **MPQ archives** and the map art is typically stored as **BLP** textures inside those archives.

That means the real problem is two-part:

1. open/search/extract files from MPQ archives
2. decode `.blp` textures into normal image files the zone editor can load

### Chosen approach

The current implementation direction is:

- use **Kanma/MPQExtractor** for archive extraction
- use **Pillow** for BLP decoding/conversion

That is a practical path because:

- MPQExtractor already supports wildcard extraction from WoW MPQs
- it can preserve archive-relative paths
- it can lowercase extracted paths
- Pillow already has BLP support available in this environment

### Build note for MPQExtractor

If `MPQExtractor` is not already available on your PATH, the intended helper repo is:

```text
https://github.com/Kanma/MPQExtractor
```

The extractor code in this repo will automatically try to find:

- `MPQExtractor` / `MPQExtractor.exe` on PATH
- `tools/MPQExtractor/build/bin/MPQExtractor(.exe)`
- `tools/third_party/MPQExtractor/build/bin/MPQExtractor(.exe)`

### Current target extraction patterns

The first extractor pass targets at least:

- `Interface\\WorldMap\\*`
- `World\\Minimaps\\*`
- `Interface\\Minimap\\*`

The icon extractor targets:

- `Interface\\Icons\\*`

These paths are where the client-side world/zone map surfaces are expected to be found for this slice.

### Why not use AzerothCore's existing extractor tools for this specific need

AzerothCore's built-in extractors are great for:

- DBC files
- runtime terrain/maps
- vmaps
- mmaps

But they do **not** directly hand us ready-to-load 2D world-map/zone-map image surfaces for the zone editor.

So for this feature, MPQ + BLP extraction is the right layer.

## Slice 1 workflow

### Step 1 - rip map assets out of the WoW client

Use the new extractor entrypoint:

```bash
cd tools/lw-zone-editor
python extract_client_maps.py --client-dir "C:\\Path\\To\\WoW-3.3.5a"
```

Expected outputs:

- raw extracted BLPs: `data/extracted_maps/raw_blp/`
- converted PNGs: `data/extracted_maps/png/`

### Step 1b - rip spell/item icon assets out of the WoW client

Use the icon extractor entrypoint:

```bash
cd tools/lw-zone-editor
python extract_client_icons.py --client-dir "C:\\Path\\To\\WoW-3.3.5a"
```

Expected outputs:

- raw extracted BLPs: `data/extracted_icons/raw_blp/`
- converted PNGs: `data/extracted_icons/png/`

### Step 2 - stage the pilot images into canonical names

1. Extract client map assets into `data/extracted_maps/png/`
2. Review or edit `data/pilot_catalog.json`
3. Run:

```bash
cd tools/lw-zone-editor
python stage_map_assets.py
```

4. Review generated outputs:
   - `data/manifests/pilot_manifest.json`
   - `data/manifests/pilot_validation_report.md`

### Step 3 - load explored zone composites in the separate app

Once explored composites exist in `data/extracted_maps/composite/`, launch the separate viewer app:

```bash
cd tools/lw-zone-editor
python view_zone_maps.py
```

Optional validation-only run:

```bash
cd tools/lw-zone-editor
python view_zone_maps.py --smoke-test --zone Elwynn
```

Current viewer slice features:

- lists canonical explored composites named `ZoneName - ZoneId.png`
- filters zones by name or zone id
- loads the selected explored composite in a dedicated standalone window
- shows basic metadata such as zone id, image dimensions, and file path

## Folder contract

The extractor pipeline uses:

```text
tools/lw-zone-editor/data/extracted_maps/raw_blp/
tools/lw-zone-editor/data/extracted_maps/png/
tools/lw-zone-editor/data/extracted_icons/raw_blp/
tools/lw-zone-editor/data/extracted_icons/png/
tools/lw-zone-editor/data/editor_routes/
tools/lw-zone-editor/data/exported_routes/
```

Route JSON saved from the viewer edit mode now writes two files:

```text
tools/lw-zone-editor/data/editor_routes/
tools/lw-zone-editor/data/exported_routes/
```

- `data/editor_routes/` stores the reloadable editor/source file with anchors, handles, and branch metadata
- `data/exported_routes/` stores the sampled runtime/export file with movement points

## Config

The editor reads environment-specific settings from:

```text
tools/lw-zone-editor/config.ini
```

Current supported config sections:

```ini
[route_sampling]
base_spacing_yards = 25.0
min_spacing_yards = 5.0
max_spacing_yards = 50.0

[route_storage]
route_data_root = data
editor_routes_subdir = editor_routes
exported_routes_subdir = exported_routes

[database]
host =
port = 3306
user =
password =
database = acore_world
mysql_binary = mysql
```

Notes:

- `route_data_root` may be a relative or absolute path
- relative paths are resolved from `tools/lw-zone-editor/`
- this makes it easy to point the editor at a server-local route bundle such as
  `D:/Wow Private/WotLK/Server/data/worldbot_routes`
- the marker-cache builder can use the `[database]` values as defaults so you do
  not need to repeat them every run

The staging pipeline then consumes PNGs from:

```text
tools/lw-zone-editor/data/extracted_maps/png/
```

## Pilot set

The initial pilot catalog uses a small Alliance-side set that is easy to reason about for early authoring:

- Stormwind City - hub / capital pilot
- Elwynn Forest - leveling-zone pilot
- Westfall - road-heavy travel pilot

This is only a starter slice. The catalog can expand later without changing the staging contract.

## Canonical naming rule

Staged files are renamed into a stable format:

```text
map_<mapId>__zone_<zoneId>__<slug>.<ext>
```

Example:

```text
map_000__zone_1519__stormwind_city.png
```

## Notes

The extractor path is now the intended source of truth for map-image inputs. The earlier temporary wording about manually sourcing images is no longer the target direction.

Instead, it establishes the separate-app contract and staging pipeline so that
when real pilot images are exported or captured, they can be dropped into a
known location and converted into a durable manifest immediately.

That keeps Slice 1 useful now while leaving room for later extraction and GUI
authoring work.
