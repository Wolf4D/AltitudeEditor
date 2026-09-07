#pragma once

#include <vector>
#include <QString>
#include <memory>
#include "FPSCData.h"
#include "UniverseDBUParser.h"
#include <QHash>
#include <QDateTime>

class VisZoneManager;

struct PortalLeakWarning {
    enum Severity {
        WARNING,
        ERROR
    };

    Severity severity;
    QString type;
    QString description;
    int layer;
    int x;
    int y;
    int zoneId = -1;
    int zoneId2 = -1;
    QString zoneName;
    int x2 = -1;
    int y2 = -1;
    bool isClash = false;
    int sideA = -1; // 0=N, 1=E, 2=S, 3=W
};

struct DBUValidationResult {
    bool fileExists = false;
    bool matchesCurrentMap = false;
    bool isOutdated = false;
    QDateTime dbuTime;
    QDateTime mapTime;
    QString message;
};

class PortalLeakAnalyzer {
public:
    PortalLeakAnalyzer(std::shared_ptr<FPSCMap> map, std::shared_ptr<VisZoneManager> visZoneManager = nullptr);

    std::vector<PortalLeakWarning> analyze();

    DBUValidationResult validateCompiledUniverse() const;

    void setVisZoneManager(std::shared_ptr<VisZoneManager> mgr) { m_visZoneManager = mgr; }
    std::shared_ptr<VisZoneManager> visZoneManager() const { return m_visZoneManager; }

    void setCheckCompiledUniverse(bool enable) { m_checkCompiledUniverse = enable; }
    void setCheckStaticMap(bool enable) { m_checkStaticMap = enable; }
    bool checkCompiledUniverseEnabled() const { return m_checkCompiledUniverse; }
    bool checkStaticMapEnabled() const { return m_checkStaticMap; }

    bool hasCompiledUniverse() const { return m_hasCompiledUniverse; }
    const std::vector<DBUPortal>& allPortals() const { return m_dbuParser.allPortals(); }
    const std::vector<DBUVisZone>& allZones() const { return m_dbuParser.zones(); }

private:
    void checkCompiledUniverse();
    void checkVerticalGaps();
    void checkCoplanarOverlaps();
    void checkWallHolesToVoid();
    void checkInvertedWalls();
    void checkDoubleWallClashes();

    std::shared_ptr<FPSCMap> m_map;
    std::shared_ptr<VisZoneManager> m_visZoneManager;
    std::vector<PortalLeakWarning> m_warnings;
    
    UniverseDBUParser m_dbuParser;
    bool m_hasCompiledUniverse = false;
    bool m_checkCompiledUniverse = true;
    bool m_checkStaticMap = true;

    struct SegmentInfo {
        bool hasVisportalmode;
        int visportalmode;
        bool isSolidWall;
        bool isFloor;
        bool isCeiling;
    };
    
    QHash<int, SegmentInfo> m_segmentInfoCache;
    
    void loadSegmentInfos();
    bool isWallAt(int layer, int x, int y);
    bool isFloorAt(int layer, int x, int y);
    bool isCeilingAt(int layer, int x, int y);
    int mapGround(int layer, int x, int y) const;
};
