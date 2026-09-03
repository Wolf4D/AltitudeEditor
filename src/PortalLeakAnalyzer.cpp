#include "PortalLeakAnalyzer.h"
#include "AssetManager.h"
#include "VisZoneManager.h"
#include "FPMReader.h"
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
        res.message = QStringLiteral("Карта не загружена.");
        return res;
    }

    QString dbuPath = AssetManager::instance().engineRoot() + "/Files/levelbank/testlevel/universe.dbu";
    QFileInfo dbuInfo(dbuPath);
    if (!dbuInfo.exists()) {
        res.fileExists = false;
        res.message = QStringLiteral("Файл universe.dbu не найден. Запустите Test Game (F9) в FPS Creator.");
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
                    res.message = QString("⚠ universe.dbu устарел (карта сохранена: %1, сборка: %2). Нажмите Test Game (F9).")
                                  .arg(res.mapTime.toString("HH:mm:ss")).arg(res.dbuTime.toString("HH:mm:ss"));
                } else {
                    res.message = QString("✓ universe.dbu актуален (собран %1 для этой карты)").arg(res.dbuTime.toString("HH:mm:ss"));
                }
            } else {
                res.message = QString("⚠ universe.dbu от ДРУГОЙ карты (собран %1). Для этой карты запустите Test Game (F9) в FPS Creator.")
                              .arg(res.dbuTime.toString("HH:mm:ss"));
            }
            return res;
        }
    }

    // Fallback if temp.fpm is absent
    res.matchesCurrentMap = !res.isOutdated;
    if (res.isOutdated) {
        res.message = QStringLiteral("⚠ universe.dbu устарел. Запустите Test Game (F9) в FPS Creator.");
    } else {
        res.message = QString("✓ universe.dbu найден (%1)").arg(res.dbuTime.toString("HH:mm:ss"));
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

        bool isHorizontal = (portal.box.maxY - portal.box.minY) < 50.0f;
        int seg = (layer >= 0 && layer < m_map->gridBlocks.size() &&
                   gy >= 0 && gy < m_map->gridBlocks[layer].size() &&
                   gx >= 0 && gx < m_map->gridBlocks[layer][gy].size())
                  ? m_map->gridBlocks[layer][gy][gx] : 0;

        // 1. Check for physical BSP ceiling or floor hole
        if (isHorizontal && seg <= 0) {
            bool hasRoomBelow = false;
            for (int l = 0; l < layer; ++l) {
                if (m_map->gridBlocks[l][gy][gx] > 0) { hasRoomBelow = true; break; }
            }
            if (hasRoomBelow && !reportedCells.contains(cellKey)) {
                reportedCells.insert(cellKey);
                PortalLeakWarning w;
                w.severity = PortalLeakWarning::ERROR;
                w.type = QStringLiteral("Compiled BSP Ceiling Hole");
                w.layer = layer;
                w.x = gx;
                w.y = gy;
                w.description = QStringLiteral("Compiled BSP portal %1 at 3D pos (%2, %3, %4) is an open hole in the ceiling into universe void. Missing ceiling slab at Floor %5 (%6, %7).")
                                .arg(i)
                                .arg(portal.box.cenX, 0, 'f', 0)
                                .arg(portal.box.cenY, 0, 'f', 0)
                                .arg(portal.box.cenZ, 0, 'f', 0)
                                .arg(layer)
                                .arg(gx)
                                .arg(gy);
                m_warnings.push_back(w);
                continue;
            }

            bool hasRoomAbove = false;
            for (int l = layer + 1; l < m_map->gridBlocks.size(); ++l) {
                if (m_map->gridBlocks[l][gy][gx] > 0) { hasRoomAbove = true; break; }
            }
            if (hasRoomAbove && !reportedCells.contains(cellKey)) {
                reportedCells.insert(cellKey);
                PortalLeakWarning w;
                w.severity = PortalLeakWarning::ERROR;
                w.type = QStringLiteral("Compiled BSP Floor Hole");
                w.layer = layer;
                w.x = gx;
                w.y = gy;
                w.description = QStringLiteral("Compiled BSP portal %1 at 3D pos (%2, %3, %4) is an open hole in the floor into universe void. Missing floor slab at Floor %5 (%6, %7).")
                                .arg(i)
                                .arg(portal.box.cenX, 0, 'f', 0)
                                .arg(portal.box.cenY, 0, 'f', 0)
                                .arg(portal.box.cenZ, 0, 'f', 0)
                                .arg(layer)
                                .arg(gx)
                                .arg(gy);
                m_warnings.push_back(w);
                continue;
            }
        }

        // 2. Check for portals touching outer limits or marked as leak
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

        for (const auto& pt : z.tiles) {
            bool covered = false;
            // Check if covered on the same floor or any floor above
            for (int l = z.floor; l <= m_map->header.layerMax; ++l) {
                int seg = m_map->gridBlocks[l][pt.y()][pt.x()];
                if (seg > 0) {
                    int g = mapGround(l, pt.x(), pt.y());
                    // Segment with roof or ceiling slab (ground == 2)
                    if (g == 2 || isCeilingAt(l, pt.x(), pt.y())) {
                        covered = true;
                        break;
                    }
                    // Any segment placed on a higher layer caps this column
                    if (l > z.floor) {
                        covered = true;
                        break;
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

    // 1. Same-layer Overlay conflicts (Base segment vs Overlay segment)
    for (int layer = 0; layer < m_map->gridBlocks.size(); ++layer) {
        for (int y = 0; y < m_map->gridBlocks[layer].size(); ++y) {
            for (int x = 0; x < m_map->gridBlocks[layer][y].size(); ++x) {
                int baseSeg = m_map->gridBlocks[layer][y][x];
                int olaySeg = (layer < m_map->gridOverlays.size() && y < m_map->gridOverlays[layer].size() && x < m_map->gridOverlays[layer][y].size())
                              ? m_map->gridOverlays[layer][y][x] : 0;
                if (baseSeg > 0 && olaySeg > 0) {
                    auto sBase = m_map->segments.value(baseSeg);
                    auto sOlay = m_map->segments.value(olaySeg);
                    if (sBase && sOlay) {
                        // An overlay without CSG punch will draw directly on top of the base segment -> true Z-fighting
                        if (!sOlay->hasPunch) {
                            PortalLeakWarning w;
                            w.severity = PortalLeakWarning::WARNING;
                            w.type = QStringLiteral("Coplanar Overlay Z-Fighting");
                            w.layer = layer; w.x = x; w.y = y;
                            w.description = QString("Overlay segment \"%1\" placed directly over \"%2\" on Floor %3 at (%4, %5) without CSG punch cutout. Causes severe in-game texture flickering (Z-fighting).")
                                                .arg(sOlay->name).arg(sBase->name).arg(layer).arg(x).arg(y);
                            m_warnings.push_back(w);
                        }
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

        for (const auto& pt : z.tiles) {
            int tile = (z.floor < m_map->gridTileType.size() && pt.y() < m_map->gridTileType[z.floor].size() && pt.x() < m_map->gridTileType[z.floor][pt.y()].size())
                       ? m_map->gridTileType[z.floor][pt.y()][pt.x()] : 0;
            int rot = (z.floor < m_map->gridRotation.size() && pt.y() < m_map->gridRotation[z.floor].size() && pt.x() < m_map->gridRotation[z.floor][pt.y()].size())
                      ? m_map->gridRotation[z.floor][pt.y()][pt.x()] : 0;

            for (int s = 0; s < 4; ++s) {
                int nx = pt.x() + dx[s];
                int ny = pt.y() + dy[s];

                // Only consider it an exterior breach if (nx, ny) is true universe void
                // (i.e. outside the map bounds or has zero segments on ANY floor)
                if (!isUniverseVoid(nx, ny)) continue;

                // If wall is present on this side, no leak
                if (isMaptileWallPresent(tile, rot, s)) continue;

                // Check if segment definition has an explicit wall mesh part on this side
                int segId = m_map->gridBlocks[z.floor][pt.y()][pt.x()];
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
                    if (p.floor == z.floor && (p.tileA == pt || p.tileB == pt)) {
                        hasDoor = true;
                        break;
                    }
                }
                if (hasDoor) continue;

                quint64 wallKey = (quint64(z.floor) << 36) | (quint64(pt.y()) << 20) | (quint64(pt.x()) << 4) | quint64(s);
                if (reportedWallLeaks.contains(wallKey)) continue;
                reportedWallLeaks.insert(wallKey);

                PortalLeakWarning w;
                w.severity = PortalLeakWarning::WARNING;
                w.type = "Missing Perimeter Wall (Void Leak)";
                w.layer = z.floor;
                w.x = pt.x();
                w.y = pt.y();
                w.description = QString("Open room edge at (%1, %2) [%3 edge] faces empty void without a wall or door. Camera may see universe void.")
                                    .arg(pt.x()).arg(pt.y()).arg(sideNames[s]);
                m_warnings.push_back(w);
            }
        }
    }
}
