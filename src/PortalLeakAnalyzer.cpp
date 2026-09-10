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

QString PortalLeakWarning::suppressionKey() const {
    QString canonicalType = QStringLiteral("Other");
    if (type.contains(QStringLiteral("Ceiling"), Qt::CaseInsensitive)) {
        canonicalType = QStringLiteral("CeilingLeak");
    } else if (type.contains(QStringLiteral("Perimeter"), Qt::CaseInsensitive)) {
        canonicalType = QStringLiteral("PerimeterWallLeak");
    } else if (type.contains(QStringLiteral("Double"), Qt::CaseInsensitive) || type.contains(QStringLiteral("Clash"), Qt::CaseInsensitive)) {
        canonicalType = QStringLiteral("DoubleWallClash");
    } else if (type.contains(QStringLiteral("Seam"), Qt::CaseInsensitive) || type.contains(QStringLiteral("Micro-Crack"), Qt::CaseInsensitive) || type.contains(QStringLiteral("Crack"), Qt::CaseInsensitive)) {
        canonicalType = QStringLiteral("MicroCrack");
    } else if (type.contains(QStringLiteral("Duplicate"), Qt::CaseInsensitive)) {
        canonicalType = QStringLiteral("DuplicateSegment");
    } else if (type.contains(QStringLiteral("Penetrat"), Qt::CaseInsensitive)) {
        canonicalType = QStringLiteral("Penetration");
    } else if (type.contains(QStringLiteral("Inverted"), Qt::CaseInsensitive)) {
        canonicalType = QStringLiteral("InvertedWall");
    } else if (type.contains(QStringLiteral("Void"), Qt::CaseInsensitive)) {
        canonicalType = QStringLiteral("VoidLeak");
    }

    if (isClash && x2 >= 0 && y2 >= 0) {
        int minX = std::min(x, x2), maxX = std::max(x, x2);
        int minY = std::min(y, y2), maxY = std::max(y, y2);
        return QStringLiteral("%1:L%2:(%3,%4)-(%5,%6)").arg(canonicalType).arg(layer).arg(minX).arg(minY).arg(maxX).arg(maxY);
    }
    return QStringLiteral("%1:L%2:(%3,%4)").arg(canonicalType).arg(layer).arg(x).arg(y);
}

PortalLeakAnalyzer::PortalLeakAnalyzer(std::shared_ptr<FPSCMap> map, std::shared_ptr<VisZoneManager> visZoneManager)
    : m_map(map), m_visZoneManager(visZoneManager) {
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
            
            int diffCount = 0;
            int totalChecked = 0;
            if (match) {
                for (int l = 0; l <= m_map->header.layerMax; ++l) {
                    if (l >= (int)m_map->gridBlocks.size() || l >= (int)tempMap->gridBlocks.size()) continue;
                    for (int y = 0; y <= m_map->header.maxY; ++y) {
                        for (int x = 0; x <= m_map->header.maxX; ++x) {
                            totalChecked++;
                            if (m_map->gridBlocks[l][y][x] != tempMap->gridBlocks[l][y][x]) {
                                diffCount++;
                            }
                        }
                    }
                }
            }

            bool sameLevel = match && (totalChecked > 0 && (float)diffCount / (float)totalChecked < 0.20f);
            res.matchesCurrentMap = sameLevel;
            if (sameLevel) {
                if (diffCount > 0 || res.isOutdated) {
                    res.isOutdated = true;
                    if (diffCount > 0) {
                        res.message = QCoreApplication::translate("PortalLeakAnalyzer", "⚠ universe.dbu has %1 modified tiles since last build (%2). Run Test Game (F9).")
                                      .arg(diffCount).arg(res.dbuTime.toString("HH:mm:ss"));
                    } else {
                        res.message = QCoreApplication::translate("PortalLeakAnalyzer", "⚠ universe.dbu is outdated (map saved: %1, build: %2). Run Test Game (F9).")
                                      .arg(res.mapTime.toString("HH:mm:ss")).arg(res.dbuTime.toString("HH:mm:ss"));
                    }
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

    if (!m_visZoneManager && m_map) {
        m_visZoneManager = std::make_shared<VisZoneManager>();
        m_visZoneManager->buildFromMap(m_map);
    }

    loadSegmentInfos();

    std::vector<PortalLeakWarning> staticWarnings;
    std::vector<PortalLeakWarning> physicalWarnings;

    // 1. Method 1: Check compiled universe.dbu (Primary ground-truth physics / BSP compiler)
    if (m_checkCompiledUniverse) {
        physicalWarnings = checkCompiledUniverse();
    }

    // 2. Method 2: Static map & segment geometry analysis
    if (m_checkStaticMap) {
        checkVerticalGaps(staticWarnings);
        checkCoplanarOverlaps(staticWarnings);
        checkWallHolesToVoid(staticWarnings);
        checkInvertedWalls(staticWarnings);
        checkDoubleWallClashes(staticWarnings);
        for (auto& w : staticWarnings) {
            w.isStaticMap = true;
        }
    }

    // 3. Merging & Deduplication when both modes are active
    if (m_checkCompiledUniverse && m_checkStaticMap && m_hasCompiledUniverse) {
        QMultiHash<quint64, int> staticCellIndex;
        for (int i = 0; i < static_cast<int>(staticWarnings.size()); ++i) {
            const auto& sw = staticWarnings[i];
            quint64 k = (quint64(sw.layer) << 32) | (quint64(sw.y) << 16) | quint64(sw.x);
            staticCellIndex.insert(k, i);
            if (sw.type.contains(QStringLiteral("Ceiling"), Qt::CaseInsensitive) && sw.layer > 0) {
                quint64 kBelow = (quint64(sw.layer - 1) << 32) | (quint64(sw.y) << 16) | quint64(sw.x);
                staticCellIndex.insert(kBelow, i);
            }
            if (sw.isClash && sw.x2 >= 0 && sw.y2 >= 0) {
                quint64 k2 = (quint64(sw.layer) << 32) | (quint64(sw.y2) << 16) | quint64(sw.x2);
                staticCellIndex.insert(k2, i);
            }
        }

        std::vector<PortalLeakWarning> physicalOnly;

        for (const auto& pw : physicalWarnings) {
            quint64 pk = (quint64(pw.layer) << 32) | (quint64(pw.y) << 16) | quint64(pw.x);
            auto matches = staticCellIndex.values(pk);
            if (matches.isEmpty() && pw.layer > 0) {
                quint64 pkBelow = (quint64(pw.layer - 1) << 32) | (quint64(pw.y) << 16) | quint64(pw.x);
                matches = staticCellIndex.values(pkBelow);
            }
            if (matches.isEmpty() && pw.layer < m_map->header.layerMax) {
                quint64 pkAbove = (quint64(pw.layer + 1) << 32) | (quint64(pw.y) << 16) | quint64(pw.x);
                matches = staticCellIndex.values(pkAbove);
            }

            if (!matches.isEmpty()) {
                int bestIdx = matches.first();
                auto& sw = staticWarnings[bestIdx];
                sw.isPhysicalBsp = true;
                sw.isMerged = true;
                sw.severity = PortalLeakWarning::ERROR;
                if (pw.hasPhysicalSize) {
                    sw.hasPhysicalSize = true;
                    sw.portalWidth = pw.portalWidth;
                    sw.portalHeight = pw.portalHeight;
                }
            } else {
                physicalOnly.push_back(pw);
            }
        }

        m_warnings = std::move(staticWarnings);
        for (auto& pw : physicalOnly) {
            m_warnings.push_back(std::move(pw));
        }
    } else if (m_checkCompiledUniverse && m_hasCompiledUniverse) {
        m_warnings = std::move(physicalWarnings);
    } else {
        m_warnings = std::move(staticWarnings);
    }

    // Populate zone details on all warnings (attributing leaks to the originating zone)
    for (auto& w : m_warnings) {
        if (w.zoneId < 0 && m_visZoneManager) {
            w.zoneId = m_visZoneManager->getZoneAt(w.layer, w.x, w.y);
        }
        if (w.zoneId < 0 && m_visZoneManager) {
            // 1. Search downwards (e.g. ceiling leaks on roof layer above an interior room)
            for (int l = w.layer - 1; l >= 0; --l) {
                int zid = m_visZoneManager->getZoneAt(l, w.x, w.y);
                if (zid >= 0) {
                    w.zoneId = zid;
                    break;
                }
            }
        }
        if (w.zoneId < 0 && m_visZoneManager) {
            // 2. Search upwards (e.g. floor gaps below an interior room)
            for (int l = w.layer + 1; l <= m_map->header.layerMax; ++l) {
                int zid = m_visZoneManager->getZoneAt(l, w.x, w.y);
                if (zid >= 0) {
                    w.zoneId = zid;
                    break;
                }
            }
        }
        if (w.zoneId < 0 && m_visZoneManager) {
            // 3. Search 4-neighbors on same layer (e.g. perimeter boundary wall leaks facing void)
            const int dx[4] = {0, 1, 0, -1};
            const int dy[4] = {-1, 0, 1, 0};
            for (int d = 0; d < 4; ++d) {
                int nx = w.x + dx[d];
                int ny = w.y + dy[d];
                int zid = m_visZoneManager->getZoneAt(w.layer, nx, ny);
                if (zid >= 0) {
                    w.zoneId = zid;
                    break;
                }
            }
        }
        if (w.zoneName.isEmpty() && w.zoneId >= 0 && m_visZoneManager) {
            const VisZone* z = m_visZoneManager->getZone(w.zoneId);
            if (z) w.zoneName = z->name;
        }
    }

    return m_warnings;
}

std::vector<PortalLeakWarning> PortalLeakAnalyzer::checkCompiledUniverse() {
    std::vector<PortalLeakWarning> res;
    auto val = validateCompiledUniverse();
    if (!val.fileExists) return res;

    if (!val.matchesCurrentMap) {
        PortalLeakWarning w;
        w.severity = PortalLeakWarning::WARNING;
        w.type = QCoreApplication::translate("PortalLeakAnalyzer", "Compiled BSP Mismatch");
        w.layer = 0; w.x = 0; w.y = 0;
        w.description = val.message;
        w.isPhysicalBsp = true;
        res.push_back(w);
        return res;
    }

    if (val.isOutdated) {
        PortalLeakWarning w;
        w.severity = PortalLeakWarning::WARNING;
        w.type = QCoreApplication::translate("PortalLeakAnalyzer", "Compiled BSP Outdated");
        w.layer = 0; w.x = 0; w.y = 0;
        w.description = val.message;
        w.isPhysicalBsp = true;
        res.push_back(w);
    }

    QString dbuPath = AssetManager::instance().engineRoot() + "/Files/levelbank/testlevel/universe.dbu";
    if (!m_dbuParser.parse(dbuPath)) return res;
    m_hasCompiledUniverse = true;

    loadSegmentInfos();

    QSet<quint64> reportedCells;

    auto isDoorOrWindowAt = [&](int layer, int gx, int gy) -> bool {
        if (!m_map) return false;
        if (layer < 0 || layer >= m_map->gridBlocks.size()) return false;
        if (gy < 0 || gy >= m_map->gridBlocks[layer].size()) return false;
        if (gx < 0 || gx >= m_map->gridBlocks[layer][gy].size()) return false;

        // Base block
        int b = m_map->gridBlocks[layer][gy][gx];
        if (b > 0) {
            auto info = m_segmentInfoCache.value(b - 1);
            if (info.hasVisportalmode && info.visportalmode > 0) return true;
            if (m_map->segments.contains(b)) {
                const auto& s = m_map->segments[b];
                if ((s->isWindow || s->name.contains("door", Qt::CaseInsensitive) || s->relPath.contains("door", Qt::CaseInsensitive)) &&
                    !s->name.contains("ceiling_window", Qt::CaseInsensitive) &&
                    !s->relPath.contains("ceiling_window", Qt::CaseInsensitive))
                    return true;
            }
        }
        // Overlay
        if (layer < m_map->gridOverlays.size() && gy < m_map->gridOverlays[layer].size() && gx < m_map->gridOverlays[layer][gy].size()) {
            int o = m_map->gridOverlays[layer][gy][gx];
            if (o > 0) {
                auto info = m_segmentInfoCache.value(o - 1);
                if (info.hasVisportalmode && info.visportalmode > 0) return true;
                if (m_map->segments.contains(o)) {
                    const auto& s = m_map->segments[o];
                    if ((s->isWindow || s->name.contains("door", Qt::CaseInsensitive) || s->relPath.contains("door", Qt::CaseInsensitive)) &&
                        !s->name.contains("ceiling_window", Qt::CaseInsensitive) &&
                        !s->relPath.contains("ceiling_window", Qt::CaseInsensitive))
                        return true;
                }
            }
        }
        // Placed entities (real doors/windows in walls, not ceiling_window prop)
        for (const auto& ent : m_map->placedEntities) {
            if (ent.floorLayer == layer || ent.floorLayer == layer - 1) {
                int ex = static_cast<int>(std::floor(ent.x / 100.0f));
                int ey = static_cast<int>(std::floor(std::abs(ent.z) / 100.0f));
                if (ex == gx && ey == gy) {
                    auto prof = m_map->entityProfiles.value(ent.bankIndex);
                    if (prof) {
                        if (prof->category == EntityCategory::Door) return true;
                        if ((prof->name.contains("door", Qt::CaseInsensitive) || prof->name.contains("window", Qt::CaseInsensitive)) &&
                            !prof->name.contains("ceiling_window", Qt::CaseInsensitive)) {
                            return true;
                        }
                    }
                }
            }
        }
        return false;
    };

    auto isUniverseVoidCell = [&](int x, int y) -> bool {
        if (!m_map) return true;
        if (x < 0 || x > m_map->header.maxX || y < 0 || y > m_map->header.maxY) return true;
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

    for (size_t i = 0; i < m_dbuParser.allPortals().size(); ++i) {
        const auto& portal = m_dbuParser.allPortals()[i];
        // Giant portals are DarkBASIC outdoor sky/terrain partitioning planes, not indoor room leaks
        if (portal.isExteriorHull) continue;
        if (!portal.isHorizontal() && (portal.spanX() > 300.0f || portal.spanZ() > 300.0f)) continue;
        if (portal.isHorizontal() && (portal.spanX() > 800.0f || portal.spanZ() > 800.0f)) continue;

        int layer = qBound(0, portal.layer(), (int)m_map->gridBlocks.size() - 1);
        float w = portal.width();
        float h = portal.height();
        bool isHoriz = portal.isHorizontal();
        bool isSubSeg = portal.isSubSegment();

        if (isHoriz) {
            // Horizontal portal (ceiling / floor)
            int minGX = qBound(0, static_cast<int>(std::floor(portal.box.minX / 100.0f)), m_map->header.maxX);
            int maxGX = qBound(0, static_cast<int>(std::floor((portal.box.maxX - 0.1f) / 100.0f)), m_map->header.maxX);
            int minGY = qBound(0, static_cast<int>(std::floor(std::abs(portal.box.maxZ) / 100.0f)), m_map->header.maxY);
            int maxGY = qBound(0, static_cast<int>(std::floor((std::abs(portal.box.minZ) - 0.1f) / 100.0f)), m_map->header.maxY);
            if (maxGX < minGX) std::swap(minGX, maxGX);
            if (maxGY < minGY) std::swap(minGY, maxGY);

            for (int cy = minGY; cy <= maxGY; ++cy) {
                for (int cx = minGX; cx <= maxGX; ++cx) {
                    // Find highest interior room layer in this column
                    int highestRoomLayer = -1;
                    int roomZid = -1;
                    if (m_visZoneManager) {
                        for (int l = m_map->header.layerMax; l >= 0; --l) {
                            int zid = m_visZoneManager->getZoneAt(l, cx, cy);
                            if (zid >= 0) {
                                highestRoomLayer = l;
                                roomZid = zid;
                                break;
                            }
                        }
                    }

                    // If no interior room exists in this column, skip (portal is over outdoor terrain / sky)
                    if (highestRoomLayer < 0) continue;

                    // If this horizontal portal is at or below the top of the room, it is inside the room volume (multi-story room / atrium)
                    if (layer <= highestRoomLayer) continue;

                    // Portal is above the top of the room. Check if the room is sealed by a ceiling or roof slab
                    bool sealed = false;
                    for (int l = highestRoomLayer; l <= layer; ++l) {
                        if (mapGround(l, cx, cy) == 2 || isCeilingAt(l, cx, cy) ||
                            (l > highestRoomLayer && isFloorAt(l, cx, cy))) {
                            sealed = true;
                            break;
                        }
                    }
                    if (sealed) continue;

                    bool isLeak = !isSubSeg;
                    bool isCrack = isSubSeg;

                    if (isLeak || isCrack) {
                        quint64 cellKey = (quint64(layer) << 32) | (quint64(cy) << 16) | quint64(cx);
                        if (!reportedCells.contains(cellKey)) {
                            reportedCells.insert(cellKey);
                            PortalLeakWarning warn;
                            warn.layer = layer;
                            warn.x = cx;
                            warn.y = cy;
                            warn.zoneId = roomZid;
                            if (m_visZoneManager) {
                                const VisZone* z = m_visZoneManager->getZone(roomZid);
                                if (z) warn.zoneName = z->name;
                            }
                            warn.isPhysicalBsp = true;
                            warn.isStaticMap = false;
                            warn.isMerged = false;
                            warn.portalWidth = w;
                            warn.portalHeight = h;
                            warn.hasPhysicalSize = (w > 0.0f && h > 0.0f);

                            if (isCrack) {
                                warn.severity = PortalLeakWarning::WARNING;
                                warn.type = QCoreApplication::translate("PortalLeakAnalyzer", "Physical Mesh Seam / Micro-Crack (CSG)");
                                warn.description = QCoreApplication::translate("PortalLeakAnalyzer",
                                    "Compiled BSP portal of sub-segment size detected at Floor %1 (%2, %3) without a door or window. Indicates misaligned geometry or CSG split seam.")
                                    .arg(layer).arg(cx).arg(cy);
                            } else {
                                QString propNote;
                                if (layer < m_map->gridOverlays.size() && cy < m_map->gridOverlays[layer].size() && cx < m_map->gridOverlays[layer][cy].size()) {
                                    int o = m_map->gridOverlays[layer][cy][cx];
                                    if (o > 0 && m_map->segments.contains(o)) {
                                        if (m_map->segments[o]->name.contains("ceiling_window", Qt::CaseInsensitive) ||
                                            m_map->segments[o]->relPath.contains("ceiling_window", Qt::CaseInsensitive)) {
                                            propNote = QCoreApplication::translate("PortalLeakAnalyzer", " (Segment '%1' placed here does not seal BSP portals)").arg(m_map->segments[o]->name);
                                        }
                                    }
                                }
                                if (propNote.isEmpty()) {
                                    for (const auto& ent : m_map->placedEntities) {
                                        if (ent.floorLayer == layer || ent.floorLayer == layer - 1) {
                                            int ex = static_cast<int>(std::floor(ent.x / 100.0f));
                                            int ey = static_cast<int>(std::floor(std::abs(ent.z) / 100.0f));
                                            if (ex == cx && ey == cy) {
                                                auto prof = m_map->entityProfiles.value(ent.bankIndex);
                                                QString entName = prof ? prof->name : ent.instanceName;
                                                if (entName.contains("ceiling_window", Qt::CaseInsensitive)) {
                                                    propNote = QCoreApplication::translate("PortalLeakAnalyzer", " (Entity '%1' placed here does not seal BSP portals)").arg(entName);
                                                    break;
                                                }
                                            }
                                        }
                                    }
                                }

                                warn.severity = PortalLeakWarning::ERROR;
                                warn.type = QCoreApplication::translate("PortalLeakAnalyzer", "Physical BSP Void Leak");
                                warn.description = QCoreApplication::translate("PortalLeakAnalyzer",
                                    "Unsealed ceiling opening opens directly into universe void at Floor %1 (%2, %3).")
                                    .arg(layer).arg(cx).arg(cy) + propNote;
                            }
                            res.push_back(warn);
                        }
                    }
                }
            }
        } else {
            // Vertical wall portal
            // In world space, a vertical portal lies on a boundary plane between adjacent grid cells.
            // Determine if this vertical portal is along an X plane (constant X) or Z plane (constant Z):
            bool isXPlane = (portal.spanX() < portal.spanZ());

            int cellA_X = 0, cellB_X = 0;
            int cellA_Y = 0, cellB_Y = 0;
            int minG = 0, maxG = 0;

            if (isXPlane) {
                // Portal lies on constant X plane: cellA is West (X-), cellB is East (X+)
                cellA_X = static_cast<int>(std::floor((portal.box.cenX - 5.0f) / 100.0f));
                cellB_X = static_cast<int>(std::floor((portal.box.cenX + 5.0f) / 100.0f));
                minG = qBound(0, static_cast<int>(std::floor(std::abs(portal.box.maxZ) / 100.0f)), m_map->header.maxY);
                maxG = qBound(0, static_cast<int>(std::floor((std::abs(portal.box.minZ) - 0.1f) / 100.0f)), m_map->header.maxY);
            } else {
                // Portal lies on constant Z plane: cellA is North (Z+ / smaller Y), cellB is South (Z- / larger Y)
                cellA_Y = static_cast<int>(std::floor((std::abs(portal.box.cenZ) - 5.0f) / 100.0f));
                cellB_Y = static_cast<int>(std::floor((std::abs(portal.box.cenZ) + 5.0f) / 100.0f));
                minG = qBound(0, static_cast<int>(std::floor(portal.box.minX / 100.0f)), m_map->header.maxX);
                maxG = qBound(0, static_cast<int>(std::floor((portal.box.maxX - 0.1f) / 100.0f)), m_map->header.maxX);
            }
            if (maxG < minG) std::swap(minG, maxG);

            for (int g = minG; g <= maxG; ++g) {
                int ax = isXPlane ? cellA_X : g;
                int ay = isXPlane ? g : cellA_Y;
                int bx = isXPlane ? cellB_X : g;
                int by = isXPlane ? g : cellB_Y;

                if (ax < 0 || ax > m_map->header.maxX || ay < 0 || ay > m_map->header.maxY) continue;
                if (bx < 0 || bx > m_map->header.maxX || by < 0 || by > m_map->header.maxY) continue;

                int zidA = -1;
                int zidB = -1;
                if (m_visZoneManager) {
                    zidA = m_visZoneManager->getZoneAt(layer, ax, ay);
                    zidB = m_visZoneManager->getZoneAt(layer, bx, by);
                }

                // If neither side is an interior room, this portal is outdoor terrain/void - ignore
                if (zidA < 0 && zidB < 0) continue;

                int roomZid = (zidA >= 0 ? zidA : zidB);
                int roomX = (zidA >= 0 ? ax : bx);
                int roomY = (zidA >= 0 ? ay : by);
                int outX  = (zidA >= 0 ? bx : ax);
                int outY  = (zidA >= 0 ? by : ay);

                bool hasDoorWin = isDoorOrWindowAt(layer, roomX, roomY);
                if (!hasDoorWin && layer > 0) {
                    hasDoorWin = isDoorOrWindowAt(layer - 1, roomX, roomY);
                }

                bool isLeak = false;
                bool isCrack = false;

                if (zidA >= 0 && zidB >= 0) {
                    // Portal connects two indoor rooms
                    if (isSubSeg && !hasDoorWin) {
                        isCrack = true;
                    }
                } else {
                    // One side is room, other side is exterior/void
                    int sideFacingOut = -1;
                    if (outX > roomX) sideFacingOut = 1;      // East
                    else if (outX < roomX) sideFacingOut = 3; // West
                    else if (outY < roomY) sideFacingOut = 0; // North
                    else if (outY > roomY) sideFacingOut = 2; // South

                    // Check if room has a solid wall on this side
                    bool hasWall = false;
                    int tile = (layer < m_map->gridTileType.size() && roomY < m_map->gridTileType[layer].size() && roomX < m_map->gridTileType[layer][roomY].size())
                               ? m_map->gridTileType[layer][roomY][roomX] : 0;
                    int rot = (layer < m_map->gridRotation.size() && roomY < m_map->gridRotation[layer].size() && roomX < m_map->gridRotation[layer][roomY].size())
                              ? m_map->gridRotation[layer][roomY][roomX] : 0;
                    if (sideFacingOut >= 0 && isMaptileWallPresent(tile, rot, sideFacingOut)) {
                        hasWall = true;
                    }

                    if (!hasDoorWin && !hasWall) {
                        // Flat roof slab (tile 6 or ground 2) open to the sky does not have side walls facing the void
                        bool isExteriorRoof = (tile == 6 || mapGround(layer, roomX, roomY) == 2 || isCeilingAt(layer, roomX, roomY));
                        if (isExteriorRoof && (layer + 1 >= m_map->gridBlocks.size() || m_map->gridBlocks[layer + 1][roomY][roomX] <= 0)) {
                            // Roof surface under open sky bordering void horizontally is not a perimeter wall leak
                        } else {
                            if (isSubSeg) isCrack = true;
                            else isLeak = true;
                        }
                    }
                }

                if (isLeak || isCrack) {
                    quint64 cellKey = (quint64(layer) << 32) | (quint64(roomY) << 16) | quint64(roomX);
                    if (!reportedCells.contains(cellKey)) {
                        reportedCells.insert(cellKey);
                        PortalLeakWarning warn;
                        warn.layer = layer;
                        warn.x = roomX;
                        warn.y = roomY;
                        warn.zoneId = roomZid;
                        if (m_visZoneManager) {
                            const VisZone* z = m_visZoneManager->getZone(roomZid);
                            if (z) warn.zoneName = z->name;
                        }
                        warn.isPhysicalBsp = true;
                        warn.isStaticMap = false;
                        warn.isMerged = false;
                        warn.portalWidth = w;
                        warn.portalHeight = h;
                        warn.hasPhysicalSize = (w > 0.0f && h > 0.0f);

                        if (isCrack) {
                            warn.severity = PortalLeakWarning::WARNING;
                            warn.type = QCoreApplication::translate("PortalLeakAnalyzer", "Physical Mesh Seam / Micro-Crack (CSG)");
                            warn.description = QCoreApplication::translate("PortalLeakAnalyzer",
                                "Compiled BSP portal of sub-segment size detected at Floor %1 (%2, %3) without a door or window. Indicates misaligned geometry or CSG split seam.")
                                .arg(layer).arg(roomX).arg(roomY);
                        } else {
                            warn.severity = PortalLeakWarning::ERROR;
                            warn.type = QCoreApplication::translate("PortalLeakAnalyzer", "Physical BSP Void Leak");
                            warn.description = QCoreApplication::translate("PortalLeakAnalyzer",
                                "Unsealed boundary opening opens directly into universe void at Floor %1 (%2, %3).")
                                .arg(layer).arg(roomX).arg(roomY);
                        }
                        res.push_back(warn);
                    }
                }
            }
        }
    }

    return res;
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

    // In FPS Creator engine, mapsymbol == 1 hides floor & roof!
    if (layer < m_map->gridSymbol.size() && y < m_map->gridSymbol[layer].size() && x < m_map->gridSymbol[layer][y].size()) {
        if (m_map->gridSymbol[layer][y][x] == 1) return false;
    }
    
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

void PortalLeakAnalyzer::checkVerticalGaps(std::vector<PortalLeakWarning>& outWarnings) {
    if (!m_map || !m_visZoneManager) return;
    const VisZoneManager& zm = *m_visZoneManager;

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
            // Find highest floor where this column is part of the zone volume or occupied by room blocks
            int topRoomLayer = z.floor;
            int colTop = z.floor;
            for (int l = z.floor; l <= m_map->header.layerMax; ++l) {
                if (zm.getZoneAt(l, pt.x(), pt.y()) == z.id) {
                    topRoomLayer = std::max(topRoomLayer, l);
                }
                if (m_map->gridBlocks[l][pt.y()][pt.x()] > 0) {
                    colTop = l;
                    if (l <= z.maxFloor) {
                        topRoomLayer = std::max(topRoomLayer, l);
                    }
                }
            }

            bool covered = false;
            // 1. Check if the segment directly at topRoomLayer has a ceiling/roof
            // Only valid if the zone does not have a higher roofline (zoneMaxCeilingFloor <= topRoomLayer)
            if (zoneMaxCeilingFloor <= topRoomLayer && m_map->gridBlocks[topRoomLayer][pt.y()][pt.x()] > 0) {
                int g = mapGround(topRoomLayer, pt.x(), pt.y());
                if (g == 2 || isCeilingAt(topRoomLayer, pt.x(), pt.y())) {
                    covered = true;
                }
            }
            // 2. Check layers above topRoomLayer up to layerMax
            if (!covered) {
                for (int l = topRoomLayer + 1; l <= m_map->header.layerMax; ++l) {
                    int seg = m_map->gridBlocks[l][pt.y()][pt.x()];
                    if (seg > 0) {
                        int g = mapGround(l, pt.x(), pt.y());
                        // Segment with roof or ceiling slab (ground == 2 or isCeilingAt)
                        if (g == 2 || isCeilingAt(l, pt.x(), pt.y())) {
                            covered = true;
                            break;
                        }
                        // A solid floor of another room directly on a higher floor also caps this column
                        if (isFloorAt(l, pt.x(), pt.y())) {
                            covered = true;
                            break;
                        }
                    }
                }
            }

            // 3. Wall mesh at or above the room's roofline
            if (!covered && zoneMaxCeilingFloor > 0 && colTop >= zoneMaxCeilingFloor) {
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

        // If room is predominantly roofed (>= 50%), but has missing ceiling tiles -> TRUE CEILING LEAK
        if (coverage >= 0.50f && coverage < 1.0f) {
            for (const auto& hole : missingTiles) {
                quint32 colKey = (static_cast<quint32>(hole.y()) << 16) | (static_cast<quint32>(hole.x()) & 0xFFFF);
                if (reportedCeilingColumns.contains(colKey)) continue;
                reportedCeilingColumns.insert(colKey);

                // Find highest occupied room layer in this column
                int topRoomLayer = z.floor;
                for (int l = z.floor; l <= m_map->header.layerMax; ++l) {
                    if (zm.getZoneAt(l, hole.x(), hole.y()) == z.id) {
                        topRoomLayer = std::max(topRoomLayer, l);
                    }
                    if (m_map->gridBlocks[l][hole.y()][hole.x()] > 0 && l <= z.maxFloor) {
                        topRoomLayer = std::max(topRoomLayer, l);
                    }
                }
                int missingRoofFloor = (zoneMaxCeilingFloor > topRoomLayer) ? zoneMaxCeilingFloor : (topRoomLayer + 1);
                if (missingRoofFloor > m_map->header.layerMax) missingRoofFloor = topRoomLayer;

                // Check if there is an entity prop placed at this hole (e.g. ceiling_window)
                QString propNote;
                QString bestEntName;
                for (const auto& ent : m_map->placedEntities) {
                    int ex = static_cast<int>(std::floor(ent.x / 100.0f));
                    int ey = static_cast<int>(std::floor(-ent.z / 100.0f));
                    if (ex == hole.x() && ey == hole.y()) {
                        int entFloor = static_cast<int>(std::floor(ent.y / 100.0f));
                        if (std::abs(entFloor - missingRoofFloor) <= 1) {
                            auto prof = m_map->entityProfiles.value(ent.bankIndex);
                            QString entName = prof ? prof->name : ent.instanceName;
                            if (!entName.isEmpty()) {
                                if (bestEntName.isEmpty() || entName.contains("window", Qt::CaseInsensitive)) {
                                    bestEntName = entName;
                                }
                            }
                        }
                    }
                }
                if (!bestEntName.isEmpty()) {
                    propNote = QString(" (Entity '%1' placed here does not seal BSP portals)").arg(bestEntName);
                }

                PortalLeakWarning w;
                w.severity = PortalLeakWarning::ERROR;
                w.type = "Missing Ceiling Leak";
                // Report on the layer where the ceiling tile is missing
                w.layer = missingRoofFloor;
                w.x = hole.x();
                w.y = hole.y();
                w.zoneId = z.id;
                w.zoneName = z.name;
                w.description = QString("Missing ceiling slab at Floor %1 over enclosed room at (%2, %3) (Room top: Floor %4)%5. Camera will leak visibility into the void.")
                                    .arg(missingRoofFloor).arg(hole.x()).arg(hole.y()).arg(topRoomLayer).arg(propNote);
                outWarnings.push_back(w);
            }
        }
    }
}

void PortalLeakAnalyzer::checkCoplanarOverlaps(std::vector<PortalLeakWarning>& outWarnings) {
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
                        outWarnings.push_back(w);
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
                            outWarnings.push_back(w);
                        }
                    }
                }
            }
        }
    }
}

void PortalLeakAnalyzer::checkWallHolesToVoid(std::vector<PortalLeakWarning>& outWarnings) {
    if (!m_map || !m_visZoneManager) return;
    const VisZoneManager& zm = *m_visZoneManager;

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

                    // Check overlay for fake (sealed) segments
                    if (fl < m_map->gridOverlays.size() && pt.y() < m_map->gridOverlays[fl].size() && pt.x() < m_map->gridOverlays[fl][pt.y()].size()) {
                        int oId = m_map->gridOverlays[fl][pt.y()][pt.x()];
                        if (oId > 0 && m_map->segments.contains(oId)) {
                            const auto& oSeg = m_map->segments[oId];
                            int oRot = m_map->gridOverlayRotation[fl][pt.y()][pt.x()] & 3;
                            if (oRot == s && oSeg->isFake) {
                                // Sealed solid decorative wall plate
                                continue;
                            }
                        }
                    }

                    // Check if there is a portal on this edge
                    const MapPortal* edgePortal = nullptr;
                    for (const auto& p : zm.portals()) {
                        if (p.floor == fl &&
                            ((p.tileA == pt && p.tileB == QPoint(nx, ny)) ||
                             (p.tileB == pt && p.tileA == QPoint(nx, ny)))) {
                            edgePortal = &p;
                            break;
                        }
                    }

                    if (edgePortal) {
                        if (edgePortal->isExterior) {
                            if (edgePortal->type == PortalType::ExteriorWindow) {
                                quint64 winKey = (quint64(fl) << 36) | (quint64(pt.y()) << 20) | (quint64(pt.x()) << 4) | quint64(s);
                                if (!reportedWallLeaks.contains(winKey)) {
                                    reportedWallLeaks.insert(winKey);
                                    PortalLeakWarning w;
                                    w.severity = PortalLeakWarning::WARNING;
                                    w.type = QStringLiteral("Exterior Window Leak (Visibility Bleed)");
                                    w.layer = fl;
                                    w.x = pt.x();
                                    w.y = pt.y();
                                    w.zoneId = z.id;
                                    w.zoneName = z.name;
                                    w.description = QString("Exterior window cutout at Floor %1 (%2, %3) side %4 opens into open sky / universe void. Causes PVS Visibility Bleed in BSP compiler.")
                                                        .arg(fl).arg(pt.x()).arg(pt.y()).arg(sideNames[s]);
                                    outWarnings.push_back(w);
                                }
                            } else {
                                quint64 doorKey = (quint64(fl) << 36) | (quint64(pt.y()) << 20) | (quint64(pt.x()) << 4) | quint64(s);
                                if (!reportedWallLeaks.contains(doorKey)) {
                                    reportedWallLeaks.insert(doorKey);
                                    PortalLeakWarning w;
                                    w.severity = PortalLeakWarning::WARNING;
                                    w.type = QStringLiteral("Exterior Doorway to Void");
                                    w.layer = fl;
                                    w.x = pt.x();
                                    w.y = pt.y();
                                    w.zoneId = z.id;
                                    w.zoneName = z.name;
                                    w.description = QString("Exterior doorway at Floor %1 (%2, %3) side %4 opens into universe void with no ground or platform below. Player will fall into the abyss.")
                                                        .arg(fl).arg(pt.x()).arg(pt.y()).arg(sideNames[s]);
                                    outWarnings.push_back(w);
                                }
                            }
                        }
                        continue;
                    }

                    quint64 wallKey = (quint64(fl) << 36) | (quint64(pt.y()) << 20) | (quint64(pt.x()) << 4) | quint64(s);
                    if (reportedWallLeaks.contains(wallKey)) continue;
                    reportedWallLeaks.insert(wallKey);

                    PortalLeakWarning w;
                    w.severity = PortalLeakWarning::WARNING;
                    w.type = "Missing Perimeter Wall (Void Leak)";
                    w.layer = fl;
                    w.x = pt.x();
                    w.y = pt.y();
                    w.zoneId = z.id;
                    w.zoneName = z.name;
                    w.description = QString("Perimeter wall missing at Floor %1 (%2, %3) side %4 facing universe void. Camera may leak into void.")
                                        .arg(fl).arg(pt.x()).arg(pt.y()).arg(sideNames[s]);
                    outWarnings.push_back(w);
                }
            }
        }
    }
}

void PortalLeakAnalyzer::checkInvertedWalls(std::vector<PortalLeakWarning>& outWarnings) {
    if (!m_map || !m_visZoneManager) return;
    const VisZoneManager& zm = *m_visZoneManager;

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
                w.zoneId = z.id;
                w.zoneName = z.name;
                w.description = QString("Exterior segment \"%1\" (groundmode=3) is placed inside an interior room at Floor %2 (%3, %4). Exterior facade faces inward into the room.")
                                    .arg(s->name).arg(z.floor).arg(pt.x()).arg(pt.y());
                outWarnings.push_back(w);
                continue;
            }
        }
    }
}

void PortalLeakAnalyzer::checkDoubleWallClashes(std::vector<PortalLeakWarning>& outWarnings) {
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
                            w.x2 = nx;
                            w.y2 = ny;
                            w.isClash = true;
                            w.sideA = s;
                            if (m_visZoneManager) {
                                w.zoneId = m_visZoneManager->getZoneAt(l, x, y);
                                w.zoneId2 = m_visZoneManager->getZoneAt(l, nx, ny);
                                const VisZone* za = m_visZoneManager->getZone(w.zoneId);
                                const VisZone* zb = m_visZoneManager->getZone(w.zoneId2);
                                if (za && zb && za->id != zb->id) {
                                    w.zoneName = QString("%1 / %2").arg(za->name.section(' ', 0, 1), zb->name.section(' ', 0, 1));
                                } else if (za) {
                                    w.zoneName = za->name;
                                } else if (zb) {
                                    w.zoneName = zb->name;
                                }
                            }
                            w.description = QString("Both adjacent rooms (\"%1\" and \"%2\") place solid walls on the exact same shared boundary at Floor %3 between (%4, %5) and (%6, %7) without a doorway. Causes severe in-game Z-fighting flickering and degenerate BSP portal bleed.")
                                                .arg(nameA).arg(nameB).arg(l).arg(x).arg(y).arg(nx).arg(ny);
                            outWarnings.push_back(w);
                        }
                    }
                }
            }
        }
    }
}
