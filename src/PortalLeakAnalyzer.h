#pragma once

#include <vector>
#include <QString>
#include <memory>
#include "FPSCData.h"
#include "UniverseDBUParser.h"
#include <QHash>

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
};

class PortalLeakAnalyzer {
public:
    PortalLeakAnalyzer(std::shared_ptr<FPSCMap> map);

    std::vector<PortalLeakWarning> analyze();

    bool hasCompiledUniverse() const { return m_hasCompiledUniverse; }
    const std::vector<DBUPortal>& allPortals() const { return m_dbuParser.allPortals(); }
    const std::vector<DBUVisZone>& allZones() const { return m_dbuParser.zones(); }

private:
    void checkCompiledUniverse();
    void checkVerticalGaps();
    void checkCoplanarOverlaps();
    void checkCornerGaps();
    void checkGroundModeMismatches();

    std::shared_ptr<FPSCMap> m_map;
    std::vector<PortalLeakWarning> m_warnings;
    
    UniverseDBUParser m_dbuParser;
    bool m_hasCompiledUniverse = false;

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
};
