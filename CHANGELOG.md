# Changelog

All notable changes to **Altitude Editor** will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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
      - `⚡ Remove Wall from Room A`: automatically clears the boundary wall from Room A while preserving its remaining walls.
      - `⚡ Remove Wall from Room B`: automatically clears the boundary wall from Room B while preserving its remaining walls.
      - `⚡ Open Passage (Both)`: clears boundary walls from both rooms to create an open archway/corridor.
    - Immediately marks the map as modified (`*` in title bar), rebuilds PVS connectivity in `VisZoneManager`, updates `MapCanvas`, and auto-refreshes `PortalLeakDialog` to clear the resolved warning.
- **Integration Points**:
  - Added `⚡ Resolve Clash / Edit...` button and right-click context menu in `PortalLeakDialog`.
  - Added right-click context menu on 2D map canvas segment tiles (`🧱 Inspect & Edit Segment...`).
  - Added main menu shortcut `Tools -> 🧱 Segment Inspector & Editor...` (`Ctrl+E`).

### Changed
- Incremented application version to **0.9.1**.
- Redesigned `SegmentEditorDialog` into a tabbed layout separating **Conflict Resolution** (with dedicated 2D visual collision diagram) from **Manual Tile Inspector** (with interactive 4-wall canvas widget).
- Installed `NoWheelFilter` on all numeric spinboxes and dropdowns across the editor, completely preventing accidental value changes when scrolling with the mouse wheel.
- Added full Russian localization for all new components (`SegmentEditorDialog`, `TileInteractiveWidget`, `ConflictDiagramWidget`, `PortalLeakDialog` zone filters, context menus), compiled into `altitude_editor_ru.qm`.
- Fixed tab title clipping in `SegmentEditorDialog` tab bar and expanded dialog dimensions to 780x640 for comfortable label readability.
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
