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
        if (m_selectedEntityIndex >= 0) {
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

void MapCanvas::setMap(std::shared_ptr<FPSCMap> map) {
    m_map = map;
    m_selectedEntityIndex = -1;
    m_hoveredEntityIndex = -1;
    if (m_map) {
        m_currentFloor = qBound(0, m_map->activeEditorLayer, m_map->header.layerMax);
        emit floorChanged(m_currentFloor);
        zoomFit();
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

void MapCanvas::highlightCell(int layer, int x, int y) {
    if (!m_map) return;
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

    // 2. Ghost lower floor (if enabled and currentFloor > 0)
    if (m_showGhostLayer && m_currentFloor > 0) {
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

    // 7. Portals & VisZones (from compiled universe.dbu)
    if (m_showPortals) {
        drawPortals(p);
    }

    // 7. Interactive Translation Gizmo on selected entity
    drawGizmo(p);

    if (m_highlightedLayer == m_currentFloor && m_highlightedX >= 0 && m_highlightedY >= 0) {
        QRectF hlRect = getCellRectScreen(m_highlightedX, m_highlightedY);
        p.setPen(QPen(Qt::red, 3));
        
        QColor fill = Qt::red;
        fill.setAlphaF(0.2f + 0.2f * std::sin(m_animPhase * 2.0f));
        p.setBrush(fill);
        p.drawRect(hlRect);
    }

    // 8. HUD Overlays
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

    // Highlight hovered tile
    if (m_hoveredTile.x() >= 0 && m_hoveredTile.x() < cols &&
        m_hoveredTile.y() >= 0 && m_hoveredTile.y() < rows)
    {
        QRectF hRect = getCellRectScreen(m_hoveredTile.x(), m_hoveredTile.y());
        p.fillRect(hRect, QColor(0, 180, 255, 45));
        p.setPen(QPen(QColor(50, 200, 255, 220), 1.5f, Qt::SolidLine));
        p.drawRect(hRect);
    }

    p.restore();
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

            auto it = m_map->segments.find(segId);
            if (it == m_map->segments.end()) continue;
            const auto& seg = it.value();

            QRectF cellRect = getCellRectScreen(x, y);
            int rot = m_map->gridRotation[layer][y][x] & 3;

            // Multi-story floor vs wall layer detection:
            bool hasBelow = (layer > 0 && m_map->gridBlocks[layer - 1][y][x] > 0);
            bool hasAbove = (layer + 1 < m_map->gridBlocks.size() && m_map->gridBlocks[layer + 1][y][x] > 0);
            if (!hasAbove && layer + 1 < m_map->gridBlocks.size()) {
                for (int dy = -1; dy <= 1 && !hasAbove; ++dy) {
                    for (int dx = -1; dx <= 1 && !hasAbove; ++dx) {
                        int ny = y + dy;
                        int nx = x + dx;
                        if (ny >= 0 && ny < rows && nx >= 0 && nx < cols) {
                            if (m_map->gridBlocks[layer + 1][ny][nx] > 0) {
                                hasAbove = true;
                            }
                        }
                    }
                }
            }

            bool drawFloor = true;
            bool drawWalls = true;

            if (hasBelow && hasAbove) {
                // Intermediate upper story of a room: WALLS ONLY, NO FLOOR!
                drawFloor = false;
                drawWalls = true;
            } else if (hasBelow && !hasAbove) {
                // Top roof / ceiling slab capping the room below: FLOOR/ROOF SLAB ONLY, NO WALLS!
                drawFloor = true;
                drawWalls = false;
            } else {
                // Base ground floor: FLOOR + WALLS
                drawFloor = true;
                drawWalls = true;
            }

            // A. Draw Floor / Ceiling Texture
            if (drawFloor) {
                QString surfaceTex = !seg->floorTexture.isEmpty() ? seg->floorTexture : seg->roofTexture;
                if (!surfaceTex.isEmpty()) {
                    if (m_showFloorTextures) {
                        QPixmap surfacePx = AssetManager::instance().loadTexture(surfaceTex);
                        if (!surfacePx.isNull()) {
                            p.drawPixmap(cellRect.toRect(), surfacePx);
                        } else {
                            p.fillRect(cellRect, QColor(50, 55, 70));
                        }
                    } else {
                        p.fillRect(cellRect, QColor(50, 55, 70));
                    }
                }
            }

            // B. Draw Large Scenery / Rock footprint
            if (seg->isScenery && !seg->floorTexture.isEmpty()) {
                p.setPen(QPen(QColor(180, 150, 100, 200), 1.5f));
                p.drawRect(cellRect);
                continue;
            }

            // C. Auto-Tiling Textured Wall Ribbons (Perimeter walls only)
            if (!drawWalls) continue;
            for (int origSide = 0; origSide < 4; ++origSide) {
                if (!seg->hasWall[origSide]) continue;

                int rotSide = (origSide + rot) % 4;

                // Auto-tiling neighbor check:
                // Dividing wall is suppressed between adjacent cells of the same room,
                // or facing the interior open volume of the room below!
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
                        // Same segment type on this layer:
                        // Suppress wall UNLESS separated by explicit partition rotations (e.g. facing wall tiles)
                        int neighborRot = m_map->gridRotation[layer][ny][nx] & 3;
                        bool isPartition = (rot != neighborRot) && (rotSide == rot || rotSide == (neighborRot + 2) % 4);
                        if (!isPartition) {
                            continue;
                        }
                    } else if (neighborSeg == 0) {
                        // Empty tile on current layer:
                        // Suppress inner wall ONLY if neighbor cell was part of the same room below!
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

        QPointF entScreen = worldToScreen(QPointF(ent.x, -ent.z));

        // Draw Light Source Radiant Halo
        if (m_showLights && ent.lightRange > 0) {
            float radScreen = ent.lightRange * m_zoom;
            QRadialGradient grad(entScreen, radScreen);
            QColor lColor = ent.lightColor;
            lColor.setAlpha(140);
            grad.setColorAt(0.0f, lColor);
            lColor.setAlpha(40);
            grad.setColorAt(0.5f, lColor);
            lColor.setAlpha(0);
            grad.setColorAt(1.0f, lColor);

            p.setBrush(grad);
            p.setPen(Qt::NoPen);
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
        if (cat == EntityCategory::Light || ent.lightRange > 0) {
            QRectF lRing = iconRect.adjusted(-3, -3, 3, 3);
            p.setPen(QPen(ent.lightColor, 2.5f));
            p.setBrush(QColor(ent.lightColor.red(), ent.lightColor.green(), ent.lightColor.blue(), 75));
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

        selectEntity(clickedEntity);
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

    // 2. Handle Gizmo Hover Cursor
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
    }

    QPointF worldPos = screenToWorld(event->pos());
    int tileX = static_cast<int>(std::floor(worldPos.x() / TILE_SIZE));
    int tileY = static_cast<int>(std::floor(worldPos.y() / TILE_SIZE));

    bool needUpdate = false;
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

    if (m_hoveredEntityIndex >= 0) {
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
    setCursor(Qt::ArrowCursor);
    update();
}

void MapCanvas::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::RightButton) {
        m_isPanning = false;
        setCursor(Qt::ArrowCursor);

        // Check if right clicked on an entity to show context menu
        if ((event->pos() - m_lastMousePos).manhattanLength() < 6 && m_map) {
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
        case Qt::Key_Delete:
        case Qt::Key_Backspace:
            if (m_selectedEntityIndex >= 0) {
                emit entityDeleteRequested(m_selectedEntityIndex);
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

void MapCanvas::drawPortals(QPainter& p) {
    if (!m_map) return;

    p.save();
    
    // 1. Render Doorway Portals on current floor
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

    // 2. Render DBU Internal Portals (if loaded from level universe.dbu)
    for (const auto& dbuP : m_portals) {
        if (dbuP.isExteriorHull) continue;
        if (!dbuP.box.intersectsLayer(m_currentFloor)) continue;

        QPointF p1 = worldToScreen(QPointF(dbuP.box.minX, -dbuP.box.minZ));
        QPointF p2 = worldToScreen(QPointF(dbuP.box.maxX, -dbuP.box.maxZ));
        if (QLineF(p1, p2).length() < 2.0f) continue;

        QColor portalBlue(0, 185, 255, 200);
        p.setPen(QPen(portalBlue, 3.0f, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(p1, p2);
    }

    // 3. Render Real Leaks from PortalLeakAnalyzer on current floor
    PortalLeakAnalyzer analyzer(m_map);
    auto warnings = analyzer.analyze();
    for (const auto& w : warnings) {
        if (w.layer == m_currentFloor && w.severity == PortalLeakWarning::ERROR) {
            QRectF cellRect = getCellRectScreen(w.x, w.y);
            
            // Draw red glowing leak box
            p.setPen(QPen(QColor(255, 45, 75, 240), 2.5f, Qt::DashLine));
            p.setBrush(QColor(255, 0, 50, 60));
            p.drawRect(cellRect);

            // Center Tag
            p.setPen(QColor(255, 120, 140));
            p.setFont(QFont("Segoe UI", 7, QFont::Bold));
            p.drawText(cellRect, Qt::AlignCenter, QStringLiteral("LEAK (Void)"));
        }
    }
    
    p.restore();
}

void MapCanvas::drawCSGCutouts(QPainter& p) {
    if (!m_map) return;
    if (m_currentFloor < 0 || m_currentFloor >= m_map->gridOverlays.size()) return;

    p.save();
    float wallRibbon = 12.0f * m_zoom;
    int rows = m_map->gridOverlays[m_currentFloor].size();
    int cols = rows > 0 ? m_map->gridOverlays[m_currentFloor][0].size() : 0;
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            int oId = m_map->gridOverlays[m_currentFloor][y][x];
            if (oId <= 0) continue;

            int effectiveRot = m_map->gridOverlayRotation[m_currentFloor][y][x] & 3;
            QRectF cellRect = getCellRectScreen(x, y);

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

            // 1. Draw Void / Cutout Hole Fill
            p.fillRect(cutoutRect, QColor(22, 25, 34, 220));

            // 2. Draw Vibrant Green Glowing Overlay Fill
            QColor fillGreen(46, 204, 113, 85);
            p.fillRect(cutoutRect, fillGreen);

            // 3. Draw BOLD Glowing Emerald-Green Border spanning full wall thickness
            QColor cutoutGreen(46, 204, 113, 255);
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
            p.setPen(QColor(160, 255, 180));
            p.setFont(QFont("Segoe UI", 8, QFont::Bold));
            QString label = (oId == 1) ? QStringLiteral("CSG Cutout (Window / Slit)") : QStringLiteral("CSG Cutout (Doorway)");
            if (effectiveRot == 0) {
                p.drawText(cutoutRect.bottomLeft() + QPointF(0, 14), label);
            } else if (effectiveRot == 2) {
                p.drawText(cutoutRect.topLeft() + QPointF(0, -6), label);
            } else {
                p.drawText(cutoutRect.topRight() + QPointF(6, 12), label);
            }
        }
    }
    p.restore();
}
