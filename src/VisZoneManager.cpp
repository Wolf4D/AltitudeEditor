#include "VisZoneManager.h"
#include <cmath>
#include <queue>
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

    // Structural roof slabs, platforms, stairs, and scenery have no interior room walls
    if (seg->groundMode == 2 || seg->isPlatformOrGantry || seg->isStairs || seg->isScenery) return false;
    if (seg->relPath.contains("ceiling", Qt::CaseInsensitive) || seg->name.contains("ceiling", Qt::CaseInsensitive)) return false;
    if (seg->relPath.contains("roof", Qt::CaseInsensitive) || seg->name.contains("roof", Qt::CaseInsensitive)) return false;

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

    // Door entity check (fast prefiltered lookup)
    float edgeMidX = (x1 + x2 + 1) * 50.0f;
    float edgeMidY = (y1 + y2 + 1) * 50.0f;
    if (l >= 0 && l < static_cast<int>(m_floorDoors.size())) {
        for (const auto& d : m_floorDoors[l]) {
            float dx = d.x - edgeMidX;
            float dy = d.y - edgeMidY;
            if (dx * dx + dy * dy < 55.0f * 55.0f) return true;
        }
    }
    return false;
}

void VisZoneManager::buildFromMap(std::shared_ptr<FPSCMap> map, const QString& /*dbuPath*/) {
    m_map = map;
    m_zones.clear();
    m_portals.clear();
    m_tileZoneMap.clear();
    m_floorDoors.clear();

    if (!m_map) return;

    int layers = m_map->gridBlocks.size();
    if (layers == 0) return;
    int rows = m_map->gridBlocks[0].size();
    if (rows == 0) return;
    int cols = m_map->gridBlocks[0][0].size();

    m_floorDoors.resize(layers);
    for (const auto& e : m_map->placedEntities) {
        if (e.floorLayer >= 0 && e.floorLayer < layers) {
            bool isDoor = (e.profile && e.profile->category == EntityCategory::Door) ||
                          (e.profile && e.profile->name.contains("door", Qt::CaseInsensitive)) ||
                          (e.instanceName.contains("door", Qt::CaseInsensitive));
            if (isDoor) {
                m_floorDoors[e.floorLayer].push_back({e.x, -e.z});
            }
        }
    }

    m_tileZoneMap.resize(layers);
    for (int l = 0; l < layers; ++l) {
        m_tileZoneMap[l].resize(rows, std::vector<int>(cols, -1));
    }

    partitionRooms();
    buildPortals();
    associateEntities();
}

void VisZoneManager::partitionRooms() {
    int layers = m_map->gridBlocks.size();
    int rows = m_map->gridBlocks[0].size();
    int cols = m_map->gridBlocks[0][0].size();

    auto canPass = [&](int l, int x1, int y1, int x2, int y2, int sideFrom1) -> bool {
        int sideFrom2 = (sideFrom1 + 2) % 4;
        if (isMaptileWallPresent(l, x1, y1, sideFrom1)) return false;
        if (isMaptileWallPresent(l, x2, y2, sideFrom2)) return false;
        if (hasDoorwayOnEdge(l, x1, y1, x2, y2, sideFrom1)) return false; // Doorway is a PORTAL, separate zones!
        return true;
    };

    int nextZoneId = 0;

    for (int l = 0; l < layers; ++l) {
        for (int y = 0; y < rows; ++y) {
            for (int x = 0; x < cols; ++x) {
                int b = m_map->gridBlocks[l][y][x];
                if (b <= 0 || m_tileZoneMap[l][y][x] >= 0) continue;

                auto seg = m_map->segments.value(b);
                if (!seg) continue;
                // Treat floor and ceiling/roof slabs equally as navigable horizontal surfaces!
                // Only skip pure decorative scenery props.
                if (seg->isScenery) continue;

                int currentZoneId = nextZoneId++;
                VisZone zone;
                zone.id = currentZoneId;
                zone.floor = l;

                // Golden ratio hue distribution for high distinctiveness
                int hue = (currentZoneId * 137) % 360;
                zone.color = QColor::fromHsv(hue, 180, 240);

                int minX = x, maxX = x, minY = y, maxY = y;

                std::vector<QPoint> q = {{x, y}};
                m_tileZoneMap[l][y][x] = currentZoneId;
                int head = 0;

                while (head < static_cast<int>(q.size())) {
                    QPoint pt = q[head++];
                    zone.tiles.push_back(pt);

                    minX = qMin(minX, pt.x());
                    maxX = qMax(maxX, pt.x());
                    minY = qMin(minY, pt.y());
                    maxY = qMax(maxY, pt.y());

                    const int dx[4] = {0, 1, 0, -1}; // N, E, S, W
                    const int dy[4] = {-1, 0, 1, 0};
                    for (int d = 0; d < 4; ++d) {
                        int nx = pt.x() + dx[d], ny = pt.y() + dy[d];
                        if (nx >= 0 && nx < cols && ny >= 0 && ny < rows) {
                            int nb = m_map->gridBlocks[l][ny][nx];
                            if (nb > 0 && m_tileZoneMap[l][ny][nx] < 0) {
                                auto nseg = m_map->segments.value(nb);
                                if (nseg && !nseg->isScenery) {
                                    if (canPass(l, pt.x(), pt.y(), nx, ny, d)) {
                                        m_tileZoneMap[l][ny][nx] = currentZoneId;
                                        q.push_back({nx, ny});
                                    }
                                }
                            }
                        }
                    }
                }

                zone.bounds = QRect(minX, minY, maxX - minX + 1, maxY - minY + 1);
                zone.name = QString("Zone %1 (Floor %2: %3 tiles)").arg(currentZoneId + 1).arg(l).arg(zone.tiles.size());
                m_zones.push_back(zone);
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

    // 2. Add doorway entity portals (including external doorways connecting room to outside)
    for (const auto& e : m_map->placedEntities) {
        bool isDoor = (e.profile && e.profile->category == EntityCategory::Door) ||
                      (e.profile && e.profile->name.contains("door", Qt::CaseInsensitive)) ||
                      (e.instanceName.contains("door", Qt::CaseInsensitive));
        if (isDoor) {
            int l = e.floorLayer;
            int tx = static_cast<int>(e.x / 100.0f);
            int ty = static_cast<int>(std::abs(e.z) / 100.0f);
            int zId = (l >= 0 && l < layers && ty >= 0 && ty < rows && tx >= 0 && tx < cols)
                      ? m_tileZoneMap[l][ty][tx] : -1;

            float cx = e.x;
            float cy = -e.z;
            int rotDeg = static_cast<int>(std::round(e.ry)) % 360;
            if (rotDeg < 0) rotDeg += 360;
            bool isNorthSouth = (rotDeg >= 45 && rotDeg < 135) || (rotDeg >= 225 && rotDeg < 315);

            QPointF p1, p2;
            if (isNorthSouth) {
                p1 = QPointF(cx, cy - 50.0f);
                p2 = QPointF(cx, cy + 50.0f);
            } else {
                p1 = QPointF(cx - 50.0f, cy);
                p2 = QPointF(cx + 50.0f, cy);
            }

            // Check if this door is already represented by an existing portal
            bool alreadyExists = false;
            for (const auto& existing : m_portals) {
                if (existing.floor == l) {
                    QPointF mid = (existing.lineWorld.p1() + existing.lineWorld.p2()) * 0.5f;
                    if (std::hypot(mid.x() - cx, mid.y() - cy) < 60.0f) {
                        alreadyExists = true;
                        break;
                    }
                }
            }

            if (!alreadyExists) {
                MapPortal portal;
                portal.id = static_cast<int>(m_portals.size());
                portal.floor = l;
                portal.tileA = QPoint(tx, ty);
                portal.tileB = QPoint(tx, ty);
                portal.zoneA = zId;
                portal.zoneB = -1; // Leads outside
                portal.lineWorld = QLineF(p1, p2);
                portal.name = QString("Portal (Doorway at %1, %2)").arg(tx).arg(ty);

                if (zId >= 0 && zId < static_cast<int>(m_zones.size())) {
                    m_zones[zId].portalIndices.push_back(portal.id);
                }
                m_portals.push_back(portal);
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
        if (m_zones[i].floor == floor) {
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
