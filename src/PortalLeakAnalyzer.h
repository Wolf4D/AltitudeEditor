#pragma once

#include <vector>
#include <QString>
#include <memory>
#include "FPSCData.h"
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

private:
    void checkVisportalmodes();
    void checkVerticalGaps();
    void checkCoplanarOverlaps();
    void checkCornerGaps();

    std::shared_ptr<FPSCMap> m_map;
    std::vector<PortalLeakWarning> m_warnings;
    
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
