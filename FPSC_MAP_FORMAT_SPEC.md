# Comprehensive Specification of FPS Creator Map Formats & Engine Structures

**Author:** Ivan Klenov (aka NavY LiK)  
**Project:** Altitude Editor (FPS Creator 2D Map Editor & Analyzer)  
**Documentation Version:** 1.0 (2026)  
**Target Engines:** FPS Creator V1, V1.04, V1.07, V1.18 Classic, FPS Creator X10, WASP Branch  

---

## 1. Introduction & High-Level Architecture

The **FPS Creator (FPSC)** game engine, originally developed by *The Game Creators* under the direction of Lee Bamber on top of **DarkBasic Pro**, employs a modular tile-and-grid architecture for level construction.

An FPS Creator level comprises:
1. **3D Segment Grid**: Modular blocks of walls, floors, ceilings, and corridors of fixed dimensions ($100 \times 100 \times 100$ world units).
2. **Segment Bank**: A registry of unique segment definition files (`.fps`) utilized across the map.
3. **Entities**: Interactive game objects (characters, weapons, ammo, doors, scenery props, trigger zones, dynamic lights, and player markers) with arbitrary continuous 3D coordinates $(X, Y, Z)$ and spatial orientations.
4. **Waypoints**: AI navigation path graphs and patrol route sequences.
5. **Lights Table**: Static and dynamic point light emitters.

All level assets and descriptors are packaged into a single archive with the **`.fpm`** extension (*FPS Creator Map*).

---

## 2. Map Container: `.FPM` Format

An `.fpm` file is a standard **ZIP archive (PKZIP 2.0)** compressed using Deflate or Store algorithms.

### 2.1. Password Protection
In most official releases of FPS Creator, maps are protected against direct extraction by standard ZIP utilities using a hardcoded default password:
```text
mypassword
```
*(Note: some custom community builds or mod packages may save `.fpm` archives unencrypted without a password).*

### 2.2. Archive Contents
The `.fpm` container typically includes the following internal files:

| Internal Filename | Purpose | Format |
| :--- | :--- | :--- |
| `map.fpm` / `map.fpmb` | 3D level grid and dimension headers | Plaintext or binary DarkBasic Pro array |
| `map.seg` | Catalog of segment types referenced in the map | Plaintext (newline-separated `.fps` relative paths) |
| `map.ele` | Serialized properties of all placed entities | Variable-length binary records |
| `map.way` | AI waypoint coordinates and patrol graphs | Plaintext or binary |
| `map.lgt` | Light emitter parameter table | Plaintext or binary |
| `header.ini` *(optional)* | Level environment metadata (skybox, fog, shaders) | INI configuration file |

---

## 3. Coordinate System & 2D Canvas Mapping

### 3.1. Grid Dimensions
- By default, the level grid spans **$41 \times 41$ cells** horizontally ($X \in [0..40]$, $Y_{\text{grid}} \in [0..40]$).
- Vertically, the level is partitioned into **21 floors / layers** ($L \in [0..20]$).
- Tile size: **$100 \times 100 \times 100$ world units**.
- Total world bounding volume: $4100 \times 4100 \times 2100$ units.

### 3.2. Engine 3D World Coordinates
In the engine codebase (*DarkBasic Pro* / `FPSC-Game.DBA`), coordinates are evaluated as follows:
- **$X$ Axis**: Extends to the right ($X \ge 0$). Cell center:
  $$X_{\text{world}} = x \times 100 + 50$$
- **$Y$ Axis (Altitude / Height)**: Extends upward ($Y \ge 0$). Floor center:
  $$Y_{\text{world}} = L \times 100 + 50$$
- **$Z$ Axis (Depth)**: In DarkBasic Pro, the $Z$ axis extends forward with a **negative sign** ($Z \le 0$). Cell center:
  $$Z_{\text{world}} = -(y \times 100 + 50)$$

### 3.3. Top-Down 2D Projection
To project 3D world space onto a top-down 2D canvas (where $(0, 0)$ corresponds to the top-left corner):
$$\text{Canvas } X = X_{\text{world}}$$
$$\text{Canvas } Y = -Z_{\text{world}}$$
$$\text{Floor Layer } L = \left\lfloor \frac{Y_{\text{world}} + 25}{100} \right\rfloor$$

---

## 4. Level Grid: Binary Format `map.fpmb`

The `map.fpmb` file stores the state of the 3D cell array.

### 4.1. DarkBasic Pro 3D Array Serialization
In DarkBasic Pro, the statement `dim map(layermax, maxx, maxy)` allocates a 3-dimensional array serialized to disk in **Column-Major (Fortran) order**.

File structure of `map.fpmb`:
1. `int32 headerCount` — Header marker (typically 0).
2. `int32 totalCells` — Total cell count ($21 \times 41 \times 41 = 35\,301$).
3. Stream of 35,301 elements, each occupying **8 bytes**:
   - `int32 cellIndex`: Sequential element index;
   - `int32 mapid`: 32-bit encoded cell bitfield.

### 4.2. Cell Index Addressing Formula
For a cell located at coordinate tuple $(\text{layer}, x, y)$, the 1D serialized element index $i$ is computed as:
$$i = \text{layer} + x \times (\text{layers}) + y \times (\text{layers} \times \text{cols})$$
where $\text{layers} = \text{layermax} + 1 = 21$, $\text{cols} = \text{maxx} + 1 = 41$.

Conversely, when reading a sequential element stream ($i = 0 \dots 35\,300$):
$$\text{layer} = i \pmod{21}$$
$$\text{rem} = \lfloor i / 21 \rfloor$$
$$x = \text{rem} \pmod{41}$$
$$y = \lfloor \text{rem} / 41 \rfloor$$

### 4.3. Bitfield Layout of `mapid`
The `mapid` (DWORD) encodes the segment index, vertical alignment, and spatial orientation:

| Bits | Field Name | Description |
| :--- | :--- | :--- |
| **31 .. 20** (12 bits) | `segId` | 1-based segment index in `map.seg` (`0` = empty air) |
| **19 .. 16** (4 bits) | `scaler` | Vertical height scaling factor |
| **15 .. 14** (2 bits) | `ground` | Floor / ceiling snap flag |
| **13 .. 12** (2 bits) | `rotation` | Yaw rotation ($0 = 0^\circ, 1 = 90^\circ, 2 = 180^\circ, 3 = 270^\circ$) |
| **11 .. 10** (2 bits) | `orient` | Mirroring / inversion flag |
| **9 .. 4** (6 bits) | `symbol` | Special marker symbol ID |
| **3 .. 0** (4 bits) | `flags` | Auxiliary rendering and visibility flags |

**C++ Bit Extraction:**
```cpp
uint32_t mapid = ...;
int segId    = (mapid >> 20) & 0x0FFF;
int scaler   = (mapid >> 16) & 0x000F;
int ground   = (mapid >> 14) & 0x0003;
int rotation = (mapid >> 12) & 0x0003;
int orient   = (mapid >> 10) & 0x0003;
int symbol   = (mapid >> 4)  & 0x003F;
```

---

## 5. Segment Bank: `map.seg` Format

The `map.seg` file is a plaintext list of relative filepaths referencing segment definition files (`.fps`), indexed starting from one ($1, 2, 3 \dots$).

Example content:
```text
segments\scifi\rooms\corridora.fps
segments\scifi\doors\door_frame.fps
segments\ww2\scenery\armoury.fps
```

### 5.1. Segment Definition Structure (`.FPS`)
An `.fps` file is an INI-like configuration script:
```ini
; Segment Configuration File
desc          = Sci-Fi Corridor A
mesh          = meshbank\scifi\corridora.x
texture       = texturebank\scifi\corridora_D.dds
materialindex = 1
kind          = 0
```
Standard texture channel conventions:
- `_D.dds` / `_D.tga` — Diffuse color map.
- `_N.dds` / `_N.tga` — Normal / bump map.
- `_S.dds` / `_S.tga` — Specular reflectance map.
- `_I.dds` / `_I.tga` — Self-illumination / emissive map.

---

## 6. Entity Database: Binary Format `map.ele`

The `map.ele` file stores all dynamic and static placed entities. The format evolved across engine revisions.

### 6.1. File Header
- `int32 version` — Format version identifier ($100 \dots 218$).
- `int32 count` — Total number of serialized entity records.

### 6.2. String Deserialization
In DarkBasic Pro, `write string 1, a$` outputs characters followed by a **CRLF (`\r\n`)** sequence.
Parsers must scan incoming bytes up to the `\r\n` delimiter (2 bytes).

### 6.3. Entity Record Binary Layout

#### Base Record Block (Version 101):
1. `int32 mainType`
2. `int32 bankIndex` (1-based index in entity registry)
3. `int32 staticFlag` ($0$ = dynamic, $1$ = static)
4. `float x, y, z` (World coordinates, 12 bytes)
5. `float rx, ry, rz` (Euler rotation angles in degrees, 12 bytes)
6. `string name$` (Entity instance identifier, CRLF)
7. `string aiInit$` (Init FPI script, CRLF)
8. `string aiMain$` (Main behavior script, CRLF)
9. `string aiDestroy$` (Destroy script, CRLF)
10. `int32 isObjective`
11. `string useKey$` (Required key name, CRLF)
12. `string ifUsed$` (Trigger script when used, CRLF)
13. `string ifUsedNear$` (Proximity script, CRLF)
14. `int32 uniqueElement`
15. `string texD$` (Custom diffuse override, CRLF)
16. `string texAltD$` (Alternative texture, CRLF)
17. `string effect$` (Custom shader path, CRLF)
18. `int32 transparency`
19. `int32 editorFixed`
20. `string soundSet$` (Audio package, CRLF)
21. `string soundSet1$` (Secondary audio package, CRLF)
22. `int32[7] spawnParams` (28 bytes: `spawnmax`, `spawndelay`, `spawnqty`, `hurtfall`, `castshadow`, `reducetexture`, `speed`)
23. `string aiShoot$` (Combat behavior script, CRLF)
24. `string hasWeapon$` (Equipped weapon descriptor, CRLF)
25. `int32[4] liveSpawn` (16 bytes: `lives`, `spawn.max`, `spawn.delay`, `spawn.qty`)
26. `float[3] coneScale` (12 bytes: `scale`, `coneheight`, `coneangle`)
27. `int32[13] propsAndTrigger` (52 bytes: `strength`, `isimmobile`, `cantakeweapon`, `quantity`, `markerindex`, `light.color`, `light.range`, `areax1`, `areay1`, `areaz1`, `areax2`, `areay2`, `areaz2`)
28. `string baseDecal$` (Decal descriptor, CRLF)

#### Extended Version Blocks:
- **$\ge 102$**: **80 bytes** (20 fields: `rateoffire`, `damage`, `accuracy`, `reloadqty`, `fireiterations`, `lifespan`, `throwspeed`, `throwangle`, `bounceqty`, `explodeonhit`, `weaponisammo`, `spawnupto`, `spawnafterdelay`, `spawnwhendead`, `spare1..6`).
- **$\ge 103$**: **36 bytes** (9 fields: ODE / Newton physics parameters — `physics`, `phyweight`, `phyfriction`, `phyforcedamage`, `rotatethrow`, `explodable`, `explodedamage`, `phydw4`, `phydw5`).
- **$\ge 104$**: **4 bytes** (`phyalways`).
- **$\ge 105$**: **24 bytes** (6 fields: extended spawn randomizers).
- **$\ge 106$**: **8 bytes** (`spawnatstart`, `spawnlife`).
- **$\ge 107$**: **4 bytes** (`light.index` — dynamic light registry index).
- **$\ge 199$** *(FPS Creator X10)*: **68 bytes** (17 advanced AI attributes).
- **$\ge 200$** *(FPS Creator X10)*: **24 bytes** (6 advanced physics fields).
- **$\ge 217$** *(FPS Creator V1.18)*: **68 bytes** (17 particle emitter fields).
- **$\ge 218$** *(FPS Creator V1.18 Final)*: **4 bytes** (`particle.animated`).

---

## 7. Waypoints & AI Patrol Graphs: `map.way`

The `map.way` file defines AI navigation graphs:
- Stores an array of nodes $(\text{waypoint\_x}, \text{waypoint\_y}, \text{waypoint\_z})$.
- Waypoints are linked into linear or closed cyclical sequences.
- Rendered in 2D top-down view as directional dashed lines with arrowheads indicating patrol routes.

---

## 8. Entity Profiles `.FPE` and Chroma-Key Transparency

Each placed entity references a profile template located under `entitybank\...\*.fpe` (*FPS Creator Entity Profile*).

### 8.1. `.FPE` Configuration Format
Plaintext key-value configuration file:
```ini
; Saved by FPS Creator
desc          = Sci-Fi Door A
model         = meshbank\scifi\door_a.x
textured      = texturebank\scifi\door_a_D.dds
ischaracter   = 0
isweapon      = 0
health        = 100
speed         = 0
collisionmode = 1
defaultstatic = 0
ai_main       = defaultdoor.fpi
```

### 8.2. Preview Icons & Auto-Chroma-Key Algorithm
Accompanying each `.fpe` file is a $64 \times 64$ 24-bit RGB bitmap icon (`.bmp`).
Because classic BMP files lack an alpha channel, icons were rendered against solid white `RGB(255, 255, 255)` or solid black `RGB(0, 0, 0)` backgrounds.

**Auto-Chroma-Key Algorithm implemented in Altitude Editor:**
1. Sample four corner pixels $(0,0)$, $(W-1,0)$, $(0,H-1)$, and $(W-1,H-1)$.
2. If the corners are uniform and match a background color $(R_0, G_0, B_0)$:
   - Compute maximum component distance for each pixel:
     $$\Delta = \max(|R - R_0|, |G - G_0|, |B - B_0|)$$
   - If $\Delta < 12 \implies \text{Alpha} = 0$ (fully transparent).
   - If $12 \le \Delta < 28 \implies \text{Alpha} = \frac{\Delta - 12}{16} \times 255$ (smooth edge anti-aliasing).
   - Otherwise $\text{Alpha} = 255$ (fully opaque).

---

## 9. Memory Footprint Analysis (Memory Budget)

Because the FPS Creator engine runs as a **32-bit DirectX 9 application**, the process address space is limited to **$1.8 \dots 2.0\text{ GB}$**. Exceeding this limit causes an immediate crash (`Runtime Error 7005: Out of Memory`).

### 9.1. Mesh Memory Estimation (Meshes RAM)
For each unique `.x` mesh:
$$\text{RAM}_{\text{mesh}} \approx \text{Header} + (\text{Vertices} \times 32\text{ bytes}) + (\text{Indices} \times 2\text{ bytes})$$

### 9.2. Texture Memory Estimation (Textures VRAM)
- **Compressed DDS DXT1**: $\frac{\text{Width} \times \text{Height}}{2}$ bytes.
- **Compressed DDS DXT3 / DXT5**: $\text{Width} \times \text{Height}$ bytes.
- **Uncompressed 32-bit TGA / BMP / PNG**: $\text{Width} \times \text{Height} \times 4$ bytes.
- **Mipmaps**: Account for an additional $+33.3\%$ of total texture size.

### 9.3. Audio Memory Estimation (Audio RAM)
- **WAV (PCM)**: Uncompressed in-memory buffer size ($\text{SampleRate} \times \text{Channels} \times \text{BytesPerSample} \times \text{Duration}$).
- **MP3 / OGG**: Encoded stream size + DirectShow decoder buffer overhead ($\sim 512\text{ KB}$ per track).

---

## 10. Conclusion

This specification provides an authoritative reverse-engineered reference for the FPS Creator binary map formats and memory models, facilitating the development of external level tools, format converters, editors, and modern engine reimplementations compatible with The Game Creators ecosystem.
