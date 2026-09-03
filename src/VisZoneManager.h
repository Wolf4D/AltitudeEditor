#pragma once

#include <QString>
#include <QVector>
#include <QPoint>
#include <QRect>
#include <QColor>
#include <memory>
#include <vector>
#include "FPSCData.h"
#include "UniverseDBUParser.h"

struct MapPortal {
    int id = 0;
    int floor = 0;
    QPoint tileA;
    QPoint tileB;
    int zoneA = -1;
    int zoneB = -1;
    QLineF lineWorld;  // 2D world line (X, -Z)
    QString name;
    bool isVertical = false;
};

struct VisZone {
    int id = 0;
    QString name;
    int floor = 0;
    QRect bounds; // Grid bounding box (minX, minY, width, height)
    std::vector<QPoint> tiles;
    std::vector<int> entityIndices;
    std::vector<int> portalIndices;
    QColor color;
};

class VisZoneManager {
public:
    VisZoneManager() = default;

    void buildFromMap(std::shared_ptr<FPSCMap> map, const QString& dbuPath = QString());

    const std::vector<VisZone>& zones() const { return m_zones; }
    const std::vector<MapPortal>& portals() const { return m_portals; }

    const VisZone* getZone(int id) const;
    const MapPortal* getPortal(int id) const;

    std::vector<int> getZonesOnFloor(int floor) const;
    int getZoneAt(int floor, int x, int y) const;

    bool isTileInZone(int zoneId, int floor, int x, int y) const;
    bool isEntityInZone(int zoneId, int entityIndex) const;

private:
    std::shared_ptr<FPSCMap> m_map;
    std::vector<VisZone> m_zones;
    std::vector<MapPortal> m_portals;

    // Fast lookup: [layer][y][x] -> zoneId (-1 if none)
    std::vector<std::vector<std::vector<int>>> m_tileZoneMap;

    void partitionRooms();
    void associateEntities();
    void buildPortals();
    bool isMaptileWallPresent(int l, int x, int y, int side) const;
    bool hasDoorwayOnEdge(int l, int x1, int y1, int x2, int y2, int sideFrom1) const;

    struct DoorPos {
        float x;
        float y;
    };
    std::vector<std::vector<DoorPos>> m_floorDoors;
};
