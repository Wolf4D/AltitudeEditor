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
    if (maptile >= 0 && maptile < 16) {
        return baseWalls[maptile][unrotatedSide];
    }
    return false;
}

bool VisZoneManager::isDoorOrWindowOnEdge(int l, int x1, int y1, int x2, int y2, int sideFrom1, bool* isWindowOut) const {
    if (!m_map) return false;
    if (isWindowOut) *isWindowOut = false;

    // 1. Overlay punch check (doors, windows, cutouts)
    auto checkOverlay = [&](int segId, int rot, int side) -> bool {
        if (segId <= 0) return false;
        auto it = m_map->segments.find(segId);
        if (it != m_map->segments.end() && it.value()->hasPunch && !it.value()->isFake) {
            if ((rot & 3) == side) {
                if (isWindowOut) *isWindowOut = it.value()->isWindow;
                return true;
            }
        }
        return false;
    };

    if (l >= 0 && l < m_map->gridTileOverlays.size()) {
        if (y1 >= 0 && y1 < m_map->gridTileOverlays[l].size() && x1 >= 0 && x1 < m_map->gridTileOverlays[l][y1].size()) {
            for (const auto& o : m_map->gridTileOverlays[l][y1][x1]) {
                if (checkOverlay(o.segmentId, o.rotate, sideFrom1)) return true;
            }
        }
    } else if (l >= 0 && l < m_map->gridOverlays.size()) {
        if (y1 >= 0 && y1 < m_map->gridOverlays[l].size() && x1 >= 0 && x1 < m_map->gridOverlays[l][y1].size()) {
            int o1 = m_map->gridOverlays[l][y1][x1];
            int rot1 = m_map->gridOverlayRotation[l][y1][x1];
            if (checkOverlay(o1, rot1, sideFrom1)) return true;
        }
    }

    int sideFrom2 = (sideFrom1 + 2) % 4;
    if (l >= 0 && l < m_map->gridTileOverlays.size()) {
        if (y2 >= 0 && y2 < m_map->gridTileOverlays[l].size() && x2 >= 0 && x2 < m_map->gridTileOverlays[l][y2].size()) {
            for (const auto& o : m_map->gridTileOverlays[l][y2][x2]) {
                if (checkOverlay(o.segmentId, o.rotate, sideFrom2)) return true;
            }
        }
    } else if (l >= 0 && l < m_map->gridOverlays.size()) {
        if (y2 >= 0 && y2 < m_map->gridOverlays[l].size() && x2 >= 0 && x2 < m_map->gridOverlays[l][y2].size()) {
            int o2 = m_map->gridOverlays[l][y2][x2];
            int rot2 = m_map->gridOverlayRotation[l][y2][x2];
            if (checkOverlay(o2, rot2, sideFrom2)) return true;
        }
    }

    // 2. Placed entity check (doors, gates, windows on the shared border)
    float midX = 0, midZ = 0;
    if (sideFrom1 == 0) {
        midX = (x1 + 0.5f) * 100.0f;
        midZ = -y1 * 100.0f;
    } else if (sideFrom1 == 1) {
        midX = (x1 + 1.0f) * 100.0f;
        midZ = -(y1 + 0.5f) * 100.0f;
    } else if (sideFrom1 == 2) {
        midX = (x1 + 0.5f) * 100.0f;
        midZ = -(y1 + 1.0f) * 100.0f;
    } else if (sideFrom1 == 3) {
        midX = x1 * 100.0f;
        midZ = -(y1 + 0.5f) * 100.0f;
    }

    for (const auto& ent : m_map->placedEntities) {
        if (ent.floorLayer == l) {
            bool nearEdge = false;
            if (sideFrom1 == 0 || sideFrom1 == 2) {
                // Horizontal edge at midZ, spanning along X
                nearEdge = (std::abs(ent.z - midZ) <= 20.0f && std::abs(ent.x - midX) <= 50.0f);
            } else {
                // Vertical edge at midX, spanning along Z
                nearEdge = (std::abs(ent.x - midX) <= 20.0f && std::abs(ent.z - midZ) <= 50.0f);
            }

            if (nearEdge) {
                auto prof = m_map->entityProfiles.value(ent.bankIndex);
                QString name = (prof ? prof->name : ent.instanceName).toLower();
                QString path = (prof ? prof->relPath : QString()).toLower();
                path.replace("slipgate", "");
                path.replace("outdoor", "");

                bool isWin = name.contains("window") || name.contains("glass") ||
                             path.contains("window") || path.contains("glass");
                bool isDr = (prof && prof->category == EntityCategory::Door) ||
                            name.contains("door") || (name.contains("gate") && !name.contains("slipgate")) ||
                            path.contains("doors") || path.contains("\\gate") || path.contains("/gate");

                if (isWin || isDr) {
                    if (isWindowOut) *isWindowOut = isWin;
                    return true;
                }
            }
        }
    }

    return false;
}

bool VisZoneManager::hasDoorwayOnEdge(int l, int x1, int y1, int x2, int y2, int sideFrom1) const {
    return isDoorOrWindowOnEdge(l, x1, y1, x2, y2, sideFrom1, nullptr);
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
    pruneOpenRoofZones();
    associateEntities();
}

void VisZoneManager::partitionRooms() {
    int layers = m_map->gridBlocks.size();
    if (layers == 0) return;
    int rows = m_map->gridBlocks[0].size();
    if (rows == 0) return;
    int cols = m_map->gridBlocks[0][0].size();

    auto isExplicitCeilingSlab = [&](int l, int x, int y) -> bool {
        if (l < 0 || l >= layers || y < 0 || y >= rows || x < 0 || x >= cols) return false;
        int b = m_map->gridBlocks[l][y][x];
        if (b <= 0) return false;
        auto seg = m_map->segments.value(b);
        if (!seg || seg->isScenery) return false;

        // If the tile has walls, it is a wall segment of the room, NOT a flat ceiling slab!
        int maptile = (l < m_map->gridTileType.size() && y < m_map->gridTileType[l].size() && x < m_map->gridTileType[l][y].size())
                      ? m_map->gridTileType[l][y][x] : 0;
        if (maptile > 0 && maptile != 6) return false;

        int gnd = (l < m_map->gridGround.size() && y < m_map->gridGround[l].size() && x < m_map->gridGround[l][y].size())
                  ? m_map->gridGround[l][y][x] : 0;
        if (gnd > 1 && (!seg->hasFloorOnThisLayer || seg->visFloor == -1)) return true;

        return (seg->groundMode == 2 && seg->hasRoofOnThisLayer && !seg->hasFloorOnThisLayer) ||
               (seg->visFloor == -1 && seg->visRoof >= 0);
    };

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

        // If the layer below has an explicit ceiling slab, that ceiling acts as the floor!
        if (l > 0 && isExplicitCeilingSlab(l - 1, x, y)) return true;

        auto seg = m_map->segments.value(b);
        if (!seg || seg->isScenery) return false;
        return (seg->visFloor >= 0 || seg->hasFloorOnThisLayer);
    };

    auto hasCeilingEntityAt = [&](int l, int x, int y) -> bool {
        for (const auto& e : m_map->placedEntities) {
            if (e.floorLayer == l && int(e.x / 100.0f) == x && int(std::abs(e.z) / 100.0f) == y) {
                auto prof = m_map->entityProfiles.value(e.bankIndex);
                QString entName = (prof ? prof->name : e.instanceName).toLower();
                if (entName.contains("skylight") ||
                    entName.contains("ceiling_window") ||
                    entName.contains("roof_window") ||
                    (entName.contains("ceiling") && entName.contains("window"))) {
                    return true;
                }
            }
        }
        return false;
    };

    auto hasCeilingCappingColumn = [&](int startL, int x, int y) -> bool {
        // Ceiling capping this room column must be directly overhead within the building
        int maxL = layers;
        for (int k = startL; k < maxL; ++k) {
            int b = m_map->gridBlocks[k][y][x];
            if (b > 0) {
                auto seg = m_map->segments.value(b);
                if (seg && !seg->isScenery) {
                    if (seg->visRoof >= 0 || seg->hasRoofOnThisLayer || isExplicitCeilingSlab(k, x, y)) {
                        return true;
                    }
                    if (seg->visFloor >= 0 || seg->hasFloorOnThisLayer) {
                        return true;
                    }
                }
            }
            // Check if there is an entity capping this tile (e.g. ceiling window)
            if (hasCeilingEntityAt(k, x, y)) {
                return true;
            }
            // Check if k is a ceiling slab layer covering this room (at least 2 adjacent ceiling slabs)
            int roofNeighbors = 0;
            const int ddx[4] = {0, 1, 0, -1};
            const int ddy[4] = {-1, 0, 1, 0};
            for (int d = 0; d < 4; ++d) {
                int nx = x + ddx[d], ny = y + ddy[d];
                if (nx >= 0 && nx < cols && ny >= 0 && ny < rows) {
                    int nb = m_map->gridBlocks[k][ny][nx];
                    if (nb > 0) {
                        auto nseg = m_map->segments.value(nb);
                        if (nseg && !nseg->isScenery) {
                            if (nseg->visRoof >= 0 || nseg->hasRoofOnThisLayer || isExplicitCeilingSlab(k, nx, ny)) {
                                roofNeighbors++;
                            }
                        }
                    }
                }
            }
            if (roofNeighbors >= 2) return true;
        }
        return false;
    };

    auto canPassVertical = [&](int lFrom, int lTo, int x, int y) -> bool {
        if (lFrom < 0 || lFrom >= layers || lTo < 0 || lTo >= layers) return false;
        if (y < 0 || y >= rows || x < 0 || x >= cols) return false;
        int bFrom = m_map->gridBlocks[lFrom][y][x];
        int bTo = m_map->gridBlocks[lTo][y][x];
        // Vertical pass is ONLY valid between actual placed segment blocks!
        if (bFrom <= 0 || bTo <= 0) return false;

        int symFrom = (lFrom < m_map->gridSymbol.size() && y < m_map->gridSymbol[lFrom].size() && x < m_map->gridSymbol[lFrom][y].size())
                      ? m_map->gridSymbol[lFrom][y][x] : 0;
        int symTo = (lTo < m_map->gridSymbol.size() && y < m_map->gridSymbol[lTo].size() && x < m_map->gridSymbol[lTo][y].size())
                    ? m_map->gridSymbol[lTo][y][x] : 0;

        auto segFrom = m_map->segments.value(bFrom);
        auto segTo = m_map->segments.value(bTo);

        int lowerL = qMin(lFrom, lTo);
        int upperL = qMax(lFrom, lTo);
        auto segUpper = (lTo > lFrom) ? segTo : segFrom;
        auto segLower = (lTo > lFrom) ? segFrom : segTo;
        int symUpper = (lTo > lFrom) ? symTo : symFrom;

        // A ceiling slab terminates the room from above; cannot step through it!
        if (isExplicitCeilingSlab(upperL, x, y)) return false;

        // An exterior roof slab (gridGround == 2 or groundMode == 2) under open sky terminates the interior from above!
        int gndUpper = (upperL < m_map->gridGround.size() && y < m_map->gridGround[upperL].size() && x < m_map->gridGround[upperL][y].size())
                       ? m_map->gridGround[upperL][y][x] : 0;
        if (gndUpper > 1 || (segUpper && segUpper->groundMode == 2)) {
            if (!hasCeilingCappingColumn(upperL + 1, x, y)) {
                return false;
            }
        }

        // Check solid ceiling on lower layer: cannot pass up through a solid ceiling unless connected by stairs or upper has open floor (symUpper == 1)
        if (hasCeilingBarrier(lowerL, x, y)) {
            bool hasStairs = (segUpper && segUpper->isStairs) || (segLower && segLower->isStairs);
            if (!hasStairs && symUpper != 1) return false;
        }

        // Check solid floor on upper layer: cannot pass down through a solid floor unless open floor/stairs/platform
        if (hasFloorBarrier(upperL, x, y)) {
            bool hasStairs = (segUpper && (segUpper->isStairs || segUpper->isPlatformOrGantry)) ||
                             (segLower && segLower->isStairs);
            if (!hasStairs && symUpper != 1) return false;
        }

        return true;
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

    auto hasBlocksAbove = [&](int startL, int x, int y) -> bool {
        for (int k = startL + 1; k < layers; ++k) {
            if (m_map->gridBlocks[k][y][x] > 0) return true;
        }
        return false;
    };

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
                            if (nseg && !nseg->isScenery && !isExplicitCeilingSlab(c.l, nx, ny)) {
                                int ngnd = (c.l < m_map->gridGround.size() && ny < m_map->gridGround[c.l].size() && nx < m_map->gridGround[c.l][ny].size())
                                           ? m_map->gridGround[c.l][ny][nx] : 0;
                                int ntile = (c.l < m_map->gridTileType.size() && ny < m_map->gridTileType[c.l].size() && nx < m_map->gridTileType[c.l][ny].size())
                                            ? m_map->gridTileType[c.l][ny][nx] : 0;
                                bool isRoofSlabUnderSky = (ngnd == 2 && (ntile == 0 || ntile == 6) && !hasBlocksAbove(c.l, nx, ny));
                                if (!isRoofSlabUnderSky) {
                                    validTile = true;
                                }
                            }
                        } else if (c.l > 0) {
                            // Empty air tile inside the room:
                            // 1) Not a ceiling entity or ceiling slab on this layer
                            // 2) Has a ceiling capping the column above (indoors under a roof)
                            // 3) Has an indoor floor/story below (either continuing this zone, or a lower floor/room slab)
                            if (!hasCeilingEntityAt(c.l, nx, ny) && !isExplicitCeilingSlab(c.l, nx, ny) && hasCeilingCappingColumn(c.l + 1, nx, ny)) {
                                if (m_tileZoneMap[c.l - 1][ny][nx] == currentZoneId) {
                                    if (!isExplicitCeilingSlab(c.l - 1, nx, ny)) {
                                        validTile = true;
                                    }
                                } else if (m_tileZoneMap[c.l - 1][ny][nx] >= 0 || m_map->gridBlocks[c.l - 1][ny][nx] > 0) {
                                    int gndBelow = (c.l - 1 < m_map->gridGround.size() && ny < m_map->gridGround[c.l - 1].size() && nx < m_map->gridGround[c.l - 1][ny].size())
                                                   ? m_map->gridGround[c.l - 1][ny][nx] : 0;
                                    int bBelow = m_map->gridBlocks[c.l - 1][ny][nx];
                                    auto segBelow = m_map->segments.value(bBelow);
                                    bool isExteriorRoofBelow = (gndBelow > 1 || (segBelow && segBelow->groundMode == 2)) && !hasCeilingCappingColumn(c.l, nx, ny);
                                    if (!isExteriorRoofBelow) {
                                        validTile = true;
                                    }
                                }
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
                } else if (!hasCeilingEntityAt(c.l + 1, c.x, c.y) && !isExplicitCeilingSlab(c.l + 1, c.x, c.y)) {
                    // Empty air above: can step up ONLY IF layer c.l + 1 does not contain the ceiling entity itself,
                    // layer c.l is not an explicit ceiling slab, layer c.l is not an exterior roof slab,
                    // and there is a ceiling directly capping the room column!
                    int gndL = (c.l < m_map->gridGround.size() && c.y < m_map->gridGround[c.l].size() && c.x < m_map->gridGround[c.l][c.y].size())
                               ? m_map->gridGround[c.l][c.y][c.x] : 0;
                    bool isExteriorRoof = (gndL > 1) && !hasCeilingCappingColumn(c.l + 1, c.x, c.y);
                    if (!isExteriorRoof && !isExplicitCeilingSlab(c.l, c.x, c.y) && hasCeilingCappingColumn(c.l + 1, c.x, c.y)) {
                        allowUp = true;
                    }
                }
                if (allowUp) {
                    m_tileZoneMap[c.l + 1][c.y][c.x] = currentZoneId;
                    q.push_back({c.l + 1, c.x, c.y});
                }
            }

            // Vertical down neighbor
            if (c.l - 1 >= 0 && m_tileZoneMap[c.l - 1][c.y][c.x] < 0) {
                int nbDown = m_map->gridBlocks[c.l - 1][c.y][c.x];
                bool allowDown = false;
                if (nbDown > 0 && m_map->gridBlocks[c.l][c.y][c.x] > 0) {
                    allowDown = canPassVertical(c.l, c.l - 1, c.x, c.y);
                } else if (m_map->gridBlocks[c.l][c.y][c.x] == 0 && nbDown == 0) {
                    // Empty air stepping down into empty air under higher ceiling
                    if (hasCeilingCappingColumn(c.l, c.x, c.y)) {
                        allowDown = true;
                    }
                }
                if (allowDown) {
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
                if (seg->visFloor == -1 && seg->visRoof >= 0 && !seg->hasFloorOnThisLayer) continue; // Pure ceiling slabs never seed rooms
                int gnd = (l < m_map->gridGround.size() && y < m_map->gridGround[l].size() && x < m_map->gridGround[l][y].size())
                          ? m_map->gridGround[l][y][x] : 0;
                if (gnd > 1 && !seg->hasFloorOnThisLayer && !seg->isPlatformOrGantry && !seg->isStairs) continue; // Skip roof/ceiling slabs
                if ((gnd > 1 || seg->groundMode == 2) && !hasCeilingCappingColumn(l + 1, x, y)) continue; // Exterior roof under open sky
                if (!hasFloorBarrier(l, x, y)) continue;

                floodFillZone(l, x, y);
            }
        }
    }

    // Pass 2: Seed any remaining placed room segments (e.g. upper mezzanines, gantries, open shafts)
    for (int l = 0; l < layers; ++l) {
        for (int y = 0; y < rows; ++y) {
            for (int x = 0; x < cols; ++x) {
                int b = m_map->gridBlocks[l][y][x];
                if (b <= 0 || m_tileZoneMap[l][y][x] >= 0) continue;
                auto seg = m_map->segments.value(b);
                if (!seg || seg->isScenery) continue;
                if (seg->visFloor == -1 && seg->visRoof >= 0 && !seg->hasFloorOnThisLayer) continue; // Pure ceiling slabs never seed rooms
                if (seg->visFloor == -1 && !seg->hasFloorOnThisLayer && !seg->isPlatformOrGantry && !seg->isStairs) continue; // Pure walls never seed rooms
                int gnd = (l < m_map->gridGround.size() && y < m_map->gridGround[l].size() && x < m_map->gridGround[l][y].size())
                          ? m_map->gridGround[l][y][x] : 0;
                if (gnd > 1 && !seg->hasFloorOnThisLayer && !seg->isPlatformOrGantry && !seg->isStairs) continue; // Skip roof/ceiling slabs
                if ((gnd > 1 || seg->groundMode == 2) && !hasCeilingCappingColumn(l + 1, x, y)) continue; // Exterior roof under open sky

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

    auto addPortal = [&](int l, int x1, int y1, int x2, int y2, int sideFrom1) {
        int z1 = (y1 >= 0 && y1 < rows && x1 >= 0 && x1 < cols) ? m_tileZoneMap[l][y1][x1] : -1;
        int z2 = (y2 >= 0 && y2 < rows && x2 >= 0 && x2 < cols) ? m_tileZoneMap[l][y2][x2] : -1;

        if (z1 < 0 && z2 < 0) return;
        if (z1 == z2) return;

        // Standardize: z1 is always the room (>= 0), z2 is the neighbor (room or -1)
        int rx1 = x1, ry1 = y1, rx2 = x2, ry2 = y2, rside = sideFrom1;
        if (z1 < 0) {
            std::swap(z1, z2);
            std::swap(rx1, rx2);
            std::swap(ry1, ry2);
            rside = (rside + 2) % 4;
        }

        int b1 = (ry1 >= 0 && ry1 < rows && rx1 >= 0 && rx1 < cols) ? m_map->gridBlocks[l][ry1][rx1] : 0;
        if (b1 <= 0) return;

        bool isWindow = false;
        bool hasAperture = isDoorOrWindowOnEdge(l, rx1, ry1, rx2, ry2, rside, &isWindow);

        // Deduplication check: only one portal between (rx1, ry1) and (rx2, ry2)
        for (const auto& existing : m_portals) {
            if (existing.floor == l &&
                ((existing.tileA == QPoint(rx1, ry1) && existing.tileB == QPoint(rx2, ry2)) ||
                 (existing.tileA == QPoint(rx2, ry2) && existing.tileB == QPoint(rx1, ry1)))) {
                return;
            }
        }

        if (z2 >= 0) {
            // Case 1: Inter-zone portal between two indoor rooms
            int b2 = (ry2 >= 0 && ry2 < rows && rx2 >= 0 && rx2 < cols) ? m_map->gridBlocks[l][ry2][rx2] : 0;
            if (b2 <= 0) return;

            int sideFrom2 = (rside + 2) % 4;
            bool wall1 = isMaptileWallPresent(l, rx1, ry1, rside);
            bool wall2 = isMaptileWallPresent(l, rx2, ry2, sideFrom2);
            bool solidWall = (wall1 || wall2);

            // If solid wall and NO aperture, occluded!
            if (solidWall && !hasAperture) return;

            MapPortal portal;
            portal.id = static_cast<int>(m_portals.size());
            portal.floor = l;
            portal.tileA = QPoint(rx1, ry1);
            portal.tileB = QPoint(rx2, ry2);
            portal.zoneA = z1;
            portal.zoneB = z2;
            portal.isExterior = false;
            portal.type = isWindow ? PortalType::InterZoneWindow : PortalType::InterZoneDoorway;

            if (rside == 0) {
                portal.lineWorld = QLineF(rx1 * 100.0f, ry1 * 100.0f, (rx1 + 1) * 100.0f, ry1 * 100.0f);
            } else if (rside == 1) {
                portal.lineWorld = QLineF((rx1 + 1) * 100.0f, ry1 * 100.0f, (rx1 + 1) * 100.0f, (ry1 + 1) * 100.0f);
            } else if (rside == 2) {
                portal.lineWorld = QLineF(rx1 * 100.0f, (ry1 + 1) * 100.0f, (rx1 + 1) * 100.0f, (ry1 + 1) * 100.0f);
            } else {
                portal.lineWorld = QLineF(rx1 * 100.0f, ry1 * 100.0f, rx1 * 100.0f, (ry1 + 1) * 100.0f);
            }

            if (isWindow) {
                portal.name = QString("Window #%1: Zone %2 <-> Zone %3").arg(portal.id + 1).arg(z1 + 1).arg(z2 + 1);
            } else {
                portal.name = QString("Portal #%1: Zone %2 <-> Zone %3").arg(portal.id + 1).arg(z1 + 1).arg(z2 + 1);
            }

            m_zones[z1].portalIndices.push_back(portal.id);
            m_zones[z2].portalIndices.push_back(portal.id);
            m_portals.push_back(portal);

        } else {
            // Case 2: Exterior portal from indoor room to outside / sky (z2 == -1)
            if (!hasAperture) return;

            MapPortal portal;
            portal.id = static_cast<int>(m_portals.size());
            portal.floor = l;
            portal.tileA = QPoint(rx1, ry1);
            portal.tileB = QPoint(rx2, ry2);
            portal.zoneA = z1;
            portal.zoneB = -1;
            portal.isExterior = true;
            portal.type = isWindow ? PortalType::ExteriorWindow : PortalType::ExteriorDoorway;

            if (rside == 0) {
                portal.lineWorld = QLineF(rx1 * 100.0f, ry1 * 100.0f, (rx1 + 1) * 100.0f, ry1 * 100.0f);
            } else if (rside == 1) {
                portal.lineWorld = QLineF((rx1 + 1) * 100.0f, ry1 * 100.0f, (rx1 + 1) * 100.0f, (ry1 + 1) * 100.0f);
            } else if (rside == 2) {
                portal.lineWorld = QLineF(rx1 * 100.0f, (ry1 + 1) * 100.0f, (rx1 + 1) * 100.0f, (ry1 + 1) * 100.0f);
            } else {
                portal.lineWorld = QLineF(rx1 * 100.0f, ry1 * 100.0f, rx1 * 100.0f, (ry1 + 1) * 100.0f);
            }

            if (isWindow) {
                portal.name = QString("Exterior Window #%1: Zone %2 ➔ Sky").arg(portal.id + 1).arg(z1 + 1);
            } else {
                portal.name = QString("Exterior Door #%1: Zone %2 ➔ Outdoors").arg(portal.id + 1).arg(z1 + 1);
            }

            m_zones[z1].portalIndices.push_back(portal.id);
            m_portals.push_back(portal);
        }
    };

    const int dx[4] = {0, 1, 0, -1};
    const int dy[4] = {-1, 0, 1, 0};

    for (int l = 0; l < layers; ++l) {
        for (int y = 0; y < rows; ++y) {
            for (int x = 0; x < cols; ++x) {
                if (m_tileZoneMap[l][y][x] < 0) continue;
                for (int s = 0; s < 4; ++s) {
                    int nx = x + dx[s];
                    int ny = y + dy[s];
                    addPortal(l, x, y, nx, ny, s);
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

    int layers = m_map ? static_cast<int>(m_map->gridBlocks.size()) : 0;
    int rows = (layers > 0) ? static_cast<int>(m_map->gridBlocks[0].size()) : 0;
    int cols = (rows > 0) ? static_cast<int>(m_map->gridBlocks[0][0].size()) : 0;

    for (size_t i = 0; i < m_zones.size(); ++i) {
        auto& z = m_zones[i];
        // Count open exterior edges (edges facing outside the zone with no wall on either side)
        int openEdges = 0;
        int coveredTiles = 0;
        bool hasAnyWalls = false;
        for (const auto& pair : z.floorTiles) {
            int fl = pair.first;
            for (const auto& pt : pair.second) {
                int mt = (fl < static_cast<int>(m_map->gridTileType.size()) &&
                          pt.y() < static_cast<int>(m_map->gridTileType[fl].size()) &&
                          pt.x() < static_cast<int>(m_map->gridTileType[fl][pt.y()].size()))
                         ? m_map->gridTileType[fl][pt.y()][pt.x()] : 0;
                if (mt > 0 && mt != 6) {
                    hasAnyWalls = true;
                }

                const int dx[4] = {0, 1, 0, -1};
                const int dy[4] = {-1, 0, 1, 0};
                for (int s = 0; s < 4; ++s) {
                    int nx = pt.x() + dx[s], ny = pt.y() + dy[s];
                    bool inZone = false;
                    for (const auto& npt : pair.second) {
                        if (npt.x() == nx && npt.y() == ny) {
                            inZone = true;
                            break;
                        }
                    }
                    if (!inZone) {
                        bool myWall = isMaptileWallPresent(fl, pt.x(), pt.y(), s);
                        int oppSide = (s + 2) % 4;
                        bool neighborWall = (nx >= 0 && nx < cols && ny >= 0 && ny < rows) &&
                                            isMaptileWallPresent(fl, nx, ny, oppSide);
                        if (!myWall && !neighborWall) {
                            openEdges++;
                        }
                    }
                }
                bool tileCovered = false;
                if (fl + 1 < layers && pt.y() < rows && pt.x() < cols) {
                    if (m_map->gridBlocks[fl + 1][pt.y()][pt.x()] > 0) {
                        tileCovered = true;
                    }
                }
                int b = m_map->gridBlocks[fl][pt.y()][pt.x()];
                if (b > 0) {
                    int gnd = (fl < static_cast<int>(m_map->gridGround.size()) &&
                               pt.y() < static_cast<int>(m_map->gridGround[fl].size()) &&
                               pt.x() < static_cast<int>(m_map->gridGround[fl][pt.y()].size()))
                              ? m_map->gridGround[fl][pt.y()][pt.x()] : 0;
                    auto seg = m_map->segments.value(b);
                    // Standard room block on this layer with roof
                    if (seg && gnd != 2 && (seg->visRoof >= 0 || seg->hasRoofOnThisLayer)) {
                        tileCovered = true;
                    }
                }
                if (tileCovered) {
                    coveredTiles++;
                }
            }
        }

        bool hasCeilingAbove = (coveredTiles > static_cast<int>(z.tiles.size() * 0.5f));

        // An unenclosed exterior roof has NO portals, NO ceiling above, and open edges to the void!
        if (z.portalIndices.empty() && !hasCeilingAbove && openEdges > 0) {
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

    // Update portal zone IDs and remove dead portals connected to pruned zones
    std::vector<MapPortal> cleanPortals;
    for (auto& portal : m_portals) {
        int newA = (portal.zoneA >= 0 && portal.zoneA < static_cast<int>(oldToNew.size())) ? oldToNew[portal.zoneA] : -1;
        if (portal.isExterior) {
            if (newA >= 0) {
                portal.id = static_cast<int>(cleanPortals.size());
                portal.zoneA = newA;
                portal.zoneB = -1;
                if (portal.type == PortalType::ExteriorWindow) {
                    portal.name = QString("Exterior Window #%1: Zone %2 ➔ Sky").arg(portal.id + 1).arg(newA + 1);
                } else {
                    portal.name = QString("Exterior Door #%1: Zone %2 ➔ Outdoors").arg(portal.id + 1).arg(newA + 1);
                }
                cleanPortals.push_back(portal);
            }
        } else {
            int newB = (portal.zoneB >= 0 && portal.zoneB < static_cast<int>(oldToNew.size())) ? oldToNew[portal.zoneB] : -1;
            if (newA >= 0 && newB >= 0 && newA != newB) {
                portal.id = static_cast<int>(cleanPortals.size());
                portal.zoneA = newA;
                portal.zoneB = newB;
                if (portal.type == PortalType::InterZoneWindow) {
                    portal.name = QString("Window #%1: Zone %2 <-> Zone %3").arg(portal.id + 1).arg(newA + 1).arg(newB + 1);
                } else {
                    portal.name = QString("Portal #%1: Zone %2 <-> Zone %3").arg(portal.id + 1).arg(newA + 1).arg(newB + 1);
                }
                cleanPortals.push_back(portal);
            }
        }
    }
    m_portals = std::move(cleanPortals);
    for (auto& cz : cleanZones) {
        cz.portalIndices.clear();
    }
    for (size_t pid = 0; pid < m_portals.size(); ++pid) {
        if (m_portals[pid].zoneA >= 0 && m_portals[pid].zoneA < static_cast<int>(cleanZones.size())) {
            cleanZones[m_portals[pid].zoneA].portalIndices.push_back(static_cast<int>(pid));
        }
        if (m_portals[pid].zoneB >= 0 && m_portals[pid].zoneB < static_cast<int>(cleanZones.size())) {
            cleanZones[m_portals[pid].zoneB].portalIndices.push_back(static_cast<int>(pid));
        }
    }

    m_zones = std::move(cleanZones);
}
