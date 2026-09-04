#include "PortalLeakAnalyzer.h"
#include "AssetManager.h"
#include "VisZoneManager.h"
#include "FPMReader.h"
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QTextStream>
#include <QDebug>
#include <cmath>
#include <queue>

PortalLeakAnalyzer::PortalLeakAnalyzer(std::shared_ptr<FPSCMap> map)
    : m_map(map) {
}

DBUValidationResult PortalLeakAnalyzer::validateCompiledUniverse() const {
    DBUValidationResult res;
    if (!m_map) {
        res.message = QCoreApplication::translate("PortalLeakAnalyzer", "Map is not loaded.");
        return res;
    }

    QString dbuPath = AssetManager::instance().engineRoot() + "/Files/levelbank/testlevel/universe.dbu";
    QFileInfo dbuInfo(dbuPath);
    if (!dbuInfo.exists()) {
        res.fileExists = false;
        res.message = QCoreApplication::translate("PortalLeakAnalyzer", "universe.dbu file not found. Run Test Game (F9) in FPS Creator.");
        return res;
    }
    res.fileExists = true;
    res.dbuTime = dbuInfo.lastModified();

    // Check against map file timestamp if available
    if (!m_map->filePath.isEmpty()) {
        QFileInfo mapInfo(m_map->filePath);
        if (mapInfo.exists()) {
            res.mapTime = mapInfo.lastModified();
            if (res.mapTime > res.dbuTime.addSecs(2)) {
                res.isOutdated = true;
            }
        }
    }

    // Check against temp.fpm (the exact map passed to compiler)
    QString tempFpmPath = AssetManager::instance().engineRoot() + "/Files/editors/gridedit/temp.fpm";
    QFileInfo tempInfo(tempFpmPath);
    if (tempInfo.exists()) {
        auto tempMap = FPMReader::loadMap(tempFpmPath, "mypassword");
        if (tempMap) {
            bool match = (m_map->header.layerMax == tempMap->header.layerMax &&
                          m_map->header.maxX == tempMap->header.maxX &&
                          m_map->header.maxY == tempMap->header.maxY &&
                          m_map->placedEntities.size() == tempMap->placedEntities.size());
            
            if (match) {
                for (int l = 0; l <= m_map->header.layerMax && match; ++l) {
                    if (l >= (int)m_map->gridBlocks.size() || l >= (int)tempMap->gridBlocks.size()) continue;
                    for (int y = 0; y <= m_map->header.maxY && match; ++y) {
                        for (int x = 0; x <= m_map->header.maxX && match; ++x) {
                            if (m_map->gridBlocks[l][y][x] != tempMap->gridBlocks[l][y][x]) {
                                match = false;
                            }
                        }
                    }
                }
            }

            res.matchesCurrentMap = match;
            if (match) {
                if (res.isOutdated) {
                    res.message = QCoreApplication::translate("PortalLeakAnalyzer", "⚠ universe.dbu is outdated (map saved: %1, build: %2). Run Test Game (F9).")
                                  .arg(res.mapTime.toString("HH:mm:ss")).arg(res.dbuTime.toString("HH:mm:ss"));
                } else {
                    res.message = QCoreApplication::translate("PortalLeakAnalyzer", "✓ universe.dbu is up to date (built %1 for this map)").arg(res.dbuTime.toString("HH:mm:ss"));
                }
            } else {
                res.message = QCoreApplication::translate("PortalLeakAnalyzer", "⚠ universe.dbu is from ANOTHER map (built %1). Run Test Game (F9) for this map.")
                              .arg(res.dbuTime.toString("HH:mm:ss"));
            }
            return res;
        }
    }

    // Fallback if temp.fpm is absent
    res.matchesCurrentMap = !res.isOutdated;
    if (res.isOutdated) {
        res.message = QCoreApplication::translate("PortalLeakAnalyzer", "⚠ universe.dbu is outdated. Run Test Game (F9) in FPS Creator.");
    } else {
        res.message = QCoreApplication::translate("PortalLeakAnalyzer", "✓ universe.dbu found (%1)").arg(res.dbuTime.toString("HH:mm:ss"));
    }
    return res;
}

std::vector<PortalLeakWarning> PortalLeakAnalyzer::analyze() {
    m_warnings.clear();
    m_hasCompiledUniverse = false;
    if (!m_map) return m_warnings;

    // 1. Method 1: Check compiled universe.dbu (Primary ground-truth physics / BSP compiler)
    if (m_checkCompiledUniverse) {
        checkCompiledUniverse();
    }

    // 2. Method 2: Static map & segment geometry analysis
    if (m_checkStaticMap) {
        loadSegmentInfos();
        checkVerticalGaps();
        checkCoplanarOverlaps();
        checkWallHolesToVoid();
        checkInvertedWalls();
        checkDoubleWallClashes();
    }

    return m_warnings;
}

void PortalLeakAnalyzer::checkCompiledUniverse() {
    auto val = validateCompiledUniverse();
    if (!val.fileExists) return;

    if (!val.matchesCurrentMap) {
        PortalLeakWarning w;
        w.severity = PortalLeakWarning::WARNING;
        w.type = QStringLiteral("Compiled BSP Mismatch");
        w.layer = 0; w.x = 0; w.y = 0;
        w.description = val.message;
        m_warnings.push_back(w);
        return;
    }

    if (val.isOutdated) {
        PortalLeakWarning w;
        w.severity = PortalLeakWarning::WARNING;
        w.type = QStringLiteral("Compiled BSP Outdated");
        w.layer = 0; w.x = 0; w.y = 0;
        w.description = val.message;
        m_warnings.push_back(w);
    }

    QString dbuPath = AssetManager::instance().engineRoot() + "/Files/levelbank/testlevel/universe.dbu";
    if (!m_dbuParser.parse(dbuPath)) return;
    m_hasCompiledUniverse = true;

    QSet<quint64> reportedCells;

    for (size_t i = 0; i < m_dbuParser.allPortals().size(); ++i) {
        const auto& portal = m_dbuParser.allPortals()[i];
        int gx = portal.gridX();
        int gy = portal.gridY();
        int layer = qBound(0, portal.minLayer(), (int)m_map->gridBlocks.size() - 1);

        quint64 cellKey = (quint64(layer) << 32) | (quint64(gy) << 16) | quint64(gx);

        // Check for portals touching outer limits or marked as leak
        if (portal.isLeak || portal.targetZone >= m_dbuParser.zones().size()) {
            if (!reportedCells.contains(cellKey)) {
                reportedCells.insert(cellKey);
                PortalLeakWarning w;
                w.severity = PortalLeakWarning::ERROR;
                w.type = QStringLiteral("Universe Portal Leak (BSP)");
                w.layer = layer;
                w.x = gx;
                w.y = gy;
                w.description = QStringLiteral("Compiled BSP Portal %1 connects VisZone %2 to outside Universe Void at 3D pos (%3, %4, %5). Normal: (%6, %7, %8)")
                                .arg(i)
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
            
            if (hasViswall) {
                info.isSolidWall = true;
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

    // In FPS Creator engine, mapsymbol == 1 hides floor & roof!
    if (layer < m_map->gridSymbol.size() && y < m_map->gridSymbol[layer].size() && x < m_map->gridSymbol[layer][y].size()) {
        if (m_map->gridSymbol[layer][y][x] == 1) return false;
    }
    
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

    // In FPS Creator, ground == 2 is an auto-generated ceiling/roof slab
    if (layer < m_map->gridGround.size() && y < m_map->gridGround[layer].size() && x < m_map->gridGround[layer][y].size()) {
        if (m_map->gridGround[layer][y][x] == 2) return true;
    }

    return m_segmentInfoCache.value(segId - 1).isCeiling;
}

int PortalLeakAnalyzer::mapGround(int layer, int x, int y) const {
    if (!m_map) return 0;
    if (layer >= 0 && layer < m_map->gridGround.size() &&
        y >= 0 && y < m_map->gridGround[layer].size() &&
        x >= 0 && x < m_map->gridGround[layer][y].size()) {
        return m_map->gridGround[layer][y][x];
    }
    return 0;
}

void PortalLeakAnalyzer::checkVerticalGaps() {
    if (!m_map) return;
    VisZoneManager zm;
    zm.buildFromMap(m_map);

    // Sort zones by floor descending so we process upper room tiers first
    auto zones = zm.zones();
    std::sort(zones.begin(), zones.end(), [](const VisZone& a, const VisZone& b) {
        return a.floor > b.floor;
    });

    QSet<quint32> reportedCeilingColumns;

    for (const auto& z : zones) {
        if (z.tiles.size() < 2) continue;

        int floorCount = 0;
        int roofCount = 0;
        int wallTileCount = 0;

        for (const auto& pt : z.tiles) {
            int g = mapGround(z.floor, pt.x(), pt.y());
            if (g == 2) roofCount++;
            else floorCount++;

            int tile = (z.floor < m_map->gridTileType.size() && pt.y() < m_map->gridTileType[z.floor].size() && pt.x() < m_map->gridTileType[z.floor][pt.y()].size())
                       ? m_map->gridTileType[z.floor][pt.y()][pt.x()] : 0;
            if (tile > 0 && tile != 6) wallTileCount++;
        }

        // Must be predominantly floor tiles, NOT roof slabs (a roof slab layer cannot leak into itself)
        if (floorCount <= roofCount || floorCount < 2) continue;

        // An enclosed interior room must have walls (cannot be a 100% flat open slab with zero walls)
        if (wallTileCount == 0) continue;

        int coveredCount = 0;
        std::vector<QPoint> missingTiles;

        int zoneMaxCeilingFloor = -1;
        for (const auto& t : z.tiles) {
            for (int l = z.floor; l <= m_map->header.layerMax; ++l) {
                if (mapGround(l, t.x(), t.y()) == 2 || isCeilingAt(l, t.x(), t.y())) {
                    if (l > zoneMaxCeilingFloor) {
                        zoneMaxCeilingFloor = l;
                    }
                }
            }
        }

        auto isUniverseVoid = [&](int x, int y) -> bool {
            if (x < 0 || x > m_map->header.maxX || y < 0 || y > m_map->header.maxY) return true;
            for (int l = 0; l <= m_map->header.layerMax; ++l) {
                if (m_map->gridBlocks[l][y][x] > 0) return false;
            }
            return true;
        };

        const int dx[4] = {0, 1, 0, -1};
        const int dy[4] = {-1, 0, 1, 0};

        for (const auto& pt : z.tiles) {
            // Find top occupied layer of this column in the structure
            int colTop = z.floor;
            for (int l = z.floor; l <= m_map->header.layerMax; ++l) {
                if (m_map->gridBlocks[l][pt.y()][pt.x()] > 0) {
                    colTop = l;
                }
            }

            bool covered = false;
            for (int l = colTop; l <= m_map->header.layerMax; ++l) {
                int seg = m_map->gridBlocks[l][pt.y()][pt.x()];
                if (seg > 0) {
                    int g = mapGround(l, pt.x(), pt.y());
                    // Segment with roof or ceiling slab (ground == 2 or isCeilingAt)
                    if (g == 2 || isCeilingAt(l, pt.x(), pt.y())) {
                        covered = true;
                        break;
                    }
                    // A solid floor of another room directly on a higher floor also caps this column
                    if (l > colTop && isFloorAt(l, pt.x(), pt.y())) {
                        covered = true;
                        break;
                    }
                }
            }

            // If the zone has no ceiling slabs above colTop, then colTop is already at or above the room's roofline
            if (!covered && zoneMaxCeilingFloor > 0 && colTop >= zoneMaxCeilingFloor) {
                covered = true;
            }

            // Wall segments at the top of a column (perimeter and exterior walls) do not require ceiling slabs
            if (!covered) {
                int segId = m_map->gridBlocks[colTop][pt.y()][pt.x()];
                if (segId > 0 && m_map->segments.contains(segId)) {
                    const auto& seg = m_map->segments[segId];
                    if (seg->name.contains("wall", Qt::CaseInsensitive) ||
                        seg->relPath.contains("wall", Qt::CaseInsensitive)) {
                        covered = true;
                    }
                }
            }

            if (covered) {
                coveredCount++;
            } else {
                missingTiles.push_back(pt);
            }
        }

        float coverage = static_cast<float>(coveredCount) / static_cast<float>(z.tiles.size());

        // If room is predominantly roofed (> 75%), but has missing ceiling tiles -> TRUE CEILING LEAK
        if (coverage >= 0.75f && coverage < 1.0f) {
            for (const auto& hole : missingTiles) {
                quint32 colKey = (static_cast<quint32>(hole.y()) << 16) | (static_cast<quint32>(hole.x()) & 0xFFFF);
                if (reportedCeilingColumns.contains(colKey)) continue;
                reportedCeilingColumns.insert(colKey);

                // Find highest occupied room layer in this column
                int topRoomLayer = z.floor;
                for (int l = z.floor; l <= m_map->header.layerMax; ++l) {
                    if (m_map->gridBlocks[l][hole.y()][hole.x()] > 0) {
                        topRoomLayer = l;
                    }
                }
                int missingRoofFloor = topRoomLayer + 1;

                PortalLeakWarning w;
                w.severity = PortalLeakWarning::ERROR;
                w.type = "Missing Ceiling Leak";
                // Report on the layer where the ceiling tile is missing
                w.layer = (missingRoofFloor <= m_map->header.layerMax) ? missingRoofFloor : topRoomLayer;
                w.x = hole.x();
                w.y = hole.y();
                w.description = QString("Missing ceiling slab at Floor %1 over enclosed room at (%2, %3) (Room top: Floor %4). Camera will leak visibility into the void.")
                                    .arg(missingRoofFloor).arg(hole.x()).arg(hole.y()).arg(topRoomLayer);
                m_warnings.push_back(w);
            }
        }
    }
}

void PortalLeakAnalyzer::checkCoplanarOverlaps() {
    if (!m_map) return;

    // 1. Same-layer duplicate conflicts (Exact duplicate segment placed as both base block and overlay)
    for (int layer = 0; layer < m_map->gridBlocks.size(); ++layer) {
        for (int y = 0; y < m_map->gridBlocks[layer].size(); ++y) {
            for (int x = 0; x < m_map->gridBlocks[layer][y].size(); ++x) {
                int baseSeg = m_map->gridBlocks[layer][y][x];
                int olaySeg = (layer < m_map->gridOverlays.size() && y < m_map->gridOverlays[layer].size() && x < m_map->gridOverlays[layer][y].size())
                              ? m_map->gridOverlays[layer][y][x] : 0;
                if (baseSeg > 0 && olaySeg > 0 && baseSeg == olaySeg) {
                    auto s = m_map->segments.value(baseSeg);
                    if (s) {
                        PortalLeakWarning w;
                        w.severity = PortalLeakWarning::WARNING;
                        w.type = QStringLiteral("Duplicate Segment Z-Fighting");
                        w.layer = layer; w.x = x; w.y = y;
                        w.description = QString("Segment \"%1\" is placed as both base block and overlay on Floor %2 at (%3, %4). Duplicate identical meshes cause severe in-game flickering (Z-fighting).")
                                            .arg(s->name).arg(layer).arg(x).arg(y);
                        m_warnings.push_back(w);
                    }
                }
            }
        }
    }

    // 2. Inter-floor volume penetration (tall segments extending across floor boundaries)
    for (int layer = 0; layer < m_map->gridBlocks.size() - 1; ++layer) {
        for (int y = 0; y < m_map->gridBlocks[layer].size(); ++y) {
            for (int x = 0; x < m_map->gridBlocks[layer][y].size(); ++x) {
                int segBelow = m_map->gridBlocks[layer][y][x];
                int segAbove = m_map->gridBlocks[layer + 1][y][x];
                if (segBelow > 0 && segAbove > 0) {
                    auto sBelow = m_map->segments.value(segBelow);
                    auto sAbove = m_map->segments.value(segAbove);
                    if (sBelow && sAbove) {
                        // Check if segment below has parts extending upward into layer above (offY > 50.0f)
                        bool penetratesAbove = false;
                        for (const auto& p : sBelow->parts) {
                            if (p.offY > 50.0f) {
                                penetratesAbove = true;
                                break;
                            }
                        }
                        if (penetratesAbove) {
                            PortalLeakWarning w;
                            w.severity = PortalLeakWarning::WARNING;
                            w.type = QStringLiteral("Segment Height Collision");
                            w.layer = layer; w.x = x; w.y = y;
                            w.description = QString("Tall segment \"%1\" on Floor %2 physically penetrates into Floor %3, colliding with \"%4\".")
                                                .arg(sBelow->name).arg(layer).arg(layer + 1).arg(sAbove->name);
                            m_warnings.push_back(w);
                        }
                    }
                }
            }
        }
    }
}

void PortalLeakAnalyzer::checkWallHolesToVoid() {
    if (!m_map) return;
    VisZoneManager zm;
    zm.buildFromMap(m_map);

    int rows = m_map->header.maxY + 1;
    int cols = m_map->header.maxX + 1;

    auto isUniverseVoid = [&](int x, int y) -> bool {
        if (x < 0 || x >= cols || y < 0 || y >= rows) return true;
        for (int l = 0; l <= m_map->header.layerMax; ++l) {
            if (m_map->gridBlocks[l][y][x] > 0) return false;
        }
        return true;
    };

    auto isMaptileWallPresent = [](int maptile, int rot, int side) -> bool {
        if (maptile <= 0 || maptile == 6) return false;
        static const bool baseWalls[16][4] = {
            {0, 0, 0, 0}, {1, 1, 1, 1}, {1, 0, 1, 1}, {1, 0, 0, 1},
            {1, 0, 1, 0}, {1, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
            {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
            {1, 0, 0, 0}, {1, 0, 0, 0}, {1, 0, 0, 0}, {1, 0, 0, 1}
        };
        int unrotatedSide = (side - (rot & 3) + 4) % 4;
        if (maptile >= 0 && maptile < 16) return baseWalls[maptile][unrotatedSide];
        return false;
    };

    const int dx[4] = {0, 1, 0, -1};
    const int dy[4] = {-1, 0, 1, 0};
    const char* sideNames[4] = {"North", "East", "South", "West"};

    QSet<quint64> reportedWallLeaks;

    for (const auto& z : zm.zones()) {
        if (z.tiles.size() < 3) continue;

        // Only check enclosed interior rooms (ground <= 1)
        bool allInterior = true;
        for (const auto& pt : z.tiles) {
            if (mapGround(z.floor, pt.x(), pt.y()) > 1) {
                allInterior = false;
                break;
            }
        }
        if (!allInterior) continue;

        for (const auto& pair : z.floorTiles) {
            int fl = pair.first;
            for (const auto& pt : pair.second) {
                int tile = (fl < m_map->gridTileType.size() && pt.y() < m_map->gridTileType[fl].size() && pt.x() < m_map->gridTileType[fl][pt.y()].size())
                           ? m_map->gridTileType[fl][pt.y()][pt.x()] : 0;
                int rot = (fl < m_map->gridRotation.size() && pt.y() < m_map->gridRotation[fl].size() && pt.x() < m_map->gridRotation[fl][pt.y()].size())
                          ? m_map->gridRotation[fl][pt.y()][pt.x()] : 0;

                for (int s = 0; s < 4; ++s) {
                    int nx = pt.x() + dx[s];
                    int ny = pt.y() + dy[s];

                    // Only consider it an exterior breach if (nx, ny) is true universe void
                    // (i.e. outside the map bounds or has zero segments on ANY floor)
                    if (!isUniverseVoid(nx, ny)) continue;

                    // If wall is present on this side, no leak
                    if (isMaptileWallPresent(tile, rot, s)) continue;

                    // Check if segment definition has an explicit wall mesh part on this side
                    int segId = m_map->gridBlocks[fl][pt.y()][pt.x()];
                    if (segId > 0 && m_map->segments.contains(segId)) {
                        const auto& seg = m_map->segments[segId];
                        bool hasWallMesh = false;
                        for (const auto& part : seg->parts) {
                            if (part.isWall) {
                                int partSide = -1;
                                int rotYInt = (static_cast<int>(std::round(part.rotY)) % 360 + 360) % 360;
                                if (part.offX <= -25.0f || rotYInt == 270) partSide = 3;
                                else if (part.offX >= 25.0f || rotYInt == 90) partSide = 1;
                                else if (part.offZ >= 25.0f || rotYInt == 0) partSide = 0;
                                else if (part.offZ <= -25.0f || rotYInt == 180) partSide = 2;
                                if (partSide >= 0) {
                                    int effSide = (partSide + (rot & 3)) % 4;
                                    if (effSide == s) {
                                        hasWallMesh = true;
                                        break;
                                    }
                                }
                            }
                        }
                        if (hasWallMesh) continue;
                    }

                    // Check if there is a door entity placed on this edge
                    bool hasDoor = false;
                    for (const auto& p : zm.portals()) {
                        if (p.floor == fl && (p.tileA == pt || p.tileB == pt)) {
                            hasDoor = true;
                            break;
                        }
                    }
                    if (hasDoor) continue;

                    quint64 wallKey = (quint64(fl) << 36) | (quint64(pt.y()) << 20) | (quint64(pt.x()) << 4) | quint64(s);
                    if (reportedWallLeaks.contains(wallKey)) continue;
                    reportedWallLeaks.insert(wallKey);

                    PortalLeakWarning w;
                    w.severity = PortalLeakWarning::WARNING;
                    w.type = "Missing Perimeter Wall (Void Leak)";
                    w.layer = fl;
                    w.x = pt.x();
                    w.y = pt.y();
                    w.description = QString("Perimeter wall missing at Floor %1 (%2, %3) side %4 facing universe void. Camera may leak into void.")
                                        .arg(fl).arg(pt.x()).arg(pt.y()).arg(sideNames[s]);
                    m_warnings.push_back(w);
                }
            }
        }
    }
}

void PortalLeakAnalyzer::checkInvertedWalls() {
    if (!m_map) return;
    VisZoneManager zm;
    zm.buildFromMap(m_map);

    const int dx[4] = {0, 1, 0, -1};
    const int dy[4] = {-1, 0, 1, 0};
    const char* sideNames[4] = {"North", "East", "South", "West"};

    auto isMaptileWallPresent = [](int maptile, int rot, int side) -> bool {
        if (maptile <= 0 || maptile == 6) return false;
        static const bool baseWalls[16][4] = {
            {0, 0, 0, 0}, {1, 1, 1, 1}, {1, 0, 1, 1}, {1, 0, 0, 1},
            {1, 0, 1, 0}, {1, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
            {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
            {1, 0, 0, 0}, {1, 0, 0, 0}, {1, 0, 0, 0}, {1, 0, 0, 1}
        };
        int unrotatedSide = (side - (rot & 3) + 4) % 4;
        if (maptile >= 0 && maptile < 16) return baseWalls[maptile][unrotatedSide];
        return false;
    };

    auto isUniverseVoid = [&](int x, int y) -> bool {
        if (x < 0 || x > m_map->header.maxX || y < 0 || y > m_map->header.maxY) return true;
        for (int l = 0; l <= m_map->header.layerMax; ++l) {
            if (m_map->gridBlocks[l][y][x] > 0) return false;
        }
        return true;
    };

    QSet<quint32> reportedInverted;

    for (const auto& z : zm.zones()) {
        if (z.tiles.size() < 2) continue;
        // Room must be interior (ground <= 1)
        bool isInterior = false;
        for (const auto& pt : z.tiles) {
            if (mapGround(z.floor, pt.x(), pt.y()) <= 1) {
                isInterior = true;
                break;
            }
        }
        if (!isInterior) continue;

        for (const auto& pt : z.tiles) {
            int segId = m_map->gridBlocks[z.floor][pt.y()][pt.x()];
            if (segId <= 0 || !m_map->segments.contains(segId)) continue;
            const auto& s = m_map->segments[segId];

            int tile = (z.floor < m_map->gridTileType.size() && pt.y() < m_map->gridTileType[z.floor].size() && pt.x() < m_map->gridTileType[z.floor][pt.y()].size())
                       ? m_map->gridTileType[z.floor][pt.y()][pt.x()] : 0;
            int rot = (z.floor < m_map->gridRotation.size() && pt.y() < m_map->gridRotation[z.floor].size() && pt.x() < m_map->gridRotation[z.floor][pt.y()].size())
                      ? m_map->gridRotation[z.floor][pt.y()][pt.x()] : 0;
            int orient = (z.floor < m_map->gridOrientation.size() && pt.y() < m_map->gridOrientation[z.floor].size() && pt.x() < m_map->gridOrientation[z.floor][pt.y()].size())
                         ? m_map->gridOrientation[z.floor][pt.y()][pt.x()] : 0;

            quint32 key = (static_cast<quint32>(z.floor) << 24) | (static_cast<quint32>(pt.y()) << 12) | static_cast<quint32>(pt.x());
            if (reportedInverted.contains(key)) continue;

            // Check 1: Exterior segment placed inside an interior room
            if (s->groundMode == 3 && tile != 6) {
                reportedInverted.insert(key);
                PortalLeakWarning w;
                w.severity = PortalLeakWarning::WARNING;
                w.type = QStringLiteral("Exterior Wall in Interior Room");
                w.layer = z.floor;
                w.x = pt.x();
                w.y = pt.y();
                w.description = QString("Exterior segment \"%1\" (groundmode=3) is placed inside an interior room at Floor %2 (%3, %4). Exterior facade faces inward into the room.")
                                    .arg(s->name).arg(z.floor).arg(pt.x()).arg(pt.y());
                m_warnings.push_back(w);
                continue;
            }
        }
    }
}

void PortalLeakAnalyzer::checkDoubleWallClashes() {
    if (!m_map) return;
    int rows = m_map->header.maxY + 1;
    int cols = m_map->header.maxX + 1;

    auto isMaptileWallPresent = [](int maptile, int rot, int side) -> bool {
        if (maptile <= 0 || maptile == 6) return false;
        static const bool baseWalls[16][4] = {
            {0, 0, 0, 0}, {1, 1, 1, 1}, {1, 0, 1, 1}, {1, 0, 0, 1},
            {1, 0, 1, 0}, {1, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
            {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
            {1, 0, 0, 0}, {1, 0, 0, 0}, {1, 0, 0, 0}, {1, 0, 0, 1}
        };
        int unrotatedSide = (side - (rot & 3) + 4) % 4;
        if (maptile >= 0 && maptile < 16) return baseWalls[maptile][unrotatedSide];
        return false;
    };

    auto isDoorOrWindowEntityBetween = [&](int l, int x1, int y1, int x2, int y2) -> bool {
        float midX = (x1 + x2 + 1) * 50.0f;
        float midZ = -(y1 + y2 + 1) * 50.0f;
        for (const auto& e : m_map->placedEntities) {
            if (e.floorLayer == l) {
                if (std::abs(e.x - midX) < 60.0f && std::abs(e.z - midZ) < 60.0f) {
                    return true;
                }
            }
        }
        return false;
    };

    const int dx[4] = {0, 1, 0, -1};
    const int dy[4] = {-1, 0, 1, 0};
    const int opp[4] = {2, 3, 0, 1};
    const char* sideNames[4] = {"North", "East", "South", "West"};

    QSet<quint64> reportedClashes;

    for (int l = 0; l <= m_map->header.layerMax; ++l) {
        if (l >= m_map->gridBlocks.size()) continue;
        for (int y = 0; y < rows; ++y) {
            for (int x = 0; x < cols; ++x) {
                int segA = m_map->gridBlocks[l][y][x];
                if (segA <= 0 || mapGround(l, x, y) > 1) continue;
                int tileA = (l < m_map->gridTileType.size() && y < m_map->gridTileType[l].size() && x < m_map->gridTileType[l][y].size())
                            ? m_map->gridTileType[l][y][x] : 0;
                int rotA = (l < m_map->gridRotation.size() && y < m_map->gridRotation[l].size() && x < m_map->gridRotation[l][y].size())
                           ? m_map->gridRotation[l][y][x] : 0;

                // Check East (s=1) and South (s=2)
                for (int s = 1; s <= 2; ++s) {
                    int nx = x + dx[s];
                    int ny = y + dy[s];
                    if (nx < 0 || nx >= cols || ny < 0 || ny >= rows) continue;

                    int segB = m_map->gridBlocks[l][ny][nx];
                    if (segB <= 0 || segB == segA || mapGround(l, nx, ny) > 1) continue;
                    int tileB = (l < m_map->gridTileType.size() && ny < m_map->gridTileType[l].size() && nx < m_map->gridTileType[l][ny].size())
                                ? m_map->gridTileType[l][ny][nx] : 0;
                    int rotB = (l < m_map->gridRotation.size() && ny < m_map->gridRotation[l].size() && nx < m_map->gridRotation[l][ny].size())
                               ? m_map->gridRotation[l][ny][nx] : 0;

                    if (isMaptileWallPresent(tileA, rotA, s) && isMaptileWallPresent(tileB, rotB, opp[s])) {
                        // Check if from different segment packs / rooms
                        auto sA = m_map->segments.value(segA);
                        auto sB = m_map->segments.value(segB);
                        QString dirA = sA ? QFileInfo(sA->relPath).path().replace("\\", "/").toLower() : "";
                        QString dirB = sB ? QFileInfo(sB->relPath).path().replace("\\", "/").toLower() : "";

                        // If different segment packs and no door/window entity between them
                        if (dirA != dirB && !isDoorOrWindowEntityBetween(l, x, y, nx, ny)) {
                            quint64 clashKey = (quint64(l) << 40) | (quint64(y) << 28) | (quint64(x) << 16) | (quint64(ny) << 8) | quint64(nx);
                            if (reportedClashes.contains(clashKey)) continue;
                            reportedClashes.insert(clashKey);

                            QString nameA = sA ? sA->name : QString::number(segA);
                            QString nameB = sB ? sB->name : QString::number(segB);

                            PortalLeakWarning w;
                            w.severity = PortalLeakWarning::WARNING;
                            w.type = QStringLiteral("Double-Wall Boundary Clash");
                            w.layer = l;
                            w.x = x;
                            w.y = y;
                            w.description = QString("Both adjacent rooms (\"%1\" and \"%2\") place solid walls on the exact same shared boundary at Floor %3 between (%4, %5) and (%6, %7) without a doorway. Causes severe in-game Z-fighting flickering and degenerate BSP portal bleed.")
                                                .arg(nameA).arg(nameB).arg(l).arg(x).arg(y).arg(nx).arg(ny);
                            m_warnings.push_back(w);
                        }
                    }
                }
            }
        }
    }
}
