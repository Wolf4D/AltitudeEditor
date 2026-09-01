#ifndef MAPCANVAS_H
#define MAPCANVAS_H

#include "FPSCData.h"
#include <QWidget>
#include <QPointF>
#include <QTimer>
#include <memory>

class MapCanvas : public QWidget {
    Q_OBJECT
public:
    explicit MapCanvas(QWidget* parent = nullptr);
    ~MapCanvas() override = default;

    void setMap(std::shared_ptr<FPSCMap> map);
    std::shared_ptr<FPSCMap> map() const { return m_map; }

    int currentFloor() const { return m_currentFloor; }
    int selectedEntityIndex() const { return m_selectedEntityIndex; }

    void renderMap(QPainter& p);

public slots:
    void setFloor(int floor);
    void floorUp();
    void floorDown();

    void selectEntity(int index);
    void focusOnEntity(int index);

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

signals:
    void floorChanged(int floor);
    void entitySelected(int index);
    void hoverInfoChanged(const QString& info);
    void zoomChanged(float zoom);

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
    void drawHUD(QPainter& p);

    std::shared_ptr<FPSCMap> m_map;
    int m_currentFloor = 0;
    int m_selectedEntityIndex = -1;
    int m_hoveredEntityIndex = -1;
    QPoint m_hoveredTile = {-1, -1};

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

    QTimer m_animTimer;
    float m_animPhase = 0.0f;
};

#endif // MAPCANVAS_H
