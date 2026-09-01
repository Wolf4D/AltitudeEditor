#include "MapCanvas.h"
#include "AssetManager.h"
#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QToolTip>
#include <QFontMetrics>
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
        zoomFit();
    }
    update();
}

void MapCanvas::setFloor(int floor) {
    if (!m_map) return;
    int bounded = qBound(0, floor, m_map->header.layerMax);
    if (m_currentFloor != bounded) {
        m_currentFloor = bounded;
        emit floorChanged(m_currentFloor);
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

    // 2. Ghost lower floor (if enabled and currentFloor > 0)
    if (m_showGhostLayer && m_currentFloor > 0) {
        drawSegments(p, m_currentFloor - 1, 0.22f);
    }

    // 3. Current active floor segments & Doom-style wall textures
    drawSegments(p, m_currentFloor, 1.0f);

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

    // 7. HUD Overlays
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

            // A. Draw Floor Texture
            if (m_showFloorTextures && !seg->floorTexture.isEmpty()) {
                QPixmap floorPx = AssetManager::instance().loadTexture(seg->floorTexture);
                if (!floorPx.isNull()) {
                    p.drawPixmap(cellRect.toRect(), floorPx);
                } else {
                    p.fillRect(cellRect, QColor(50, 55, 70));
                }
            } else {
                p.fillRect(cellRect, QColor(50, 55, 70));
            }

            // B. Draw Large Scenery / Rock footprint
            if (seg->isScenery && !seg->floorTexture.isEmpty()) {
                p.setPen(QPen(QColor(180, 150, 100, 200), 1.5f));
                p.drawRect(cellRect);
                continue;
            }

            // C. Doom-Style Textured Wall Ribbons
            // Original wall directions: 0=North (Z-top), 1=East (X-right), 2=South (Z-bottom), 3=West (X-left)
            // Rotated direction = (original + rot) % 4
            for (int origSide = 0; origSide < 4; ++origSide) {
                if (!seg->hasWall[origSide]) continue;

                int rotSide = (origSide + rot) % 4;
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
    update();
}

void MapCanvas::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::RightButton || event->button() == Qt::MiddleButton) {
        m_isPanning = false;
        unsetCursor();
        event->accept();
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
