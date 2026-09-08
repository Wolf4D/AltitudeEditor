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

enum class PortalType {
    InterZoneDoorway,
    InterZoneWindow,
    ExteriorDoorway,
    ExteriorWindow
};

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
    bool isExterior = false;
    PortalType type = PortalType::InterZoneDoorway;
};

#include <map>

struct VisZone {
    int id = 0;
    QString name;
    int floor = 0;    // Primary (lowest) floor
    int minFloor = 0;
    int maxFloor = 0;
    QRect bounds; // Grid bounding box (minX, minY, width, height)
    std::vector<QPoint> tiles; // Horizontal footprint
    std::map<int, std::vector<QPoint>> floorTiles; // Tiles grouped by floor
    std::vector<int> entityIndices;
    std::vector<int> portalIndices;
    QColor color;

    bool hasFloor(int f) const {
        return floorTiles.find(f) != floorTiles.end();
    }

    const std::vector<QPoint>& getTilesOnFloor(int f) const {
        static const std::vector<QPoint> s_empty;
        auto it = floorTiles.find(f);
        if (it != floorTiles.end()) return it->second;
        return s_empty;
    }
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

    void recolorAllZones(int hueOffset = 0);

    bool isMaptileWallPresent(int l, int x, int y, int side) const;
    bool hasDoorwayOnEdge(int l, int x1, int y1, int x2, int y2, int sideFrom1) const;
    bool isDoorOrWindowOnEdge(int l, int x1, int y1, int x2, int y2, int sideFrom1, bool* isWindowOut = nullptr) const;

private:
    std::shared_ptr<FPSCMap> m_map;
    std::vector<VisZone> m_zones;
    std::vector<MapPortal> m_portals;

    // Fast lookup: [layer][y][x] -> zoneId (-1 if none)
    std::vector<std::vector<std::vector<int>>> m_tileZoneMap;

    void partitionRooms();
    void associateEntities();
    void buildPortals();
    void pruneOpenRoofZones();
};
