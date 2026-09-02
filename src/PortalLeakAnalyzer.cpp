#include "PortalLeakAnalyzer.h"
#include "AssetManager.h"
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QDebug>
#include <cmath>
#include <queue>

PortalLeakAnalyzer::PortalLeakAnalyzer(std::shared_ptr<FPSCMap> map)
    : m_map(map) {
}

std::vector<PortalLeakWarning> PortalLeakAnalyzer::analyze() {
    m_warnings.clear();
    m_hasCompiledUniverse = false;
    if (!m_map) return m_warnings;

    // 1. Check if compiled universe.dbu is present (Primary ground-truth geometry)
    checkCompiledUniverse();

    // 2. Load segment properties for fallback / pre-build static checking
    loadSegmentInfos();
    checkVerticalGaps();
    checkCoplanarOverlaps();
    checkCornerGaps();

    return m_warnings;
}

void PortalLeakAnalyzer::checkCompiledUniverse() {
    QString dbuPath = AssetManager::instance().engineRoot() + "/Files/levelbank/testlevel/universe.dbu";
    if (QFile::exists(dbuPath)) {
        if (m_dbuParser.parse(dbuPath)) {
            m_hasCompiledUniverse = true;
            for (const auto& portal : m_dbuParser.leakingPortals()) {
                PortalLeakWarning w;
                w.severity = PortalLeakWarning::ERROR;
                w.type = "Universe Portal Leak (BSP)";
                w.layer = std::max(0, std::min(19, portal.minLayer()));
                w.x = std::max(0, std::min(39, portal.gridX()));
                w.y = std::max(0, std::min(39, portal.gridY()));
                w.description = QString("Compiled BSP Portal connects VisZone %1 to outside Universe Void at 3D pos (%2, %3, %4). Normal: (%5, %6, %7)")
                                .arg(portal.fromZone)
                                .arg(portal.box.cenX, 0, 'f', 0)
                                .arg(portal.box.cenY, 0, 'f', 0)
                                .arg(portal.box.cenZ, 0, 'f', 0)
                                .arg(portal.normal.x, 0, 'f', 1)
                                .arg(portal.normal.y, 0, 'f', 1)
                                .arg(portal.normal.z, 0, 'f', 1);
                m_warnings.push_back(w);
            }
        }
    }
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
            bool hasViswall = false;
            while (!in.atEnd()) {
                QString line = in.readLine().trimmed().toLower();
                if (line.startsWith("visportalmode")) {
                    info.hasVisportalmode = true;
                    info.visportalmode = line.section('=', 1).trimmed().toInt();
                } else if (line.startsWith("viswall")) {
                    hasViswall = true;
                } else if (line.startsWith("visfloor")) {
                    int val = line.section('=', 1).trimmed().toInt();
                    if (val >= 0) info.isFloor = true;
                } else if (line.startsWith("visroof")) {
                    int val = line.section('=', 1).trimmed().toInt();
                    if (val >= 0) info.isCeiling = true;
                }
            }
            file.close();
            
            if (hasViswall || fpsPath.toLower().contains("wall") || fpsPath.toLower().contains("solid")) {
                info.isSolidWall = true;
            }
            if (fpsPath.toLower().contains("floor") || fpsPath.toLower().contains("ground")) {
                info.isFloor = true;
            }
            if (fpsPath.toLower().contains("ceiling") || fpsPath.toLower().contains("roof")) {
                info.isCeiling = true;
            }
        }
        m_segmentInfoCache[i] = info;
    }
}

bool PortalLeakAnalyzer::isWallAt(int layer, int x, int y) {
    if (!m_map) return false;
    if (layer < 0 || layer >= m_map->gridBlocks.size()) return false;
    if (y < 0 || y >= m_map->gridBlocks[layer].size()) return false;
    if (x < 0 || x >= m_map->gridBlocks[layer][y].size()) return false;
    
    int segId = m_map->gridBlocks[layer][y][x];
    if (segId <= 0 || segId > m_map->segmentsBank.size()) return false;
    return m_segmentInfoCache.value(segId - 1).isSolidWall;
}

bool PortalLeakAnalyzer::isFloorAt(int layer, int x, int y) {
    if (!m_map) return false;
    if (layer < 0 || layer >= m_map->gridBlocks.size()) return false;
    if (y < 0 || y >= m_map->gridBlocks[layer].size()) return false;
    if (x < 0 || x >= m_map->gridBlocks[layer][y].size()) return false;
    
    int segId = m_map->gridBlocks[layer][y][x];
    if (segId <= 0 || segId > m_map->segmentsBank.size()) return false;
    return m_segmentInfoCache.value(segId - 1).isFloor;
}

bool PortalLeakAnalyzer::isCeilingAt(int layer, int x, int y) {
    if (!m_map) return false;
    if (layer < 0 || layer >= m_map->gridBlocks.size()) return false;
    if (y < 0 || y >= m_map->gridBlocks[layer].size()) return false;
    if (x < 0 || x >= m_map->gridBlocks[layer][y].size()) return false;
    
    int segId = m_map->gridBlocks[layer][y][x];
    if (segId <= 0 || segId > m_map->segmentsBank.size()) return false;
    return m_segmentInfoCache.value(segId - 1).isCeiling;
}

void PortalLeakAnalyzer::checkVerticalGaps() {
    int layers = m_map->gridBlocks.size();
    if (layers == 0) return;
    int rows = m_map->gridBlocks[0].size();
    if (rows == 0) return;
    int cols = m_map->gridBlocks[0][0].size();

    // Check connected interior floor clusters per layer
    for (int baseLayer = 0; baseLayer < layers; ++baseLayer) {
        std::vector<std::vector<bool>> visited(rows, std::vector<bool>(cols, false));

        for (int y = 0; y < rows; ++y) {
            for (int x = 0; x < cols; ++x) {
                int seg = m_map->gridBlocks[baseLayer][y][x];
                if (seg <= 0 || visited[y][x]) continue;

                // Flood-fill cluster
                std::vector<QPoint> cluster;
                std::queue<QPoint> q;
                q.push(QPoint(x, y));
                visited[y][x] = true;

                while (!q.empty()) {
                    QPoint pt = q.front();
                    q.pop();
                    cluster.push_back(pt);

                    const int dx[4] = {1, -1, 0, 0};
                    const int dy[4] = {0, 0, 1, -1};
                    for (int d = 0; d < 4; ++d) {
                        int nx = pt.x() + dx[d];
                        int ny = pt.y() + dy[d];
                        if (nx >= 0 && nx < cols && ny >= 0 && ny < rows) {
                            if (!visited[ny][nx] && m_map->gridBlocks[baseLayer][ny][nx] > 0) {
                                visited[ny][nx] = true;
                                q.push(QPoint(nx, ny));
                            }
                        }
                    }
                }

                if (cluster.size() < 2) continue;

                // Check ceiling coverage for this room cluster
                int roofedCount = 0;
                std::vector<QPoint> unroofedTiles;
                int maxRoofLayer = baseLayer;

                for (const auto& pt : cluster) {
                    bool hasRoof = false;
                    for (int l = baseLayer + 1; l < layers; ++l) {
                        if (isCeilingAt(l, pt.x(), pt.y())) {
                            hasRoof = true;
                            maxRoofLayer = std::max(maxRoofLayer, l);
                            break;
                        }
                    }
                    if (hasRoof) {
                        roofedCount++;
                    } else {
                        unroofedTiles.push_back(pt);
                    }
                }

                float coverage = static_cast<float>(roofedCount) / static_cast<float>(cluster.size());

                // If room is predominantly roofed (> 50%), but has missing ceiling tiles -> TRUE DEFECT LEAK
                if (coverage >= 0.5f && coverage < 1.0f) {
                    for (const auto& hole : unroofedTiles) {
                        bool alreadyFound = false;
                        for (const auto& w : m_warnings) {
                            if (w.layer == maxRoofLayer && w.x == hole.x() && w.y == hole.y()) {
                                alreadyFound = true;
                                break;
                            }
                        }
                        if (!alreadyFound) {
                            PortalLeakWarning w;
                            w.severity = PortalLeakWarning::ERROR;
                            w.type = "Vertical Gap Leak";
                            w.layer = maxRoofLayer; 
                            w.x = hole.x(); 
                            w.y = hole.y();
                            w.description = QString("Missing ceiling tile at (%1, %2) in an enclosed roofed room (Layer %3). Camera can see into the void.")
                                                .arg(hole.x()).arg(hole.y()).arg(maxRoofLayer);
                            m_warnings.push_back(w);
                        }
                    }
                }
            }
        }
    }
}

void PortalLeakAnalyzer::checkCoplanarOverlaps() {
    for (int layer = 0; layer < m_map->gridBlocks.size() - 1; ++layer) {
        for (int y = 0; y < m_map->gridBlocks[layer].size(); ++y) {
            for (int x = 0; x < m_map->gridBlocks[layer][y].size(); ++x) {
                int segBelow = m_map->gridBlocks[layer][y][x];
                int segAbove = m_map->gridBlocks[layer + 1][y][x];
                if (segBelow > 0 && segAbove > 0 && segBelow != segAbove) {
                    bool ceilingHere = isCeilingAt(layer, x, y);
                    bool floorAbove = isFloorAt(layer + 1, x, y);
                    if (ceilingHere && floorAbove) {
                        PortalLeakWarning w;
                        w.severity = PortalLeakWarning::WARNING;
                        w.type = "Coplanar CSG Overlap";
                        w.layer = layer; w.x = x; w.y = y;
                        w.description = QString("Ceiling on Layer %1 shares exact height plane with Floor on Layer %2. May cause degenerate BSP portal recursion.").arg(layer).arg(layer + 1);
                        m_warnings.push_back(w);
                    }
                }
            }
        }
    }
}

void PortalLeakAnalyzer::checkCornerGaps() {
    for (int layer = 0; layer < m_map->gridBlocks.size(); ++layer) {
        for (int y = 0; y < m_map->gridBlocks[layer].size() - 1; ++y) {
            for (int x = 0; x < m_map->gridBlocks[layer][y].size() - 1; ++x) {
                bool tl = isWallAt(layer, x, y);
                bool tr = isWallAt(layer, x+1, y);
                bool bl = isWallAt(layer, x, y+1);
                bool br = isWallAt(layer, x+1, y+1);
                
                if ((tl && br && !tr && !bl) || (!tl && !br && tr && bl)) {
                    PortalLeakWarning w;
                    w.severity = PortalLeakWarning::WARNING;
                    w.type = "Corner Miter Gap";
                    w.layer = layer; w.x = x; w.y = y;
                    w.description = "Diagonal wall intersection without corner post. Looking at the seam may leak visibility into the void.";
                    m_warnings.push_back(w);
                }
            }
        }
    }
}

