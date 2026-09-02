#include "PortalLeakAnalyzer.h"
#include "AssetManager.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>

PortalLeakAnalyzer::PortalLeakAnalyzer(std::shared_ptr<FPSCMap> map)
    : m_map(map)
{
}

std::vector<PortalLeakWarning> PortalLeakAnalyzer::analyze() {
    m_warnings.clear();
    if (!m_map) return m_warnings;

    loadSegmentInfos();

    checkVisportalmodes();
    checkVerticalGaps();
    checkCoplanarOverlaps();
    checkCornerGaps();

    return m_warnings;
}

void PortalLeakAnalyzer::loadSegmentInfos() {
    m_segmentInfoCache.clear();
    
    for (int i = 0; i < m_map->segmentsBank.size(); ++i) {
        QString fpsPath = m_map->segmentsBank[i];
        if (fpsPath.isEmpty()) continue;
        
        QString fullPath = AssetManager::instance().resolvePath(fpsPath);
        if (fullPath.isEmpty()) {
            fullPath = AssetManager::instance().engineRoot() + "/Files/segments/" + fpsPath;
            fullPath.replace("\\", "/");
        }
        
        QFile file(fullPath);
        SegmentInfo info = {false, 0, false, false, false};
        
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            bool hasViswallb = false;
            bool hasViswallf = false;
            while (!in.atEnd()) {
                QString line = in.readLine().trimmed().toLower();
                if (line.startsWith(";")) continue;
                
                QStringList parts = line.split("=");
                if (parts.size() >= 2) {
                    QString key = parts[0].trimmed();
                    QString val = parts[1].trimmed();
                    
                    if (key == "visportalmode") {
                        info.hasVisportalmode = true;
                        info.visportalmode = val.toInt();
                    } else if (key == "visfloor" && val != "-1") {
                        info.isFloor = true;
                    } else if (key == "visroof" && val != "-1") {
                        info.isCeiling = true;
                    } else if (key == "viswallb" && val != "-1") {
                        hasViswallb = true;
                    } else if (key == "viswallf" && val != "-1") {
                        hasViswallf = true;
                    }
                }
            }
            file.close();
            
            // Heuristic for solid wall
            if (hasViswallb || hasViswallf || fpsPath.toLower().contains("wall") || fpsPath.toLower().contains("solid")) {
                info.isSolidWall = true;
            }
            
            // Groundmode fallback for floor
            if (fpsPath.toLower().contains("floor") || fpsPath.toLower().contains("ground")) {
                info.isFloor = true;
            }
        } else {
            printf("Failed to open FPS file: %s\n", qPrintable(fullPath));
        }
        m_segmentInfoCache[i] = info;
        printf("Parsed Seg %d %s Wall: %d Floor: %d Ceiling: %d VP: %d %d\n",
               i+1, qPrintable(fpsPath), info.isSolidWall, info.isFloor, info.isCeiling, info.hasVisportalmode, info.visportalmode);
    }
}

bool PortalLeakAnalyzer::isWallAt(int layer, int x, int y) {
    if (layer < 0 || layer >= m_map->gridBlocks.size()) return false;
    if (x < 0 || x >= m_map->gridBlocks[layer].size()) return false;
    if (y < 0 || y >= m_map->gridBlocks[layer][x].size()) return false;
    int segId = m_map->gridBlocks[layer][x][y];
    if (segId <= 0 || segId > m_map->segmentsBank.size()) return false;
    return m_segmentInfoCache[segId - 1].isSolidWall;
}

bool PortalLeakAnalyzer::isFloorAt(int layer, int x, int y) {
    if (layer < 0 || layer >= m_map->gridBlocks.size()) return false;
    if (x < 0 || x >= m_map->gridBlocks[layer].size()) return false;
    if (y < 0 || y >= m_map->gridBlocks[layer][x].size()) return false;
    int segId = m_map->gridBlocks[layer][x][y];
    if (segId <= 0 || segId > m_map->segmentsBank.size()) return false;
    return m_segmentInfoCache[segId - 1].isFloor;
}

bool PortalLeakAnalyzer::isCeilingAt(int layer, int x, int y) {
    if (layer < 0 || layer >= m_map->gridBlocks.size()) return false;
    if (x < 0 || x >= m_map->gridBlocks[layer].size()) return false;
    if (y < 0 || y >= m_map->gridBlocks[layer][x].size()) return false;
    int segId = m_map->gridBlocks[layer][x][y];
    if (segId <= 0 || segId > m_map->segmentsBank.size()) return false;
    return m_segmentInfoCache[segId - 1].isCeiling;
}

void PortalLeakAnalyzer::checkVisportalmodes() {
    for (int layer = 0; layer < m_map->gridBlocks.size(); ++layer) {
        for (int x = 0; x < m_map->gridBlocks[layer].size(); ++x) {
            for (int y = 0; y < m_map->gridBlocks[layer][x].size(); ++y) {
                int segId = m_map->gridBlocks[layer][x][y];
                if (segId > 0 && segId <= m_map->segmentsBank.size()) {
                    const SegmentInfo& info = m_segmentInfoCache[segId - 1];
                    if (info.isSolidWall && (!info.hasVisportalmode || info.visportalmode == 0 || info.visportalmode == 2)) {
                        PortalLeakWarning w;
                        w.severity = PortalLeakWarning::WARNING;
                        w.type = "Missing visportalmode";
                        w.layer = layer; w.x = x; w.y = y;
                        w.description = QString("Solid wall segment '%1' is missing visportalmode=1. Auto-CSG might mark this as open.").arg(m_map->segmentsBank[segId - 1]);
                        m_warnings.push_back(w);
                    }
                }
            }
        }
    }
}


void PortalLeakAnalyzer::checkVerticalGaps() {
    for (int layer = 0; layer < m_map->gridBlocks.size() - 1; ++layer) {
        for (int x = 0; x < m_map->gridBlocks[layer].size(); ++x) {
            for (int y = 0; y < m_map->gridBlocks[layer][x].size(); ++y) {
                // If there's an enclosed room here on 'layer' but 'layer+1' is empty, we need a ceiling
                if (isFloorAt(layer, x, y) || isWallAt(layer, x, y)) {
                    bool ceilingHere = isCeilingAt(layer, x, y);
                    bool floorAbove = isFloorAt(layer + 1, x, y);
                    bool ceilingAbove = isCeilingAt(layer + 1, x, y);
                    
                    if (!ceilingHere && !floorAbove && !ceilingAbove) {
                        // Check if it's open to the void above
                        bool wallAbove = isWallAt(layer + 1, x, y);
                        if (!wallAbove) {
                            PortalLeakWarning w;
                            w.severity = PortalLeakWarning::ERROR;
                            w.type = "Vertical Gap Leak";
                            w.layer = layer; w.x = x; w.y = y;
                            w.description = "Missing ceiling/floor between layers. Camera can look up and leak into the void.";
                            m_warnings.push_back(w);
                            printf("Found Vertical Gap at %d %d %d\n", layer, x, y);
                        }
                    }
                }
            }
        }
    }
}

void PortalLeakAnalyzer::checkCoplanarOverlaps() {
    for (int layer = 0; layer < m_map->gridBlocks.size() - 1; ++layer) {
        for (int x = 0; x < m_map->gridBlocks[layer].size(); ++x) {
            for (int y = 0; y < m_map->gridBlocks[layer][x].size(); ++y) {
                bool ceilingHere = isCeilingAt(layer, x, y);
                bool floorAbove = isFloorAt(layer + 1, x, y);
                if (ceilingHere && floorAbove) {
                    PortalLeakWarning w;
                    w.severity = PortalLeakWarning::ERROR;
                    w.type = "Coplanar Z-Fighting";
                    w.layer = layer; w.x = x; w.y = y;
                    w.description = "Ceiling on layer N and floor on layer N+1 are on the same cell, causing degenerate portal geometry.";
                    m_warnings.push_back(w);
                }
                
                // Check map.fpmo (overlays) removed as it's not in parsed map
            }
        }
    }
}

void PortalLeakAnalyzer::checkCornerGaps() {
    for (int layer = 0; layer < m_map->gridBlocks.size(); ++layer) {
        for (int x = 0; x < m_map->gridBlocks[layer].size() - 1; ++x) {
            for (int y = 0; y < m_map->gridBlocks[layer][x].size() - 1; ++y) {
                // Miter gap detection (simplified): 
                // A diagonal connection of walls where the interior corner is missing.
                bool tl = isWallAt(layer, x, y);
                bool tr = isWallAt(layer, x+1, y);
                bool bl = isWallAt(layer, x, y+1);
                bool br = isWallAt(layer, x+1, y+1);
                
                if ((tl && br && !tr && !bl) || (!tl && !br && tr && bl)) {
                    PortalLeakWarning w;
                    w.severity = PortalLeakWarning::ERROR;
                    w.type = "Corner Miter Gap";
                    w.layer = layer; w.x = x; w.y = y;
                    w.description = "Diagonal walls meeting at a corner without an overlapping post block. Generates portal micro-fissures.";
                    m_warnings.push_back(w);
                }
            }
        }
    }
}
