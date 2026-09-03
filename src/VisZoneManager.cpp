#include "VisZoneManager.h"
#include <cmath>
#include <queue>
#include <set>
#include <QDebug>

bool VisZoneManager::isMaptileWallPresent(int l, int x, int y, int side) const {
    if (!m_map) return false;
    if (l < 0 || l >= static_cast<int>(m_map->gridBlocks.size())) return false;
    if (y < 0 || y >= static_cast<int>(m_map->gridBlocks[l].size())) return false;
    if (x < 0 || x >= static_cast<int>(m_map->gridBlocks[l][y].size())) return false;

    int b = m_map->gridBlocks[l][y][x];
    if (b <= 0) return false;
    auto seg = m_map->segments.value(b);
    if (!seg) return false;

    // Scenery props, platforms and stairs do not have standard wall ribbons
    if (seg->isPlatformOrGantry || seg->isStairs || seg->isScenery) return false;

    int maptile = m_map->gridTileType[l][y][x];
    int rot = m_map->gridRotation[l][y][x];
    if (maptile <= 0 || maptile == 6) return false;

    static const bool baseWalls[16][4] = {
        {0, 0, 0, 0}, // 0: none
        {1, 1, 1, 1}, // 1: 4 walls
        {1, 0, 1, 1}, // 2: 3 walls (no East)
        {1, 0, 0, 1}, // 3: 2 walls corner (North + West)
        {1, 0, 1, 0}, // 4: 2 walls opposite (North + South)
        {1, 0, 0, 0}, // 5: 1 wall (North)
        {0, 0, 0, 0}, // 6: 0 walls (floor only)
        {0, 0, 0, 0}, // 7: corner
        {0, 0, 0, 0}, // 8: corner
        {0, 0, 0, 0}, // 9: corner
        {0, 0, 0, 0}, // 10: corner
        {0, 0, 0, 0}, // 11: corner
        {1, 0, 0, 0}, // 12: straight
        {1, 0, 0, 0}, // 13: straight
        {1, 0, 0, 0}, // 14: straight
        {1, 0, 0, 1}  // 15: corner
    };
    int unrotatedSide = (side - (rot & 3) + 4) % 4;
    if (!seg->hasWall[unrotatedSide]) return false;

    if (maptile >= 0 && maptile < 16) {
        return baseWalls[maptile][unrotatedSide];
    }
    return false;
}

bool VisZoneManager::hasDoorwayOnEdge(int l, int x1, int y1, int x2, int y2, int sideFrom1) const {
    if (!m_map) return false;
    // Overlay punch check (doors, windows, cutouts)
    if (l >= 0 && l < m_map->gridOverlays.size()) {
        if (y1 >= 0 && y1 < m_map->gridOverlays[l].size() && x1 >= 0 && x1 < m_map->gridOverlays[l][y1].size()) {
            int o1 = m_map->gridOverlays[l][y1][x1];
            if (o1 > 0) {
                auto it = m_map->segments.find(o1);
                if (it != m_map->segments.end() && it.value()->hasPunch) {
                    int rot1 = m_map->gridOverlayRotation[l][y1][x1] & 3;
                    if (rot1 == sideFrom1) return true;
                }
            }
        }
        int sideFrom2 = (sideFrom1 + 2) % 4;
        if (y2 >= 0 && y2 < m_map->gridOverlays[l].size() && x2 >= 0 && x2 < m_map->gridOverlays[l][y2].size()) {
            int o2 = m_map->gridOverlays[l][y2][x2];
            if (o2 > 0) {
                auto it = m_map->segments.find(o2);
                if (it != m_map->segments.end() && it.value()->hasPunch) {
                    int rot2 = m_map->gridOverlayRotation[l][y2][x2] & 3;
                    if (rot2 == sideFrom2) return true;
                }
            }
        }
    }
    return false;
}

void VisZoneManager::buildFromMap(std::shared_ptr<FPSCMap> map, const QString& /*dbuPath*/) {
    m_map = map;
    m_zones.clear();
    m_portals.clear();
    m_tileZoneMap.clear();

    if (!m_map) return;

    int layers = m_map->gridBlocks.size();
    if (layers == 0) return;
    int rows = m_map->gridBlocks[0].size();
    if (rows == 0) return;
    int cols = m_map->gridBlocks[0][0].size();

    m_tileZoneMap.resize(layers);
    for (int l = 0; l < layers; ++l) {
        m_tileZoneMap[l].resize(rows, std::vector<int>(cols, -1));
    }

    partitionRooms();
    buildPortals();
    associateEntities();
    pruneOpenRoofZones();
}

void VisZoneManager::partitionRooms() {
    int layers = m_map->gridBlocks.size();
    if (layers == 0) return;
    int rows = m_map->gridBlocks[0].size();
    if (rows == 0) return;
    int cols = m_map->gridBlocks[0][0].size();

    auto hasCeilingBarrier = [&](int l, int x, int y) -> bool {
        if (l < 0 || l >= layers || y < 0 || y >= rows || x < 0 || x >= cols) return true;
        int b = m_map->gridBlocks[l][y][x];
        if (b <= 0) return false;
        int sym = (l < m_map->gridSymbol.size() && y < m_map->gridSymbol[l].size() && x < m_map->gridSymbol[l][y].size())
                  ? m_map->gridSymbol[l][y][x] : 0;
        if (sym == 1) return false; // mapsymbol == 1 hides vis.r in FPS Creator
        auto seg = m_map->segments.value(b);
        if (!seg || seg->isScenery) return false;
        return (seg->visRoof >= 0 || seg->hasRoofOnThisLayer);
    };

    auto hasFloorBarrier = [&](int l, int x, int y) -> bool {
        if (l < 0 || l >= layers || y < 0 || y >= rows || x < 0 || x >= cols) return true;
        int b = m_map->gridBlocks[l][y][x];
        if (b <= 0) return false;
        int sym = (l < m_map->gridSymbol.size() && y < m_map->gridSymbol[l].size() && x < m_map->gridSymbol[l][y].size())
                  ? m_map->gridSymbol[l][y][x] : 0;
        if (sym == 1) return false; // mapsymbol == 1 hides vis.f in FPS Creator
        auto seg = m_map->segments.value(b);
        if (!seg || seg->isScenery) return false;
        return (seg->visFloor >= 0 || seg->hasFloorOnThisLayer);
    };

    auto canPassVertical = [&](int lFrom, int lTo, int x, int y) -> bool {
        if (lFrom < 0 || lFrom >= layers || lTo < 0 || lTo >= layers) return false;
        if (y < 0 || y >= rows || x < 0 || x >= cols) return false;
        int bFrom = m_map->gridBlocks[lFrom][y][x];
        int bTo = m_map->gridBlocks[lTo][y][x];
        // Vertical pass is ONLY valid between actual placed segment blocks!
        if (bFrom <= 0 || bTo <= 0) return false;

        if (lTo == lFrom + 1) {
            if (hasCeilingBarrier(lFrom, x, y)) return false;
            if (hasFloorBarrier(lTo, x, y)) return false;
            return true;
        } else if (lTo == lFrom - 1) {
            if (hasCeilingBarrier(lTo, x, y)) return false;
            if (hasFloorBarrier(lFrom, x, y)) return false;
            return true;
        }
        return false;
    };

    auto hasStructureAbove = [&](int startL, int x, int y) -> bool {
        for (int k = startL; k < layers; ++k) {
            if (m_map->gridBlocks[k][y][x] > 0) return true;
        }
        return false;
    };

    auto canPassHorizontal = [&](int l, int x1, int y1, int x2, int y2, int sideFrom1) -> bool {
        int sideFrom2 = (sideFrom1 + 2) % 4;
        int b1 = m_map->gridBlocks[l][y1][x1];
        int b2 = m_map->gridBlocks[l][y2][x2];
        if (b1 > 0 && isMaptileWallPresent(l, x1, y1, sideFrom1)) return false;
        if (b2 > 0 && isMaptileWallPresent(l, x2, y2, sideFrom2)) return false;
        if (hasDoorwayOnEdge(l, x1, y1, x2, y2, sideFrom1)) return false; // Doorway is a PORTAL
        return true;
    };

    struct Cell3D { int l, x, y; };
    int nextZoneId = 0;

    auto floodFillZone = [&](int startL, int startX, int startY) {
        int currentZoneId = nextZoneId++;
        VisZone zone;
        zone.id = currentZoneId;
        zone.minFloor = startL;
        zone.maxFloor = startL;

        int hue = (currentZoneId * 137) % 360;
        zone.color = QColor::fromHsv(hue, 180, 240);

        int minX = startX, maxX = startX, minY = startY, maxY = startY;
        std::vector<Cell3D> q = {{startL, startX, startY}};
        m_tileZoneMap[startL][startY][startX] = currentZoneId;
        int head = 0;

        std::set<std::pair<int, int>> uniqueFootprint;

        while (head < static_cast<int>(q.size())) {
            Cell3D c = q[head++];
            zone.floorTiles[c.l].push_back(QPoint(c.x, c.y));
            uniqueFootprint.insert({c.x, c.y});

            minX = qMin(minX, c.x);
            maxX = qMax(maxX, c.x);
            minY = qMin(minY, c.y);
            maxY = qMax(maxY, c.y);
            zone.minFloor = qMin(zone.minFloor, c.l);
            zone.maxFloor = qMax(zone.maxFloor, c.l);

            // 4 horizontal neighbors
            const int dx[4] = {0, 1, 0, -1}; // N, E, S, W
            const int dy[4] = {-1, 0, 1, 0};
            for (int d = 0; d < 4; ++d) {
                int nx = c.x + dx[d], ny = c.y + dy[d];
                if (nx >= 0 && nx < cols && ny >= 0 && ny < rows) {
                    if (m_tileZoneMap[c.l][ny][nx] < 0) {
                        int nb = m_map->gridBlocks[c.l][ny][nx];
                        bool validTile = false;
                        if (nb > 0) {
                            auto nseg = m_map->segments.value(nb);
                            if (nseg && !nseg->isScenery) validTile = true;
                        } else if (c.l > 0 && m_tileZoneMap[c.l - 1][ny][nx] == currentZoneId) {
                            // Empty air tile directly above our room floor that is capped by a structure above
                            if (!hasCeilingBarrier(c.l - 1, nx, ny) && hasStructureAbove(c.l, nx, ny)) {
                                validTile = true;
                            }
                        }
                        if (validTile && canPassHorizontal(c.l, c.x, c.y, nx, ny, d)) {
                            m_tileZoneMap[c.l][ny][nx] = currentZoneId;
                            q.push_back({c.l, nx, ny});
                        }
                    }
                }
            }

            // Vertical up neighbor
            if (c.l + 1 < layers && m_tileZoneMap[c.l + 1][c.y][c.x] < 0) {
                int nbUp = m_map->gridBlocks[c.l + 1][c.y][c.x];
                bool allowUp = false;
                if (nbUp > 0) {
                    allowUp = canPassVertical(c.l, c.l + 1, c.x, c.y);
                } else if (!hasCeilingBarrier(c.l, c.x, c.y) && hasStructureAbove(c.l + 1, c.x, c.y)) {
                    allowUp = true;
                }
                if (allowUp) {
                    m_tileZoneMap[c.l + 1][c.y][c.x] = currentZoneId;
                    q.push_back({c.l + 1, c.x, c.y});
                }
            }

            // Vertical down neighbor
            if (c.l - 1 >= 0 && m_tileZoneMap[c.l - 1][c.y][c.x] < 0) {
                if (canPassVertical(c.l, c.l - 1, c.x, c.y)) {
                    m_tileZoneMap[c.l - 1][c.y][c.x] = currentZoneId;
                    q.push_back({c.l - 1, c.x, c.y});
                }
            }
        }

        zone.floor = zone.minFloor;
        zone.bounds = QRect(minX, minY, maxX - minX + 1, maxY - minY + 1);
        for (const auto& pt : uniqueFootprint) {
            zone.tiles.push_back(QPoint(pt.first, pt.second));
        }

        if (zone.minFloor == zone.maxFloor) {
            zone.name = QString("Zone %1 (Floor %2: %3 tiles)").arg(currentZoneId + 1).arg(zone.floor).arg(zone.tiles.size());
        } else {
            zone.name = QString("Zone %1 (Floors %2..%3: %4 tiles)").arg(currentZoneId + 1).arg(zone.minFloor).arg(zone.maxFloor).arg(zone.tiles.size());
        }
        m_zones.push_back(zone);
    };

    // Pass 1: Seed from floor slabs (bottom of rooms)
    for (int l = 0; l < layers; ++l) {
        for (int y = 0; y < rows; ++y) {
            for (int x = 0; x < cols; ++x) {
                int b = m_map->gridBlocks[l][y][x];
                if (b <= 0 || m_tileZoneMap[l][y][x] >= 0) continue;
                auto seg = m_map->segments.value(b);
                if (!seg || seg->isScenery) continue;
                if (!hasFloorBarrier(l, x, y)) continue;

                floodFillZone(l, x, y);
            }
        }
    }
}

void VisZoneManager::buildPortals() {
    int layers = m_map->gridBlocks.size();
    if (layers == 0) return;
    int rows = m_map->gridBlocks[0].size();
    int cols = m_map->gridBlocks[0][0].size();

    auto addPortal = [&](int l, int x1, int y1, int x2, int y2, int sideFrom1, bool isHorizontal) {
        int z1 = m_tileZoneMap[l][y1][x1];
        int z2 = m_tileZoneMap[l][y2][x2];
        if (z1 >= 0 && z2 >= 0 && z1 != z2) {
            // A portal between two adjacent zones can ONLY exist if:
            // 1. There is an actual doorway / window / cutout on this edge, OR
            // 2. There is NO solid wall between them (open archway / open passage connecting two rooms)
            bool doorway = hasDoorwayOnEdge(l, x1, y1, x2, y2, sideFrom1);
            int sideFrom2 = (sideFrom1 + 2) % 4;
            bool wall1 = isMaptileWallPresent(l, x1, y1, sideFrom1);
            bool wall2 = isMaptileWallPresent(l, x2, y2, sideFrom2);
            bool solidWall = (wall1 || wall2);

            // If there is a solid wall and NO doorway/punch, this wall OCCLUDES visibility; it is NOT a portal!
            if (solidWall && !doorway) {
                return;
            }

            // Check if portal already exists between these two tiles
            for (const auto& existing : m_portals) {
                if (existing.floor == l &&
                    ((existing.tileA == QPoint(x1, y1) && existing.tileB == QPoint(x2, y2)) ||
                     (existing.tileA == QPoint(x2, y2) && existing.tileB == QPoint(x1, y1)))) {
                    return;
                }
            }

            MapPortal portal;
            portal.id = static_cast<int>(m_portals.size());
            portal.floor = l;
            portal.tileA = QPoint(x1, y1);
            portal.tileB = QPoint(x2, y2);
            portal.zoneA = z1;
            portal.zoneB = z2;

            if (isHorizontal) {
                // Separates (x1, y1) and (x1, y1+1)
                float wy = (y1 + 1) * 100.0f; // POSITIVE Y in MapCanvas
                portal.lineWorld = QLineF(x1 * 100.0f, wy, (x1 + 1) * 100.0f, wy);
            } else {
                // Separates (x1, y1) and (x1+1, y1)
                float wx = (x1 + 1) * 100.0f;
                portal.lineWorld = QLineF(wx, y1 * 100.0f, wx, (y1 + 1) * 100.0f); // POSITIVE Y in MapCanvas
            }

            portal.name = QString("Portal #%1: Zone %2 <-> Zone %3")
                          .arg(portal.id + 1).arg(z1 + 1).arg(z2 + 1);

            m_zones[z1].portalIndices.push_back(portal.id);
            m_zones[z2].portalIndices.push_back(portal.id);
            m_portals.push_back(portal);
        }
    };

    // 1. Grid boundary portals between adjacent rooms
    for (int l = 0; l < layers; ++l) {
        for (int y = 0; y < rows; ++y) {
            for (int x = 0; x < cols; ++x) {
                if (m_tileZoneMap[l][y][x] < 0) continue;

                // Check East edge (sideFrom1 = 1)
                if (x + 1 < cols && m_tileZoneMap[l][y][x + 1] >= 0) {
                    addPortal(l, x, y, x + 1, y, 1, false);
                }

                // Check South edge (sideFrom1 = 2)
                if (y + 1 < rows && m_tileZoneMap[l][y + 1][x] >= 0) {
                    addPortal(l, x, y, x, y + 1, 2, true);
                }
            }
        }
    }
}

void VisZoneManager::associateEntities() {
    if (!m_map) return;
    int layers = m_tileZoneMap.size();
    if (layers == 0) return;
    int rows = m_tileZoneMap[0].size();
    int cols = m_tileZoneMap[0][0].size();

    for (int i = 0; i < m_map->placedEntities.size(); ++i) {
        const auto& e = m_map->placedEntities[i];
        int l = e.floorLayer;
        int tx = static_cast<int>(e.x / 100.0f);
        int ty = static_cast<int>(std::abs(e.z) / 100.0f);

        if (l >= 0 && l < layers && ty >= 0 && ty < rows && tx >= 0 && tx < cols) {
            int zId = m_tileZoneMap[l][ty][tx];
            if (zId >= 0 && zId < static_cast<int>(m_zones.size())) {
                m_zones[zId].entityIndices.push_back(i);
            }
        }
    }
}

const VisZone* VisZoneManager::getZone(int id) const {
    if (id >= 0 && id < static_cast<int>(m_zones.size())) {
        return &m_zones[id];
    }
    return nullptr;
}

const MapPortal* VisZoneManager::getPortal(int id) const {
    if (id >= 0 && id < static_cast<int>(m_portals.size())) {
        return &m_portals[id];
    }
    return nullptr;
}

std::vector<int> VisZoneManager::getZonesOnFloor(int floor) const {
    std::vector<int> result;
    for (size_t i = 0; i < m_zones.size(); ++i) {
        if (m_zones[i].hasFloor(floor)) {
            result.push_back(static_cast<int>(i));
        }
    }
    return result;
}

int VisZoneManager::getZoneAt(int floor, int x, int y) const {
    if (floor >= 0 && floor < static_cast<int>(m_tileZoneMap.size())) {
        if (y >= 0 && y < static_cast<int>(m_tileZoneMap[floor].size())) {
            if (x >= 0 && x < static_cast<int>(m_tileZoneMap[floor][y].size())) {
                return m_tileZoneMap[floor][y][x];
            }
        }
    }
    return -1;
}

bool VisZoneManager::isTileInZone(int zoneId, int floor, int x, int y) const {
    if (zoneId < 0) return true; // Show all
    return getZoneAt(floor, x, y) == zoneId;
}

bool VisZoneManager::isEntityInZone(int zoneId, int entityIndex) const {
    if (zoneId < 0) return true; // Show all
    const VisZone* z = getZone(zoneId);
    if (!z) return true;
    for (int idx : z->entityIndices) {
        if (idx == entityIndex) return true;
    }
    return false;
}

void VisZoneManager::recolorAllZones(int hueOffset) {
    for (size_t i = 0; i < m_zones.size(); ++i) {
        int hue = (static_cast<int>(i * 137) + hueOffset) % 360;
        if (hue < 0) hue += 360;
        m_zones[i].color = QColor::fromHsv(hue, 180, 240);
    }
}

void VisZoneManager::pruneOpenRoofZones() {
    std::vector<int> oldToNew(m_zones.size(), -1);
    std::vector<VisZone> cleanZones;
    int nextId = 0;

    for (size_t i = 0; i < m_zones.size(); ++i) {
        auto& z = m_zones[i];
        // Check if this zone has any walls or ceiling/structure above
        bool hasAnyWalls = false;
        bool hasCeilingAbove = false;
        for (const auto& pair : z.floorTiles) {
            int fl = pair.first;
            for (const auto& pt : pair.second) {
                for (int s = 0; s < 4; ++s) {
                    if (isMaptileWallPresent(fl, pt.x(), pt.y(), s)) {
                        hasAnyWalls = true;
                        break;
                    }
                }
                if (fl + 1 < static_cast<int>(m_map->gridBlocks.size()) &&
                    pt.y() < static_cast<int>(m_map->gridBlocks[fl + 1].size()) &&
                    pt.x() < static_cast<int>(m_map->gridBlocks[fl + 1][pt.y()].size())) {
                    if (m_map->gridBlocks[fl + 1][pt.y()][pt.x()] > 0) {
                        hasCeilingAbove = true;
                    }
                }
            }
        }

        // A bare exterior roof has NO portals, NO entities, NO walls, and NO ceiling above (open to the void)!
        if (!hasAnyWalls && z.portalIndices.empty() && z.entityIndices.empty() && !hasCeilingAbove) {
            for (const auto& pair : z.floorTiles) {
                int fl = pair.first;
                for (const auto& pt : pair.second) {
                    if (fl >= 0 && fl < static_cast<int>(m_tileZoneMap.size()) &&
                        pt.y() >= 0 && pt.y() < static_cast<int>(m_tileZoneMap[fl].size()) &&
                        pt.x() >= 0 && pt.x() < static_cast<int>(m_tileZoneMap[fl][pt.y()].size())) {
                        m_tileZoneMap[fl][pt.y()][pt.x()] = -1;
                    }
                }
            }
            continue;
        }

        z.id = nextId;
        oldToNew[i] = nextId++;
        int hue = (z.id * 137) % 360;
        z.color = QColor::fromHsv(hue, 180, 240);
        if (z.minFloor == z.maxFloor) {
            z.name = QString("Zone %1 (Floor %2: %3 tiles)").arg(z.id + 1).arg(z.floor).arg(z.tiles.size());
        } else {
            z.name = QString("Zone %1 (Floors %2..%3: %4 tiles)").arg(z.id + 1).arg(z.minFloor).arg(z.maxFloor).arg(z.tiles.size());
        }
        cleanZones.push_back(z);
    }

    // Update tileZoneMap IDs
    for (size_t l = 0; l < m_tileZoneMap.size(); ++l) {
        for (size_t y = 0; y < m_tileZoneMap[l].size(); ++y) {
            for (size_t x = 0; x < m_tileZoneMap[l][y].size(); ++x) {
                int oldId = m_tileZoneMap[l][y][x];
                if (oldId >= 0 && oldId < static_cast<int>(oldToNew.size())) {
                    m_tileZoneMap[l][y][x] = oldToNew[oldId];
                }
            }
        }
    }

    // Update portal zone IDs
    for (auto& portal : m_portals) {
        if (portal.zoneA >= 0 && portal.zoneA < static_cast<int>(oldToNew.size())) {
            portal.zoneA = oldToNew[portal.zoneA];
        }
        if (portal.zoneB >= 0 && portal.zoneB < static_cast<int>(oldToNew.size())) {
            portal.zoneB = oldToNew[portal.zoneB];
        }
    }

    m_zones = std::move(cleanZones);
}
