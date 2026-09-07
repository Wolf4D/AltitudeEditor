#ifndef MAPCANVAS_H
#define MAPCANVAS_H

#include "FPSCData.h"
#include "UniverseDBUParser.h"
#include "VisZoneManager.h"
#include <QWidget>
#include <QPointF>
#include <QTimer>
#include <memory>

struct EditorPortal {
    enum Type {
        Doorway,
        Passage,
        LeakMissingCeiling,
        LeakMissingWall
    };
    Type type = Doorway;
    int layer = 0;
    QPointF p1;
    QPointF p2;
    QRectF tileRect;
    QString label;
};

class MapCanvas : public QWidget {
    Q_OBJECT
public:
    explicit MapCanvas(QWidget* parent = nullptr);
    ~MapCanvas() override = default;

    void setMap(std::shared_ptr<FPSCMap> map, bool preserveView = false);
    std::shared_ptr<FPSCMap> map() const { return m_map; }

    int currentFloor() const { return m_currentFloor; }
    int selectedEntityIndex() const { return m_selectedEntityIndex; }
    float zoom() const { return m_zoom; }
    QPointF panOffset() const { return m_panOffset; }
    void renderMap(QPainter& p);

    enum class GizmoHandle {
        None,
        CenterFree,
        AxisX,
        AxisZ
    };

public slots:
    void setFloor(int floor);
    void floorUp();
    void floorDown();

    void selectEntity(int index);
    void focusOnEntity(int index);
    void highlightCell(int layer, int x, int y);

    void zoomIn();
    void zoomOut();
    void zoomReset();
    void zoomFit();

    void setShowWallTextures(bool show);
    void setShowFloorTextures(bool show);
    void setShowGrid(bool show);
    void setShowEntities(bool show);
    void setShowLights(bool show);
    void setShowZones(bool show);
    void setShowWaypoints(bool show);
    void setShowGhostLayer(bool show);
    void setShowPortals(bool show);
    bool showPortals() const { return m_showPortals; }
    void setPortals(const std::vector<DBUPortal>& portals, const std::vector<DBUVisZone>& zones);

    void setActiveVisZone(int zoneId);
    int activeVisZone() const { return m_activeVisZoneId; }
    void setVisZoneCulling(bool enable, float dimOpacity = 0.0f);
    bool visZoneCulling() const { return m_cullInactiveVisZones; }
    float visZoneDimOpacity() const { return m_visZoneDimOpacity; }
    void setColorAllVisZones(bool enable);
    bool colorAllVisZones() const { return m_colorAllVisZones; }
    void setVisZoneManager(std::shared_ptr<VisZoneManager> mgr);
    std::shared_ptr<VisZoneManager> visZoneManager() const { return m_visZoneManager; }

signals:
    void floorChanged(int floor);
    void entitySelected(int index);
    void entityModified(int index);
    void entityDeleteRequested(int index);
    void hoverInfoChanged(const QString& info);
    void zoomChanged(float zoom);
    void visZoneSelected(int zoneId);
    void segmentInspectRequested(int layer, int x, int y);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    QPointF worldToScreen(const QPointF& worldPos) const;
    QPointF screenToWorld(const QPointF& screenPos) const;
    QRectF getCellRectScreen(int x, int y) const;

    void drawGrid(QPainter& p);
    void drawSegments(QPainter& p, int layer, float opacity = 1.0f);
    void drawWaypoints(QPainter& p);
    void drawZonesAndLights(QPainter& p);
    void drawEntities(QPainter& p);
    void drawCSGCutouts(QPainter& p);
    void drawPortals(QPainter& p);
    void drawGizmo(QPainter& p);
    void drawHUD(QPainter& p);

    GizmoHandle hitTestGizmo(const QPointF& screenPos) const;

    std::shared_ptr<FPSCMap> m_map;
    int m_currentFloor = 0;
    int m_selectedEntityIndex = -1;
    int m_hoveredEntityIndex = -1;
    bool m_isDraggingEntity = false;
    QPoint m_hoveredTile = {-1, -1};

    int m_highlightedLayer = -1;
    int m_highlightedX = -1;
    int m_highlightedY = -1;

    // Viewport logic
    GizmoHandle m_hoveredGizmo = GizmoHandle::None;
    GizmoHandle m_activeGizmo = GizmoHandle::None;
    QPointF m_dragStartMousePos;
    float m_dragStartEntX = 0.0f;
    float m_dragStartEntZ = 0.0f;

    float m_zoom = 1.0f;
    QPointF m_panOffset = {200.0f, 200.0f};

    bool m_isPanning = false;
    QPoint m_lastMousePos;

    bool m_showWallTextures = true;
    bool m_showFloorTextures = true;
    bool m_showGrid = true;
    bool m_showEntities = true;
    bool m_showLights = true;
    bool m_showZones = true;
    bool m_showWaypoints = true;
    bool m_showGhostLayer = true;
    bool m_showPortals = false;
    std::vector<DBUPortal> m_portals;
    std::vector<DBUVisZone> m_zones;

    std::shared_ptr<VisZoneManager> m_visZoneManager;
    int m_activeVisZoneId = -1; // -1 = Show All (normal)
    bool m_colorAllVisZones = false;
    bool m_cullInactiveVisZones = false;
    float m_visZoneDimOpacity = 0.0f; // 0.0f = completely hide, 0.15f = dimmed ghost

    QTimer m_animTimer;
    float m_animPhase = 0.0f;
};

#endif // MAPCANVAS_H
