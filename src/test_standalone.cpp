#include <QApplication>
#include <iostream>
#include "FPMReader.h"
#include "VisZoneManager.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    // Test 1.fpm
    {
        QString fpmPath = "C:/Program Files (x86)/The Game Creators/FPS Creator/Files/mapbank/1.fpm";
        auto map = FPMReader::loadMap(fpmPath, "mypassword");
        VisZoneManager vm;
        vm.buildFromMap(map);
        printf("=== 1.fpm zones ===\n");
        for (size_t i = 0; i < vm.zones().size(); ++i) {
            const auto& z = vm.zones()[i];
            printf("  Zone %zu: minFloor=%d maxFloor=%d name='%s'\n",
                   i + 1, z.minFloor, z.maxFloor, qPrintable(z.name));
        }
    }

    // Test CloseContacts.fpm
    {
        QString fpmPath = "C:/Program Files (x86)/The Game Creators/FPS Creator/Files/mapbank/Slipgate/Full/1_CloseContacts.fpm";
        auto map = FPMReader::loadMap(fpmPath, "mypassword");
        VisZoneManager vm;
        vm.buildFromMap(map);
        printf("=== CloseContacts.fpm: checking zones with 0 portals and open edges ===\n");
        for (size_t i = 0; i < vm.zones().size(); ++i) {
            const auto& z = vm.zones()[i];
            int openEdges = 0;
            bool hasCeilingAbove = false;
            for (const auto& pair : z.floorTiles) {
                int fl = pair.first;
                for (const auto& pt : pair.second) {
                    const int dx[4] = {0, 1, 0, -1};
                    const int dy[4] = {-1, 0, 1, 0};
                    for (int s = 0; s < 4; ++s) {
                        int nx = pt.x() + dx[s], ny = pt.y() + dy[s];
                        bool inZone = false;
                        for (const auto& npt : pair.second) {
                            if (npt.x() == nx && npt.y() == ny) { inZone = true; break; }
                        }
                        if (!inZone) {
                            if (!vm.isMaptileWallPresent(fl, pt.x(), pt.y(), s)) {
                                openEdges++;
                            }
                        }
                    }
                    if (fl + 1 < static_cast<int>(map->gridBlocks.size()) &&
                        pt.y() < static_cast<int>(map->gridBlocks[fl + 1].size()) &&
                        pt.x() < static_cast<int>(map->gridBlocks[fl + 1][pt.y()].size())) {
                        if (map->gridBlocks[fl + 1][pt.y()][pt.x()] > 0) hasCeilingAbove = true;
                    }
                }
            }

            bool isPruned = z.portalIndices.empty() && !hasCeilingAbove && (openEdges > 0);
            printf("  Zone %2zu (id %2d): floor=%d minFloor=%d maxFloor=%d tiles=%2zu portals=%zu openEdges=%2d ceiling=%d -> %s\n",
                   i + 1, z.id, z.floor, z.minFloor, z.maxFloor, z.tiles.size(), z.portalIndices.size(),
                   openEdges, hasCeilingAbove, isPruned ? "PRUNED (ROOF)" : "KEEP (ROOM)");
        }
    }

    return 0;
}
