# Altitude Editor (Qt5 / Modern C++)

[![Qt](https://img.shields.io/badge/Qt-5.15.2-green.svg)](https://www.qt.io/)
[![C++](https://img.shields.io/badge/C++-17-blue.svg)](https://en.cppreference.com/)
[![Platform](https://img.shields.io/badge/Platform-Windows-lightgrey.svg)](https://microsoft.com/windows)
[![License](https://img.shields.io/badge/License-MIT-brightgreen.svg)](LICENSE)

A modern, high-performance 2D level editor, map viewer, and memory footprint analyzer for the classic **FPS Creator** game engine (The Game Creators / DarkBasic Pro).

**Lead Developer:** Ivan Klenov (aka NavY LiK)  
**Studio:** Madness Studio  

---

## 📌 Why Altitude Editor?

The original **FPS Creator** editor (V1, X9, X10, WASP, Classic V1.18+) was designed in the early 2000s around a fixed screen resolution and a heavy 3D rendering pipeline based on DarkBasic Pro. In modern development and modding workflows, creators frequently need to:
1. **Quickly inspect multi-floor levels (`.fpm`)** without waiting for the bulky 3D engine to initialize.
2. **Review room layouts and wall textures in a clean, Doom/Build-engine-style 2D floorplan**, complete with linedefs, orientations, and height planes.
3. **Locate entities and assets instantly** (characters, weapons, ammo, props, dynamic lights, trigger zones, AI waypoints).
4. **Audit the level memory budget before compiling** to prevent crashes caused by the 32-bit engine memory ceiling (~1.8–2.0 GB RAM).
5. **Detect geometry and portal leaks** before lighting bake and CSG BSP compilation.

**Altitude Editor** solves these challenges with sub-second level loading, layer-by-layer visualization, asynchronous memory footprint profiling, and a rich command-line automation toolkit.

---

## ✨ Features

- 🗺️ **Layer-by-Layer 2D Map Visualization (Doom/Build-Style)**:
  - Textured segment walls rendered as linedef strips with high-contrast outlines.
  - Multi-floor browsing (Floors 0 .. 20) with instant switching and floor entity/segment counts (`[ent, seg]`).
  - **Ghost Layer Mode**: Semitransparent underlay of the layer above or below for visualizing stairs, shafts, and elevator shafts.
- 🧩 **100% Binary Compatibility with FPS Creator Formats**:
  - Reads encrypted and plaintext `.fpm` archives (ZIP containers with password `mypassword`).
  - Decodes column-major 3D grid data arrays (`map.fpmb`).
  - Unpacks cell bitfields (Segment ID, ground flag, rotation 0° / 90° / 180° / 270°).
  - Full support for `map.ele` entity versions (101 .. 107, 199, 200, 217, 218).
- 👾 **Full Entity Rendering & Management**:
  - Automatic chromakey transparency for 24-bit `.bmp` entity icons (no black or magenta halos).
  - Dynamic light visualization with radial halos, color indicators, and outer falloff rings matching light entities.
  - Trigger zones, AI waypoint graphs, and patrol routes.
- 🔍 **Palette & Search Dock**:
  - Search by name, entity type, AI script, sound effect, or custom attributes.
  - Filter by category (Player Start, Characters, Weapons, Ammo, Props, Doors, Lights, Zones).
  - Filter by current floor; double-click centers the camera on any entity.
- 📋 **Detailed Entity Property Inspector**:
  - Live inspection of 3D coordinates (X, Y, Z), Euler rotations, scale, AI scripts, physics attributes, spawn variables, and lighting parameters.
- 📊 **Asynchronous Memory Footprint Analyzer**:
  - Background calculation of mesh (`.x`), texture (`.dds`, `.tga`, `.bmp`), and audio (`.wav`, `.mp3`) RAM allocations.
  - Risk assessment gauge for the 32-bit engine limit (Safe / Caution / Critical).
  - Full numeric sorting across all columns (RAM, instance count, audio, textures).
- 🚪 **Portal & PVS Visibility Zone Analysis**:
  - Visual inspection of compiled CSG portals and visibility cells (`universe.dbu`).
  - Automated leak detection finding inverted walls, orphan portals, coplanar overlaps, and gaps to the void.
- 🔢 **Auto-Incrementing Build System**:
  - Automatic build number tracking displayed in title bars, about dialogs, and CLI tools.

---

## 🖥️ Executables Overview

The project provides two tailored executables:

| Executable | Subsystem | Description |
| :--- | :--- | :--- |
| **`AltitudeEditor.exe`** | Windows GUI | Primary visual editor. Runs without a console window; double-click to launch or associate with `.fpm` files. |
| **`AltitudeEditor-cli.exe`** | Console | Dedicated command-line automation and batch analysis utility. Headless: never opens GUI windows. |

---

## ⚙️ Command-Line Interface (CLI)

`AltitudeEditor-cli.exe` provides headless map auditing, memory estimation, leak detection, and high-resolution rendering for build pipelines, batch scripts, and mod management tools.

### General Options

```text
  -h, --help                          Show command reference and usage guide
  -v, --version                       Print application version and build number
```

### Map Analysis & Auditing

```text
  --analyze-memory <map.fpm>          Calculate exact level RAM footprint (Segments, Entities, CSG, Lights)
  --check-leaks <map.fpm>             Analyze portals and CSG geometry for leaks and errors
  --dump-floor <map.fpm> <floor>      Print dimensions and block stats for a specific floor
```

### Headless Rendering & Export

```text
  --export-png <map.fpm> <out.png> [floor] [--color-zones]
                                      Render map floor to a high-resolution PNG image
  --snapshot-window <map.fpm> <out.png> [entity_idx] [--size W H] [--color-zones] [--floor N]
                                      Headless offscreen window snapshot for automated testing
  --snapshot-memory <map.fpm> <out.png>
                                      Headless offscreen memory analyzer dialog snapshot
```

### CLI Examples

**1. Print Version and Build:**
```cmd
AltitudeEditor-cli.exe --version
# Output: Altitude Editor v0.9.0b (build 88)
```

**2. Analyze Memory Footprint:**
```cmd
AltitudeEditor-cli.exe --analyze-memory "Files/mapbank/1.fpm"
# Output:
# === MEMORY ANALYSIS: Level 1 ===
# Total Estimated Level RAM: 384.84 MB
#   - Segment Architecture: 109.61 MB (5 types, 70 blocks)
#   - Placed Entities: 14.71 MB (5 types, 8 placed)
#   - Universe & Lightmaps: 25.52 MB
#   - Engine Baseline: 235.00 MB
# Engine 32-bit Limit: 20.8% (Status: Safe (Optimal Memory))
```

**3. Check Portals & CSG Leaks:**
```cmd
AltitudeEditor-cli.exe --check-leaks "Files/mapbank/1.fpm"
# Output:
# === PORTAL & CSG LEAK ANALYSIS: Level 1 ===
# Total issues detected: 0
# Summary: 0 Errors, 0 Warnings
```

**4. Export Floor to High-Resolution PNG:**
```cmd
AltitudeEditor-cli.exe --export-png "Files/mapbank/1.fpm" "minimap_floor0.png" 0
```

---

## ⌨️ Keyboard & Mouse Shortcuts

| Key / Shortcut | Action |
| :--- | :--- |
| **`+`** / **`=`** | Move one floor up (`Floor Up`) |
| **`-`** / **`_`** | Move one floor down (`Floor Down`) |
| **`PageUp`** / **`PageDown`** | Navigate floors sequentially |
| **`Ctrl + Wheel`** / **`Shift + Wheel`** | Cycle floors with mouse wheel |
| **`[`** / **`]`** | Zoom out / Zoom in |
| **`0`** | Reset zoom to 100% |
| **`Home`** | Fit whole level in view (`Zoom Fit`) |
| **Right-click / Middle-click Drag** | Pan canvas |
| **Left-click** | Select entity / Clear selection |

---

## 🛠️ Building from Source

### Prerequisites
- **Operating System**: Windows 7 / 8 / 10 / 11 (32-bit or 64-bit)
- **Qt Toolkit**: Qt 5.15+ (MinGW 32-bit or MSVC 32-bit)
- **C++ Compiler**: MinGW 8.1.0 32-bit (GCC 8.1+) or compatible
- **Build System**: CMake >= 3.16, Ninja

### Build Instructions (PowerShell)

```powershell
# 1. Add Qt5 and MinGW to environment PATH:
$env:PATH = "C:\Qt5\5.15.2\mingw81_32\bin;C:\Qt5\Tools\mingw810_32\bin;C:\Qt5\Tools\Ninja;" + $env:PATH

# 2. Enter project directory and create build directory:
mkdir -p "C:\FPSC Maped\build"
cd "C:\FPSC Maped\build"

# 3. Configure with CMake and Ninja:
cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="C:\Qt5\5.15.2\mingw81_32" ..

# 4. Compile targets (automatically increments build number):
ninja
```

### Launching

Run the graphical editor:
```powershell
& "C:\FPSC Maped\build\AltitudeEditor.exe"
```
Or launch via the included helper script:
```cmd
run_viewer.bat
```

---

## 📦 Standalone Portable Distribution

A portable distribution folder is generated under `distribution/` containing all required Qt libraries, image format plugins, platform integrations, and localizations. This folder is completely self-contained and ready to be transferred to any other Windows PC without requiring Qt or development tools installed.

---

## 📚 Technical Documentation

A comprehensive reverse-engineered specification of the FPS Creator binary map formats, bitfields, and data structures is available here:  
👉 **[FPSC_MAP_FORMAT_SPEC.md](FPSC_MAP_FORMAT_SPEC.md)**
