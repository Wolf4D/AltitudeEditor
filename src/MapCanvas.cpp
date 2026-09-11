#include "MapCanvas.h"
#include "AssetManager.h"
#include "PortalLeakAnalyzer.h"
#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QMenu>
#include <cmath>
#include <set>
#include <QElapsedTimer>

static const float TILE_SIZE = 100.0f;

MapCanvas::MapCanvas(QWidget* parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setAttribute(Qt::WA_OpaquePaintEvent);

    connect(&m_animTimer, &QTimer::timeout, this, [this]() {
        m_animPhase += 0.1f;
        if (m_animPhase > 6.28318f) m_animPhase -= 6.28318f;
        if (m_selectedEntityIndex >= 0 || m_highlightedLayer >= 0 || !m_leakWarnings.empty()) {
            update();
        }
    });
}

void MapCanvas::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    m_animTimer.start(50);
}

void MapCanvas::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    m_animTimer.stop();
}

void MapCanvas::setMap(std::shared_ptr<FPSCMap> map, bool preserveView) {
    int savedFloor = m_currentFloor;
    QPointF savedPan = m_panOffset;
    float savedZoom = m_zoom;
    int savedSelectedEntity = m_selectedEntityIndex;
    int savedVisZone = m_activeVisZoneId;

    m_map = map;
    m_hoveredEntityIndex = -1;
    if (!m_visZoneManager) {
        m_visZoneManager = std::make_shared<VisZoneManager>();
    }
    if (m_visZoneManager && m_map) {
        m_visZoneManager->buildFromMap(m_map);
    }
    if (m_map) {
        if (preserveView) {
            m_currentFloor = qBound(0, savedFloor, m_map->header.layerMax);
            m_panOffset = savedPan;
            m_zoom = savedZoom;
            if (savedSelectedEntity >= 0 && savedSelectedEntity < m_map->placedEntities.size()) {
                m_selectedEntityIndex = savedSelectedEntity;
            } else {
                m_selectedEntityIndex = -1;
            }
            if (savedVisZone >= 0 && m_visZoneManager && savedVisZone < static_cast<int>(m_visZoneManager->zones().size())) {
                m_activeVisZoneId = savedVisZone;
            } else {
                m_activeVisZoneId = -1;
            }
            emit floorChanged(m_currentFloor);
            emit zoomChanged(m_zoom);
        } else {
            m_selectedEntityIndex = -1;
            m_activeVisZoneId = -1;
            m_currentFloor = qBound(0, m_map->activeEditorLayer, m_map->header.layerMax);
            emit floorChanged(m_currentFloor);
            zoomFit();
        }
    }
    update();
}

void MapCanvas::setFloor(int floor) {
    if (!m_map) return;
    int bounded = qBound(0, floor, m_map->header.layerMax);
    if (m_currentFloor != bounded) {
        m_currentFloor = bounded;
        m_highlightedLayer = -1; // Reset highlight on manual floor change
        emit floorChanged(m_currentFloor);
        update();
    }
}

void MapCanvas::clearHighlight() {
    if (m_highlightedLayer != -1 || m_highlightedX != -1 || m_highlightedY != -1) {
        m_highlightedLayer = -1;
        m_highlightedX = -1;
        m_highlightedY = -1;
        update();
    }
}

void MapCanvas::highlightCell(int layer, int x, int y) {
    if (!m_map) return;
    if (layer < 0 || x < 0 || y < 0) {
        clearHighlight();
        return;
    }
    setFloor(layer);
    m_highlightedLayer = layer;
    m_highlightedX = x;
    m_highlightedY = y;

    // Center camera on this cell
    float cellWorldX = (x * TILE_SIZE) + (TILE_SIZE / 2.0f);
    float cellWorldY = (y * TILE_SIZE) + (TILE_SIZE / 2.0f);

    if (m_zoom < 0.5f) {
        m_zoom = 0.8f;
        emit zoomChanged(m_zoom);
    }

    m_panOffset = QPointF(width() / 2.0f - cellWorldX * m_zoom, height() / 2.0f - cellWorldY * m_zoom);

    update();
}

void MapCanvas::setLeakWarnings(const std::vector<PortalLeakWarning>& warnings) {
    m_leakWarnings = warnings;
    update();
}

void MapCanvas::clearLeakWarnings() {
    if (!m_leakWarnings.empty()) {
        m_leakWarnings.clear();
        update();
    }
}

void MapCanvas::floorUp() {
    setFloor(m_currentFloor + 1);
}

void MapCanvas::floorDown() {
    setFloor(m_currentFloor - 1);
}

void MapCanvas::selectEntity(int index) {
    if (m_selectedEntityIndex == index) return;
    m_selectedEntityIndex = index;
    if (m_map && index >= 0 && index < m_map->placedEntities.size()) {
        const PlacedEntity& ent = m_map->placedEntities[index];
        setFloor(ent.floorLayer);
    }
    emit entitySelected(index);
    update();
}

void MapCanvas::focusOnEntity(int index) {
    if (!m_map || index < 0 || index >= m_map->placedEntities.size()) return;
    const PlacedEntity& ent = m_map->placedEntities[index];
    setFloor(ent.floorLayer);
    m_selectedEntityIndex = index;

    // Center screen on entity position (World Z is negative in FPSC coords)
    QPointF entScreen = worldToScreen(QPointF(ent.x, -ent.z));
    QPointF centerScreen(width() / 2.0f, height() / 2.0f);
    m_panOffset += (centerScreen - entScreen);

    emit entitySelected(index);
    update();
}

void MapCanvas::zoomIn() {
    m_zoom = qMin(m_zoom * 1.25f, 10.0f);
    emit zoomChanged(m_zoom);
    update();
}

void MapCanvas::zoomOut() {
    m_zoom = qMax(m_zoom / 1.25f, 0.1f);
    emit zoomChanged(m_zoom);
    update();
}

void MapCanvas::zoomReset() {
    m_zoom = 1.0f;
    emit zoomChanged(m_zoom);
    update();
}

void MapCanvas::zoomFit() {
    if (!m_map) return;
    int cols = m_map->header.maxX + 1;
    int rows = m_map->header.maxY + 1;
    float mapW = cols * TILE_SIZE;
    float mapH = rows * TILE_SIZE;

    float scaleX = (width() - 80.0f) / mapW;
    float scaleY = (height() - 80.0f) / mapH;
    m_zoom = qBound(0.1f, qMin(scaleX, scaleY), 5.0f);

    m_panOffset.setX((width() - mapW * m_zoom) / 2.0f);
    m_panOffset.setY((height() - mapH * m_zoom) / 2.0f);

    emit zoomChanged(m_zoom);
    update();
}

void MapCanvas::setShowWallTextures(bool show) { m_showWallTextures = show; update(); }
void MapCanvas::setShowFloorTextures(bool show) { m_showFloorTextures = show; update(); }
void MapCanvas::setShowGrid(bool show) { m_showGrid = show; update(); }
void MapCanvas::setShowEntities(bool show) { m_showEntities = show; update(); }
void MapCanvas::setShowLights(bool show) { m_showLights = show; update(); }
void MapCanvas::setShowZones(bool show) { m_showZones = show; update(); }
void MapCanvas::setShowWaypoints(bool show) { m_showWaypoints = show; update(); }
void MapCanvas::setShowGhostLayer(bool show) { m_showGhostLayer = show; update(); }

QPointF MapCanvas::worldToScreen(const QPointF& worldPos) const {
    return QPointF(
        m_panOffset.x() + worldPos.x() * m_zoom,
        m_panOffset.y() + worldPos.y() * m_zoom
    );
}

QPointF MapCanvas::screenToWorld(const QPointF& screenPos) const {
    return QPointF(
        (screenPos.x() - m_panOffset.x()) / m_zoom,
        (screenPos.y() - m_panOffset.y()) / m_zoom
    );
}

QRectF MapCanvas::getCellRectScreen(int x, int y) const {
    QPointF tl = worldToScreen(QPointF(x * TILE_SIZE, y * TILE_SIZE));
    float sz = TILE_SIZE * m_zoom;
    return QRectF(tl.x(), tl.y(), sz, sz);
}

void MapCanvas::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    renderMap(p);
}

void MapCanvas::renderMap(QPainter& p) {
    // Deep Dark Level Background
    p.fillRect(rect(), QColor(22, 25, 34));

    if (!m_map) {
        p.setPen(QColor(140, 145, 160));
        p.setFont(QFont("Segoe UI", 13));
        p.drawText(rect(), Qt::AlignCenter, QStringLiteral("No map loaded. Open a .FPM map file from File menu."));
        return;
    }

    // 1. Grid
    if (m_showGrid) {
        drawGrid(p);
    }

    // 2. Ghost lower floor (if enabled, currentFloor > 0 and no isolated zone)
    if (m_showGhostLayer && m_currentFloor > 0 && (!m_cullInactiveVisZones || m_activeVisZoneId < 0)) {
        drawSegments(p, m_currentFloor - 1, 0.22f);
    }

    // 3. Current active floor segments & Doom-style wall textures
    drawSegments(p, m_currentFloor, 1.0f);

    // 3b. CSG Wall Cutouts & Overlays (always visible)
    drawCSGCutouts(p);

    // 4. AI Waypoints
    if (m_showWaypoints) {
        drawWaypoints(p);
    }

    // 5. Trigger zones & Lights
    if (m_showZones || m_showLights) {
        drawZonesAndLights(p);
    }

    // 6. Entities
    if (m_showEntities) {
        drawEntities(p);
    }

    // 7. Portals & VisZones (from compiled universe.dbu or topological zones)
    if (m_showPortals || m_activeVisZoneId >= 0 || m_colorAllVisZones) {
        drawPortals(p);
    }

    // 7b. CSG / Portal Leak Warnings (when leak detector is active)
    if (!m_leakWarnings.empty()) {
        drawLeakWarnings(p);
    }

    // 7c. Visibility Trace Path Overlay (when Tracer is active)
    if (!m_activeTracePath.empty()) {
        drawTracePath(p);
    }

    // 7d. Interactive Translation Gizmo on selected entity
    drawGizmo(p);

    if (m_highlightedLayer == m_currentFloor && m_highlightedX >= 0 && m_highlightedY >= 0) {
        QRectF hlRect = getCellRectScreen(m_highlightedX, m_highlightedY);
        float pulse = 0.5f + 0.5f * std::sin(m_animPhase * 3.0f);
        QColor outlineColor = QColor::fromRgbF(1.0f, 0.2f * (1.0f - pulse), 0.1f);
        p.setPen(QPen(outlineColor, 3.5f, Qt::SolidLine));
        
        QColor fill = Qt::red;
        fill.setAlphaF(0.20f + 0.15f * pulse);
        p.setBrush(fill);
        p.drawRect(hlRect);

        // Corner target reticle markers
        p.setPen(QPen(Qt::yellow, 2.5f));
        float arm = qMin(hlRect.width(), hlRect.height()) * 0.28f;
        // TL
        p.drawLine(hlRect.topLeft(), hlRect.topLeft() + QPointF(arm, 0));
        p.drawLine(hlRect.topLeft(), hlRect.topLeft() + QPointF(0, arm));
        // TR
        p.drawLine(hlRect.topRight(), hlRect.topRight() - QPointF(arm, 0));
        p.drawLine(hlRect.topRight(), hlRect.topRight() + QPointF(0, arm));
        // BL
        p.drawLine(hlRect.bottomLeft(), hlRect.bottomLeft() + QPointF(arm, 0));
        p.drawLine(hlRect.bottomLeft(), hlRect.bottomLeft() - QPointF(0, arm));
        // BR
        p.drawLine(hlRect.bottomRight(), hlRect.bottomRight() - QPointF(arm, 0));
        p.drawLine(hlRect.bottomRight(), hlRect.bottomRight() - QPointF(0, arm));
    }

    // 8. VisZone Badges (Topmost Layer over Entities, CSG cutouts, and Gizmos)
    drawZoneBadges(p);

    // 9. HUD Overlays
    drawHUD(p);
}

void MapCanvas::drawGrid(QPainter& p) {
    int cols = m_map->header.maxX + 1;
    int rows = m_map->header.maxY + 1;

    p.save();
    p.setRenderHint(QPainter::Antialiasing, false);

    // Minor cell grid lines
    p.setPen(QPen(QColor(38, 43, 56), 1, Qt::SolidLine));
    for (int x = 0; x <= cols; ++x) {
        QPointF p1 = worldToScreen(QPointF(x * TILE_SIZE, 0));
        QPointF p2 = worldToScreen(QPointF(x * TILE_SIZE, rows * TILE_SIZE));
        p.drawLine(p1, p2);
    }
    for (int y = 0; y <= rows; ++y) {
        QPointF p1 = worldToScreen(QPointF(0, y * TILE_SIZE));
        QPointF p2 = worldToScreen(QPointF(cols * TILE_SIZE, y * TILE_SIZE));
        p.drawLine(p1, p2);
    }

    // Outer level border
    QPointF mapTL = worldToScreen(QPointF(0, 0));
    QPointF mapBR = worldToScreen(QPointF(cols * TILE_SIZE, rows * TILE_SIZE));
    p.setPen(QPen(QColor(75, 85, 110), 2, Qt::SolidLine));
    p.drawRect(QRectF(mapTL, mapBR));

    // Highlight hovered tile (neutral soft highlight so it doesn't look like a portal or zone)
    if (m_hoveredTile.x() >= 0 && m_hoveredTile.x() < cols &&
        m_hoveredTile.y() >= 0 && m_hoveredTile.y() < rows)
    {
        QRectF hRect = getCellRectScreen(m_hoveredTile.x(), m_hoveredTile.y());
        p.fillRect(hRect, QColor(255, 255, 255, 18));
        p.setPen(QPen(QColor(180, 200, 230, 130), 1.0f, Qt::DashLine));
        p.drawRect(hRect);
    }

    p.restore();
}

static bool isMaptileWallPresent(int maptile, int rot, int side) {
    if (maptile <= 0 || maptile == 6) return false;
    // Base walls at rot = 0: [0: North, 1: East, 2: South, 3: West]
    static const bool baseWalls[16][4] = {
        {0, 0, 0, 0}, // 0: none
        {1, 1, 1, 1}, // 1: 4 walls
        {1, 0, 1, 1}, // 2: 3 walls (no East)
        {1, 0, 0, 1}, // 3: 2 walls corner (North + West)
        {1, 0, 1, 0}, // 4: 2 walls opposite (North + South)
        {1, 0, 0, 0}, // 5: 1 wall (North)
        {0, 0, 0, 0}, // 6: 0 walls (floor only)
        {0, 0, 0, 0}, // 7: corner
        {0, 0, 0, 0}, // 8: corner
        {0, 0, 0, 0}, // 9: corner
        {0, 0, 0, 0}, // 10: corner
        {0, 0, 0, 0}, // 11: corner
        {1, 0, 0, 0}, // 12: straight
        {1, 0, 0, 0}, // 13: straight
        {1, 0, 0, 0}, // 14: straight
        {1, 0, 0, 1}  // 15: corner
    };
    int unrotatedSide = (side - (rot & 3) + 4) % 4;
    if (maptile >= 0 && maptile < 16) {
        return baseWalls[maptile][unrotatedSide];
    }
    return false;
}

// Returns true if side (0=N, 1=E, 2=S, 3=W) has a railing/barrier for this corridor/gantry
// Based directly on FPS Creator engine logic (FPSC-Game.DBA:48558-48592)
static bool getGantryRailingOnSide(int kindOf, int mode, int orient, int side) {
    int k = kindOf;
    if (k == 0 && mode >= 3) k = mode - 2;
    int o = orient & 3;

    if (k == 1) { // Straight
        if (o % 2 == 0) return (side == 1 || side == 3);
        else return (side == 0 || side == 2);
    }
    if (k == 2) { // Corner
        if (o == 0) return (side == 0 || side == 3);
        if (o == 1) return (side == 0 || side == 1);
        if (o == 2) return (side == 2 || side == 1);
        if (o == 3) return (side == 2 || side == 3);
    }
    if (k == 3) { // TJunction
        return (side == o);
    }
    if (k == 4) { // Cross
        return false;
    }
    if (k == 5) { // Deadend
        int openSide = (o + 2) % 4;
        return (side != openSide);
    }
    return false;
}

void MapCanvas::drawSegments(QPainter& p, int layer, float opacity) {
    if (!m_map || layer < 0 || layer >= m_map->gridBlocks.size()) return;

    int cols = m_map->header.maxX + 1;
    int rows = m_map->header.maxY + 1;
    float sz = TILE_SIZE * m_zoom;
    float wallRibbon = qMax(4.0f, qMin(16.0f, sz * 0.16f));

    p.save();
    p.setOpacity(opacity);

    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            int segId = m_map->gridBlocks[layer][y][x];
            if (segId <= 0) continue;

            // Visibility Zone Isolation / Culling
            float cellOpacity = opacity;
            bool isDimmedGray = false;
            bool shouldIsolate = (m_activeVisZoneId >= 0 && m_cullInactiveVisZones) || m_highlightedPortal.isValid();
            if (shouldIsolate && m_visZoneManager) {
                bool inZone = isTileInActiveOrPath(layer, x, y);
                if (!inZone) {
                    if (m_visZoneDimOpacity <= 0.01f) {
                        continue; // Strictly hidden if user explicitly pulls slider to 0%
                    } else {
                        cellOpacity = opacity * m_visZoneDimOpacity;
                        isDimmedGray = true;
                    }
                }
            }
            p.setOpacity(cellOpacity);

            auto it = m_map->segments.find(segId);
            if (it == m_map->segments.end()) continue;
            const auto& seg = it.value();

            QRectF cellRect = getCellRectScreen(x, y);
            int rot = m_map->gridRotation[layer][y][x] & 3;

            int ground = (layer < m_map->gridGround.size() && y < m_map->gridGround[layer].size() && x < m_map->gridGround[layer][y].size())
                         ? m_map->gridGround[layer][y][x] : 0;

            // In .fps spec, groundMode == 2 and visRoof >= 0 or (visFloor == -1 && visRoof >= 0) defines a ceiling/roof slab.
            bool isCeilingSeg = (seg->groundMode == 2 && seg->hasRoofOnThisLayer) ||
                                (seg->visFloor == -1 && seg->visRoof >= 0);

            int symbol = (layer < m_map->gridSymbol.size() && y < m_map->gridSymbol[layer].size() && x < m_map->gridSymbol[layer][y].size())
                         ? m_map->gridSymbol[layer][y][x] : 0;

            // In FPS Creator engine (FPSC-Game.DBA lines 7225-7229):
            // if mapsymbol=1
            //  if segmentprofile(seg).vis.f<>-1 then hide limb obj,segmentprofile(seg).vis.f
            //  if segmentprofile(seg).vis.r<>-1 then hide limb obj,segmentprofile(seg).vis.r
            // endif
            bool drawFloor = false;
            if (isCeilingSeg) {
                drawFloor = (symbol != 1) && (!seg->roofTexture.isEmpty() || !seg->floorTexture.isEmpty());
            } else if (seg->isPlatformOrGantry || seg->isStairs) {
                // Platforms, Gantries, and Stairs always render their floor surface
                drawFloor = !seg->floorTexture.isEmpty();
            } else if (seg->hasFloorOnThisLayer && seg->visFloor >= 0) {
                drawFloor = (symbol != 1) && !seg->floorTexture.isEmpty();
            }
            // Segments render their walls if they have wall geometry defined
            bool drawWalls = !seg->isPlatformOrGantry && !seg->isStairs &&
                             (seg->hasWall[0] || seg->hasWall[1] || seg->hasWall[2] || seg->hasWall[3]);

            int orient = (layer < m_map->gridOrientation.size() && y < m_map->gridOrientation[layer].size() && x < m_map->gridOrientation[layer][y].size())
                         ? m_map->gridOrientation[layer][y][x] : 0;

            // A. Draw Floor / Slab Top Surface Texture
            if (drawFloor) {
                // In top-down 2D view, the visible top surface of any slab is floorTexture.
                // Only fall back to roofTexture if no top surface texture exists on the segment.
                QString surfaceTex = !seg->floorTexture.isEmpty() ? seg->floorTexture : seg->roofTexture;
                if (!surfaceTex.isEmpty()) {
                    if (m_showFloorTextures) {
                        QPixmap surfacePx = AssetManager::instance().loadTexture(surfaceTex);
                        if (!surfacePx.isNull()) {
                            if (seg->isPlatformOrGantry && surfacePx.width() > 16) {
                                int gratingW = static_cast<int>(surfacePx.width() * 0.70f);
                                surfacePx = surfacePx.copy(0, 0, gratingW, surfacePx.height());
                            }
                            if (orient == 0) {
                                p.drawPixmap(cellRect.toRect(), surfacePx);
                            } else {
                                p.save();
                                p.translate(cellRect.center());
                                p.rotate(orient * 90.0);
                                p.drawPixmap(-cellRect.width() / 2.0, -cellRect.height() / 2.0,
                                             cellRect.width(), cellRect.height(), surfacePx);
                                p.restore();
                            }
                        } else {
                            p.fillRect(cellRect, QColor(50, 55, 70));
                        }
                    } else {
                        p.fillRect(cellRect, QColor(50, 55, 70));
                    }
                }
            }

            // B. Draw Platform / Gantry / Stairs detail
            if (seg->isPlatformOrGantry || seg->isStairs) {
                if (!drawFloor || !m_showFloorTextures) {
                    p.fillRect(cellRect, QColor(40, 48, 58, 220));
                    // High-tech metal grating crosshatch
                    p.setPen(QPen(QColor(80, 115, 145, 130), 1.0f));
                    float step = cellRect.width() / 4.0f;
                    for (int k = 1; k < 4; ++k) {
                        p.drawLine(cellRect.left() + k * step, cellRect.top(), cellRect.left() + k * step, cellRect.bottom());
                        p.drawLine(cellRect.left(), cellRect.top() + k * step, cellRect.right(), cellRect.top() + k * step);
                    }
                }

                if (seg->isStairs) {
                    p.save();
                    p.translate(cellRect.center());
                    p.rotate(orient * 90.0);
                    QRectF localRect(-cellRect.width() / 2.0f, -cellRect.height() / 2.0f, cellRect.width(), cellRect.height());

                    p.setPen(QPen(QColor(255, 185, 30, 220), 1.8f));
                    float stepH = cellRect.height() / 6.0f;
                    for (int s = 1; s < 6; ++s) {
                        float sy = localRect.top() + s * stepH;
                        p.drawLine(localRect.left() + 4, sy, localRect.right() - 4, sy);
                    }
                    // Direction arrow
                    p.setPen(QPen(QColor(255, 215, 0), 2.0f));
                    p.drawLine(0, localRect.height() * 0.28f, 0, -localRect.height() * 0.28f);
                    p.drawLine(-5, -localRect.height() * 0.28f + 7, 0, -localRect.height() * 0.28f);
                    p.drawLine(5, -localRect.height() * 0.28f + 7, 0, -localRect.height() * 0.28f);
                    p.restore();
                } else {
                    // Railings strictly according to engine FPSC-Game.DBA:48558-48592
                    QPen railPen(QColor(245, 185, 25), 2.5f, Qt::DashLine);
                    p.setPen(railPen);

                    if (getGantryRailingOnSide(seg->kindOf, seg->mode, orient, 0)) {
                        p.drawLine(cellRect.left(), cellRect.top(), cellRect.right(), cellRect.top()); // North
                    }
                    if (getGantryRailingOnSide(seg->kindOf, seg->mode, orient, 1)) {
                        p.drawLine(cellRect.right(), cellRect.top(), cellRect.right(), cellRect.bottom()); // East
                    }
                    if (getGantryRailingOnSide(seg->kindOf, seg->mode, orient, 2)) {
                        p.drawLine(cellRect.left(), cellRect.bottom(), cellRect.right(), cellRect.bottom()); // South
                    }
                    if (getGantryRailingOnSide(seg->kindOf, seg->mode, orient, 3)) {
                        p.drawLine(cellRect.left(), cellRect.top(), cellRect.left(), cellRect.bottom()); // West
                    }
                }

                // Platform label
                p.setPen(QColor(255, 210, 90));
                p.setFont(QFont("Segoe UI", 7, QFont::Bold));
                QString pLabel = seg->isStairs ? QStringLiteral("STAIRS") : QStringLiteral("GANTRY");
                p.drawText(cellRect, Qt::AlignCenter, pLabel);
                continue;
            }

            // C. Draw Large Scenery / Rock footprint
            if (seg->isScenery && !seg->floorTexture.isEmpty()) {
                p.setPen(QPen(QColor(180, 150, 100, 200), 1.5f));
                p.drawRect(cellRect);
                continue;
            }

            // D. Auto-Tiling Textured Wall Ribbons (Perimeter walls only)
            if (!drawWalls) continue;
            int maptile = (layer < m_map->gridTileType.size() && y < m_map->gridTileType[layer].size() && x < m_map->gridTileType[layer][y].size())
                          ? m_map->gridTileType[layer][y][x] : 0;

            for (int rotSide = 0; rotSide < 4; ++rotSide) {
                int origSide = (rotSide - rot + 4) % 4;

                if (maptile > 0) {
                    if (!isMaptileWallPresent(maptile, rot, rotSide)) {
                        continue;
                    }
                } else {
                    if (!seg->hasWall[origSide]) continue;

                    // Fallback auto-tiling neighbor check for custom placed segments without maptile
                    int nx = x;
                    int ny = y;
                    switch (rotSide) {
                        case 0: ny -= 1; break; // North (y - 1)
                        case 1: nx += 1; break; // East (x + 1)
                        case 2: ny += 1; break; // South (y + 1)
                        case 3: nx -= 1; break; // West (x - 1)
                    }

                    if (ny >= 0 && ny < rows && nx >= 0 && nx < cols) {
                        int neighborSeg = m_map->gridBlocks[layer][ny][nx];
                        if (neighborSeg == segId) {
                            int neighborRot = m_map->gridRotation[layer][ny][nx] & 3;
                            bool isPartition = (rot != neighborRot) && (rotSide == rot || rotSide == (neighborRot + 2) % 4);
                            if (!isPartition) {
                                continue;
                            }
                        } else if (neighborSeg == 0) {
                            bool insideRoomBelow = false;
                            for (int l = layer - 1; l >= 0; --l) {
                                int bSelfSeg = m_map->gridBlocks[l][y][x];
                                int bNeighSeg = m_map->gridBlocks[l][ny][nx];
                                if (bSelfSeg > 0 && bNeighSeg == bSelfSeg) {
                                    insideRoomBelow = true;
                                    break;
                                }
                            }
                            if (insideRoomBelow) {
                                continue;
                            }
                        }
                    }
                }



                QRectF wallRect;
                QLineF outerLine, innerLine;

                switch (rotSide) {
                    case 0: // North (Top edge)
                        wallRect = QRectF(cellRect.left(), cellRect.top(), cellRect.width(), wallRibbon);
                        outerLine = QLineF(cellRect.topLeft(), cellRect.topRight());
                        innerLine = QLineF(wallRect.bottomLeft(), wallRect.bottomRight());
                        break;
                    case 1: // East (Right edge)
                        wallRect = QRectF(cellRect.right() - wallRibbon, cellRect.top(), wallRibbon, cellRect.height());
                        outerLine = QLineF(cellRect.topRight(), cellRect.bottomRight());
                        innerLine = QLineF(wallRect.topLeft(), wallRect.bottomLeft());
                        break;
                    case 2: // South (Bottom edge)
                        wallRect = QRectF(cellRect.left(), cellRect.bottom() - wallRibbon, cellRect.width(), wallRibbon);
                        outerLine = QLineF(cellRect.bottomLeft(), cellRect.bottomRight());
                        innerLine = QLineF(wallRect.topLeft(), wallRect.topRight());
                        break;
                    case 3: // West (Left edge)
                        wallRect = QRectF(cellRect.left(), cellRect.top(), wallRibbon, cellRect.height());
                        outerLine = QLineF(cellRect.topLeft(), cellRect.bottomLeft());
                        innerLine = QLineF(wallRect.topRight(), wallRect.bottomRight());
                        break;
                }

                // Textured Wall Strip
                QString wallTex = seg->wallTextures[origSide];
                if (m_showWallTextures && !wallTex.isEmpty()) {
                    QPixmap wallPx = AssetManager::instance().loadTexture(wallTex);
                    if (!wallPx.isNull()) {
                        p.drawPixmap(wallRect.toRect(), wallPx);
                    } else {
                        p.fillRect(wallRect, QColor(140, 100, 70));
                    }
                } else {
                    p.fillRect(wallRect, QColor(140, 100, 70));
                }

                // Doom-Style Linedef Outlines (Outer heavy line & inner subtle line)
                p.setPen(QPen(QColor(220, 180, 110), 1.5f));
                p.drawLine(outerLine);
                p.setPen(QPen(QColor(60, 45, 30, 180), 1.0f));
                p.drawLine(innerLine);
            }

            if (isDimmedGray) {
                p.fillRect(cellRect, QColor(10, 15, 25, 120)); // Soft dark wash over inactive textures & walls
            }
        }
    }

    p.restore();
}

void MapCanvas::drawWaypoints(QPainter& p) {
    if (!m_map || m_map->waypoints.isEmpty()) return;

    p.save();
    for (int i = 0; i < m_map->waypoints.size(); ++i) {
        const AIWaypoint& wp = m_map->waypoints[i];
        int wpLayer = qBound(0, static_cast<int>(std::floor((wp.y + 25.0f) / 100.0f)), 20);
        if (wpLayer != m_currentFloor) continue;

        QPointF pScreen = worldToScreen(QPointF(wp.x, -wp.z));

        // Connect path lines
        p.setPen(QPen(QColor(52, 152, 219, 160), 2, Qt::DashLine));
        for (int targetIdx : wp.connections) {
            if (targetIdx >= 0 && targetIdx < m_map->waypoints.size()) {
                const AIWaypoint& targetWp = m_map->waypoints[targetIdx];
                QPointF tScreen = worldToScreen(QPointF(targetWp.x, -targetWp.z));
                p.drawLine(pScreen, tScreen);
            }
        }

        // Draw node
        p.setBrush(QColor(41, 128, 185));
        p.setPen(QPen(Qt::white, 1.5f));
        p.drawEllipse(pScreen, 5, 5);
    }
    p.restore();
}

void MapCanvas::drawZonesAndLights(QPainter& p) {
    if (!m_map) return;

    p.save();
    for (int i = 0; i < m_map->placedEntities.size(); ++i) {
        const PlacedEntity& ent = m_map->placedEntities[i];
        if (ent.floorLayer != m_currentFloor) continue;

        bool inZone = true;
        // Visibility Zone Isolation / Culling
        bool shouldIsolate = (m_activeVisZoneId >= 0 && m_cullInactiveVisZones) || m_highlightedPortal.isValid();
        if (shouldIsolate && m_visZoneManager) {
            inZone = isEntityInActiveOrPath(i);
            if (!inZone) {
                if (m_visZoneDimOpacity <= 0.01f) {
                    continue;
                } else {
                    p.setOpacity(m_visZoneDimOpacity);
                }
            } else {
                p.setOpacity(1.0f);
            }
        }

        QPointF entScreen = worldToScreen(QPointF(ent.x, -ent.z));

        // Draw Light Source Radiant Halo
        float lRange = ent.effectiveLightRange();
        if (m_showLights && lRange > 0) {
            float radScreen = lRange * m_zoom;
            QRadialGradient grad(entScreen, radScreen);
            QColor lColor = inZone ? ent.effectiveLightColor() : QColor(130, 140, 150);
            lColor.setAlpha(91);
            grad.setColorAt(0.0f, lColor);
            lColor.setAlpha(26);
            grad.setColorAt(0.5f, lColor);
            lColor.setAlpha(0);
            grad.setColorAt(1.0f, lColor);

            p.setBrush(grad);
            QColor ringColor = ent.effectiveLightColor();
            ringColor.setAlpha(77);
            p.setPen(QPen(ringColor, 1.0f));
            p.drawEllipse(entScreen, radScreen, radScreen);
        }

        // Draw Trigger Zone Area Bounds
        if (m_showZones && (ent.trigX1 != ent.trigX2 || ent.trigZ1 != ent.trigZ2)) {
            float z1 = qMin(-ent.trigZ1, -ent.trigZ2);
            float z2 = qMax(-ent.trigZ1, -ent.trigZ2);
            float x1 = qMin(ent.trigX1, ent.trigX2);
            float x2 = qMax(ent.trigX1, ent.trigX2);
            QPointF p1 = worldToScreen(QPointF(x1, z1));
            QPointF p2 = worldToScreen(QPointF(x2, z2));
            QRectF zoneRect(p1, p2);

            p.setBrush(QColor(104, 109, 224, 40));
            p.setPen(QPen(QColor(104, 109, 224, 200), 1.5f, Qt::DashLine));
            p.drawRect(zoneRect);
        }
    }
    p.restore();
}

void MapCanvas::drawEntities(QPainter& p) {
    if (!m_map) return;

    p.save();

    for (int i = 0; i < m_map->placedEntities.size(); ++i) {
        const PlacedEntity& ent = m_map->placedEntities[i];
        if (ent.floorLayer != m_currentFloor) continue;

        // Visibility Zone Isolation / Culling
        float entOpacity = 1.0f;
        bool shouldIsolate = (m_activeVisZoneId >= 0 && m_cullInactiveVisZones) || m_highlightedPortal.isValid();
        if (shouldIsolate && m_visZoneManager) {
            bool inZone = isEntityInActiveOrPath(i);
            if (!inZone) {
                if (m_visZoneDimOpacity <= 0.01f) {
                    continue; // Strictly hidden if user sets slider to 0%
                } else {
                    entOpacity = m_visZoneDimOpacity;
                }
            }
        }
        p.setOpacity(entOpacity);

        QPointF entScreen = worldToScreen(QPointF(ent.x, -ent.z));
        bool isSelected = (i == m_selectedEntityIndex);
        bool isHovered = (i == m_hoveredEntityIndex);
        EntityCategory cat = ent.profile ? ent.profile->category : EntityCategory::Unknown;

        float iconSize = qBound(24.0f, 36.0f * m_zoom, 64.0f);
        QRectF iconRect(entScreen.x() - iconSize / 2.0f, entScreen.y() - iconSize / 2.0f, iconSize, iconSize);

        // A. Draw Selected / Hover Glow Halo
        if (isSelected) {
            float pulse = 6.0f + 3.0f * std::sin(m_animPhase);
            QRectF glowRect = iconRect.adjusted(-pulse, -pulse, pulse, pulse);
            p.setBrush(QColor(255, 204, 0, 80));
            p.setPen(QPen(QColor(255, 215, 0), 2.0f));
            p.drawEllipse(glowRect);
        } else if (isHovered) {
            QRectF hovRect = iconRect.adjusted(-4, -4, 4, 4);
            p.setBrush(QColor(70, 130, 240, 60));
            p.setPen(QPen(QColor(100, 180, 255), 1.5f));
            p.drawEllipse(hovRect);
        }

        // Draw colored base ring for light sources
        if (cat == EntityCategory::Light || ent.lightRange > 0 || ent.effectiveLightRange() > 0) {
            QRectF lRing = iconRect.adjusted(-3, -3, 3, 3);
            QColor c = ent.effectiveLightColor();
            p.setPen(QPen(c, 2.5f));
            p.setBrush(QColor(c.red(), c.green(), c.blue(), 75));
            p.drawEllipse(lRing);
        }

        // B. Draw Entity Icon Image (.BMP / Billboard / Marker)
        QPixmap iconPx;
        if (ent.profile && !ent.profile->iconBmpPath.isEmpty()) {
            iconPx = AssetManager::instance().loadIcon(ent.profile->iconBmpPath);
        }
        if (iconPx.isNull() && !ent.texd.isEmpty()) {
            iconPx = AssetManager::instance().loadIcon(ent.texd);
        }

        if (!iconPx.isNull()) {
            p.drawPixmap(iconRect.toRect(), iconPx);
        } else {
            QColor catColor = entityCategoryColor(cat);
            p.setBrush(catColor);
            p.setPen(QPen(Qt::white, 1.5f));
            p.drawEllipse(iconRect);

            p.setPen(Qt::white);
            p.setFont(QFont("Segoe UI", 9, QFont::Bold));
            QString initial = ent.instanceName.left(1).toUpper();
            if (initial.isEmpty() && ent.profile) initial = ent.profile->name.left(1).toUpper();
            p.drawText(iconRect, Qt::AlignCenter, initial);
        }

        // C. Draw Player Start Green Direction Arrow or Orientation Pointer
        float yawRad = (ent.ry - 90.0f) * 3.14159265f / 180.0f; // Compass angle
        float arrowLen = iconSize * 0.75f;
        QPointF arrowEnd(entScreen.x() + arrowLen * std::cos(yawRad), entScreen.y() + arrowLen * std::sin(yawRad));

        if (cat == EntityCategory::PlayerStart) {
            // Vibrant Green Arrow
            p.setPen(QPen(QColor(0, 230, 64), 3.0f));
            p.drawLine(entScreen, arrowEnd);

            float headLen = 8.0f;
            float headAngle = 0.5f;
            QPointF a1(arrowEnd.x() - headLen * std::cos(yawRad - headAngle), arrowEnd.y() - headLen * std::sin(yawRad - headAngle));
            QPointF a2(arrowEnd.x() - headLen * std::cos(yawRad + headAngle), arrowEnd.y() - headLen * std::sin(yawRad + headAngle));
            QPolygonF head;
            head << arrowEnd << a1 << a2;
            p.setBrush(QColor(0, 230, 64));
            p.drawPolygon(head);
        } else {
            // Subtle direction dot / pointer
            p.setPen(QPen(QColor(255, 255, 255, 200), 2.0f));
            p.drawLine(entScreen, arrowEnd);
            p.setBrush(Qt::white);
            p.drawEllipse(arrowEnd, 2.5f, 2.5f);
        }

        // D. Draw Entity Name Tag if selected or zoom >= 1.5
        if (isSelected || m_zoom >= 1.5f) {
            p.setFont(QFont("Segoe UI", 9, QFont::Bold));
            QString label = ent.instanceName;
            if (label.isEmpty() && ent.profile) label = ent.profile->name;

            QFontMetrics fm(p.font());
            int txtW = fm.horizontalAdvance(label);
            QRectF tagRect(entScreen.x() - txtW / 2.0f - 4, iconRect.bottom() + 2, txtW + 8, fm.height() + 2);

            p.setBrush(QColor(15, 18, 25, 210));
            p.setPen(QPen(isSelected ? QColor(255, 204, 0) : QColor(80, 90, 110), 1.0f));
            p.drawRoundedRect(tagRect, 3, 3);

            p.setPen(isSelected ? QColor(255, 230, 120) : Qt::white);
            p.drawText(tagRect, Qt::AlignCenter, label);
        }
    }

    p.restore();
}

void MapCanvas::drawGizmo(QPainter& p) {
    if (!m_map || m_selectedEntityIndex < 0 || m_selectedEntityIndex >= m_map->placedEntities.size()) return;
    const PlacedEntity& ent = m_map->placedEntities[m_selectedEntityIndex];
    if (ent.floorLayer != m_currentFloor) return;

    QPointF origin = worldToScreen(QPointF(ent.x, -ent.z));
    const float axisLen = 65.0f;
    const float centerBoxSize = 9.0f;

    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);

    // 1. Center Handle (Free 2D move)
    bool centerActive = (m_activeGizmo == GizmoHandle::CenterFree || m_hoveredGizmo == GizmoHandle::CenterFree);
    QRectF centerRect(origin.x() - centerBoxSize, origin.y() - centerBoxSize, centerBoxSize * 2.0f, centerBoxSize * 2.0f);

    // Subtle 2D quadrant plane between +X and +Z
    QRectF quadRect(origin.x() + 2, origin.y() - 20, 18, 18);
    p.setPen(QPen(centerActive ? QColor(255, 235, 100, 220) : QColor(255, 200, 50, 100), 1.5f, Qt::DashLine));
    p.setBrush(centerActive ? QColor(255, 235, 100, 140) : QColor(255, 200, 50, 45));
    p.drawRect(quadRect);

    // Center handle circle/rounded rect
    p.setPen(QPen(centerActive ? Qt::white : QColor(30, 30, 30), centerActive ? 2.0f : 1.5f));
    p.setBrush(centerActive ? QColor(255, 240, 80) : QColor(255, 190, 0, 220));
    p.drawRoundedRect(centerRect, 3.0f, 3.0f);

    // Crosshair inside center handle
    p.setPen(QPen(centerActive ? QColor(40, 40, 40) : QColor(60, 40, 0), 1.5f));
    p.drawLine(origin.x() - 4, origin.y(), origin.x() + 4, origin.y());
    p.drawLine(origin.x(), origin.y() - 4, origin.x(), origin.y() + 4);

    // 2. X Axis (Red -> screen right)
    bool xActive = (m_activeGizmo == GizmoHandle::AxisX || m_hoveredGizmo == GizmoHandle::AxisX);
    QPointF xShaftStart = origin + QPointF(centerBoxSize + 1, 0);
    QPointF xShaftEnd = origin + QPointF(axisLen, 0);
    QColor xColor = xActive ? QColor(255, 90, 110) : QColor(240, 45, 60);

    // X shaft
    p.setPen(QPen(xColor, xActive ? 4.5f : 3.0f, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(xShaftStart, xShaftEnd);

    // X arrow cone
    QPolygonF xArrow;
    xArrow << origin + QPointF(axisLen + 12, 0)
           << origin + QPointF(axisLen - 2, -6)
           << origin + QPointF(axisLen, 0)
           << origin + QPointF(axisLen - 2, 6);
    p.setPen(QPen(xActive ? Qt::white : QColor(180, 20, 30), 1.0f));
    p.setBrush(xColor);
    p.drawPolygon(xArrow);

    // X label
    p.setFont(QFont("Segoe UI", 9, QFont::Bold));
    p.setPen(xActive ? Qt::white : QColor(255, 120, 130));
    p.drawText(QRectF(origin.x() + axisLen + 15, origin.y() - 8, 20, 16), Qt::AlignCenter, "X");

    // 3. Z Axis (Blue/Cyan -> screen up, representing world +Z)
    bool zActive = (m_activeGizmo == GizmoHandle::AxisZ || m_hoveredGizmo == GizmoHandle::AxisZ);
    QPointF zShaftStart = origin + QPointF(0, -(centerBoxSize + 1));
    QPointF zShaftEnd = origin + QPointF(0, -axisLen);
    QColor zColor = zActive ? QColor(80, 190, 255) : QColor(30, 135, 255);

    // Z shaft
    p.setPen(QPen(zColor, zActive ? 4.5f : 3.0f, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(zShaftStart, zShaftEnd);

    // Z arrow cone
    QPolygonF zArrow;
    zArrow << origin + QPointF(0, -axisLen - 12)
           << origin + QPointF(-6, -axisLen + 2)
           << origin + QPointF(0, -axisLen)
           << origin + QPointF(6, -axisLen + 2);
    p.setPen(QPen(zActive ? Qt::white : QColor(10, 80, 180), 1.0f));
    p.setBrush(zColor);
    p.drawPolygon(zArrow);

    // Z label
    p.setFont(QFont("Segoe UI", 9, QFont::Bold));
    p.setPen(zActive ? Qt::white : QColor(120, 200, 255));
    p.drawText(QRectF(origin.x() - 10, origin.y() - axisLen - 28, 20, 16), Qt::AlignCenter, "Z");

    p.restore();
}

MapCanvas::GizmoHandle MapCanvas::hitTestGizmo(const QPointF& screenPos) const {
    if (!m_map || m_selectedEntityIndex < 0 || m_selectedEntityIndex >= m_map->placedEntities.size())
        return GizmoHandle::None;
    const PlacedEntity& ent = m_map->placedEntities[m_selectedEntityIndex];
    if (ent.floorLayer != m_currentFloor)
        return GizmoHandle::None;

    QPointF origin = worldToScreen(QPointF(ent.x, -ent.z));
    const float axisLen = 65.0f;
    const float centerBoxSize = 9.0f;

    // 1. Center Handle & Quadrant
    QRectF centerHit(origin.x() - centerBoxSize - 3, origin.y() - centerBoxSize - 3, (centerBoxSize + 3) * 2, (centerBoxSize + 3) * 2);
    if (centerHit.contains(screenPos)) {
        return GizmoHandle::CenterFree;
    }
    QRectF quadHit(origin.x(), origin.y() - 22, 22, 22);
    if (quadHit.contains(screenPos)) {
        return GizmoHandle::CenterFree;
    }

    // 2. X Axis (Shaft + Arrow)
    QRectF xShaftHit(origin.x() + centerBoxSize, origin.y() - 10, axisLen - centerBoxSize + 25, 20);
    if (xShaftHit.contains(screenPos)) {
        return GizmoHandle::AxisX;
    }

    // 3. Z Axis (Shaft + Arrow)
    QRectF zShaftHit(origin.x() - 10, origin.y() - axisLen - 25, 20, axisLen - centerBoxSize + 25);
    if (zShaftHit.contains(screenPos)) {
        return GizmoHandle::AxisZ;
    }

    return GizmoHandle::None;
}

std::vector<MapCanvas::VisZoneBadge> MapCanvas::getVisibleZoneBadges() const {
    std::vector<VisZoneBadge> badges;
    if (!m_visZoneManager || !m_map) return badges;
    if (m_zoom < 0.15f) return badges;

    auto computeBadgeRect = [&](const std::vector<QPoint>& tiles, const QString& text, bool isActive) -> QRectF {
        if (tiles.empty()) return QRectF();

        int sumX = 0, sumY = 0;
        for (const auto& t : tiles) {
            sumX += t.x();
            sumY += t.y();
        }
        float avgX = static_cast<float>(sumX) / tiles.size();
        float avgY = static_cast<float>(sumY) / tiles.size();

        float finalX = avgX + 0.5f;
        float finalY = avgY + 0.5f;

        // Ensure the badge center point lies within one of the zone's tiles
        // (crucial for concave, L-shaped, or U-shaped rooms)
        int cTileX = static_cast<int>(std::floor(finalX));
        int cTileY = static_cast<int>(std::floor(finalY));
        bool inside = false;
        for (const auto& t : tiles) {
            if (t.x() == cTileX && t.y() == cTileY) {
                inside = true;
                break;
            }
        }
        if (!inside) {
            float bestDistSq = 1e9f;
            for (const auto& t : tiles) {
                float dx = (t.x() + 0.5f) - finalX;
                float dy = (t.y() + 0.5f) - finalY;
                float dSq = dx * dx + dy * dy;
                if (dSq < bestDistSq) {
                    bestDistSq = dSq;
                    finalX = t.x() + 0.5f;
                    finalY = t.y() + 0.5f;
                }
            }
        }

        QPointF centerScreen = worldToScreen(QPointF(finalX * TILE_SIZE, finalY * TILE_SIZE));

        QFont badgeFont("Segoe UI", 9, QFont::Bold);
        QFontMetrics fm(badgeFont);
        int tw = fm.horizontalAdvance(text) + (isActive ? 14 : 10);
        int th = fm.height() + (isActive ? 6 : 4);
        return QRectF(centerScreen.x() - tw / 2.0f, centerScreen.y() - th / 2.0f, tw, th);
    };

    if (m_activeVisZoneId >= 0) {
        std::set<int> visibleFromPortal;
        if (m_highlightedPortal.isValid()) {
            if (m_highlightedPortal.focusedVisibleZone >= 0) {
                visibleFromPortal.insert(m_highlightedPortal.focusedVisibleZone);
            } else if (m_highlightedPortal.toZone >= 0) {
                visibleFromPortal.insert(m_highlightedPortal.toZone);
            }
        }

        // 1. Inactive background zones
        for (const auto& z : m_visZoneManager->zones()) {
            if (z.id == m_activeVisZoneId) continue;
            if (visibleFromPortal.find(z.id) != visibleFromPortal.end()) continue;
            if (!z.hasFloor(m_currentFloor)) continue;

            const auto& tiles = z.getTilesOnFloor(m_currentFloor);
            if (tiles.empty()) continue;

            QString text = QString("Z%1").arg(z.id + 1);
            QRectF rect = computeBadgeRect(tiles, text, false);
            if (rect.isEmpty()) continue;

            VisZoneBadge b;
            b.zoneId = z.id;
            b.rect = rect;
            b.text = text;
            b.borderColor = QColor(90, 105, 125, 160);
            b.textColor = QColor(160, 175, 195);
            b.fillColor = QColor(15, 23, 42, 230);
            b.isActive = false;
            b.isFocused = false;
            badges.push_back(b);
        }

        // 2. Visible zone(s) from highlighted portal
        if (m_highlightedPortal.isValid()) {
            for (int vzId : visibleFromPortal) {
                if (vzId == m_activeVisZoneId) continue;
                const VisZone* vz = m_visZoneManager->getZone(vzId);
                if (!vz || !vz->hasFloor(m_currentFloor)) continue;
                const auto& vzTiles = vz->getTilesOnFloor(m_currentFloor);
                if (vzTiles.empty()) continue;

                bool isFocused = (vzId == m_highlightedPortal.focusedVisibleZone);
                bool isBreach = m_highlightedPortal.isBreach;

                QString text;
                if (isBreach) {
                    text = isFocused ? QString("Z%1 [🚨 УТЕЧКА - ВЫБРАНА]").arg(vz->id + 1)
                                     : QString("Z%1 [🚨 Утечка]").arg(vz->id + 1);
                } else {
                    text = isFocused ? QString("Z%1 [👁 ВЫБРАНА]").arg(vz->id + 1)
                                     : QString("Z%1 [Видно]").arg(vz->id + 1);
                }

                QRectF rect = computeBadgeRect(vzTiles, text, false);
                if (rect.isEmpty()) continue;

                VisZoneBadge b;
                b.zoneId = vz->id;
                b.rect = rect;
                b.text = text;
                b.borderColor = isBreach ? QColor(239, 68, 68) : QColor(56, 189, 248);
                b.textColor = isBreach ? QColor(254, 202, 202) : QColor(186, 230, 253);
                b.fillColor = QColor(15, 23, 42, 240);
                b.isActive = false;
                b.isFocused = isFocused;
                badges.push_back(b);
            }
        }

        // 3. Active zone (illumined)
        const VisZone* curZone = m_visZoneManager->getZone(m_activeVisZoneId);
        if (curZone && curZone->hasFloor(m_currentFloor)) {
            const auto& tiles = curZone->getTilesOnFloor(m_currentFloor);
            if (!tiles.empty()) {
                QString text = QString("Z%1 [Активная]").arg(curZone->id + 1);
                QRectF rect = computeBadgeRect(tiles, text, true);
                if (!rect.isEmpty()) {
                    VisZoneBadge b;
                    b.zoneId = curZone->id;
                    b.rect = rect;
                    b.text = text;
                    b.borderColor = curZone->color;
                    b.textColor = Qt::white;
                    b.fillColor = QColor(15, 23, 42, 245);
                    b.isActive = true;
                    b.isFocused = false;
                    badges.push_back(b);
                }
            }
        }
    } else if (m_colorAllVisZones) {
        for (const auto& zone : m_visZoneManager->zones()) {
            if (!zone.hasFloor(m_currentFloor)) continue;
            const auto& tiles = zone.getTilesOnFloor(m_currentFloor);
            if (tiles.empty()) continue;

            QString text = QString("Z%1").arg(zone.id + 1);
            QRectF rect = computeBadgeRect(tiles, text, false);
            if (rect.isEmpty()) continue;

            VisZoneBadge b;
            b.zoneId = zone.id;
            b.rect = rect;
            b.text = text;
            b.borderColor = zone.color;
            b.textColor = Qt::white;
            b.fillColor = QColor(18, 22, 30, 230);
            b.isActive = false;
            b.isFocused = false;
            badges.push_back(b);
        }
    }

    return badges;
}

void MapCanvas::drawZoneBadges(QPainter& p) {
    auto badges = getVisibleZoneBadges();
    if (badges.empty()) return;

    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);

    QFont badgeFont("Segoe UI", 9, QFont::Bold);
    p.setFont(badgeFont);

    for (const auto& badge : badges) {
        bool isHovered = (badge.zoneId == m_hoveredZoneBadgeId);

        // 1. Drop shadow for distinct visual separation above entities and floor textures
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 180));
        p.drawRoundedRect(badge.rect.translated(1, 2), 4, 4);

        // 2. Hover glow
        if (isHovered) {
            QRectF glowRect = badge.rect.adjusted(-2, -2, 2, 2);
            p.setBrush(QColor(255, 255, 255, 35));
            p.setPen(QPen(QColor(255, 255, 255, 200), 1.8f));
            p.drawRoundedRect(glowRect, 5, 5);
        }

        // 3. Badge body & border
        p.setBrush(badge.fillColor);
        float penWidth = badge.isActive ? 2.2f : (badge.isFocused ? 2.0f : 1.5f);
        QColor borderCol = isHovered ? Qt::white : badge.borderColor;
        p.setPen(QPen(borderCol, penWidth));
        p.drawRoundedRect(badge.rect, 4, 4);

        // 4. Badge text
        p.setPen(isHovered ? Qt::white : badge.textColor);
        p.drawText(badge.rect, Qt::AlignCenter, badge.text);
    }

    p.restore();
}

void MapCanvas::drawHUD(QPainter& p) {
    p.save();
    p.setFont(QFont("Segoe UI", 10, QFont::Bold));

    // Floor Badge (Top-Left)
    QString floorStr = QString("Floor %1 / %2 (Height Y: %3)")
        .arg(m_currentFloor)
        .arg(m_map->header.layerMax)
        .arg(m_currentFloor * 100);

    QFontMetrics fm(p.font());
    int w = fm.horizontalAdvance(floorStr) + 20;
    QRect badgeRect(14, 14, w, 32);

    p.setBrush(QColor(25, 30, 42, 220));
    p.setPen(QPen(QColor(70, 80, 105), 1.5f));
    p.drawRoundedRect(badgeRect, 6, 6);

    p.setPen(QColor(100, 200, 255));
    p.drawText(badgeRect, Qt::AlignCenter, floorStr);

    // Zoom Indicator (Bottom-Right)
    QString zoomStr = QString("%1%").arg(qRound(m_zoom * 100));
    int zw = fm.horizontalAdvance(zoomStr) + 16;
    QRect zoomRect(width() - zw - 14, height() - 38, zw, 26);
    p.setBrush(QColor(25, 30, 42, 200));
    p.setPen(QPen(QColor(70, 80, 105), 1.0f));
    p.drawRoundedRect(zoomRect, 4, 4);
    p.setPen(QColor(180, 190, 210));
    p.drawText(zoomRect, Qt::AlignCenter, zoomStr);

    p.restore();
}

void MapCanvas::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::RightButton || event->button() == Qt::MiddleButton) {
        m_isPanning = true;
        m_lastMousePos = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton) {
        if (!m_map) return;

        // 0. Topmost Priority: VisZone Badge Click (On top of entities, CSG, and gizmos)
        // Check visible zone badges first so clicking on a zone label selects the zone
        // even if an entity, waypoint, or gizmo is located underneath it.
        auto badges = getVisibleZoneBadges();
        for (auto it = badges.rbegin(); it != badges.rend(); ++it) {
            QRectF hitRect = it->rect.adjusted(-4, -4, 4, 4);
            if (hitRect.contains(event->pos())) {
                int zid = it->zoneId;
                selectEntity(-1);
                setActiveVisZone(zid);
                emit visZoneSelected(zid);
                event->accept();
                update();
                return;
            }
        }

        // 1. First check if clicking on the Gizmo of selected entity
        if (m_selectedEntityIndex >= 0 && m_selectedEntityIndex < m_map->placedEntities.size()) {
            GizmoHandle hitG = hitTestGizmo(event->pos());
            if (hitG != GizmoHandle::None) {
                m_activeGizmo = hitG;
                m_dragStartMousePos = screenToWorld(event->pos());
                const PlacedEntity& ent = m_map->placedEntities[m_selectedEntityIndex];
                m_dragStartEntX = ent.x;
                m_dragStartEntZ = ent.z;
                event->accept();
                update();
                return;
            }
        }

        // 1b. Check if clicking on a portal line to navigate across zones
        if (m_visZoneManager && (m_showPortals || m_activeVisZoneId >= 0)) {
            for (const auto& portal : m_visZoneManager->portals()) {
                if (portal.floor != m_currentFloor) continue;
                QPointF p1 = worldToScreen(portal.lineWorld.p1());
                QPointF p2 = worldToScreen(portal.lineWorld.p2());
                float segLen = std::hypot(p2.x() - p1.x(), p2.y() - p1.y());
                if (segLen > 0.1f) {
                    QPointF mPos = event->pos();
                    float u = ((mPos.x() - p1.x()) * (p2.x() - p1.x()) + (mPos.y() - p1.y()) * (p2.y() - p1.y())) / (segLen * segLen);
                    if (u >= -0.05f && u <= 1.05f) {
                        QPointF proj = p1 + qBound(0.0f, u, 1.0f) * (p2 - p1);
                        float dist = std::hypot(mPos.x() - proj.x(), mPos.y() - proj.y());
                        if (dist < 10.0f) {
                            int targetZone = (portal.zoneA == m_activeVisZoneId) ? portal.zoneB : portal.zoneA;
                            if (targetZone >= 0) {
                                setActiveVisZone(targetZone);
                                emit visZoneSelected(targetZone);
                                event->accept();
                                return;
                            }
                        }
                    }
                }
            }
        }

        // 2. Otherwise perform entity selection
        QPointF worldPos = screenToWorld(event->pos());
        int clickedEntity = -1;
        float bestDist = 24.0f / m_zoom; // Hit tolerance in world units

        for (int i = 0; i < m_map->placedEntities.size(); ++i) {
            const PlacedEntity& ent = m_map->placedEntities[i];
            if (ent.floorLayer != m_currentFloor) continue;

            float dx = ent.x - worldPos.x();
            float dz = (-ent.z) - worldPos.y();
            float dist = std::sqrt(dx * dx + dz * dz);
            if (dist < bestDist) {
                bestDist = dist;
                clickedEntity = i;
            }
        }

        if (clickedEntity >= 0) {
            selectEntity(clickedEntity);
        } else {
            selectEntity(-1);
            int tileX = static_cast<int>(std::floor(worldPos.x() / TILE_SIZE));
            int tileY = static_cast<int>(std::floor(worldPos.y() / TILE_SIZE));
            int zId = m_visZoneManager ? m_visZoneManager->getZoneAt(m_currentFloor, tileX, tileY) : -1;
            if (zId >= 0) {
                if (zId != m_activeVisZoneId) {
                    setActiveVisZone(zId);
                    emit visZoneSelected(zId);
                }
            } else {
                // Clicked into empty space outside all zones -> reset zone selection
                if (m_activeVisZoneId >= 0 || m_cullInactiveVisZones) {
                    setActiveVisZone(-1);
                    setVisZoneCulling(false, 0.0f);
                    clearTracePath();
                    clearHighlight();
                    emit visZoneSelected(-1);
                }
            }
        }
        event->accept();
    }
}

void MapCanvas::mouseMoveEvent(QMouseEvent* event) {
    if (m_isPanning) {
        QPoint delta = event->pos() - m_lastMousePos;
        m_lastMousePos = event->pos();
        m_panOffset += delta;
        update();
        event->accept();
        return;
    }

    if (!m_map) return;

    // 1. Handle Active Gizmo Dragging
    if (m_activeGizmo != GizmoHandle::None && m_selectedEntityIndex >= 0 && m_selectedEntityIndex < m_map->placedEntities.size()) {
        QPointF currentWorldMouse = screenToWorld(event->pos());
        QPointF delta = currentWorldMouse - m_dragStartMousePos;

        PlacedEntity& ent = m_map->placedEntities[m_selectedEntityIndex];
        if (m_activeGizmo == GizmoHandle::AxisX) {
            ent.x = m_dragStartEntX + delta.x();
        } else if (m_activeGizmo == GizmoHandle::AxisZ) {
            ent.z = m_dragStartEntZ - delta.y(); // screen -Y corresponds to world +Z
        } else if (m_activeGizmo == GizmoHandle::CenterFree) {
            ent.x = m_dragStartEntX + delta.x();
            ent.z = m_dragStartEntZ - delta.y();
        }

        m_map->isModified = true;
        emit entityModified(m_selectedEntityIndex);
        update();
        event->accept();
        return;
    }

    bool needUpdate = false;

    // 0. Check zone badge hover
    int hoveredBadgeId = -1;
    if (m_activeGizmo == GizmoHandle::None) {
        auto badges = getVisibleZoneBadges();
        for (auto it = badges.rbegin(); it != badges.rend(); ++it) {
            QRectF hitRect = it->rect.adjusted(-3, -3, 3, 3);
            if (hitRect.contains(event->pos())) {
                hoveredBadgeId = it->zoneId;
                break;
            }
        }
    }

    if (m_hoveredZoneBadgeId != hoveredBadgeId) {
        m_hoveredZoneBadgeId = hoveredBadgeId;
        needUpdate = true;
    }

    // 2. Handle Cursor: Zone Badge Hover -> PointingHandCursor, Gizmo Hover -> Transform Cursor
    if (m_hoveredZoneBadgeId >= 0 && m_activeGizmo == GizmoHandle::None) {
        setCursor(Qt::PointingHandCursor);
    } else {
        GizmoHandle hitG = (m_selectedEntityIndex >= 0) ? hitTestGizmo(event->pos()) : GizmoHandle::None;
        if (hitG != m_hoveredGizmo) {
            m_hoveredGizmo = hitG;
            if (m_hoveredGizmo == GizmoHandle::CenterFree) {
                setCursor(Qt::SizeAllCursor);
            } else if (m_hoveredGizmo == GizmoHandle::AxisX) {
                setCursor(Qt::SizeHorCursor);
            } else if (m_hoveredGizmo == GizmoHandle::AxisZ) {
                setCursor(Qt::SizeVerCursor);
            } else {
                setCursor(Qt::ArrowCursor);
            }
            update();
        } else if (m_hoveredGizmo == GizmoHandle::None) {
            setCursor(Qt::ArrowCursor);
        }
    }

    QPointF worldPos = screenToWorld(event->pos());
    int tileX = static_cast<int>(std::floor(worldPos.x() / TILE_SIZE));
    int tileY = static_cast<int>(std::floor(worldPos.y() / TILE_SIZE));

    QPoint newTile(tileX, tileY);
    if (m_hoveredTile != newTile) {
        m_hoveredTile = newTile;
        needUpdate = true;
    }

    int hoveredEntity = -1;
    float bestDist = 20.0f / m_zoom;

    for (int i = 0; i < m_map->placedEntities.size(); ++i) {
        const PlacedEntity& ent = m_map->placedEntities[i];
        if (ent.floorLayer != m_currentFloor) continue;

        float dx = ent.x - worldPos.x();
        float dz = (-ent.z) - worldPos.y();
        float dist = std::sqrt(dx * dx + dz * dz);
        if (dist < bestDist) {
            bestDist = dist;
            hoveredEntity = i;
        }
    }

    if (m_hoveredEntityIndex != hoveredEntity) {
        m_hoveredEntityIndex = hoveredEntity;
        needUpdate = true;
    }

    if (needUpdate) {
        update();
    }

    // Emit hover info string for status bar
    QString info = QString("Tile: (%1, %2) | World: (X: %3, Z: %4)")
        .arg(tileX).arg(tileY)
        .arg(worldPos.x(), 0, 'f', 1)
        .arg(-worldPos.y(), 0, 'f', 1);

    if (m_hoveredZoneBadgeId >= 0) {
        info += QString(" | VisZone %1 (Click to select)").arg(m_hoveredZoneBadgeId + 1);
    } else if (m_hoveredEntityIndex >= 0) {
        const PlacedEntity& ent = m_map->placedEntities[m_hoveredEntityIndex];
        info += QString(" | Entity: %1 [%2]")
            .arg(ent.instanceName.isEmpty() ? (ent.profile ? ent.profile->name : "Object") : ent.instanceName)
            .arg(entityCategoryToString(ent.profile ? ent.profile->category : EntityCategory::Unknown));
    } else if (tileX >= 0 && tileX <= m_map->header.maxX && tileY >= 0 && tileY <= m_map->header.maxY) {
        int segId = m_map->gridBlocks[m_currentFloor][tileY][tileX];
        if (segId > 0 && m_map->segments.contains(segId)) {
            info += QString(" | Segment: %1").arg(m_map->segments[segId]->name);
        }
    }

    emit hoverInfoChanged(info);
}

void MapCanvas::leaveEvent(QEvent* event) {
    QWidget::leaveEvent(event);
    m_hoveredTile = QPoint(-1, -1);
    m_hoveredEntityIndex = -1;
    m_hoveredGizmo = GizmoHandle::None;
    m_hoveredZoneBadgeId = -1;
    setCursor(Qt::ArrowCursor);
    update();
}

void MapCanvas::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::RightButton) {
        m_isPanning = false;
        setCursor(Qt::ArrowCursor);

        // Check if right clicked on an entity to show context menu
        if ((event->pos() - m_lastMousePos).manhattanLength() < 6 && m_map) {
            // 0. Check if right clicked on a zone badge first!
            auto badges = getVisibleZoneBadges();
            for (auto it = badges.rbegin(); it != badges.rend(); ++it) {
                QRectF hitRect = it->rect.adjusted(-4, -4, 4, 4);
                if (hitRect.contains(event->pos())) {
                    int zid = it->zoneId;
                    selectEntity(-1);
                    setActiveVisZone(zid);
                    emit visZoneSelected(zid);
                    QMenu menu(this);
                    QAction* titleAct = menu.addAction(tr("Visibility Zone %1").arg(zid + 1));
                    titleAct->setEnabled(false);
                    menu.addSeparator();
                    QAction* actTrace = menu.addAction(tr("👁️ Trace Visibility from Zone %1 (Ctrl+T)").arg(zid + 1));
                    QAction* actDichotomy = menu.addAction(tr("✂️ Dichotomy Tool: Delete Room (Zone %1)").arg(zid + 1));
                    QAction* chosen = menu.exec(mapToGlobal(event->pos()));
                    if (chosen == actTrace) {
                        emit traceVisibilityRequested(zid);
                    } else if (chosen == actDichotomy) {
                        emit dichotomyDeleteZoneRequested(zid);
                    }
                    event->accept();
                    return;
                }
            }
            QPointF worldPos = screenToWorld(event->pos());
            int clickedEntity = -1;
            float bestDist = 24.0f / m_zoom;

            for (int i = 0; i < m_map->placedEntities.size(); ++i) {
                const PlacedEntity& ent = m_map->placedEntities[i];
                if (ent.floorLayer != m_currentFloor) continue;

                float dx = ent.x - worldPos.x();
                float dz = (-ent.z) - worldPos.y();
                float dist = std::sqrt(dx * dx + dz * dz);
                if (dist < bestDist) {
                    bestDist = dist;
                    clickedEntity = i;
                }
            }

            if (clickedEntity >= 0) {
                selectEntity(clickedEntity);
                QMenu menu(this);
                const PlacedEntity& ent = m_map->placedEntities[clickedEntity];
                QString name = ent.instanceName.isEmpty() ? (ent.profile ? ent.profile->name : QString("Entity #%1").arg(clickedEntity)) : ent.instanceName;
                QAction* titleAct = menu.addAction(QString("Entity #%1: %2").arg(clickedEntity).arg(name));
                titleAct->setEnabled(false);
                menu.addSeparator();
                QAction* actInspect = menu.addAction(QStringLiteral("Inspect Properties"));
                QAction* actFocus = menu.addAction(QStringLiteral("Focus View"));
                menu.addSeparator();
                QAction* actDelete = menu.addAction(QStringLiteral("🗑️ Delete Entity (Del)"));

                QAction* chosen = menu.exec(mapToGlobal(event->pos()));
                if (chosen == actInspect) {
                    emit entitySelected(clickedEntity);
                } else if (chosen == actFocus) {
                    focusOnEntity(clickedEntity);
                } else if (chosen == actDelete) {
                    emit entityDeleteRequested(clickedEntity);
                }
                event->accept();
                return;
            }

            // Check if right clicked on a segment tile or inside a visibility zone
            int tileX = static_cast<int>(std::floor(worldPos.x() / TILE_SIZE));
            int tileY = static_cast<int>(std::floor(worldPos.y() / TILE_SIZE));
            if (tileX >= 0 && tileX <= m_map->header.maxX && tileY >= 0 && tileY <= m_map->header.maxY) {
                int segId = (m_currentFloor < m_map->gridBlocks.size() &&
                             tileY < m_map->gridBlocks[m_currentFloor].size() &&
                             tileX < m_map->gridBlocks[m_currentFloor][tileY].size())
                            ? m_map->gridBlocks[m_currentFloor][tileY][tileX] : 0;
                int zoneAtTile = m_visZoneManager ? m_visZoneManager->getZoneAt(m_currentFloor, tileX, tileY) : -1;
                if (segId > 0 || zoneAtTile >= 0) {
                    QMenu menu(this);
                    QString segName = (segId > 0 && m_map->segments.contains(segId)) ? m_map->segments[segId]->name : tr("Empty Tile");
                    QAction* titleAct = menu.addAction(tr("Tile (%1, %2): %3").arg(tileX).arg(tileY).arg(segName));
                    titleAct->setEnabled(false);
                    menu.addSeparator();
                    QAction* actInspectSeg = nullptr;
                    if (segId > 0) {
                        actInspectSeg = menu.addAction(tr("🧱 Inspect & Edit Segment..."));
                    }
                    QAction* actTraceVis = nullptr;
                    if (zoneAtTile >= 0) {
                        actTraceVis = menu.addAction(tr("👁️ Trace Visibility from Zone %1 (Ctrl+T)...").arg(zoneAtTile + 1));
                    }
                    QAction* actDichotomy = nullptr;
                    if (zoneAtTile >= 0) {
                        actDichotomy = menu.addAction(tr("✂️ Dichotomy Tool: Delete Room (Zone %1)").arg(zoneAtTile + 1));
                    }
                    QAction* chosen = menu.exec(mapToGlobal(event->pos()));
                    if (actInspectSeg && chosen == actInspectSeg) {
                        emit segmentInspectRequested(m_currentFloor, tileX, tileY);
                    } else if (actTraceVis && chosen == actTraceVis) {
                        emit traceVisibilityRequested(zoneAtTile);
                    } else if (actDichotomy && chosen == actDichotomy) {
                        emit dichotomyDeleteZoneRequested(zoneAtTile);
                    }
                    event->accept();
                    return;
                }
            }
        }
        event->accept();
        return;
    }

    if (event->button() == Qt::MiddleButton) {
        m_isPanning = false;
        setCursor(Qt::ArrowCursor);
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton && m_activeGizmo != GizmoHandle::None) {
        m_activeGizmo = GizmoHandle::None;
        setCursor(Qt::ArrowCursor);
        update();
        event->accept();
        return;
    }
}

void MapCanvas::wheelEvent(QWheelEvent* event) {
    // Ctrl + Wheel or Shift + Wheel -> Switch Floors
    if (event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier)) {
        if (event->angleDelta().y() > 0) {
            floorUp();
        } else if (event->angleDelta().y() < 0) {
            floorDown();
        }
        event->accept();
        return;
    }

    // Normal Wheel -> Zoom at mouse cursor
    float oldZoom = m_zoom;
    if (event->angleDelta().y() > 0) {
        m_zoom = qMin(m_zoom * 1.15f, 10.0f);
    } else {
        m_zoom = qMax(m_zoom / 1.15f, 0.1f);
    }

    QPointF mouseScreen = event->pos();
    QPointF mouseWorld = (mouseScreen - m_panOffset) / oldZoom;
    m_panOffset = mouseScreen - mouseWorld * m_zoom;

    emit zoomChanged(m_zoom);
    update();
    event->accept();
}

void MapCanvas::keyPressEvent(QKeyEvent* event) {
    switch (event->key()) {
        case Qt::Key_Escape:
            clearHighlight();
            if (m_activeVisZoneId >= 0 || m_cullInactiveVisZones) {
                setActiveVisZone(-1);
                setVisZoneCulling(false, 0.0f);
                emit visZoneSelected(-1);
            }
            if (m_selectedEntityIndex >= 0) {
                selectEntity(-1);
            }
            break;
        case Qt::Key_Delete:
        case Qt::Key_Backspace:
            if (event->modifiers() & Qt::ShiftModifier) {
                if (m_activeVisZoneId >= 0) {
                    emit dichotomyDeleteZoneRequested(m_activeVisZoneId);
                    event->accept();
                    return;
                }
            } else if (m_selectedEntityIndex >= 0) {
                emit entityDeleteRequested(m_selectedEntityIndex);
                event->accept();
                return;
            } else if (m_activeVisZoneId >= 0) {
                emit dichotomyDeleteZoneRequested(m_activeVisZoneId);
                event->accept();
                return;
            }
            break;
        case Qt::Key_T:
        case Qt::Key_V:
            if (!(event->modifiers() & (Qt::ControlModifier | Qt::AltModifier))) {
                int targetZone = m_activeVisZoneId;
                if (targetZone < 0 && m_visZoneManager) {
                    targetZone = m_visZoneManager->getZoneAt(m_currentFloor, m_hoveredTile.x(), m_hoveredTile.y());
                }
                emit traceVisibilityRequested(targetZone);
                event->accept();
                return;
            }
            break;
        case Qt::Key_PageUp:
        case Qt::Key_Plus:
        case Qt::Key_Equal:
            floorUp();
            break;
        case Qt::Key_PageDown:
        case Qt::Key_Minus:
        case Qt::Key_Underscore:
            floorDown();
            break;
        case Qt::Key_BracketRight:
            zoomIn();
            break;
        case Qt::Key_BracketLeft:
            zoomOut();
            break;
        case Qt::Key_0:
            zoomReset();
            break;
        case Qt::Key_Home:
            zoomFit();
            break;
        case Qt::Key_Left:
        case Qt::Key_A:
            m_panOffset.rx() += 50.0f;
            update();
            break;
        case Qt::Key_Right:
        case Qt::Key_D:
            m_panOffset.rx() -= 50.0f;
            update();
            break;
        case Qt::Key_Up:
        case Qt::Key_W:
            m_panOffset.ry() += 50.0f;
            update();
            break;
        case Qt::Key_Down:
        case Qt::Key_S:
            m_panOffset.ry() -= 50.0f;
            update();
            break;
        default:
            QWidget::keyPressEvent(event);
    }
}

void MapCanvas::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
}

void MapCanvas::setShowPortals(bool show) {
    m_showPortals = show;
    update();
}

void MapCanvas::setPortals(const std::vector<DBUPortal>& portals, const std::vector<DBUVisZone>& zones) {
    m_portals = portals;
    m_zones = zones;
    update();
}

void MapCanvas::setActiveVisZone(int zoneId) {
    if (m_activeVisZoneId != zoneId) {
        m_activeVisZoneId = zoneId;
        m_highlightedPortal.clear();
        if (zoneId >= 0) {
            m_cullInactiveVisZones = true;
        } else {
            m_cullInactiveVisZones = false;
        }
        if (m_visZoneManager && zoneId >= 0) {
            const VisZone* z = m_visZoneManager->getZone(zoneId);
            if (z && !z->hasFloor(m_currentFloor)) {
                setFloor(z->floor);
            }
        }
        update();
    }
}

void MapCanvas::setVisZoneCulling(bool enable, float dimOpacity) {
    m_cullInactiveVisZones = enable;
    m_visZoneDimOpacity = qBound(0.0f, dimOpacity, 1.0f);
    update();
}

void MapCanvas::setColorAllVisZones(bool enable) {
    if (m_colorAllVisZones != enable) {
        m_colorAllVisZones = enable;
        update();
    }
}

void MapCanvas::setVisZoneManager(std::shared_ptr<VisZoneManager> mgr) {
    m_visZoneManager = mgr;
    if (m_visZoneManager && m_map) {
        m_visZoneManager->buildFromMap(m_map);
    }
    update();
}

void MapCanvas::drawPortals(QPainter& p) {
    if (!m_map) return;

    p.save();

    // 0. Render VisZone Contours & Tile Tints
    if (m_visZoneManager) {
        if (m_activeVisZoneId >= 0) {
            auto drawZonePerimeter = [&](const std::vector<QPoint>& tiles, const QColor& fillColor, const QPen& contourPen, const QString& badgeText, const QColor& badgeBorder, const QColor& badgeTextColor) {
                if (tiles.empty()) return;
                p.save();
                p.setRenderHint(QPainter::Antialiasing, true);

                std::set<std::pair<int, int>> tileSet;
                for (const auto& t : tiles) {
                    tileSet.insert({t.x(), t.y()});
                    QRectF cr = getCellRectScreen(t.x(), t.y());
                    p.fillRect(cr, fillColor);
                }

                p.setPen(contourPen);
                for (const auto& t : tiles) {
                    int tx = t.x();
                    int ty = t.y();
                    QRectF cr = getCellRectScreen(tx, ty);

                    if (tileSet.find({tx, ty - 1}) == tileSet.end()) {
                        p.drawLine(cr.topLeft(), cr.topRight());
                    }
                    if (tileSet.find({tx, ty + 1}) == tileSet.end()) {
                        p.drawLine(cr.bottomLeft(), cr.bottomRight());
                    }
                    if (tileSet.find({tx - 1, ty}) == tileSet.end()) {
                        p.drawLine(cr.topLeft(), cr.bottomLeft());
                    }
                    if (tileSet.find({tx + 1, ty}) == tileSet.end()) {
                        p.drawLine(cr.topRight(), cr.bottomRight());
                    }
                }

                p.restore();
            };

            std::set<int> visibleFromPortal;
            if (m_highlightedPortal.isValid()) {
                if (m_highlightedPortal.focusedVisibleZone >= 0) {
                    visibleFromPortal.insert(m_highlightedPortal.focusedVisibleZone);
                } else if (m_highlightedPortal.toZone >= 0) {
                    visibleFromPortal.insert(m_highlightedPortal.toZone);
                }
            }

            // 1. Draw inactive surrounding zones in neutral gray (background)
            for (const auto& z : m_visZoneManager->zones()) {
                if (z.id == m_activeVisZoneId) continue;
                if (visibleFromPortal.find(z.id) != visibleFromPortal.end()) continue;
                if (!z.hasFloor(m_currentFloor)) continue;

                const auto& tiles = z.getTilesOnFloor(m_currentFloor);
                if (tiles.empty()) continue;

                QColor grayFill(70, 85, 105, 40);
                QPen grayContour(QColor(130, 145, 165, 180), 1.5f, Qt::SolidLine, Qt::SquareCap);
                QString badgeText = QString("Z%1").arg(z.id + 1);
                drawZonePerimeter(tiles, grayFill, grayContour, badgeText, QColor(90, 105, 125, 160), QColor(160, 175, 195));
            }

            // 2. Draw visible zone(s) from highlighted portal with cyan contour
            if (m_highlightedPortal.isValid()) {
                for (int vzId : visibleFromPortal) {
                    if (vzId == m_activeVisZoneId) continue;
                    const VisZone* vz = m_visZoneManager->getZone(vzId);
                    if (!vz || !vz->hasFloor(m_currentFloor)) continue;
                    const auto& vzTiles = vz->getTilesOnFloor(m_currentFloor);
                    if (vzTiles.empty()) continue;

                    bool isFocused = (vzId == m_highlightedPortal.focusedVisibleZone);
                    bool isBreach = m_highlightedPortal.isBreach;

                    QColor cyanFill;
                    if (isBreach) {
                        cyanFill = isFocused ? QColor(239, 68, 68, 70) : QColor(239, 68, 68, 35);
                    } else {
                        cyanFill = isFocused ? QColor(56, 189, 248, 70) : QColor(56, 189, 248, 35);
                    }

                    float contourWidth = isFocused ? 4.5f : 2.5f;
                    QPen cyanContour(isBreach ? QColor(239, 68, 68) : QColor(56, 189, 248),
                                     contourWidth,
                                     isFocused ? Qt::SolidLine : Qt::DashLine,
                                     Qt::SquareCap);

                    QString badgeText;
                    if (isBreach) {
                        badgeText = isFocused ? QString("Z%1 [🚨 УТЕЧКА - ВЫБРАНА]").arg(vz->id + 1)
                                              : QString("Z%1 [🚨 Утечка]").arg(vz->id + 1);
                    } else {
                        badgeText = isFocused ? QString("Z%1 [👁 ВЫБРАНА]").arg(vz->id + 1)
                                              : QString("Z%1 [Видно]").arg(vz->id + 1);
                    }
                    QColor badgeBorder = isBreach ? QColor(239, 68, 68) : QColor(56, 189, 248);
                    QColor badgeTextCol = isBreach ? QColor(254, 202, 202) : QColor(186, 230, 253);

                    drawZonePerimeter(vzTiles, cyanFill, cyanContour, badgeText, badgeBorder, badgeTextCol);
                }
            }

            // 3. Draw active zone fully illuminated
            const VisZone* curZone = m_visZoneManager->getZone(m_activeVisZoneId);
            if (curZone && curZone->hasFloor(m_currentFloor)) {
                const auto& tiles = curZone->getTilesOnFloor(m_currentFloor);
                if (!tiles.empty()) {
                    QColor activeFill = curZone->color;
                    activeFill.setAlpha(85);
                    QPen activeContour(curZone->color, 3.5f, Qt::SolidLine, Qt::SquareCap);
                    QString badgeText = QString("Z%1 [Активная]").arg(curZone->id + 1);
                    drawZonePerimeter(tiles, activeFill, activeContour, badgeText, curZone->color, Qt::white);
                }
            }
        } else if (m_colorAllVisZones) {
            for (const auto& zone : m_visZoneManager->zones()) {
                if (!zone.hasFloor(m_currentFloor)) continue;
                const auto& tiles = zone.getTilesOnFloor(m_currentFloor);
                if (tiles.empty()) continue;

                p.save();
                QColor zColor = zone.color;
                zColor.setAlpha(40);
                p.setBrush(zColor);
                p.setPen(QPen(zone.color, 1.2f, Qt::SolidLine));

                for (const auto& tile : tiles) {
                    QRectF cr = getCellRectScreen(tile.x(), tile.y());
                    p.drawRect(cr);
                }

                p.restore();
            }
        }
    }

    // 0b. Render Topo Portals from VisZoneManager
    if (m_visZoneManager && (m_showPortals || m_activeVisZoneId >= 0)) {
        for (const auto& portal : m_visZoneManager->portals()) {
            if (portal.floor != m_currentFloor) continue;

            QPointF p1 = worldToScreen(portal.lineWorld.p1());
            QPointF p2 = worldToScreen(portal.lineWorld.p2());

            bool isConnectedToActive = (m_activeVisZoneId >= 0) &&
                                       (portal.zoneA == m_activeVisZoneId || portal.zoneB == m_activeVisZoneId);

            if (m_activeVisZoneId >= 0 && m_cullInactiveVisZones) {
                if (!isConnectedToActive) {
                    if (m_visZoneDimOpacity <= 0.001f) continue;
                    p.setOpacity(m_visZoneDimOpacity);
                } else {
                    p.setOpacity(1.0f);
                }
            } else {
                p.setOpacity(1.0f);
            }

            QColor pColor;
            Qt::PenStyle pStyle = Qt::SolidLine;
            if (portal.isExterior) {
                pStyle = Qt::DashLine;
                pColor = (portal.type == PortalType::ExteriorWindow) ? QColor(0, 220, 255, 240) : QColor(255, 175, 40, 240);
            } else if (portal.type == PortalType::InterZoneWindow) {
                pColor = isConnectedToActive ? QColor(0, 255, 230, 240) : QColor(0, 200, 255, 200);
            } else {
                pColor = isConnectedToActive ? QColor(0, 255, 180, 240) : QColor(0, 185, 255, 180);
            }
            if (m_activeVisZoneId >= 0 && !isConnectedToActive) {
                pColor = QColor(130, 145, 160, 140);
            }
            float pWidth = isConnectedToActive ? 4.5f : 1.8f;

            p.setPen(QPen(pColor, pWidth, pStyle, Qt::RoundCap));
            p.drawLine(p1, p2);

            // Perpendicular ticks
            QPointF dir = (p2 - p1);
            float len = std::hypot(dir.x(), dir.y());
            if (len > 0.1f) {
                QPointF perp(-dir.y() / len * 6.0f, dir.x() / len * 6.0f);
                p.drawLine(p1 - perp, p1 + perp);
                p.drawLine(p2 - perp, p2 + perp);
            }

            // Portal Label
            QPointF centerScreen = (p1 + p2) * 0.5f;
            p.setFont(QFont("Segoe UI", 8, QFont::Bold));
            p.setPen(pColor);
            if (portal.isExterior) {
                QString extName = (portal.type == PortalType::ExteriorWindow) ? QString("Ext Window (Z%1 -> Sky)").arg(portal.zoneA + 1) : QString("Ext Door (Z%1 -> Out)").arg(portal.zoneA + 1);
                p.drawText(centerScreen + QPointF(6, -6), extName);
            } else if (m_activeVisZoneId >= 0) {
                int otherZone = (portal.zoneA == m_activeVisZoneId) ? portal.zoneB : portal.zoneA;
                QString pName = (portal.type == PortalType::InterZoneWindow) ? QString("Window -> Zone %1").arg(otherZone + 1) : QString("Portal -> Zone %1").arg(otherZone + 1);
                p.drawText(centerScreen + QPointF(6, -6), pName);
            } else {
                QString pName = (portal.type == PortalType::InterZoneWindow) ? QString("Window (Z%1 <-> Z%2)").arg(portal.zoneA + 1).arg(portal.zoneB + 1) : QString("Portal (Z%1 <-> Z%2)").arg(portal.zoneA + 1).arg(portal.zoneB + 1);
                p.drawText(centerScreen + QPointF(6, -6), pName);
            }
        }
    }

    // 0c. Render Highlighted Portal from Dock Selection
    if (m_highlightedPortal.isValid() && m_highlightedPortal.layer == m_currentFloor) {
        p.save();
        float pulse = 0.5f + 0.5f * std::sin(m_animPhase * 3.5f);
        QColor portalColor = m_highlightedPortal.isBreach 
            ? QColor::fromRgbF(1.0f, 0.2f * (1.0f - pulse), 0.1f)
            : QColor::fromRgbF(0.0f, 0.9f + 0.1f * pulse, 0.7f + 0.3f * pulse);

        if (m_highlightedPortal.isHorizontal) {
            // Horizontal slab breach / portal
            QRectF cr = getCellRectScreen(m_highlightedPortal.x1, m_highlightedPortal.y1);
            p.setPen(QPen(portalColor, 5.0f, Qt::SolidLine));
            QColor fill = portalColor;
            fill.setAlphaF(0.25f + 0.15f * pulse);
            p.setBrush(fill);
            p.drawRect(cr);

            // Bold Hatch lines inside tile
            p.setPen(QPen(portalColor, 3.0f, Qt::SolidLine));
            p.drawLine(cr.topLeft(), cr.bottomRight());
            p.drawLine(cr.bottomLeft(), cr.topRight());

            // Badge
            QString pLabel = m_highlightedPortal.isBreach 
                ? QString("🚨 Пробоина перекрытия (Эт.%1)").arg(m_highlightedPortal.layer)
                : QString("⬆ Проем перекрытия (Эт.%1)").arg(m_highlightedPortal.layer);
            QFont pFont("Segoe UI", 9, QFont::Bold);
            QFontMetrics fm(pFont);
            int tw = fm.horizontalAdvance(pLabel) + 12;
            int th = fm.height() + 6;
            QRectF badgeRect(cr.center().x() - tw / 2.0f, cr.top() - th - 4, tw, th);
            p.setBrush(QColor(18, 22, 30, 230));
            p.setPen(QPen(portalColor, 2.0f));
            p.drawRoundedRect(badgeRect, 4, 4);
            p.setFont(pFont);
            p.setPen(Qt::white);
            p.drawText(badgeRect, Qt::AlignCenter, pLabel);
        } else {
            // Vertical portal between cells (x1, y1) and (x2, y2)
            int x1 = m_highlightedPortal.x1;
            int y1 = m_highlightedPortal.y1;
            int x2 = m_highlightedPortal.x2;
            int y2 = m_highlightedPortal.y2;

            QPointF p1, p2;
            if (x2 >= 0 && y2 >= 0) {
                if (x1 != x2) {
                    float edgeX = qMax(x1, x2) * TILE_SIZE;
                    p1 = worldToScreen(QPointF(edgeX, y1 * TILE_SIZE));
                    p2 = worldToScreen(QPointF(edgeX, (y1 + 1) * TILE_SIZE));
                } else if (y1 != y2) {
                    float edgeY = qMax(y1, y2) * TILE_SIZE;
                    p1 = worldToScreen(QPointF(x1 * TILE_SIZE, edgeY));
                    p2 = worldToScreen(QPointF((x1 + 1) * TILE_SIZE, edgeY));
                } else {
                    p1 = worldToScreen(QPointF(x1 * TILE_SIZE, (y1 + 0.5f) * TILE_SIZE));
                    p2 = worldToScreen(QPointF((x1 + 1) * TILE_SIZE, (y1 + 0.5f) * TILE_SIZE));
                }
            } else {
                p1 = worldToScreen(QPointF(x1 * TILE_SIZE, (y1 + 0.5f) * TILE_SIZE));
                p2 = worldToScreen(QPointF((x1 + 1) * TILE_SIZE, (y1 + 0.5f) * TILE_SIZE));
            }

            // Layer 1: Extra-wide outer halo
            QColor haloColor = portalColor;
            haloColor.setAlpha(65 + static_cast<int>(35 * pulse));
            p.setPen(QPen(haloColor, 28.0f, Qt::SolidLine, Qt::RoundCap));
            p.drawLine(p1, p2);

            // Layer 2: Bright medium glow
            QColor glowColor = portalColor;
            glowColor.setAlpha(140 + static_cast<int>(50 * pulse));
            p.setPen(QPen(glowColor, 14.0f, Qt::SolidLine, Qt::RoundCap));
            p.drawLine(p1, p2);

            // Layer 3: Extra-thick solid core line
            p.setPen(QPen(portalColor, 6.5f, Qt::SolidLine, Qt::RoundCap));
            p.drawLine(p1, p2);

            // Bold Perpendicular ticks
            QPointF dir = (p2 - p1);
            float len = std::hypot(dir.x(), dir.y());
            if (len > 0.1f) {
                QPointF perp(-dir.y() / len * 12.0f, dir.x() / len * 12.0f);
                p.setPen(QPen(portalColor, 3.5f, Qt::SolidLine, Qt::RoundCap));
                p.drawLine(p1 - perp, p1 + perp);
                p.drawLine(p2 - perp, p2 + perp);
            }

            // Bold Line of Sight arrow into destination zone
            if (x2 >= 0 && y2 >= 0) {
                QPointF fromScreen = worldToScreen(QPointF((x1 + 0.5f) * TILE_SIZE, (y1 + 0.5f) * TILE_SIZE));
                QPointF toScreen = worldToScreen(QPointF((x2 + 0.5f) * TILE_SIZE, (y2 + 0.5f) * TILE_SIZE));
                QPointF rayDir = toScreen - fromScreen;
                float rayLen = std::hypot(rayDir.x(), rayDir.y());
                if (rayLen > 1.0f) {
                    QPointF rayNorm = rayDir / rayLen;
                    QPointF pMid = (p1 + p2) * 0.5f;
                    QPointF arrowEnd = pMid + rayNorm * qBound(25.0f, 50.0f * m_zoom, 85.0f);

                    p.setPen(QPen(portalColor, 3.5f, Qt::SolidLine, Qt::RoundCap));
                    p.drawLine(pMid, arrowEnd);

                    // Bold Arrowhead
                    QPointF arrowSide(-rayNorm.y() * 9.0f, rayNorm.x() * 9.0f);
                    p.drawLine(arrowEnd, arrowEnd - rayNorm * 13.0f + arrowSide);
                    p.drawLine(arrowEnd, arrowEnd - rayNorm * 13.0f - arrowSide);
                }
            }

            // Badge
            QPointF centerScreen = (p1 + p2) * 0.5f;
            QString pLabel;
            if (m_highlightedPortal.isBreach) {
                pLabel = QString("🚨 ПРОБОИНА ➔ Z%1").arg(m_highlightedPortal.toZone + 1);
            } else if (m_highlightedPortal.isWindow) {
                pLabel = QString("🪟 ОКНО ➔ Z%1").arg(m_highlightedPortal.toZone + 1);
            } else if (m_highlightedPortal.isExterior) {
                pLabel = QString("🚪 ВЫХОД ➔ Улица");
            } else {
                pLabel = QString("🚪 ПОРТАЛ ➔ Z%1").arg(m_highlightedPortal.toZone + 1);
            }

            QFont pFont("Segoe UI", 9, QFont::Bold);
            QFontMetrics fm(pFont);
            int tw = fm.horizontalAdvance(pLabel) + 12;
            int th = fm.height() + 6;
            QRectF badgeRect(centerScreen.x() - tw / 2.0f, centerScreen.y() - th / 2.0f - 18.0f, tw, th);

            p.setBrush(QColor(18, 22, 30, 235));
            p.setPen(QPen(portalColor, 2.0f));
            p.drawRoundedRect(badgeRect, 4, 4);

            p.setFont(pFont);
            p.setPen(m_highlightedPortal.isBreach ? QColor(255, 120, 120) : Qt::white);
            p.drawText(badgeRect, Qt::AlignCenter, pLabel);
        }
        p.restore();
    }

    
    // 1. Render Doorway Portals on current floor (if VisZoneManager not active)
    if (!m_visZoneManager && m_showPortals) {
        for (const auto& ent : m_map->placedEntities) {
            if (ent.floorLayer != m_currentFloor) continue;

            QString name = ent.instanceName.toLower();
            bool isDoor = false;
            
            if (name.contains("door") || name.contains("gate") || name.contains("portal")) {
                isDoor = true;
            }
            
            if (ent.bankIndex > 0 && ent.bankIndex <= m_map->entityProfiles.size()) {
                const auto& prof = m_map->entityProfiles.value(ent.bankIndex);
                if (prof) {
                    QString path = prof->relPath.toLower();
                    path.replace("outdoor", ""); // Don't match 'outdoor' rocks
                if (path.contains("door") || path.contains("gate") || path.contains("portal")) {
                    isDoor = true;
                }
            }
        }

        if (isDoor) {
            float cx = ent.x;
            float cy = -ent.z;

            int rotDeg = static_cast<int>(std::round(ent.ry)) % 360;
            if (rotDeg < 0) rotDeg += 360;

            QPointF p1, p2;
            if ((rotDeg >= 45 && rotDeg < 135) || (rotDeg >= 225 && rotDeg < 315)) {
                // North-South doorway along Y
                p1 = worldToScreen(QPointF(cx, cy - 50.0f));
                p2 = worldToScreen(QPointF(cx, cy + 50.0f));
            } else {
                // East-West doorway along X
                p1 = worldToScreen(QPointF(cx - 50.0f, cy));
                p2 = worldToScreen(QPointF(cx + 50.0f, cy));
            }

            // Draw vibrant electric blue/cyan doorway portal line
            QColor portalBlue(0, 185, 255, 240);
            p.setPen(QPen(portalBlue, 3.5f, Qt::SolidLine, Qt::RoundCap));
            p.drawLine(p1, p2);

            // Draw small perpendicular end tick lines
            QPointF dir = (p2 - p1);
            float len = std::hypot(dir.x(), dir.y());
            if (len > 0.1f) {
                QPointF perp(-dir.y() / len * 6.0f, dir.x() / len * 6.0f);
                p.setPen(QPen(portalBlue, 2.0f, Qt::SolidLine, Qt::RoundCap));
                p.drawLine(p1 - perp, p1 + perp);
                p.drawLine(p2 - perp, p2 + perp);
            }

            // Portal Label
            QPointF centerScreen = (p1 + p2) * 0.5f;
            p.setPen(QColor(150, 220, 255));
            p.setFont(QFont("Segoe UI", 8, QFont::Bold));
            p.drawText(centerScreen + QPointF(6, -6), QStringLiteral("Portal (Door)"));
        }
    }
    }

    p.restore();
}

void MapCanvas::drawCSGCutouts(QPainter& p) {
    if (!m_map) return;
    if (m_currentFloor < 0 || m_currentFloor >= m_map->gridBlocks.size()) return;

    p.save();
    float wallRibbon = 12.0f * m_zoom;
    int rows = m_map->gridBlocks[m_currentFloor].size();
    int cols = rows > 0 ? m_map->gridBlocks[m_currentFloor][0].size() : 0;
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            QVector<PlacedOverlay> tileOlays;
            if (m_currentFloor < m_map->gridTileOverlays.size() &&
                y < m_map->gridTileOverlays[m_currentFloor].size() &&
                x < m_map->gridTileOverlays[m_currentFloor][y].size() &&
                !m_map->gridTileOverlays[m_currentFloor][y][x].isEmpty()) {
                tileOlays = m_map->gridTileOverlays[m_currentFloor][y][x];
            } else if (m_currentFloor < m_map->gridOverlays.size() &&
                       y < m_map->gridOverlays[m_currentFloor].size() &&
                       x < m_map->gridOverlays[m_currentFloor][y].size()) {
                int oId = m_map->gridOverlays[m_currentFloor][y][x];
                if (oId > 0) {
                    PlacedOverlay po;
                    po.segmentId = oId;
                    po.orient = m_map->gridOverlayRotation[m_currentFloor][y][x];
                    tileOlays.append(po);
                }
            }

            if (tileOlays.isEmpty()) continue;

            bool shouldIsolate = (m_activeVisZoneId >= 0 && m_cullInactiveVisZones) || m_highlightedPortal.isValid();
            bool inZone = true;
            if (shouldIsolate && m_visZoneManager) {
                inZone = isTileInActiveOrPath(m_currentFloor, x, y);
                if (!inZone) {
                    if (m_visZoneDimOpacity <= 0.01f) {
                        continue;
                    } else {
                        p.setOpacity(m_visZoneDimOpacity);
                    }
                } else {
                    p.setOpacity(1.0f);
                }
            }
            bool isInactive = shouldIsolate && !inZone;

            // Draw gantry/platform floor first, then CSG punch cutouts on top
            std::stable_sort(tileOlays.begin(), tileOlays.end(), [&](const PlacedOverlay& a, const PlacedOverlay& b) {
                auto segA = m_map->segments.value(a.segmentId);
                auto segB = m_map->segments.value(b.segmentId);
                bool punchA = segA && segA->hasPunch;
                bool punchB = segB && segB->hasPunch;
                return !punchA && punchB;
            });

            QRectF cellRect = getCellRectScreen(x, y);

            for (const auto& olay : tileOlays) {
                int oId = olay.segmentId;
                if (oId <= 0) continue;
                auto it = m_map->segments.find(oId);
                if (it == m_map->segments.end()) continue;
                const auto& seg = it.value();

                if (seg->hasPunch) {
                    int effectiveRot = olay.orient & 3;

                    int nx = x, ny = y;
                    switch (effectiveRot) {
                        case 0: ny -= 1; break;
                        case 1: nx += 1; break;
                        case 2: ny += 1; break;
                        case 3: nx -= 1; break;
                    }

                    bool hasDoubleWall = false;
                    if (ny >= 0 && ny < rows && nx >= 0 && nx < cols) {
                        if (m_map->gridBlocks[m_currentFloor][ny][nx] > 0) {
                            hasDoubleWall = true;
                        }
                    }

                    QRectF cutoutRect;
                    // effectiveRot: 0 = North edge, 1 = East edge, 2 = South edge, 3 = West edge
                    if (effectiveRot == 0) {
                        float topY = hasDoubleWall ? (cellRect.top() - wallRibbon) : cellRect.top();
                        float h = hasDoubleWall ? (2.0f * wallRibbon) : wallRibbon;
                        cutoutRect = QRectF(cellRect.left() + cellRect.width() * 0.18f, topY, cellRect.width() * 0.64f, h);
                    } else if (effectiveRot == 1) {
                        float w = hasDoubleWall ? (2.0f * wallRibbon) : wallRibbon;
                        cutoutRect = QRectF(cellRect.right() - wallRibbon, cellRect.top() + cellRect.height() * 0.18f, w, cellRect.height() * 0.64f);
                    } else if (effectiveRot == 2) {
                        float h = hasDoubleWall ? (2.0f * wallRibbon) : wallRibbon;
                        cutoutRect = QRectF(cellRect.left() + cellRect.width() * 0.18f, cellRect.bottom() - wallRibbon, cellRect.width() * 0.64f, h);
                    } else {
                        float leftX = hasDoubleWall ? (cellRect.left() - wallRibbon) : cellRect.left();
                        float w = hasDoubleWall ? (2.0f * wallRibbon) : wallRibbon;
                        cutoutRect = QRectF(leftX, cellRect.top() + cellRect.height() * 0.18f, w, cellRect.height() * 0.64f);
                    }

                    if (seg->isFake) {
                        // 1. Draw solid dark plate backing
                        p.fillRect(cutoutRect, QColor(40, 44, 52, 230));

                        // 2. Draw Slate-Gray decorative border
                        QColor fakeBorder(120, 135, 150, 220);
                        p.setPen(QPen(fakeBorder, 2.0f, Qt::SolidLine, Qt::SquareCap));
                        p.drawRect(cutoutRect);

                        // 3. Draw blind crosshatch pattern
                        p.setPen(QPen(QColor(90, 105, 120, 160), 1.0f));
                        p.drawLine(cutoutRect.topLeft(), cutoutRect.bottomRight());
                        p.drawLine(cutoutRect.bottomLeft(), cutoutRect.topRight());

                        // Decorative Label
                        p.setPen(QColor(180, 195, 210));
                        p.setFont(QFont("Segoe UI", 8, QFont::Bold));
                        QString label = seg->isWindow ? QStringLiteral("Fake Window (Blind)") : QStringLiteral("Fake Door (Static)");
                        if (effectiveRot == 0) {
                            p.drawText(cutoutRect.bottomLeft() + QPointF(0, 14), label);
                        } else if (effectiveRot == 2) {
                            p.drawText(cutoutRect.topLeft() + QPointF(0, -6), label);
                        } else {
                            p.drawText(cutoutRect.topRight() + QPointF(6, 12), label);
                        }
                    } else {
                        // 1. Draw Void / Cutout Hole Fill
                        p.fillRect(cutoutRect, QColor(22, 25, 34, 220));

                        // 2. Draw Glowing Overlay Fill
                        QColor fillGreen = isInactive ? QColor(70, 85, 100, 30) : QColor(46, 204, 113, 85);
                        p.fillRect(cutoutRect, fillGreen);

                        // 3. Draw Glowing Border spanning full wall thickness
                        QColor cutoutGreen = isInactive ? QColor(120, 135, 150, 180) : QColor(46, 204, 113, 255);
                        p.setPen(QPen(cutoutGreen, 2.5f, Qt::SolidLine, Qt::SquareCap));
                        p.drawRect(cutoutRect);

                        // 4. Center division line across the wall thickness
                        p.setPen(QPen(cutoutGreen, 1.5f, Qt::DashLine));
                        if (effectiveRot == 0 || effectiveRot == 2) {
                            float midX = cutoutRect.center().x();
                            p.drawLine(QPointF(midX, cutoutRect.top()), QPointF(midX, cutoutRect.bottom()));
                        } else {
                            float midY = cutoutRect.center().y();
                            p.drawLine(QPointF(cutoutRect.left(), midY), QPointF(cutoutRect.right(), midY));
                        }

                        // Cutout Label
                        p.setPen(isInactive ? QColor(140, 155, 170) : QColor(160, 255, 180));
                        QFont csgFont("Segoe UI", 8, QFont::Bold);
                        p.setFont(csgFont);
                        bool isWindow = (oId == 1) || seg->isWindow || seg->name.contains("window", Qt::CaseInsensitive) || seg->relPath.contains("window", Qt::CaseInsensitive);
                        QString label = isWindow ? QStringLiteral("CSG Cutout (Window)") : QStringLiteral("CSG Cutout (Doorway)");
                        if (effectiveRot == 0) {
                            p.drawText(cutoutRect.bottomLeft() + QPointF(0, 14), label);
                        } else if (effectiveRot == 2) {
                            p.drawText(cutoutRect.topLeft() + QPointF(0, -6), label);
                        } else {
                            p.drawText(cutoutRect.topRight() + QPointF(6, 12), label);
                        }
                    }
                } else if (seg->isPlatformOrGantry || seg->isStairs) {
                    int orient = olay.orient & 3;

                    // 1. Draw floor / walkway surface texture (or metal grating pattern)
                    bool drewTex = false;
                    if (!seg->floorTexture.isEmpty() && m_showFloorTextures) {
                        QPixmap surfacePx = AssetManager::instance().loadTexture(seg->floorTexture);
                        if (!surfacePx.isNull()) {
                            if (seg->isPlatformOrGantry && surfacePx.width() > 16) {
                                int gratingW = static_cast<int>(surfacePx.width() * 0.70f);
                                surfacePx = surfacePx.copy(0, 0, gratingW, surfacePx.height());
                            }
                            p.drawPixmap(cellRect.toRect(), surfacePx);
                            drewTex = true;
                        }
                    }
                    if (!drewTex) {
                        p.fillRect(cellRect, isInactive ? QColor(30, 36, 44, 220) : QColor(40, 48, 58, 220));
                        // Metal grating crosshatch
                        p.setPen(QPen(isInactive ? QColor(70, 85, 100, 100) : QColor(80, 115, 145, 130), 1.0f));
                        float step = cellRect.width() / 4.0f;
                        for (int k = 1; k < 4; ++k) {
                            p.drawLine(cellRect.left() + k * step, cellRect.top(), cellRect.left() + k * step, cellRect.bottom());
                            p.drawLine(cellRect.left(), cellRect.top() + k * step, cellRect.right(), cellRect.top() + k * step);
                        }
                    }
                    if (isInactive) {
                        p.fillRect(cellRect, QColor(10, 15, 25, 110)); // Dark wash over inactive gantry surface
                    }

                    if (seg->isStairs) {
                        p.save();
                        p.translate(cellRect.center());
                        p.rotate(orient * 90.0);
                        QRectF localRect(-cellRect.width() / 2.0f, -cellRect.height() / 2.0f, cellRect.width(), cellRect.height());

                        p.setPen(QPen(isInactive ? QColor(130, 140, 155, 180) : QColor(255, 185, 30, 220), 1.8f));
                        float stepH = cellRect.height() / 6.0f;
                        for (int s = 1; s < 6; ++s) {
                            float sy = localRect.top() + s * stepH;
                            p.drawLine(localRect.left() + 4, sy, localRect.right() - 4, sy);
                        }
                        p.setPen(QPen(isInactive ? QColor(140, 155, 170) : QColor(255, 215, 0), 2.0f));
                        p.drawLine(0, localRect.height() * 0.28f, 0, -localRect.height() * 0.28f);
                        p.drawLine(-5, -localRect.height() * 0.28f + 7, 0, -localRect.height() * 0.28f);
                        p.drawLine(5, -localRect.height() * 0.28f + 7, 0, -localRect.height() * 0.28f);
                        p.restore();
                    } else {
                        // Railings strictly according to engine FPSC-Game.DBA:48558-48592
                        QPen railPen(isInactive ? QColor(120, 135, 150, 140) : QColor(245, 185, 25), isInactive ? 1.8f : 2.5f, Qt::DashLine);
                        p.setPen(railPen);

                        if (getGantryRailingOnSide(seg->kindOf, seg->mode, orient, 0)) {
                            p.drawLine(cellRect.left(), cellRect.top(), cellRect.right(), cellRect.top()); // North
                        }
                        if (getGantryRailingOnSide(seg->kindOf, seg->mode, orient, 1)) {
                            p.drawLine(cellRect.right(), cellRect.top(), cellRect.right(), cellRect.bottom()); // East
                        }
                        if (getGantryRailingOnSide(seg->kindOf, seg->mode, orient, 2)) {
                            p.drawLine(cellRect.left(), cellRect.bottom(), cellRect.right(), cellRect.bottom()); // South
                        }
                        if (getGantryRailingOnSide(seg->kindOf, seg->mode, orient, 3)) {
                            p.drawLine(cellRect.left(), cellRect.top(), cellRect.left(), cellRect.bottom()); // West
                        }
                    }

                    p.setPen(isInactive ? QColor(130, 140, 155) : QColor(255, 210, 90));
                    p.setFont(QFont("Segoe UI", 7, QFont::Bold));
                    QString pLabel = seg->isStairs ? QStringLiteral("STAIRS") : QStringLiteral("GANTRY");
                    p.drawText(cellRect, Qt::AlignCenter, pLabel);
                } else if (seg->visOverlay > 0) {
                    // Decorative wall overlay (e.g. scifiwall1D, wall trim, molding)
                    int orient = olay.orient & 3;
                    float trimW = 6.0f * m_zoom;
                    QRectF trimRect;
                    switch (orient) {
                        case 0: trimRect = QRectF(cellRect.left(), cellRect.top(), cellRect.width(), trimW); break;
                        case 1: trimRect = QRectF(cellRect.right() - trimW, cellRect.top(), trimW, cellRect.height()); break;
                        case 2: trimRect = QRectF(cellRect.left(), cellRect.bottom() - trimW, cellRect.width(), trimW); break;
                        case 3: trimRect = QRectF(cellRect.left(), cellRect.top(), trimW, cellRect.height()); break;
                    }
                    p.fillRect(trimRect, isInactive ? QColor(50, 60, 70, 140) : QColor(70, 130, 180, 160)); // Steel-blue decorative trim
                    p.setPen(QPen(isInactive ? QColor(90, 100, 115, 160) : QColor(135, 206, 250, 200), 1.0f));
                    p.drawRect(trimRect);
                }
            }
        }
    }
    p.restore();
}

void MapCanvas::drawLeakWarnings(QPainter& p) {
    if (m_leakWarnings.empty() || !m_map) return;

    QRectF viewBounds(0, 0, width(), height());
    p.save();
    QFont font = p.font();
    font.setBold(true);
    font.setPointSize(9);
    p.setFont(font);

    // Filter unique warnings for the current floor
    QMap<QPair<int, int>, const PortalLeakWarning*> floorWarnings;
    for (const auto& w : m_leakWarnings) {
        if (w.layer == m_currentFloor) {
            floorWarnings.insert({w.x, w.y}, &w);
        }
    }

    float pulse = 0.5f + 0.5f * std::sin(m_animPhase * 2.0f);

    for (auto it = floorWarnings.constBegin(); it != floorWarnings.constEnd(); ++it) {
        int x = it.key().first;
        int y = it.key().second;
        const auto* w = it.value();

        QRectF rect = getCellRectScreen(x, y);
        if (!rect.intersects(viewBounds)) continue;

        if (w->isClash) {
            bool isBreach = w->type.contains(QStringLiteral("Breach"), Qt::CaseInsensitive);
            QColor border = isBreach ? QColor(255, 60, 60, 240) : QColor(255, 180, 20, 220);
            QColor fill = isBreach ? QColor(255, 50, 50, static_cast<int>(40 + 25 * pulse))
                                   : QColor(255, 170, 0, static_cast<int>(35 + 20 * pulse));
            p.setPen(QPen(border, 2.0f, Qt::DashLine));
            p.setBrush(fill);
            p.drawRect(rect);

            // If x2, y2 are set, also draw the cell rect and a thick indicator on the shared edge
            if (w->x2 >= 0 && w->y2 >= 0) {
                QRectF rect2 = getCellRectScreen(w->x2, w->y2);
                p.drawRect(rect2);

                QPointF p1, p2;
                if (w->x != w->x2) {
                    float sharedX = (w->x < w->x2) ? rect.right() : rect.left();
                    p1 = QPointF(sharedX, rect.top());
                    p2 = QPointF(sharedX, rect.bottom());
                } else {
                    float sharedY = (w->y < w->y2) ? rect.bottom() : rect.top();
                    p1 = QPointF(rect.left(), sharedY);
                    p2 = QPointF(rect.right(), sharedY);
                }
                QPen wallPen(isBreach ? QColor(255, 40, 40, 255) : QColor(255, 200, 40, 255), 4.0f, Qt::SolidLine, Qt::RoundCap);
                p.setPen(wallPen);
                p.drawLine(p1, p2);
            }

            p.setPen(border);
            p.drawText(rect.adjusted(2, 2, -2, -2), Qt::AlignTop | Qt::AlignRight, isBreach ? QStringLiteral("🚫") : QStringLiteral("⚡"));
        } else {
            // Red leak indicator (missing ceiling, exterior breach, etc.)
            QColor fill(255, 40, 40, static_cast<int>(35 + 25 * pulse));
            QColor border(255, 50, 50, 220);
            p.setPen(QPen(border, 2.0f, Qt::DashLine));
            p.setBrush(fill);
            p.drawRect(rect);

            // Icon
            p.setPen(border);
            p.drawText(rect.adjusted(2, 2, -2, -2), Qt::AlignTop | Qt::AlignRight, QStringLiteral("⚠️"));
        }
    }

    p.restore();
}

void MapCanvas::setTracePath(const std::vector<ZoneConnection>& path) {
    m_activeTracePath = path;
    update();
}

void MapCanvas::clearTracePath() {
    m_activeTracePath.clear();
    update();
}

void MapCanvas::drawTracePath(QPainter& p) {
    if (m_activeTracePath.empty() || !m_map) return;

    // If no segment/portal in this trace path belongs to the current floor, do not render foreign-floor rays
    bool hasAnyStepOnCurrentFloor = false;
    for (const auto& conn : m_activeTracePath) {
        if (conn.layer == m_currentFloor) {
            hasAnyStepOnCurrentFloor = true;
            break;
        }
    }
    if (!hasAnyStepOnCurrentFloor) return;

    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);

    float pulse = 0.5f + 0.5f * std::sin(m_animPhase * 3.5f);

    auto getZoneCenterScreen = [this](int zoneId) -> QPointF {
        if (!m_visZoneManager) return QPointF(-1.0, -1.0);
        const VisZone* z = m_visZoneManager->getZone(zoneId);
        if (!z) return QPointF(-1.0, -1.0);
        const auto& floorTiles = z->getTilesOnFloor(m_currentFloor);
        if (floorTiles.empty()) return QPointF(-1.0, -1.0);
        float sumX = 0, sumY = 0;
        for (const QPoint& pt : floorTiles) {
            sumX += (pt.x() + 0.5f) * TILE_SIZE;
            sumY += (pt.y() + 0.5f) * TILE_SIZE;
        }
        return worldToScreen(QPointF(sumX / floorTiles.size(), sumY / floorTiles.size()));
    };

    auto getPortalPointWorld = [this](const ZoneConnection& conn) -> QPointF {
        if (conn.layer != m_currentFloor || conn.x1 < 0 || conn.y1 < 0) return QPointF(-1.0, -1.0);
        float worldX = (conn.x1 + 0.5f) * TILE_SIZE;
        float worldY = (conn.y1 + 0.5f) * TILE_SIZE;
        if (conn.x2 >= 0 && conn.y2 >= 0) {
            worldX = (conn.x1 + conn.x2 + 1.0f) * 0.5f * TILE_SIZE;
            worldY = (conn.y1 + conn.y2 + 1.0f) * 0.5f * TILE_SIZE;
        }
        return QPointF(worldX, worldY);
    };

    auto getZoneCenterWorld = [this](int zoneId) -> QPointF {
        if (!m_visZoneManager) return QPointF(-1.0, -1.0);
        const VisZone* z = m_visZoneManager->getZone(zoneId);
        if (!z) return QPointF(-1.0, -1.0);
        const auto& floorTiles = z->getTilesOnFloor(m_currentFloor);
        if (floorTiles.empty()) return QPointF(-1.0, -1.0);
        float sumX = 0, sumY = 0;
        for (const QPoint& pt : floorTiles) {
            sumX += (pt.x() + 0.5f) * TILE_SIZE;
            sumY += (pt.y() + 0.5f) * TILE_SIZE;
        }
        return QPointF(sumX / floorTiles.size(), sumY / floorTiles.size());
    };

    std::vector<QPointF> portalWorldPts;
    portalWorldPts.reserve(m_activeTracePath.size());
    for (const auto& conn : m_activeTracePath) {
        portalWorldPts.push_back(getPortalPointWorld(conn));
    }

    if (portalWorldPts.empty() || portalWorldPts.front().x() < 0) return;

    QPointF wStart(-1, -1);
    QPointF wEnd(-1, -1);
    QPointF dir(0, 1);

    auto clipRayToZone = [this](QPointF startPt, QPointF rayDir, int zoneId, float maxDist) -> QPointF {
        if (!m_visZoneManager || maxDist <= 0.0f || zoneId < 0) return startPt;
        float stepSize = 4.0f; // pixels per step (~0.04 tile)
        int steps = static_cast<int>(maxDist / stepSize);
        QPointF lastValid = startPt;

        int prevTx = -1, prevTy = -1;
        for (int i = 1; i <= steps; ++i) {
            QPointF cur = startPt + rayDir * (i * stepSize);
            int tx = static_cast<int>(std::floor(cur.x() / TILE_SIZE));
            int ty = static_cast<int>(std::floor(cur.y() / TILE_SIZE));

            if (!m_map || tx < 0 || tx > m_map->header.maxX || ty < 0 || ty > m_map->header.maxY) {
                break;
            }

            // Must stay inside the specified zone on current floor
            if (m_visZoneManager->getZoneAt(m_currentFloor, tx, ty) != zoneId) {
                break;
            }

            // Check if crossing a solid wall between adjacent tiles inside the zone
            if (prevTx >= 0 && prevTy >= 0 && (tx != prevTx || ty != prevTy)) {
                if (tx == prevTx + 1 && ty == prevTy) {
                    bool w1 = m_visZoneManager->isMaptileWallPresent(m_currentFloor, prevTx, prevTy, 1);
                    bool w2 = m_visZoneManager->isMaptileWallPresent(m_currentFloor, tx, ty, 3);
                    if ((w1 || w2) && !m_visZoneManager->isDoorOrWindowOnEdge(m_currentFloor, prevTx, prevTy, tx, ty, 1)) {
                        break;
                    }
                } else if (tx == prevTx - 1 && ty == prevTy) {
                    bool w1 = m_visZoneManager->isMaptileWallPresent(m_currentFloor, prevTx, prevTy, 3);
                    bool w2 = m_visZoneManager->isMaptileWallPresent(m_currentFloor, tx, ty, 1);
                    if ((w1 || w2) && !m_visZoneManager->isDoorOrWindowOnEdge(m_currentFloor, prevTx, prevTy, tx, ty, 3)) {
                        break;
                    }
                } else if (ty == prevTy + 1 && tx == prevTx) {
                    bool w1 = m_visZoneManager->isMaptileWallPresent(m_currentFloor, prevTx, prevTy, 2);
                    bool w2 = m_visZoneManager->isMaptileWallPresent(m_currentFloor, tx, ty, 0);
                    if ((w1 || w2) && !m_visZoneManager->isDoorOrWindowOnEdge(m_currentFloor, prevTx, prevTy, tx, ty, 2)) {
                        break;
                    }
                } else if (ty == prevTy - 1 && tx == prevTx) {
                    bool w1 = m_visZoneManager->isMaptileWallPresent(m_currentFloor, prevTx, prevTy, 0);
                    bool w2 = m_visZoneManager->isMaptileWallPresent(m_currentFloor, tx, ty, 2);
                    if ((w1 || w2) && !m_visZoneManager->isDoorOrWindowOnEdge(m_currentFloor, prevTx, prevTy, tx, ty, 0)) {
                        break;
                    }
                }
            }

            lastValid = cur;
            prevTx = tx;
            prevTy = ty;
        }
        return lastValid;
    };

    if (m_activeTracePath.size() == 1) {
        const auto& conn = m_activeTracePath.front();
        QPointF pw = portalWorldPts.front();
        QPointF cwStart = getZoneCenterWorld(conn.fromZone);

        // Normal from fromZone into toZone
        QPointF normal(0, 1);
        if (conn.x2 >= 0 && conn.y2 >= 0) {
            float dx = conn.x2 - conn.x1;
            float dy = conn.y2 - conn.y1;
            float len = std::hypot(dx, dy);
            if (len > 0.001f) normal = QPointF(dx / len, dy / len);
        } else if (conn.isHorizontal) {
            normal = QPointF(0, 1);
        } else {
            normal = QPointF(1, 0);
        }

        dir = normal;
        if (cwStart.x() >= 0) {
            QPointF fromCenter = pw - cwStart;
            float len = std::hypot(fromCenter.x(), fromCenter.y());
            if (len > 0.001f) {
                QPointF uFromCenter = fromCenter / len;
                if (uFromCenter.x() * normal.x() + uFromCenter.y() * normal.y() > 0.1f) {
                    dir = uFromCenter;
                    wStart = cwStart;
                }
            }
        }

        if (wStart.x() < 0) {
            wStart = clipRayToZone(pw, -dir, conn.fromZone, 50.0f * TILE_SIZE);
        }
        wEnd = clipRayToZone(pw, dir, conn.toZone, 50.0f * TILE_SIZE);
    } else {
        // Multi-hop path: Ray direction is determined strictly by the portal-to-portal line of sight!
        QPointF p0 = portalWorldPts.front();
        QPointF pLast = portalWorldPts.back();
        QPointF delta = pLast - p0;
        float dist = std::hypot(delta.x(), delta.y());
        if (dist > 0.001f) {
            dir = delta / dist;
        } else {
            dir = QPointF(0, 1);
        }

        // Trace ray backwards inside source zone to wall, and forwards inside target zone to wall.
        // Neither point will ever exit the respective zone boundaries!
        wStart = clipRayToZone(p0, -dir, m_activeTracePath.front().fromZone, 50.0f * TILE_SIZE);
        wEnd = clipRayToZone(pLast, dir, m_activeTracePath.back().toZone, 50.0f * TILE_SIZE);
    }

    // Build the strictly collinear line-of-sight waypoints:
    struct PathSegment {
        QPointF from;
        QPointF to;
        bool isBreach;
    };
    std::vector<PathSegment> segments;

    // Segment 1: Zone A start -> Portal 0 (strictly along `dir`)
    if (wStart.x() >= 0 && std::hypot(wStart.x() - portalWorldPts.front().x(), wStart.y() - portalWorldPts.front().y()) > 1.0f) {
        segments.push_back({
            worldToScreen(wStart),
            worldToScreen(portalWorldPts.front()),
            m_activeTracePath.front().isBreach || m_activeTracePath.front().isCrack
        });
    }

    // Segment 2..N: Inter-portal segments (strictly along `dir` for collinear sight)
    for (size_t i = 0; i + 1 < portalWorldPts.size(); ++i) {
        if (portalWorldPts[i].x() >= 0 && portalWorldPts[i + 1].x() >= 0) {
            bool breach = m_activeTracePath[i + 1].isBreach || m_activeTracePath[i + 1].isCrack;
            segments.push_back({
                worldToScreen(portalWorldPts[i]),
                worldToScreen(portalWorldPts[i + 1]),
                breach
            });
        }
    }

    // Segment Last: Last Portal -> Penetration into Target Zone (strictly along `dir`, NO bend to room center!)
    if (wEnd.x() >= 0 && std::hypot(wEnd.x() - portalWorldPts.back().x(), wEnd.y() - portalWorldPts.back().y()) > 1.0f) {
        segments.push_back({
            worldToScreen(portalWorldPts.back()),
            worldToScreen(wEnd),
            m_activeTracePath.back().isBreach || m_activeTracePath.back().isCrack
        });
    }

    // 1. Draw soft glow under rays
    for (const auto& seg : segments) {
        QColor glowColor = seg.isBreach ? QColor(239, 68, 68, static_cast<int>(50 + 30 * pulse))
                                        : QColor(56, 189, 248, static_cast<int>(40 + 25 * pulse));
        p.setPen(QPen(glowColor, 6.0f, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawLine(seg.from, seg.to);
    }

    // 2. Draw core ray & directional arrow
    for (const auto& seg : segments) {
        QColor rayColor = seg.isBreach ? QColor(239, 68, 68, static_cast<int>(210 + 45 * pulse))
                                       : QColor(56, 189, 248, static_cast<int>(190 + 55 * pulse));
        float penWidth = seg.isBreach ? 3.0f : 2.2f;
        p.setPen(QPen(rayColor, penWidth, seg.isBreach ? Qt::DashLine : Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawLine(seg.from, seg.to);

        // Draw directional arrowhead at midpoint
        QPointF delta = seg.to - seg.from;
        float dist = std::hypot(delta.x(), delta.y());
        if (dist > 35.0f) {
            QPointF mid = (seg.from + seg.to) * 0.5;
            QPointF u = delta / dist;
            QPointF n(-u.y(), u.x());
            float arrLen = 7.0f;
            float arrWidth = 4.5f;
            QPointF tip = mid + u * (arrLen * 0.6f);
            QPointF left = mid - u * (arrLen * 0.4f) + n * arrWidth;
            QPointF right = mid - u * (arrLen * 0.4f) - n * arrWidth;

            p.setBrush(rayColor);
            p.setPen(Qt::NoPen);
            QPolygonF poly;
            poly << tip << left << right;
            p.drawPolygon(poly);
        }
    }

    // 3. Draw portal aperture markers
    for (size_t i = 0; i < m_activeTracePath.size(); ++i) {
        const auto& conn = m_activeTracePath[i];
        bool isBreachStep = (conn.isBreach || conn.isCrack);
        QPointF pPortal = (i < portalWorldPts.size() && portalWorldPts[i].x() >= 0)
            ? worldToScreen(portalWorldPts[i]) : QPointF(-1, -1);

        if (pPortal.x() >= 0) {
            if (isBreachStep) {
                QRectF cellRect = getCellRectScreen(conn.x1, conn.y1);
                p.setBrush(QColor(239, 68, 68, static_cast<int>(60 + 40 * pulse)));
                p.setPen(QPen(QColor(255, 60, 60, 240), 2.5f));
                p.drawRect(cellRect);

                p.setBrush(QColor(255, 30, 30, 230));
                p.drawEllipse(pPortal, 6.0f + 2.0f * pulse, 6.0f + 2.0f * pulse);

                p.setFont(QFont("Segoe UI", 9, QFont::Bold));
                p.setPen(QColor(255, 220, 220));
                p.drawText(cellRect.adjusted(2, 2, -2, -2), Qt::AlignCenter, QStringLiteral("🚨 LEAK"));
            } else {
                p.setBrush(QColor(14, 165, 233, 220));
                p.setPen(QPen(QColor(255, 255, 255), 1.5f));
                p.drawEllipse(pPortal, 5.0f, 5.0f);
            }
        }
    }

    p.restore();
}

void MapCanvas::setHighlightedPortal(const HighlightedPortalInfo& info) {
    m_highlightedPortal = info;
    if (info.isValid()) {
        if (info.focusedVisibleZone >= 0 && m_visZoneManager) {
            const VisZone* vz = m_visZoneManager->getZone(info.focusedVisibleZone);
            if (vz) {
                if (!vz->hasFloor(m_currentFloor)) {
                    setFloor(vz->floor);
                }
                const auto& pts = vz->getTilesOnFloor(m_currentFloor);
                const auto& targetTiles = pts.empty() ? vz->tiles : pts;
                if (!targetTiles.empty()) {
                    float sumX = 0, sumY = 0;
                    for (const auto& t : targetTiles) {
                        sumX += (t.x() + 0.5f) * TILE_SIZE;
                        sumY += (t.y() + 0.5f) * TILE_SIZE;
                    }
                    float cx = sumX / targetTiles.size();
                    float cy = sumY / targetTiles.size();
                    if (m_zoom < 0.5f) {
                        m_zoom = 0.8f;
                        emit zoomChanged(m_zoom);
                    }
                    m_panOffset = QPointF(width() / 2.0f - cx * m_zoom, height() / 2.0f - cy * m_zoom);
                }
            }
        } else if (info.layer >= 0) {
            if (info.layer != m_currentFloor) {
                setFloor(info.layer);
            }
            float cellWorldX = (info.x1 * TILE_SIZE) + (TILE_SIZE / 2.0f);
            float cellWorldY = (info.y1 * TILE_SIZE) + (TILE_SIZE / 2.0f);
            if (m_zoom < 0.5f) {
                m_zoom = 0.8f;
                emit zoomChanged(m_zoom);
            }
            m_panOffset = QPointF(width() / 2.0f - cellWorldX * m_zoom, height() / 2.0f - cellWorldY * m_zoom);
        }
    }
    update();
}

void MapCanvas::clearHighlightedPortal() {
    m_highlightedPortal.clear();
    update();
}

bool MapCanvas::isTileInActiveOrPath(int layer, int x, int y) const {
    if (m_activeVisZoneId < 0 && !m_highlightedPortal.isValid()) return true;
    if (m_visZoneManager && m_activeVisZoneId >= 0 && m_visZoneManager->isTileInZone(m_activeVisZoneId, layer, x, y)) {
        return true;
    }
    if (m_highlightedPortal.isValid() && m_visZoneManager) {
        if (m_highlightedPortal.focusedVisibleZone >= 0) {
            if (m_visZoneManager->isTileInZone(m_highlightedPortal.focusedVisibleZone, layer, x, y)) {
                return true;
            }
        } else if (m_highlightedPortal.toZone >= 0) {
            if (m_visZoneManager->isTileInZone(m_highlightedPortal.toZone, layer, x, y)) {
                return true;
            }
        }
    }
    if (!m_activeTracePath.empty() && m_visZoneManager) {
        for (const auto& conn : m_activeTracePath) {
            if (m_visZoneManager->isTileInZone(conn.fromZone, layer, x, y) ||
                m_visZoneManager->isTileInZone(conn.toZone, layer, x, y)) {
                return true;
            }
        }
    }
    return false;
}

bool MapCanvas::isEntityInActiveOrPath(int entityIndex) const {
    if (m_activeVisZoneId < 0 && !m_highlightedPortal.isValid()) return true;
    if (m_visZoneManager && m_activeVisZoneId >= 0 && m_visZoneManager->isEntityInZone(m_activeVisZoneId, entityIndex)) {
        return true;
    }
    if (m_highlightedPortal.isValid() && m_visZoneManager) {
        if (m_highlightedPortal.focusedVisibleZone >= 0) {
            if (m_visZoneManager->isEntityInZone(m_highlightedPortal.focusedVisibleZone, entityIndex)) {
                return true;
            }
        } else if (m_highlightedPortal.toZone >= 0) {
            if (m_visZoneManager->isEntityInZone(m_highlightedPortal.toZone, entityIndex)) {
                return true;
            }
        }
    }
    if (!m_activeTracePath.empty() && m_visZoneManager) {
        for (const auto& conn : m_activeTracePath) {
            if (m_visZoneManager->isEntityInZone(conn.fromZone, entityIndex) ||
                m_visZoneManager->isEntityInZone(conn.toZone, entityIndex)) {
                return true;
            }
        }
    }
    return false;
}



