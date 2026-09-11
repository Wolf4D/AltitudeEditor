# Changelog

All notable changes to **Altitude Editor** will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [0.9.2] - 2026-09-11

### Added
- **Straight Optical Line-of-Sight (LOS) Raycasting**:
  - Direct optical line-of-sight ray traces visibility through doorway and window portals between adjacent rooms.
  - Geometry-clipped strictly to inner room boundaries (`clipRayToZone`), eliminating misleading zigzags and wall-piercing rays.
- **Enhanced VisZone Badges & Click Priority**:
  - VisZone labels (`Z#`) rendered with crisp drop shadows and dark backdrops on top of entities, gizmos, and CSG cutouts.
  - High-priority click hit-testing allows selecting rooms directly even when dense entities or light sources are clustered in the room center.
- **Empty Space Deselection**:
  - Left-clicking anywhere in empty space outside of all visibility zones resets active zone selection, unhides all rooms, and clears active trace rays.
- **PVS Reachability & Culprit Analysis (`ZoneVisibilityDialog`)**:
  - Identify distant rooms rendered through portal cascades and jump directly to the culprit doorway causing unwanted through-wall visibility.
- **Portal Inspector & Multi-Row Visibility Chips (`VisZoneDock`)**:
  - Redesigned dock with clickable portal list, leak indicators (`🚨 Leak` / `👁 Visible`), and multi-row FlowLayout chips.
  - Full bilingual localization (English / Russian) across `VisZoneDock` and `MapCanvas`.
- **Documentation & Close-Up Views**:
  - Added dedicated optical LOS raycasting & portal inspector close-up screenshot (`docs/4.jpg`) and updated `README.md`.

### Changed
- Incremented application version to **0.9.2**.

---

## [0.9.1] - 2026-09-07

### Added
- **Portal Leak Filter by VisZones (`PortalLeakDialog`)**:
  - Interactive **VisZone Dropdown Filter** allowing level designers to isolate warnings to a specific room or visibility zone.
  - Displays dynamic per-zone issue counts in the dropdown (e.g., `All Vis Zones (229 issues)`, `Zone 1 — 3 issues`, `Outside / Void / Unzoned — 213 issues`).
  - Added dedicated **Vis Zone column** in the results table indicating which zone(s) each leak affects.
  - Smart dual-zone association for boundary leaks (`zoneId` and `zoneId2`): clashes between adjacent rooms appear in filter results for both involved zones.
- **Segment Inspector & Conflict Resolver Tool (`SegmentEditorDialog`)**:
  - **Single Segment Inspector**:
    - Inspect and modify any map tile `(Floor, X, Y)`.
    - Live segment bank asset switcher to swap segment meshes and textures.
    - `mapground` mode selector (Standard Room Floor, Interior 2, Roof / Ceiling Slab, Exterior Ground).
    - `mapsymbol` hole flag configuration (e.g. hole/void in floor).
  - **Visual 4-Wall Configuration Widget**:
    - Checkboxes for North, East, South, and West walls.
    - Mathematical bi-directional mapping between wall checkboxes and DarkBasic/FPS Creator `(maptile, maprotate)` auto-tiling parameters.
    - Quick presets: *All 4 Walls*, *3 Walls (U-Shape)*, *Corner (2 Walls)*, *Opposite (2 Walls)*, *1 Wall Divider*, *Open Floor (Interior)*, *Clear Tile (Empty Void)*.
    - Live auto-apply toggle with instant viewport refresh.
  - **Double-Wall Boundary Conflict Resolver**:
    - Dedicated conflict view displaying Room A and Room B side-by-side with boundary coordinates and segment names.
    - Identifies and highlights the exact clashing wall on each room's shared border.
    - **1-Click Fix Buttons**:
      - `🟢 Keep Room A Wall`: automatically clears the boundary wall from Room B while preserving its remaining walls.
      - `🟢 Keep Room B Wall`: automatically clears the boundary wall from Room A while preserving its remaining walls.
    - Immediately marks the map as modified (`*` in title bar), rebuilds PVS connectivity in `VisZoneManager`, updates `MapCanvas`, and auto-refreshes `PortalLeakDialog` to clear the resolved warning.
  - **Segment Asset Filter & Constrained View**:
    - Added instant substring search filter (`🔍 Filter...` / `🔍 Фильтр...`) next to the segment combobox with one-click clear button.
    - Constrained segment and preset dropdown popups to 10 visible items with native scrollbars (`combobox-popup: 0; max-height: 240px;`), preventing fullscreen vertical overflow on large segment banks.
- **Integration Points**:
  - Added `⚡ Resolve Clash / Edit...` button and right-click context menu in `PortalLeakDialog`.
  - Added right-click context menu on 2D map canvas segment tiles (`🧱 Inspect & Edit Segment...`).
  - Added main menu shortcut `Tools -> 🧱 Segment Inspector & Editor...` (`Ctrl+E`).

### Changed
- Incremented application version to **0.9.1**.
- Redesigned `SegmentEditorDialog` into a tabbed layout separating **Conflict Resolution** (with dedicated 2D visual collision diagram) from **Manual Tile Inspector** (with interactive 4-wall canvas widget).
- Streamlined Double-Wall Conflict Resolver options to specifically keep either Room A or Room B wall, removing the ambiguous "open passage" option.
- Conflict Resolver dialog now automatically closes upon applying a resolution instead of switching tabs or displaying modal message boxes.
- `PortalLeakDialog` now strictly preserves the user's active Vis Zone filter and table row selection when auto-refreshing after conflict resolutions, retaining zero-issue zones as `Zone X — 0 issues (Resolved)`.
- Constrained `PortalLeakDialog` Vis Zone filter dropdown to 15 visible items with native scrollbar and max height 280px.
- **VisZone Multi-Story Merging & Air Tile Void Fixing (`VisZoneManager`)**:
  - Fixed vertical room segmentation where stacked segments with `mapsymbol = 1` (hidden floor mesh) were severed into separate zones due to lower-level segment nominal `visRoof >= 0` descriptors; multi-story corridors and rooms (e.g. Zone 7 on Floor 5 and former Zone 45 on Floor 6) now cleanly merge into a single multi-floor VisZone.
  - Fixed "holes" in multi-story high-ceiling rooms (e.g. Zone 2 and Zone 5 / former Zone 37) on upper floors where interior air tiles (`block = 0`) were left unzoned because perimeter walls had roof flags (`gridGround = 2`); refined `isExplicitCeilingSlab` to ensure wall-bearing segments (`maptile != 6`) are never treated as impassable flat ceiling slabs.
  - Multi-story halls and rooms now completely and solidly fill interior air volumes across all intermediate floors without black holes or unzoned voids.
  - Automatically clear defect / tile selection highlight in `MapCanvas` when defect is resolved or user clicks on canvas or presses Escape.
- Installed `NoWheelFilter` on all numeric spinboxes and dropdowns across the editor, completely preventing accidental value changes when scrolling with the mouse wheel.
- Added full Russian localization for all new components (`SegmentEditorDialog`, `TileInteractiveWidget`, `ConflictDiagramWidget`, `PortalLeakDialog` zone filters, context menus), compiled into `altitude_editor_ru.qm`.
- Fixed tab title clipping in `SegmentEditorDialog` tab bar and expanded dialog dimensions to 780x640 for comfortable label readability.
- Automatically load the last opened map on application launch; if no recent map exists, start cleanly with an empty editor (completely removed test map `1.fpm`).
- Fixed disappearance of secondary double-wall boundary conflicts: fixed `VisZoneManager::pruneOpenRoofZones()` erroneously deleting doorless rooms as open exterior roofs upon removing a clashing wall, and properly check opposite neighbor walls and ceiling presence to preserve zone integrity.
- Fixed persistent red canvas highlight box: automatically reset and clear the defect highlight rectangle on `MapCanvas` upon applying a segment resolution, deselecting, pressing Escape, or clicking.
- Fixed unzoned areas on upper floors: recognized ceilings of lower rooms as physical floor barriers for upper rooms (handling FPS Creator `mapsymbol = 1` floor limb hiding), added a second seeding pass for non-scenery room segments, and preserved enclosed top-floor rooms, mezzanines, and stairwells in `VisZoneManager`.
- Reused cached `VisZoneManager` instances across static leak detection passes, improving leak analysis execution speed.

---

## [0.9.0b] - 2026-09-04

### Added
- **Portal & CSG Leak Detector**:
  - **Physical Compiled BSP Analysis**: reads `universe.dbu` from testlevel to verify world-space see-through portals and CSG gaps into the void.
  - Timestamp verification comparing `universe.dbu`, `.fpm`, and compiler `temp.fpm` to detect outdated or mismatched universe builds.
  - **Static Grid / Topological Analysis**: detects missing ceiling slabs over enclosed rooms, outer wall perimeter breaches into universe void, exterior segments placed inside interior rooms, duplicate identical block/overlay Z-fighting, and double-wall boundary clashes between adjacent rooms.
  - Interactive table with double-click viewport navigation to fault coordinates.
- **RAM Analyzer & Memory Budget Inspector**:
  - Multi-threaded asynchronous scanning of mesh geometries (`.x`), textures (`.dds`, `.tga`, `.bmp`), and sound effects (`.wav`, `.mp3`).
  - "Bigger Elephant in the Room" asset ranking for memory optimization.
  - 32-bit DirectX 9 memory budget danger gauge (Safe / Caution / Critical / Over Budget).
  - CSV report export and clipboard summary diagnostics.
- **Visibility Zones (PVS) & Portals Manager**:
  - Room flood-fill partitioning and portal doorway calculation matching the FPS Creator engine.
  - Dedicated right-dock panel with active room isolation, dimming (ghost mode), and camera line-of-sight tracking.
- **Save Map Support (`FPMWriter`)**:
  - High-performance binary level serialization supporting DarkBasic XOR encrypted `.fpm` maps.
- **Headless Console Tool (`AltitudeEditor-cli`)**:
  - Automated batch verification, RAM analysis, and minimap rendering without a GUI.
- **Bilingual Internationalization**:
  - English and Russian UI support with runtime language switching.
