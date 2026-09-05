<div align="center">

<img src="app.png" alt="Altitude Editor Logo" width="128" height="128" />

# Altitude Editor

**High-speed 2D inspection, diagnostic, and editing tool for FPS Creator maps**

[![Qt](https://img.shields.io/badge/Qt-5.15.2-green.svg)](https://www.qt.io/)
[![C++](https://img.shields.io/badge/C++-17-blue.svg)](https://en.cppreference.com/)
[![Platform](https://img.shields.io/badge/Platform-Windows-lightgrey.svg)](https://microsoft.com/windows)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

**Lead Developer:** Ivan Klenov (aka NavY LiK)  
**Studio:** Madness Studio  

</div>

---

## 📌 Overview

**Altitude Editor** is designed primarily as a fast, lightweight **inspection and editing tool**, rather than a full-fledged replacement for the original FPS Creator 3D editor.

It provides streamlined, high-speed **2D navigation** (Doom/Build-style floorplan) that allows you to view FPS Creator levels with layer-by-layer floor switching, visual display of segments and entities, along with focused tools for editing and optimizing your maps.

<p align="center">
  <img src="docs/0.jpg" alt="Altitude Editor Main Interface" width="850" />
</p>

---

## 🌟 Core Highlights & Tools

### 1. 🚪 Portal Leak Detector
Level geometry issues can easily disrupt the **PVS portal generation** performed by the FPS Creator compiler, resulting in visual glitches, hall-of-mirrors effects, or broken visibility culling. The Leak Detector automatically scans geometry for bad segments and holes between segments:

* **Two Diagnostic Modes**:
  * **Logical Analysis**: Examines the raw segment structure, boundaries, and bitfields directly from `.fpm` / `.fps` files.
  * **Physical Analysis**: Reads the compiled universe (`universe.dbu`) after a test run in FPS Creator to verify actual BSP portals in 3D world space.
* **Smart Heuristics**: Pinpoints open ceiling/floor gaps into the void, inverted exterior walls facing outside, disconnected zones, and coplanar overlapping faces.
* **Interactive Viewport Highlighting**: Highlights problematic tiles with red warning overlays directly on the 2D grid; double-clicking any error in the list instantly centers the camera on the fault.

<p align="center">
  <img src="docs/3.jpg" alt="Portal & CSG Leak Detector" width="850" />
</p>

---

### 2. 📊 RAM Analyzer & Memory Budget Inspector
Due to the 32-bit architecture and DirectX 9 memory management of the classic FPS Creator engine, levels that exceed ~1.85 GB of RAM will crash with Out Of Memory (OOM) errors.

* **Precise Footprint Breakdown**: Asynchronously calculates memory allocations across mesh geometries (`.x`), textures (`.dds`, `.tga`, `.bmp`), sound effects (`.wav`, `.mp3`), segments, and universe data.
* **"Bigger Elephant in the Room"**: Automatically sorts entities and assets by memory consumption, giving you instant clues about which heavy textures or high-poly models you should downscale or optimize first.
* **Engine Limit Gauge**: Visual danger gauge (Safe / Caution / Critical / Over Budget) alerting you before you even launch a build.
* **Exportable Reports**: Generate detailed CSV tables or copy summary diagnostics directly to the clipboard.

<p align="center">
  <img src="docs/2.jpg" alt="RAM Analyzer and Memory Footprint Inspector" width="850" />
</p>

---

### 3. 👁️ Visibility Zone Manager (PVS)
Visualizes visibility sectors, generated portals, and doorways, letting you see exactly how the engine divides your map into distinct rooms.

* Replicates the engine's portal-cutting algorithm (modeled loosely after the classic BSP/PVS compiler to give practical visibility boundaries without 3D compile delays).
* Allows isolating active rooms, dimming outside sectors (Ghost mode), and previewing line-of-sight paths between adjacent zones.

<p align="center">
  <img src="docs/1.jpg" alt="2D Level Navigation and Visibility Zone Manager" width="850" />
</p>

---

### 4. 🔍 Instant Entity Search & Palette
The built-in dock panel allows you to quickly locate any placed item across the entire map:
* Search by entity name, category, script (`.fpi`), sound, or custom parameters.
* Filter by the current active floor or view map-wide totals.
* Double-click any entry in the list to immediately snap the viewport to that entity.

---

### 5. 🔄 Smart Reload from Disk (`F5`)
Working on dual monitors? Keep **Altitude Editor** open alongside the official FPS Creator editor. Whenever you save changes in FPS Creator, press **`F5`** in Altitude Editor to instantly reload the map from disk, giving you continuous live feedback on memory budgets, portal integrity, and entity placement.

---

### 6. 💻 Headless Console Mode (CLI)
For build pipelines, batch verification, or automated map diagnostics, `AltitudeEditor-cli.exe` can inspect maps without ever opening a GUI window.
* Automatically validate maps and detect leaks in batch scripts.
* Compute exact memory footprints and produce tabular reports.
* Render and export high-resolution minimap PNGs for documentation or web viewers.

> *Don't know if anybody needs that, but hey, I've already done it! 😬*

---

## 🖥️ Executables Overview

The distribution includes two standalone executables:

| Executable | Type | Description |
| :--- | :--- | :--- |
| **`AltitudeEditor.exe`** | GUI | Interactive visual editor and inspector. Runs without a console window; double-click or associate with `.fpm` files. |
| **`AltitudeEditor-cli.exe`** | CLI | Headless command-line utility for batch auditing, leak testing, memory estimation, and offscreen rendering. |

---

## ⚙️ Command-Line Interface (CLI)

`AltitudeEditor-cli.exe` options and commands:

### General Options
```text
  -h, --help                          Show command reference and usage guide
  -v, --version                       Print application version and build number
```

### Map Auditing & Diagnostics
```text
  --analyze-memory <map.fpm>          Calculate exact level RAM footprint (Segments, Entities, CSG, Lights)
  --check-leaks <map.fpm>             Analyze portals and CSG geometry for leaks and errors
  --dump-floor <map.fpm> <floor>      Print dimensions and block stats for a specific floor
```

### Headless Rendering & Minimap Export
```text
  --export-png <map.fpm> <out.png> [floor] [--color-zones]
                                      Render specified map floor to a high-resolution PNG image
  --snapshot-window <map.fpm> <out.png> [entity_idx] [--size W H] [--color-zones] [--floor N]
                                      Render offscreen editor window snapshot
  --snapshot-memory <map.fpm> <out.png>
                                      Render offscreen memory analyzer dialog snapshot
```

### CLI Usage Examples
```cmd
:: 1. Check version and build
AltitudeEditor-cli.exe --version

:: 2. Analyze memory footprint and 32-bit ceiling status
AltitudeEditor-cli.exe --analyze-memory "Files/mapbank/1.fpm"

:: 3. Run portal and leak checks
AltitudeEditor-cli.exe --check-leaks "Files/mapbank/1.fpm"

:: 4. Render floor 0 to PNG
AltitudeEditor-cli.exe --export-png "Files/mapbank/1.fpm" "minimap_floor0.png" 0
```

---

## ⌨️ Keyboard & Mouse Shortcuts

| Key / Shortcut | Action |
| :--- | :--- |
| **`F5`** | **Smart Reload**: Reload active map from disk |
| **`+`** / **`=`** | Move one floor up (`Floor Up`) |
| **`-`** / **`_`** | Move one floor down (`Floor Down`) |
| **`PageUp`** / **`PageDown`** | Navigate floors sequentially |
| **`Ctrl + Wheel`** / **`Shift + Wheel`** | Cycle floors with mouse wheel |
| **`[`** / **`]`** | Zoom out / Zoom in |
| **`0`** | Reset zoom to 100% |
| **`Home`** | Fit entire level in viewport (`Zoom Fit`) |
| **Right-click / Middle-click Drag** | Pan canvas |
| **Left-click** | Select entity / Clear selection |

---

## 🛠️ Building from Source

### Prerequisites
* **Operating System**: Windows 7 / 8 / 10 / 11 (32-bit or 64-bit)
* **Qt Toolkit**: Qt 5.15+ (MinGW 32-bit or MSVC 32-bit)
* **C++ Compiler**: MinGW 8.1.0 32-bit (GCC 8.1+) or compatible
* **Build System**: CMake >= 3.16, Ninja

### Build Steps (PowerShell)
```powershell
# 1. Add Qt5 and MinGW to environment PATH:
$env:PATH = "C:\Qt5\5.15.2\mingw81_32\bin;C:\Qt5\Tools\mingw810_32\bin;C:\Qt5\Tools\Ninja;" + $env:PATH

# 2. Enter project directory and create build directory:
mkdir -p "C:\FPSC Maped\build"
cd "C:\FPSC Maped\build"

# 3. Configure with CMake and Ninja:
cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="C:\Qt5\5.15.2\mingw81_32" ..

# 4. Compile targets (auto-increments build number):
ninja
```

---

## 📚 Technical Documentation

For details on the reverse-engineered FPS Creator binary map formats (`map.fpmb`, `map.ele`, bitfields, and segment containers), refer to:  
👉 **[FPSC_MAP_FORMAT_SPEC.md](FPSC_MAP_FORMAT_SPEC.md)**

---

## 📄 License

This project is licensed under the **MIT License** — see the [LICENSE](LICENSE) file for details.

```text
MIT License

Copyright (c) 2026 Ivan Klenov (aka NavY LiK) / Madness Studio
```
